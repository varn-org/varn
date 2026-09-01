#include "varn/http/HttpClientModule.h"

#include "HttpClientResponseLua.h"
#include "HttpUrl.h"
#include "varn/async/Promise.h"
#include "varn/http/HttpClientPerform.h"
#include "varn/log/Log.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/runtime/Runtime.h"

#include <lua.hpp>

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace varn::http
{

using varn::async::Promise;
using varn::runtime::Runtime;

namespace
{
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

    static void logCallbackError(lua_State* L, const char* fallback)
    {
        const char* message = lua_tostring(L, -1);
        varn::log::Log::error("HttpClientModule", message != nullptr ? message : fallback);
        lua_pop(L, 1);
    }
};
} // namespace

// lua surface wrapping the raw primitives into an ergonomic response plus query/json options
static const char* const kClientPrelude = R"lua(
local client = ...
local requestRaw = client.requestRaw
local streamRaw = client.streamRaw
local async = require("async")

local ok, json = pcall(require, "json")
if not ok then
    json = nil
end

-- percent-encode a query component so keys and values survive transport unambiguously
local function encodeComponent(value)
    return (tostring(value):gsub("[^%w%-%_%.%~]", function(c)
        return string.format("%%%02X", string.byte(c))
    end))
end

-- fold a { k = v } table into a sorted query string for stable urls
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

-- append a query string to a url, respecting an existing '?' already present
local function withQuery(url, params)
    local query = buildQuery(params)
    if query == "" then
        return url
    end

    local separator = url:find("?", 1, true) and "&" or "?"
    return url .. separator .. query
end

-- augment the resolved { status, headers, body } table with ok and a lazy json accessor
local function makeResponse(res)
    res.ok = res.status < 400
    function res.json()
        if not json then
            error("http.client: the json module is not available")
        end
        return json.decode(res.body)
    end

    return res
end

-- normalize a string/table argument into request options, applying query and json shortcuts
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

    -- json option serializes the body and sets the content type unless the caller already set one
    if opts.json ~= nil then
        if not json then
            error("http.client: the json module is not available")
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

-- streams the response body in chunks and invokes onChunk for each piece as it arrives
function client.stream(opts, onChunk)
    local options = buildOptions(opts)
    return async.promise(function()
        local done, err = streamRaw(options, onChunk, opts.onResponse):await()
        if err then
            error(err, 0)
        end

        return done
    end)
end
)lua";

Runtime& HttpClientModule::luaRuntime(lua_State* L)
{
    return *static_cast<Runtime*>(varn::lua::LuaHelpers::getRuntime(L));
}

namespace
{
class HttpClientHelpers
{
public:
    static void readHeadersTable(lua_State* L, int absIndex, std::map<std::string, std::string>& out)
    {
        lua_pushnil(L);
        while (lua_next(L, absIndex) != 0)
        {
            // coerce a copy of the key so lua_next still sees the original on the next step
            lua_pushvalue(L, -2);
            std::size_t keyLen = 0;
            std::size_t valLen = 0;
            const char* key = lua_tolstring(L, -1, &keyLen);
            const char* val = lua_tolstring(L, -2, &valLen);
            if (key != nullptr && val != nullptr)
            {
                out.emplace(std::string(key, keyLen), std::string(val, valLen));
            }

            lua_pop(L, 2);
        }
    }

    static void readClientOptions(lua_State* L, std::string& method, std::map<std::string, std::string>& headers, std::string& body, varn::http::client::ClientRequestOptions& options)
    {
        lua_getfield(L, 1, "method");
        method = lua_isstring(L, -1) ? lua_tostring(L, -1) : "GET";
        lua_pop(L, 1);

        lua_getfield(L, 1, "headers");
        if (lua_istable(L, -1))
        {
            HttpClientHelpers::readHeadersTable(L, lua_absindex(L, -1), headers);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "body");
        if (lua_isstring(L, -1))
        {
            std::size_t len = 0;
            const char* chunk = lua_tolstring(L, -1, &len);
            if (chunk != nullptr)
            {
                body.assign(chunk, len);
            }
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "timeoutSeconds");
        if (lua_isinteger(L, -1))
        {
            const int value = static_cast<int>(lua_tointeger(L, -1));
            if (value > 0)
            {
                options.timeoutSeconds = value;
            }
        }
        lua_pop(L, 1);

        // tls verification is on by default, with insecure as an explicit opt-in for dev certs
        lua_getfield(L, 1, "verifyTls");
        if (lua_isboolean(L, -1))
        {
            options.verifyTls = lua_toboolean(L, -1) != 0;
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "insecure");
        if (lua_isboolean(L, -1) && lua_toboolean(L, -1) != 0)
        {
            options.verifyTls = false;
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "maxResponseBytes");
        if (lua_isinteger(L, -1))
        {
            const long long value = lua_tointeger(L, -1);
            if (value > 0)
            {
                options.maxResponseBytes = static_cast<std::size_t>(value);
            }
        }
        lua_pop(L, 1);
    }
};
} // namespace

int HttpClientModule::luaClientRequest(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_getfield(L, 1, "url");
    const std::string urlStr = luaL_checkstring(L, -1);
    lua_pop(L, 1);

    std::string methodStr;
    std::map<std::string, std::string> headers;
    std::string body;
    varn::http::client::ClientRequestOptions options;
    HttpClientHelpers::readClientOptions(L, methodStr, headers, body, options);

    auto& rt = luaRuntime(L);
    auto promise = std::make_shared<Promise>(rt);

#if VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC
    try
    {
        varn::http::client::HttpClientPerform::performAsync(
            promise, methodStr, urlStr, headers, body, options.timeoutSeconds, options.maxResponseBytes);
    }
    catch (const std::exception& ex)
    {
        promise->reject(ex.what());
    }
#else
    // clang-format off
    rt.ioPool().post([promise, methodStr, urlStr, headers, body, options]
    {
        try
        {
            auto resp = varn::http::client::HttpClientPerform::perform(methodStr, urlStr, headers, body, options);
            promise->resolveCustom([resp = std::move(resp)](lua_State* lua)
            {
                varn::http::client::HttpClientResponseLua::pushResponse(lua, resp.status, resp.headers, resp.body);
            });
        }
        catch (const std::exception& ex)
        {
            promise->reject(ex.what());
        }
        catch (...)
        {
            // a worker thread must never let an exception escape and terminate the process
            promise->reject("[HttpClientModule] The request failed with a non-standard error.");
        }
    });
    // clang-format on
#endif

    Promise::push(L, promise);
    return 1;
}

int HttpClientModule::luaClientStream(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_getfield(L, 1, "url");
    const std::string urlStr = luaL_checkstring(L, -1);
    lua_pop(L, 1);

    std::string methodStr;
    std::map<std::string, std::string> headers;
    std::string body;
    varn::http::client::ClientRequestOptions options;
    HttpClientHelpers::readClientOptions(L, methodStr, headers, body, options);

    // hold the chunk callback and the optional response callback in the registry for the duration of the stream
    lua_pushvalue(L, 2);
    const int onChunkRef = luaL_ref(L, LUA_REGISTRYINDEX);

    int onResponseRef = LUA_NOREF;
    if (lua_isfunction(L, 3))
    {
        lua_pushvalue(L, 3);
        onResponseRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    auto& rt = luaRuntime(L);
    Runtime* rtp = &rt;
    auto promise = std::make_shared<Promise>(rt);

#if VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC
    luaL_unref(L, LUA_REGISTRYINDEX, onChunkRef);
    if (onResponseRef != LUA_NOREF)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, onResponseRef);
    }
    promise->reject("[HttpClientModule] Streaming is not supported in the wasm build.");
#else
    // clang-format off
    rt.ioPool().post([rtp, promise, methodStr, urlStr, headers, body, options, onChunkRef, onResponseRef]
    {
        auto releaseRefs = [rtp, onChunkRef, onResponseRef]
        {
            rtp->mainLoop().post([rtp, onChunkRef, onResponseRef]
            {
                if (rtp->stopped())
                {
                    return;
                }

                lua_State* lua = rtp->luaState();
                luaL_unref(lua, LUA_REGISTRYINDEX, onChunkRef);
                if (onResponseRef != LUA_NOREF)
                {
                    luaL_unref(lua, LUA_REGISTRYINDEX, onResponseRef);
                }
            });
        };

        try
        {
            varn::http::client::StreamResponseFn onResponse = [rtp, onResponseRef](int status, const varn::http::client::ResponseHeaders& hdrs)
            {
                if (onResponseRef == LUA_NOREF)
                {
                    return;
                }

                auto headersCopy = std::make_shared<varn::http::client::ResponseHeaders>(hdrs);
                rtp->mainLoop().post([rtp, onResponseRef, status, headersCopy]
                {
                    if (rtp->stopped())
                    {
                        return;
                    }

                    lua_State* lua = rtp->luaState();
                    lua_rawgeti(lua, LUA_REGISTRYINDEX, onResponseRef);
                    lua_pushinteger(lua, status);
                    varn::http::client::HttpClientResponseLua::pushHeaders(lua, *headersCopy);
                    if (lua_pcall(lua, 2, 0, 0) != LUA_OK)
                    {
                        HttpClientUrlLua::logCallbackError(lua, "the stream onResponse callback failed");
                    }
                });
            };

            varn::http::client::StreamChunkFn onChunk = [rtp, onChunkRef](const char* data, std::size_t len)
            {
                auto chunk = std::make_shared<std::string>(data, len);
                rtp->mainLoop().post([rtp, onChunkRef, chunk]
                {
                    if (rtp->stopped())
                    {
                        return;
                    }

                    lua_State* lua = rtp->luaState();
                    lua_rawgeti(lua, LUA_REGISTRYINDEX, onChunkRef);
                    lua_pushlstring(lua, chunk->data(), chunk->size());
                    if (lua_pcall(lua, 1, 0, 0) != LUA_OK)
                    {
                        HttpClientUrlLua::logCallbackError(lua, "the stream onChunk callback failed");
                    }
                });
            };

            varn::http::client::HttpClientPerform::performStream(methodStr, urlStr, headers, body, options, onResponse, onChunk);
            releaseRefs();
            promise->resolve("ok");
        }
        catch (const std::exception& ex)
        {
            releaseRefs();
            promise->reject(ex.what());
        }
        catch (...)
        {
            releaseRefs();
            promise->reject("[HttpClientModule] The stream failed with a non-standard error.");
        }
    });
    // clang-format on
#endif

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

    // pass the client table currently on top of the stack to the prelude as its single argument
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

    // the raw primitives resolve to a { status, headers, body } table and the prelude layers ergonomics over them
    lua_pushcfunction(L, &HttpClientModule::luaClientRequest);
    lua_setfield(L, -2, "requestRaw");

    lua_pushcfunction(L, &HttpClientModule::luaClientStream);
    lua_setfield(L, -2, "streamRaw");

    installPrelude(L);
    lua_setfield(L, -2, "client");
}

} // namespace varn::http
