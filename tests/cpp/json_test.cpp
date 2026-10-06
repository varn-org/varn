#include "varn/json/JsonModule.h"
#include "varn/json/JsonSerializer.h"

#include <gtest/gtest.h>
#include <lua.hpp>

#include <memory>
#include <string>

namespace
{
class JsonTestHelpers
{
public:
    static std::unique_ptr<lua_State, decltype(&lua_close)> open()
    {
        std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), &lua_close);
        luaL_openlibs(state.get());
        varn::json::JsonModule::install(state.get());
        return state;
    }

    // Runs a chunk and answers its error message, or an empty string when it succeeds.
    static std::string run(lua_State* L, const char* chunk)
    {
        if (luaL_dostring(L, chunk) == LUA_OK)
        {
            return {};
        }

        std::string message = lua_tostring(L, -1);
        lua_pop(L, 1);
        return message;
    }

    // Serializes its first argument for the host, the way the bridge of the runtime does.
    static int serializeArgument(lua_State* L)
    {
        const std::string text = varn::json::JsonSerializer::serialize(L, 1);
        lua_pushlstring(L, text.data(), text.size());
        return 1;
    }
};
} // namespace

namespace varn::json
{

TEST(JsonSerializer, SerializeLeavesTheStackAsItFoundIt)
{
    auto state = JsonTestHelpers::open();
    lua_State* L = state.get();

    lua_newtable(L);
    lua_pushinteger(L, 7);
    lua_setfield(L, -2, "n");
    const int top = lua_gettop(L);

    EXPECT_EQ(JsonSerializer::serialize(L, -1), "{\"n\":7}");
    EXPECT_EQ(lua_gettop(L), top);
}

TEST(JsonSerializer, SerializeRaisesALuaErrorThatNamesTheKey)
{
    auto state = JsonTestHelpers::open();
    lua_State* L = state.get();

    lua_pushcfunction(L, &JsonTestHelpers::serializeArgument);
    lua_setglobal(L, "serialize");

    const std::string message = JsonTestHelpers::run(L, "serialize({ handler = { on = print } })");
    EXPECT_NE(message.find("The value at key \"handler.on\" is a function and cannot be encoded."), std::string::npos) << message;
    EXPECT_EQ(JsonTestHelpers::run(L, "assert(serialize({ 1, 2 }) == '[1,2]')"), "");
}

TEST(JsonSerializer, DeserializeRefusesInvalidTextWithoutPushing)
{
    auto state = JsonTestHelpers::open();
    lua_State* L = state.get();

    const int top = lua_gettop(L);
    EXPECT_FALSE(JsonSerializer::deserialize(L, "{\"a\":"));
    EXPECT_EQ(lua_gettop(L), top);

    ASSERT_TRUE(JsonSerializer::deserialize(L, "[1,null,3]"));
    EXPECT_EQ(lua_gettop(L), top + 1);
    lua_rawgeti(L, -1, 3);
    EXPECT_EQ(lua_tointeger(L, -1), 3);
}

TEST(JsonSerializer, DecodeNamesTheLineAndColumn)
{
    auto state = JsonTestHelpers::open();
    lua_State* L = state.get();

    std::string error;
    EXPECT_FALSE(JsonSerializer::decode(L, "{\n  \"a\": tru\n}", 0, error));
    EXPECT_EQ(error, "The input is not valid JSON at line 2, column 11.");

    EXPECT_FALSE(JsonSerializer::decode(L, std::string(201, '['), 0, error));
    EXPECT_EQ(error, "The input nests deeper than 200 levels at line 1, column 201.");
}

TEST(JsonSerializer, DecodeUsesTheNullValueGiven)
{
    auto state = JsonTestHelpers::open();
    lua_State* L = state.get();

    lua_pushliteral(L, "none");
    std::string error;
    ASSERT_TRUE(JsonSerializer::decode(L, "{\"a\":null}", -1, error));
    lua_getfield(L, -1, "a");
    EXPECT_STREQ(lua_tostring(L, -1), "none");
}

TEST(JsonSerializer, EncodeStaysWholeWhileFinalizersEncode)
{
    auto state = JsonTestHelpers::open();
    lua_State* L = state.get();

    // Finalizers that encode run while the buffer of the outer encode grows, so each one must take a thread of its own.
    const char* chunk = R"(
        local json = require("json")
        local finalized = 0
        local document = {}
        for i = 1, 20000 do
            document[i] = { id = i, name = "item-" .. i, tags = { "a", "b" } }
        end
        local expected = json.encode(document)
        for round = 1, 5 do
            for _ = 1, 2000 do
                setmetatable({}, { __gc = function() finalized = finalized + #json.encode({ round = round, list = { 1, 2, 3 } }) end })
            end
            assert(json.encode(document) == expected, "The encode stays whole")
        end
        collectgarbage()
        assert(finalized > 0, "The finalizers ran")
    )";
    EXPECT_EQ(JsonTestHelpers::run(L, chunk), "");
}

TEST(JsonSerializer, EncodeRecoversAfterAFailure)
{
    auto state = JsonTestHelpers::open();
    lua_State* L = state.get();

    const char* chunk = R"(
        local json = require("json")
        local cyclic = { list = {} }
        cyclic.list[1] = cyclic
        for _ = 1, 3 do
            local ok, err = pcall(json.encode, cyclic)
            assert(not ok and err:find('The table at key "list[1]" contains itself', 1, true), err)
            assert(json.encode({ a = { 1, 2 } }) == '{"a":[1,2]}')
        end
    )";
    EXPECT_EQ(JsonTestHelpers::run(L, chunk), "");
}

} // namespace varn::json
