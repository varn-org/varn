#pragma once

#include "varn/http/HttpClientHandle.h"
#include "varn/http/HttpClientTypes.h"

#include <lua.hpp>

#include <deque>
#include <memory>
#include <optional>
#include <string>

namespace varn::async
{
class Promise;
}

namespace varn::runtime
{
class Runtime;
}

namespace varn::http::client
{

/// The response of `http.client.open`, whose body Lua pulls one chunk at a time with `read`, while the chunks it has not read yet hold the transport back.
class HttpClientReaderLua
{
public:
    HttpClientReaderLua() = delete;

    /// What one response keeps between its reads, touched only on the loop, which cancels its request once nothing in Lua refers to it.
    struct Reader
    {
        Reader(varn::runtime::Runtime& runtime, Handle handle, ResponseHead head);
        ~Reader();

        Reader(const Reader&) = delete;
        Reader& operator=(const Reader&) = delete;

        void take(std::string chunk);
        void end(const std::optional<Error>& error);

        varn::runtime::Runtime& runtime;
        Handle handle;
        ResponseHead head;
        std::deque<std::string> chunks;
        std::shared_ptr<varn::async::Promise> pending;
        bool ended = false;
        std::optional<Error> failure;
    };

    static void push(lua_State* L, const std::shared_ptr<Reader>& reader);

private:
    static std::shared_ptr<Reader>& check(lua_State* L);
    static void settle(varn::async::Promise& promise, Reader& reader);
    static int read(lua_State* L);
    static int cancel(lua_State* L);
    static int index(lua_State* L);
    static int gc(lua_State* L);
};

} // namespace varn::http::client
