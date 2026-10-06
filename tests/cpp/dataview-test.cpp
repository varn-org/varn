#include "varn/dataview/DataView.h"
#include "varn/dataview/DataViewJson.h"
#include "varn/varn.h"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace
{

using varn::dataview::DataView;
using varn::dataview::DataViewJson;

// What the hosts of these tests saw, kept where their C callbacks reach it without capturing.
class Seen
{
public:
    static std::string& text()
    {
        static std::string storage;
        return storage;
    }

    static std::atomic<int>& releases()
    {
        static std::atomic<int> count{0};
        return count;
    }

    static void countRelease(void*, void*)
    {
        releases().fetch_add(1, std::memory_order_acq_rel);
    }

    // Reads the bytes of the view whose token the argument holds, the way a host reads a camera frame that Lua hands it.
    static const char* readArgument(const char* argument, void*)
    {
        const nlohmann::json parsed = nlohmann::json::parse(argument);
        const DataView::Token token = *DataViewJson::read(parsed.at("frame"));
        varn_dataview* view = varn_dataview_acquire(token.id, reinterpret_cast<const void*>(token.pointer), token.size);
        text().assign(static_cast<const char*>(varn_dataview_data(view)), varn_dataview_size(view));
        varn_dataview_release(view);
        return "null";
    }

    // Answers with a view made for the answer, which the runtime gives back once Lua holds its own reference.
    static const char* answerView(const char*, void* userdata)
    {
        auto* runtime = static_cast<varn_runtime*>(userdata);
        varn_dataview* view = varn_dataview_new(5);
        std::memcpy(varn_dataview_data(view), "frame", 5);

        static std::array<char, 128> token;
        varn_dataview_token(view, token.data(), token.size());
        varn_runtime_release_dataview(runtime, view);
        return token.data();
    }

    static const char* record(const char* argument, void*)
    {
        text() = argument;
        return "null";
    }
};

} // namespace

// A wrapped view hands its memory back once, when the last reference ends, on the thread that ends it.
TEST(DataView, HandsWrappedMemoryBackOnceAfterTheLastReference)
{
    std::array<std::byte, 16> memory{};
    std::atomic<int> released{0};
    DataView view = DataView::wrap(memory.data(), memory.size(), [&released] { released.fetch_add(1); });
    DataView copy = view;

    view.reset();
    EXPECT_EQ(released.load(), 0);

    std::thread other([moved = std::move(copy)]() mutable { moved.reset(); });
    other.join();
    EXPECT_EQ(released.load(), 1);
}

// A token reaches its view only while the view lives and only with the address and the size it was given.
TEST(DataView, AcquiresOnlyALiveViewThatMatchesItsToken)
{
    DataView view = DataView::allocate(32);
    const DataView::Token token = view.token();

    EXPECT_EQ(DataView::acquire(token), view);
    EXPECT_TRUE(DataView::acquire({token.id, token.pointer + 1, token.size}).empty());
    EXPECT_TRUE(DataView::acquire({token.id, token.pointer, token.size + 1}).empty());

    view.reset();
    EXPECT_TRUE(DataView::acquire(token).empty());
}

// A new view is zeroed and keeps an id that no later view reuses.
TEST(DataView, AllocatesZeroedMemoryWithIdsThatAreNeverReused)
{
    DataView first = DataView::allocate(64);
    const std::uint64_t id = first.id();
    for (std::size_t index = 0; index < first.size(); ++index)
    {
        EXPECT_EQ(first.data()[index], std::byte{0});
    }

    first.reset();
    EXPECT_GT(DataView::allocate(64).id(), id);
}

// The JSON token carries the id, the address as hexadecimal text and the size, and a malformed one is refused.
TEST(DataView, WritesAndReadsItsJsonToken)
{
    DataView view = DataView::allocate(3);
    const nlohmann::json token = DataViewJson::write(view);
    EXPECT_EQ(token.at("pointer").get<std::string>().rfind("0x", 0), 0u);
    EXPECT_EQ(DataViewJson::acquire(nlohmann::json{{"frames", {token, token}}}).size(), 2u);

    std::array<char, 128> text{};
    varn_dataview_token(view.handle(), text.data(), text.size());
    EXPECT_EQ(nlohmann::json::parse(text.data()), token);

    EXPECT_FALSE(DataViewJson::read(nlohmann::json{{"size", 3}}).has_value());
    EXPECT_THROW(DataViewJson::read(nlohmann::json{{"$view", 1}, {"pointer", 4096}, {"size", 3}}), std::invalid_argument);

    const nlohmann::json gone = DataViewJson::write(view);
    view.reset();
    EXPECT_THROW(DataViewJson::acquire(gone), std::invalid_argument);
}

// Threads that take and drop references to one view while it ends never revive it, and its memory goes back once.
TEST(DataView, SurvivesReferencesTakenAndDroppedOnManyThreads)
{
    Seen::releases().store(0);
    std::array<std::byte, 8> memory{};
    varn_dataview* view = varn_dataview_wrap(memory.data(), memory.size(), &Seen::countRelease, nullptr);
    const DataView::Token token = DataView::share(view).token();

    std::vector<std::thread> threads;
    for (int thread = 0; thread < 4; ++thread)
    {
        threads.emplace_back([token]
        {
            for (int round = 0; round < 2000; ++round)
            {
                varn_dataview_release(varn_dataview_acquire(token.id, reinterpret_cast<const void*>(token.pointer), token.size));
            }
        });
    }

    varn_dataview_release(view);
    for (std::thread& thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(Seen::releases().load(), 1);
}

// A view that Lua hands a host function reaches it as a token whose bytes the host reads in place.
TEST(DataView, ReachesAHostFunctionAsATokenFromLua)
{
    varn_runtime* runtime = varn_runtime_new();
    ASSERT_EQ(varn_runtime_register(runtime, "show", &Seen::readArgument, nullptr), 0);

    EXPECT_EQ(varn_runtime_run_string(runtime, "host.show({frame = require('dataview').from('pixels')})", "=test"), 0);
    EXPECT_EQ(Seen::text(), "pixels");
    varn_runtime_free(runtime);
}

// A host function answers with a view made for the answer, and Lua reads it after the host gave its reference back.
TEST(DataView, ReachesLuaFromTheAnswerOfAHostFunction)
{
    varn_runtime* runtime = varn_runtime_new();
    ASSERT_EQ(varn_runtime_register(runtime, "capture", &Seen::answerView, runtime), 0);
    ASSERT_EQ(varn_runtime_register(runtime, "record", &Seen::record, nullptr), 0);

    const char* script = R"(
        local async = require("async")
        local view = host.capture()
        async.run(function()
            async.sleep(10):await()
            host.record(view:read())
        end)
    )";
    EXPECT_EQ(varn_runtime_run_string(runtime, script, "=test"), 0);
    EXPECT_EQ(Seen::text(), "\"frame\"");
    varn_runtime_free(runtime);
}

// An emitted event holds its views from the emit on, so the host releases its own view at once, and an event that names a view that is gone is refused.
TEST(DataView, KeepsTheViewsOfAnEventFromItsEmit)
{
    varn_runtime* runtime = varn_runtime_new();
    ASSERT_EQ(varn_runtime_register(runtime, "record", &Seen::record, nullptr), 0);
    ASSERT_EQ(varn_runtime_run_string(runtime, "host.on('frame', function(payload) host.record(payload.frame:read()) end)", "=test"), 0);

    varn_dataview* view = varn_dataview_new(4);
    std::memcpy(varn_dataview_data(view), "data", 4);
    std::array<char, 128> token{};
    varn_dataview_token(view, token.data(), token.size());
    EXPECT_EQ(varn_runtime_emit(runtime, "frame", (std::string("{\"frame\":") + token.data() + "}").c_str()), 0);
    varn_dataview_release(view);

    varn_dataview* gone = varn_dataview_new(4);
    varn_dataview_token(gone, token.data(), token.size());
    varn_dataview_release(gone);
    EXPECT_EQ(varn_runtime_emit(runtime, "frame", (std::string("{\"frame\":") + token.data() + "}").c_str()), 1);

    while (varn_runtime_poll(runtime) == 1)
    {
    }

    EXPECT_EQ(Seen::text(), "\"data\"");
    varn_runtime_free(runtime);
}
