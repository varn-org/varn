#include "varn/runtime/Runtime.h"

#include <gtest/gtest.h>
#include <lua.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

// A function with C linkage that the test adds under a name nothing exports.
extern "C" int varn_ffi_test_triple(int value)
{
    return value * 3;
}

namespace varn::ffi
{

namespace
{
class FfiTestHelpers
{
public:
    struct Record
    {
        int values[4];
        char name[16];
    };

    struct Quad
    {
        float parts[4];
    };

    struct Bits
    {
        unsigned char tag;
        unsigned flags : 3;
        unsigned mode : 5;
        int level : 4;
        unsigned char tail;
        unsigned long long wide : 40;
    };

    struct Mixed
    {
        float ratio;
        unsigned flag : 1;
        float scale;
    };

    static int negate(int value) { return -value; }

    static Record scaleRecord(Record record, int factor)
    {
        for (int& value : record.values)
        {
            value *= factor;
        }

        record.name[0] = 'S';
        return record;
    }

    static Quad reverseQuad(Quad quad) { return Quad{{quad.parts[3], quad.parts[2], quad.parts[1], quad.parts[0]}}; }

    static void fillBits(Bits* bits)
    {
        bits->tag = 0xAB;
        bits->flags = 5;
        bits->mode = 17;
        bits->level = -3;
        bits->tail = 0x7E;
        bits->wide = 0x12345678ABULL;
    }

    static int checkBits(Bits bits) { return bits.tag == 0x11 && bits.flags == 2 && bits.mode == 9 && bits.level == -1 && bits.tail == 0x22 && bits.wide == 0xABCDEF1234ULL; }

    static Mixed flipMixed(Mixed mixed)
    {
        mixed.ratio *= 2;
        mixed.flag = !mixed.flag;
        mixed.scale *= 3;
        return mixed;
    }

    static void publishSize(lua_State* L, const char* name, std::size_t value)
    {
        lua_pushinteger(L, static_cast<lua_Integer>(value));
        lua_setglobal(L, name);
    }

    static void publish(lua_State* L, const char* name, void* address)
    {
        lua_pushlightuserdata(L, address);
        lua_setglobal(L, name);
    }

    static std::string global(lua_State* L, const char* name)
    {
        lua_getglobal(L, name);
        const char* value = lua_tostring(L, -1);
        std::string text = value ? value : std::string();
        lua_pop(L, 1);
        return text;
    }

    static bool defined(lua_State* L, const char* name)
    {
        const bool found = lua_getglobal(L, name) != LUA_TNIL;
        lua_pop(L, 1);
        return found;
    }

    // Answers the address a script left in the global `callbackAddress` as the native function it points at.
    static int (*callback(lua_State* L))(int)
    {
        lua_getglobal(L, "callbackAddress");
        const auto address = static_cast<std::uintptr_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        return reinterpret_cast<int (*)(int)>(address);
    }
};

// Sets the failure handler and leaves in a global the address of a callback that always fails.
const char* const kFailingCallback = R"lua(
    local ffi = require("ffi")
    local async = require("async")

    async.onFailure(function(err, traceback)
        failure = err
        failureTraceback = traceback
        host.where()
    end)

    callback = ffi.cast("int (*)(int)", function(value)
        ran = true
        error("The callback refused " .. value .. ".")
    end)
    callbackAddress = ffi.tonumber(ffi.cast("uint64_t", callback))
)lua";
} // namespace

// A light userdata casts to a data pointer and to a function pointer, and both point at its address.
TEST(FfiCast, ALightUserdataCastsToAnyPointer)
{
    runtime::Runtime runtime(std::vector<std::string>{"varn"});
    lua_State* L = runtime.luaState();
    int stored = 41;

    FfiTestHelpers::publish(L, "stored", &stored);
    FfiTestHelpers::publish(L, "negate", reinterpret_cast<void*>(&FfiTestHelpers::negate));

    const char* script = R"lua(
        local ffi = require("ffi")

        local value = ffi.cast("int *", stored)
        assert(value[0] == 41, "The cast pointer should read the stored value.")
        value[0] = 42

        local call = ffi.cast("int (*)(int)", negate)
        assert(call(5) == -5, "The cast function pointer should call the native function.")
    )lua";

    EXPECT_EQ(runtime.runString(script, "=cast"), 0);
    EXPECT_EQ(stored, 42);
}

// A native function takes and returns by value structs whose fields are arrays, a float array included, which the platform passes in vector registers where it can.
TEST(FfiStructArrays, ANativeFunctionPassesThemByValue)
{
    runtime::Runtime runtime(std::vector<std::string>{"varn"});
    lua_State* L = runtime.luaState();

    FfiTestHelpers::publish(L, "scaleRecord", reinterpret_cast<void*>(&FfiTestHelpers::scaleRecord));
    FfiTestHelpers::publish(L, "reverseQuad", reinterpret_cast<void*>(&FfiTestHelpers::reverseQuad));

    const char* script = R"lua(
        local ffi = require("ffi")

        ffi.cdef [[
            typedef struct { int values[4]; char name[16]; } record_t;
            typedef struct { float parts[4]; } quad_t;
        ]]

        local scale = ffi.cast("record_t (*)(record_t, int)", scaleRecord)
        local scaled = scale(ffi.new("record_t", { { 1, 2, 3, 4 }, "knight" }), 3)
        assert(scaled.values[0] == 3 and scaled.values[3] == 12, "The native function should scale every value.")
        assert(ffi.string(scaled.name) == "Snight", "The native function should return the name it changed.")

        local reverse = ffi.cast("quad_t (*)(quad_t)", reverseQuad)
        local reversed = reverse(ffi.new("quad_t", { { 1.5, 2.5, 3.5, 4.5 } }))
        assert(reversed.parts[0] == 4.5 and reversed.parts[1] == 3.5, "The native function should return the parts reversed.")
        assert(reversed.parts[2] == 2.5 and reversed.parts[3] == 1.5, "The native function should return the parts reversed.")
    )lua";

    EXPECT_EQ(runtime.runString(script, "=arrays"), 0);
}

// A symbol the host added is found through `ffi.C` by the name it was added under, which no library exports.
TEST(FfiSymbols, AnAddedSymbolIsCalledThroughTheDefaultNamespace)
{
    runtime::Runtime runtime(std::vector<std::string>{"varn"});
    ASSERT_TRUE(runtime.addSymbol("varn_ffi_hidden_triple", reinterpret_cast<void*>(&varn_ffi_test_triple)));

    const char* script = R"lua(
        local ffi = require("ffi")

        ffi.cdef [[
            int varn_ffi_hidden_triple(int value);
            int varn_ffi_hidden_missing(int value);
        ]]

        assert(ffi.C.varn_ffi_hidden_triple(14) == 42, "The added symbol should be called through \"ffi.C\".")

        local ok, err = pcall(function()
            return ffi.C.varn_ffi_hidden_missing
        end)
        assert(not ok and err:find('"varn_ffi_hidden_missing"', 1, true), "A symbol nobody added or exports should be refused.")
    )lua";

    EXPECT_EQ(runtime.runString(script, "=symbols"), 0);
}

// A callback that fails while no ffi call runs answers zero and hands its error and its traceback to the failure handler at once.
TEST(FfiCallbacks, AFailureOutsideAnyCallReachesTheHandler)
{
    runtime::Runtime runtime(std::vector<std::string>{"varn"});
    lua_State* L = runtime.luaState();
    std::thread::id handlerThread;

    // clang-format off
    runtime.registerHostFunction("where", [&handlerThread](const std::string&)
    {
        handlerThread = std::this_thread::get_id();
        return std::string("null");
    });
    // clang-format on

    ASSERT_EQ(runtime.runString(kFailingCallback, "=outside"), 0);

    EXPECT_EQ(FfiTestHelpers::callback(L)(7), 0);
    EXPECT_EQ(handlerThread, std::this_thread::get_id());
    EXPECT_NE(FfiTestHelpers::global(L, "failure").find("The callback refused 7."), std::string::npos);
    EXPECT_NE(FfiTestHelpers::global(L, "failureTraceback").find("stack traceback:"), std::string::npos);
}

// A callback called from another thread answers zero without running Lua, and the handler receives the failure on the thread that runs Lua.
TEST(FfiCallbacks, ACallFromAnotherThreadReachesTheHandlerOnTheLuaThread)
{
    runtime::Runtime runtime(std::vector<std::string>{"varn"});
    lua_State* L = runtime.luaState();
    std::thread::id handlerThread;

    // clang-format off
    runtime.registerHostFunction("where", [&handlerThread](const std::string&)
    {
        handlerThread = std::this_thread::get_id();
        return std::string("null");
    });
    // clang-format on

    ASSERT_EQ(runtime.runString(kFailingCallback, "=thread"), 0);

    int (*callback)(int) = FfiTestHelpers::callback(L);
    int answer = -1;
    // clang-format off
    std::thread caller([callback, &answer]
    {
        answer = callback(7);
    });
    // clang-format on
    caller.join();

    EXPECT_EQ(answer, 0);
    EXPECT_FALSE(FfiTestHelpers::defined(L, "failure"));

    runtime.poll();

    const std::string failure = FfiTestHelpers::global(L, "failure");
    EXPECT_EQ(handlerThread, std::this_thread::get_id());
    EXPECT_NE(failure.find("\"int (*)(int)\""), std::string::npos);
    EXPECT_NE(failure.find("another thread"), std::string::npos);
    EXPECT_FALSE(FfiTestHelpers::defined(L, "ran"));
}

// Bitfields are laid out, read and written the way the C++ compiler that built the runtime lays them out, and pass to native functions by pointer and by value.
TEST(FfiBitfields, TheLayoutMatchesTheCompiler)
{
    runtime::Runtime runtime(std::vector<std::string>{"varn"});
    lua_State* L = runtime.luaState();

    FfiTestHelpers::publish(L, "fillBits", reinterpret_cast<void*>(&FfiTestHelpers::fillBits));
    FfiTestHelpers::publish(L, "checkBits", reinterpret_cast<void*>(&FfiTestHelpers::checkBits));
    FfiTestHelpers::publish(L, "flipMixed", reinterpret_cast<void*>(&FfiTestHelpers::flipMixed));
    FfiTestHelpers::publishSize(L, "bitsSize", sizeof(FfiTestHelpers::Bits));
    FfiTestHelpers::publishSize(L, "bitsTail", offsetof(FfiTestHelpers::Bits, tail));
    FfiTestHelpers::publishSize(L, "mixedSize", sizeof(FfiTestHelpers::Mixed));
    FfiTestHelpers::publishSize(L, "mixedScale", offsetof(FfiTestHelpers::Mixed, scale));

    const char* script = R"lua(
        local ffi = require("ffi")

        ffi.cdef [[
            typedef struct { unsigned char tag; unsigned flags : 3; unsigned mode : 5; int level : 4; unsigned char tail; unsigned long long wide : 40; } bits_t;
            typedef struct { float ratio; unsigned flag : 1; float scale; } mixed_t;
        ]]

        assert(ffi.sizeof("bits_t") == bitsSize, "The struct \"bits_t\" should be as large as the compiler makes it.")
        assert(ffi.offsetof("bits_t", "tail") == bitsTail, "The field \"tail\" should sit where the compiler puts it.")
        assert(ffi.sizeof("mixed_t") == mixedSize, "The struct \"mixed_t\" should be as large as the compiler makes it.")
        assert(ffi.offsetof("mixed_t", "scale") == mixedScale, "The field \"scale\" should sit where the compiler puts it.")

        local bits = ffi.new("bits_t")
        ffi.cast("void (*)(bits_t *)", fillBits)(bits)
        assert(bits.tag == 0xAB and bits.flags == 5 and bits.mode == 17, "The bitfields the compiler wrote should read back.")
        assert(bits.level == -3 and bits.tail == 0x7E and bits.wide == 0x12345678AB, "The bitfields the compiler wrote should read back.")

        bits.tag = 0x11
        bits.flags = 2
        bits.mode = 9
        bits.level = -1
        bits.tail = 0x22
        bits.wide = 0xABCDEF1234
        assert(ffi.cast("int (*)(bits_t)", checkBits)(bits) == 1, "The compiler should read the bitfields Lua wrote.")

        local flipped = ffi.cast("mixed_t (*)(mixed_t)", flipMixed)(ffi.new("mixed_t", { 1.5, 0, 2.5 }))
        assert(flipped.ratio == 3 and flipped.flag == 1 and flipped.scale == 7.5, "A struct of floats and a bitfield should pass by value both ways.")
    )lua";

    EXPECT_EQ(runtime.runString(script, "=bitfields"), 0);
}

} // namespace varn::ffi
