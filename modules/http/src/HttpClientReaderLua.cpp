#include "HttpClientReaderLua.h"

#include "HttpClientResponseLua.h"
#include "varn/async/Promise.h"

#include <cstring>
#include <new>
#include <utility>

namespace varn::http::client
{

namespace
{
constexpr const char* kReaderMeta = "varn.http.response";
} // namespace

HttpClientReaderLua::Reader::Reader(varn::runtime::Runtime& runtime, Handle handle, ResponseHead head)
    : runtime(runtime)
    , handle(std::move(handle))
    , head(std::move(head))
{
}

// A response nothing reads any more ends its request, which would otherwise wait for room until its deadline.
HttpClientReaderLua::Reader::~Reader()
{
    handle.cancel();
}

// Hands a chunk to the read that waits for one, or keeps it, still counted against the window, until a read takes it.
void HttpClientReaderLua::Reader::take(std::string chunk)
{
    if (!pending)
    {
        chunks.push_back(std::move(chunk));
        return;
    }

    const std::shared_ptr<varn::async::Promise> waiting = std::exchange(pending, nullptr);
    handle.release(chunk.size());
    waiting->resolve(std::move(chunk));
}

void HttpClientReaderLua::Reader::end(const std::optional<Error>& error)
{
    ended = true;
    failure = error;

    if (pending)
    {
        const std::shared_ptr<varn::async::Promise> waiting = std::exchange(pending, nullptr);
        settle(*waiting, *this);
    }
}

void HttpClientReaderLua::push(lua_State* L, const std::shared_ptr<Reader>& reader)
{
    void* memory = lua_newuserdatauv(L, sizeof(std::shared_ptr<Reader>), 0);
    new (memory) std::shared_ptr<Reader>(reader);

    if (luaL_newmetatable(L, kReaderMeta))
    {
        lua_pushcfunction(L, &HttpClientReaderLua::index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, &HttpClientReaderLua::cancel);
        lua_setfield(L, -2, "__close");
        lua_pushcfunction(L, &HttpClientReaderLua::gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
}

std::shared_ptr<HttpClientReaderLua::Reader>& HttpClientReaderLua::check(lua_State* L)
{
    return *static_cast<std::shared_ptr<Reader>*>(luaL_checkudata(L, 1, kReaderMeta));
}

// Settles a read with the next chunk the response holds, or with the end of the body, `nil` once it ended whole and its failure otherwise.
void HttpClientReaderLua::settle(varn::async::Promise& promise, Reader& reader)
{
    if (!reader.chunks.empty())
    {
        std::string chunk = std::move(reader.chunks.front());
        reader.chunks.pop_front();
        reader.handle.release(chunk.size());
        promise.resolve(std::move(chunk));
        return;
    }

    if (reader.failure)
    {
        promise.reject(reader.failure->message);
        return;
    }

    // clang-format off
    promise.resolveCustom([](lua_State* lua) { lua_pushnil(lua); });
    // clang-format on
}

// Answers a promise of the next chunk of the body, which a response reads one at a time.
int HttpClientReaderLua::read(lua_State* L)
{
    Reader& reader = *check(L);
    if (reader.pending)
    {
        luaL_error(L, "The response is already being read. Await the read before the next one.");
    }

    auto promise = std::make_shared<varn::async::Promise>(reader.runtime);
    if (reader.chunks.empty() && !reader.ended)
    {
        reader.pending = promise;
    }
    else
    {
        settle(*promise, reader);
    }

    varn::async::Promise::push(L, promise);
    return 1;
}

// Ends the request of the response, and a read that waits or comes later rejects with the cancellation unless the body already ended.
int HttpClientReaderLua::cancel(lua_State* L)
{
    check(L)->handle.cancel();
    return 0;
}

int HttpClientReaderLua::index(lua_State* L)
{
    const Reader& reader = *check(L);
    const char* key = luaL_checkstring(L, 2);

    if (std::strcmp(key, "read") == 0)
    {
        lua_pushcfunction(L, &HttpClientReaderLua::read);
        return 1;
    }

    if (std::strcmp(key, "cancel") == 0)
    {
        lua_pushcfunction(L, &HttpClientReaderLua::cancel);
        return 1;
    }

    if (std::strcmp(key, "status") == 0)
    {
        lua_pushinteger(L, reader.head.status);
        return 1;
    }

    if (std::strcmp(key, "ok") == 0)
    {
        lua_pushboolean(L, reader.head.status < 400 ? 1 : 0);
        return 1;
    }

    if (std::strcmp(key, "headers") == 0)
    {
        HttpClientResponseLua::pushHeaders(L, reader.head.headers);
        return 1;
    }

    if (std::strcmp(key, "url") == 0)
    {
        lua_pushlstring(L, reader.head.url.data(), reader.head.url.size());
        return 1;
    }

    if (std::strcmp(key, "contentLength") == 0)
    {
        if (!reader.head.contentLength)
        {
            return 0;
        }

        lua_pushinteger(L, static_cast<lua_Integer>(*reader.head.contentLength));
        return 1;
    }

    return 0;
}

int HttpClientReaderLua::gc(lua_State* L)
{
    using Shared = std::shared_ptr<Reader>;
    check(L).~Shared();
    return 0;
}

} // namespace varn::http::client
