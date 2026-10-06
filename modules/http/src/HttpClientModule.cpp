#include "varn/http/HttpClientModule.h"

#include "HttpClientPerform.h"
#include "HttpClientResponseLua.h"
#include "HttpUrl.h"
#include "varn/async/AsyncModule.h"
#include "varn/async/Promise.h"
#include "varn/http/HttpClient.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/runtime/Runtime.h"

#include <lua.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace varn::http
{

using varn::async::Promise;
using varn::runtime::Runtime;

namespace
{
// The registry keeps the one client of a Lua state under this name, and its metatable under the next one.
constexpr const char* kClientKey = "varn.http.client";
constexpr const char* kClientMeta = "varn.http.client.meta";

// A timeout of more seconds than this is no deadline anyway, and it stays a number of milliseconds an integer holds.
constexpr double kLongestTimeoutSeconds = 1e9;

class HttpClientUrlLua
{
public:
    static int luaUrlEncode(lua_State* L)
    {
        std::size_t len = 0;
        const char* data = luaL_checklstring(L, 1, &len);
        const std::string out = HttpUrl::encode(std::string_view(data, len));
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }

    static int luaUrlDecode(lua_State* L)
    {
        std::size_t len = 0;
        const char* data = luaL_checklstring(L, 1, &len);
        const std::string out = HttpUrl::decode(std::string_view(data, len));
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
};

// The first failure of a callback of a stream, which the stream rejects with once it ends, read and written only on the loop.
struct StreamFailure
{
    bool failed = false;
    std::string message;
};

// A Lua function a request calls later, held in the registry until the request lets it go on the loop.
// A runtime that stopped closes its whole registry, so the reference is left to it then.
class LuaFunctionRef
{
public:
    LuaFunctionRef(Runtime& runtime, int ref)
        : runtime(runtime)
        , ref(ref)
    {
    }

    LuaFunctionRef(const LuaFunctionRef&) = delete;
    LuaFunctionRef& operator=(const LuaFunctionRef&) = delete;

    ~LuaFunctionRef()
    {
        if (!runtime.stopped())
        {
            luaL_unref(runtime.luaState(), LUA_REGISTRYINDEX, ref);
        }
    }

    void push(lua_State* L) const { lua_rawgeti(L, LUA_REGISTRYINDEX, ref); }

private:
    Runtime& runtime;
    int ref;
};

// What one stream keeps between its callbacks, all of them run on the loop.
struct StreamState
{
    std::unique_ptr<LuaFunctionRef> onChunk;
    std::unique_ptr<LuaFunctionRef> onResponse;
    StreamFailure failure;
    client::Handle handle;
};

// What a buffered request gathers before it resolves, shared rather than copied by every resume of its promise.
struct BufferedResponse
{
    client::ResponseHead head;
    std::string body;
};

class HttpClientStreamLua
{
public:
    // Calls a callback of a stream with the arguments at the top of the stack, after the callback below them, and pops them.
    // A failure reaches the failure handler of the application or else the log, and the stream skips its later callbacks and rejects with it.
    static void call(lua_State* L, int nargs, StreamFailure& failure)
    {
        if (failure.failed)
        {
            lua_pop(L, nargs + 1);
            return;
        }

        lua_pushcfunction(L, &async::AsyncModule::capture);
        lua_insert(L, -nargs - 2);
        const int handler = lua_gettop(L) - nargs - 1;
        if (lua_pcall(L, nargs, 0, handler) == LUA_OK)
        {
            lua_pop(L, 1);
            return;
        }

        lua_getfield(L, -1, "error");
        const char* message = lua_tostring(L, -1);
        failure.failed = true;
        failure.message = message != nullptr ? message : "A callback of the stream failed without a message.";
        lua_pop(L, 1);

        async::AsyncModule::report(L, "HttpClientModule");
        lua_pop(L, 1);
    }

    // Settles a stream that ended, rejecting with the failure of a callback first, since the handler already received it, then with the error of the transfer, and otherwise resolving.
    static void settle(Promise& promise, const StreamFailure& failure, const std::optional<client::Error>& error)
    {
        if (failure.failed)
        {
            promise.rejectReported(failure.message);
            return;
        }

        if (error)
        {
            promise.reject(error->message);
            return;
        }

        promise.resolve("ok");
    }
};

class HttpClientOptionsLua
{
public:
    // Reads the request of the options table at index 1, raising on an option that is present but wrong.
    static client::Request read(lua_State* L)
    {
        client::Request request;

        lua_getfield(L, 1, "url");
        request.url = luaL_checkstring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 1, "method");
        request.method = lua_isstring(L, -1) ? lua_tostring(L, -1) : "GET";
        lua_pop(L, 1);

        lua_getfield(L, 1, "headers");
        if (lua_istable(L, -1))
        {
            readHeaders(L, lua_absindex(L, -1), request.headers);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "body");
        if (lua_isstring(L, -1))
        {
            std::size_t len = 0;
            const char* chunk = lua_tolstring(L, -1, &len);
            request.body.assign(chunk, len);
        }
        lua_pop(L, 1);

        readTimeout(L, request);

        // TLS verification is on by default, with `insecure` as an explicit opt-in for dev certs.
        lua_getfield(L, 1, "verifyTls");
        if (lua_isboolean(L, -1))
        {
            request.verifyTls = lua_toboolean(L, -1) != 0;
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "insecure");
        if (lua_isboolean(L, -1) && lua_toboolean(L, -1) != 0)
        {
            request.verifyTls = false;
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "maxResponseBytes");
        if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0)
        {
            request.maxResponseBytes = static_cast<std::uint64_t>(lua_tointeger(L, -1));
        }
        lua_pop(L, 1);

        readRedirects(L, request);
        return request;
    }

private:
    static void readHeaders(lua_State* L, int absIndex, client::Headers& out)
    {
        lua_pushnil(L);
        while (lua_next(L, absIndex) != 0)
        {
            // Coerce a copy of the key so `lua_next` still sees the original on the next step.
            lua_pushvalue(L, -2);
            std::size_t keyLen = 0;
            std::size_t valLen = 0;
            const char* key = lua_tolstring(L, -1, &keyLen);
            const char* val = lua_tolstring(L, -2, &valLen);
            if (key != nullptr && val != nullptr)
            {
                out.emplace_back(std::string(key, keyLen), std::string(val, valLen));
            }

            lua_pop(L, 2);
        }
    }

    // The timeout is the one deadline of the whole request, redirects included, and any positive number of seconds is one.
    static void readTimeout(lua_State* L, client::Request& request)
    {
        lua_getfield(L, 1, "timeoutSeconds");
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            return;
        }

        const lua_Number value = lua_type(L, -1) == LUA_TNUMBER ? lua_tonumber(L, -1) : 0;
        lua_pop(L, 1);
        if (!(value > 0) || !std::isfinite(value))
        {
            luaL_error(L, "The option \"timeoutSeconds\" must be a positive number of seconds.");
        }

        const double milliseconds = std::ceil(std::min(static_cast<double>(value), kLongestTimeoutSeconds) * 1000.0);
        request.timeout = std::chrono::milliseconds(static_cast<long long>(milliseconds));
    }

    // Reads what a caller wants done about a response that points somewhere else, by the names the fetch standard settled on.
    static void readRedirects(lua_State* L, client::Request& request)
    {
        lua_getfield(L, 1, "redirect");
        if (lua_isstring(L, -1))
        {
            const std::string wanted = lua_tostring(L, -1);

            if (wanted == "follow")
            {
                request.redirects = client::RedirectPolicy::Follow;
            }
            else if (wanted == "manual")
            {
                request.redirects = client::RedirectPolicy::Manual;
            }
            else if (wanted == "error")
            {
                request.redirects = client::RedirectPolicy::Error;
            }
            else
            {
                lua_pop(L, 1);
                luaL_error(L, "The option \"redirect\" must be \"follow\", \"manual\" or \"error\", got \"%s\".", wanted.c_str());
            }
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "maxRedirects");
        if (lua_isinteger(L, -1))
        {
            const long long value = lua_tointeger(L, -1);

            if (value < 0)
            {
                lua_pop(L, 1);
                luaL_error(L, "The option \"maxRedirects\" cannot be negative.");
            }

            request.maxRedirects = static_cast<int>(value);
        }
        lua_pop(L, 1);

#if VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC
        // A browser follows a redirect itself and hands over only where it ended up, so a caller asking to see one is told it cannot be, rather than being handed the final answer as if it were the first.
        if (request.redirects != client::RedirectPolicy::Follow)
        {
            luaL_error(L, "A browser follows redirects itself, so the option \"redirect\" must be \"follow\" here.");
        }
#endif
    }
};
} // namespace

// Lua surface wrapping the raw primitives into an ergonomic response plus the query and JSON options.
static const char* const kClientPrelude = R"lua(
local client = ...
local requestRaw = client.requestRaw
local streamRaw = client.streamRaw
local async = require("async")

local ok, json = pcall(require, "json")
if not ok then
    json = nil
end

-- Percent-encode a query component so keys and values survive transport unambiguously.
local function encodeComponent(value)
    return (tostring(value):gsub("[^%w%-%_%.%~]", function(c)
        return string.format("%%%02X", string.byte(c))
    end))
end

-- Fold a `{ k = v }` table into a sorted query string for stable URLs.
local function buildQuery(params)
    local keys = {}
    for key in pairs(params) do
        keys[#keys + 1] = key
    end
    table.sort(keys)

    local pairsOut = {}
    for _, key in ipairs(keys) do
        pairsOut[#pairsOut + 1] = encodeComponent(key) .. "=" .. encodeComponent(params[key])
    end

    return table.concat(pairsOut, "&")
end

-- Append a query string to a URL, respecting an existing `?` already present.
local function withQuery(url, params)
    local query = buildQuery(params)
    if query == "" then
        return url
    end

    local separator = url:find("?", 1, true) and "&" or "?"
    return url .. separator .. query
end

-- Augment the resolved `{ status, headers, body }` table with `ok` and a lazy JSON accessor.
local function makeResponse(res)
    res.ok = res.status < 400
    function res.json()
        if not json then
            error('The "json" module is not available to "http.client".')
        end
        return json.decode(res.body)
    end

    return res
end

-- Normalize a string or table argument into request options, applying the query and JSON shortcuts.
local function buildOptions(opts)
    opts = opts or {}

    local options = {
        url = opts.url,
        method = opts.method,
        headers = {},
        body = opts.body,
        timeoutSeconds = opts.timeoutSeconds,
        verifyTls = opts.verifyTls,
        insecure = opts.insecure,
        maxResponseBytes = opts.maxResponseBytes,
    }

    if opts.headers then
        for key, value in pairs(opts.headers) do
            options.headers[key] = value
        end
    end

    if opts.query then
        options.url = withQuery(options.url, opts.query)
    end

    -- The `json` option serializes the body and sets the content type unless the caller already set one.
    if opts.json ~= nil then
        if not json then
            error('The "json" module is not available to "http.client".')
        end
        options.body = json.encode(opts.json)
        local hasType = false
        for key in pairs(options.headers) do
            if key:lower() == "content-type" then
                hasType = true
                break
            end
        end
        if not hasType then
            options.headers["Content-Type"] = "application/json"
        end
    end

    return options
end

function client.request(opts)
    local options = buildOptions(opts)
    return async.promise(function()
        local res, err = requestRaw(options):await()
        if err then
            error(err, 0)
        end

        return makeResponse(res)
    end)
end

function client.get(url, opts)
    opts = opts or {}
    opts.url = url
    opts.method = "GET"
    return client.request(opts)
end

function client.post(url, opts)
    opts = opts or {}
    opts.url = url
    opts.method = "POST"
    return client.request(opts)
end

-- Streams the response body in chunks and invokes `onChunk` for each piece as it arrives.
-- Answers the promise of the stream itself, which already settles the way a caller reads it, so a rejection the failure handler received is never reported again.
function client.stream(opts, onChunk)
    return streamRaw(buildOptions(opts), onChunk, opts.onResponse)
end
)lua";
Runtime& HttpClientModule::luaRuntime(lua_State* L)
{
    return *static_cast<Runtime*>(varn::lua::LuaHelpers::getRuntime(L));
}

// Answers the one client of the Lua state, made on first use and destroyed with the state, so every request of a script shares its connections.
client::HttpClient& HttpClientModule::luaClient(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, kClientKey);
    if (auto* existing = static_cast<client::HttpClient*>(luaL_testudata(L, -1, kClientMeta)))
    {
        lua_pop(L, 1);
        return *existing;
    }

    lua_pop(L, 1);
    void* memory = lua_newuserdatauv(L, sizeof(client::HttpClient), 0);
    auto* created = new (memory) client::HttpClient(luaRuntime(L));

    if (luaL_newmetatable(L, kClientMeta))
    {
        lua_pushcfunction(L, &HttpClientModule::luaClientGc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    lua_setfield(L, LUA_REGISTRYINDEX, kClientKey);
    return *created;
}

int HttpClientModule::luaClientGc(lua_State* L)
{
    static_cast<client::HttpClient*>(luaL_checkudata(L, 1, kClientMeta))->~HttpClient();
    return 0;
}

int HttpClientModule::luaClientRequest(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    client::Request request = HttpClientOptionsLua::read(L);

    auto promise = std::make_shared<Promise>(luaRuntime(L));
    auto response = std::make_shared<BufferedResponse>();

    client::HttpClient::Callbacks callbacks;

    // clang-format off
    callbacks.onHead = [response](const client::ResponseHead& head)
    {
        response->head = head;

        // The body is gathered in one allocation when the server says how long it is, which the limit already bounds.
        if (head.contentLength && *head.contentLength <= response->body.max_size())
        {
            response->body.reserve(static_cast<std::size_t>(*head.contentLength));
        }
    };

    callbacks.onChunk = [response](std::string_view chunk)
    {
        response->body.append(chunk);
    };

    callbacks.onComplete = [promise, response](const std::optional<client::Error>& error)
    {
        if (error)
        {
            promise->reject(error->message);
            return;
        }

        promise->resolveCustom([response](lua_State* lua)
        {
            client::HttpClientResponseLua::pushResponse(lua, response->head.status, response->head.headers, response->body);
        });
    };
    // clang-format on

    luaClient(L).send(std::move(request), std::move(callbacks));
    Promise::push(L, promise);
    return 1;
}

int HttpClientModule::luaClientStream(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    client::Request request = HttpClientOptionsLua::read(L);

    Runtime& rt = luaRuntime(L);
    auto promise = std::make_shared<Promise>(rt);
    auto state = std::make_shared<StreamState>();

    // Hold the chunk callback and the optional response callback in the registry for the duration of the stream.
    lua_pushvalue(L, 2);
    state->onChunk = std::make_unique<LuaFunctionRef>(rt, luaL_ref(L, LUA_REGISTRYINDEX));
    if (lua_isfunction(L, 3))
    {
        lua_pushvalue(L, 3);
        state->onResponse = std::make_unique<LuaFunctionRef>(rt, luaL_ref(L, LUA_REGISTRYINDEX));
    }

    client::HttpClient::Callbacks callbacks;
    Runtime* rtp = &rt;

    // A callback that fails ends the transfer, so nothing more is read for a stream that will reject anyway.
    // clang-format off
    callbacks.onHead = [rtp, state](const client::ResponseHead& head)
    {
        if (!state->onResponse)
        {
            return;
        }

        lua_State* lua = rtp->luaState();
        state->onResponse->push(lua);
        lua_pushinteger(lua, head.status);
        client::HttpClientResponseLua::pushHeaders(lua, head.headers);
        HttpClientStreamLua::call(lua, 2, state->failure);
        if (state->failure.failed)
        {
            state->handle.cancel();
        }
    };

    callbacks.onChunk = [rtp, state](std::string_view chunk)
    {
        lua_State* lua = rtp->luaState();
        state->onChunk->push(lua);
        lua_pushlstring(lua, chunk.data(), chunk.size());
        HttpClientStreamLua::call(lua, 1, state->failure);
        if (state->failure.failed)
        {
            state->handle.cancel();
        }
    };

    // The transfer settles through the loop behind the head and the chunks it already posted, so a caller that awaits late never sees it end before them.
    callbacks.onComplete = [promise, state](const std::optional<client::Error>& error)
    {
        HttpClientStreamLua::settle(*promise, state->failure, error);
    };
    // clang-format on

    state->handle = luaClient(L).send(std::move(request), std::move(callbacks));
    Promise::push(L, promise);
    return 1;
}

void HttpClientModule::installPrelude(lua_State* L)
{
    if (luaL_loadstring(L, kClientPrelude) != LUA_OK)
    {
        const char* message = lua_tostring(L, -1);
        luaL_error(L, "[HttpClientModule] The client prelude failed to compile: %s", message ? message : "");
    }

    // Pass the client table currently on top of the stack to the prelude as its single argument.
    lua_pushvalue(L, -2);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK)
    {
        const char* message = lua_tostring(L, -1);
        luaL_error(L, "[HttpClientModule] The client prelude failed to run: %s", message ? message : "");
    }
}

void HttpClientModule::registerClient(lua_State* L)
{
    luaL_checktype(L, -1, LUA_TTABLE);

    lua_pushcfunction(L, &HttpClientUrlLua::luaUrlEncode);
    lua_setfield(L, -2, "urlEncode");

    lua_pushcfunction(L, &HttpClientUrlLua::luaUrlDecode);
    lua_setfield(L, -2, "urlDecode");

    lua_newtable(L);

    // The raw primitives resolve to a { status, headers, body } table and the prelude layers ergonomics over them.
    lua_pushcfunction(L, &HttpClientModule::luaClientRequest);
    lua_setfield(L, -2, "requestRaw");

    lua_pushcfunction(L, &HttpClientModule::luaClientStream);
    lua_setfield(L, -2, "streamRaw");

    installPrelude(L);
    lua_setfield(L, -2, "client");
}

} // namespace varn::http
