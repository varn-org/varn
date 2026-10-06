#include "varn/http/HttpClientModule.h"

#include "HttpClientControllerLua.h"
#include "HttpClientDownload.h"
#include "HttpClientLuaFunction.h"
#include "HttpClientPerform.h"
#include "HttpClientReaderLua.h"
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
#include <filesystem>
#include <fstream>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
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

// What one stream keeps between its callbacks, all of them run on the loop.
struct StreamState
{
    std::unique_ptr<client::HttpClientLuaFunction> onChunk;
    std::unique_ptr<client::HttpClientLuaFunction> onResponse;
    StreamFailure failure;
    client::Handle handle;
};

// What a download keeps between its callbacks, all of them run on the loop.
struct DownloadState
{
    std::unique_ptr<client::HttpClientLuaFunction> onProgress;
    StreamFailure failure;
    client::ResponseHead head;
    client::Handle handle;
};

class HttpClientDownloadLua
{
public:
    // Pushes what a download resolves to: the status and headers of its response, the file, its size and whether it continued an earlier attempt.
    static void pushResult(lua_State* L, const client::ResponseHead& head, const client::HttpClientDownload& download, std::uint64_t size)
    {
        lua_createtable(L, 0, 5);
        lua_pushinteger(L, head.status);
        lua_setfield(L, -2, "status");

        client::HttpClientResponseLua::pushHeaders(L, head.headers);
        lua_setfield(L, -2, "headers");

        const std::string written = download.path().string();
        lua_pushlstring(L, written.data(), written.size());
        lua_setfield(L, -2, "path");

        lua_pushinteger(L, static_cast<lua_Integer>(size));
        lua_setfield(L, -2, "size");

        lua_pushboolean(L, download.resumed() ? 1 : 0);
        lua_setfield(L, -2, "resumed");
    }
};

// What the callbacks of an opened response share: the promise until the head resolves it, then the response, which only Lua keeps alive.
struct OpenLink
{
    std::shared_ptr<Promise> opening;
    std::weak_ptr<client::HttpClientReaderLua::Reader> reader;
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

        async::AsyncModule::report(L);
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

        readBodyFile(L, request);

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

    // Streams the body from a file on the thread of the transport, so an upload of any size never sits in memory.
    static void readBodyFile(lua_State* L, client::Request& request)
    {
        lua_getfield(L, 1, "bodyFile");
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            return;
        }

        if (lua_type(L, -1) != LUA_TSTRING)
        {
            luaL_error(L, "The option \"bodyFile\" must be the path of a file.");
        }

        const std::filesystem::path path(lua_tostring(L, -1));
        lua_pop(L, 1);
        if (!request.body.empty())
        {
            luaL_error(L, "A request takes either \"body\" or \"bodyFile\", not both.");
        }

        std::error_code code;
        const std::uintmax_t size = std::filesystem::file_size(path, code);
        auto file = std::make_shared<std::ifstream>(path, std::ios::binary);
        if (code || !file->is_open())
        {
            luaL_error(L, "The file \"%s\" of \"bodyFile\" could not be opened.", path.string().c_str());
        }

        request.bodyLength = size;

        // clang-format off
        request.bodySource = [file, name = path.string()](char* buffer, std::size_t capacity)
        {
            file->read(buffer, static_cast<std::streamsize>(capacity));
            if (file->bad())
            {
                throw std::runtime_error("The file \"" + name + "\" could not be read.");
            }

            return static_cast<std::size_t>(file->gcount());
        };
        // clang-format on
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
        redirect = opts.redirect,
        maxRedirects = opts.maxRedirects,
        bodyFile = opts.bodyFile,
        controller = opts.controller,
        path = opts.path,
        resume = opts.resume,
        onProgress = opts.onProgress,
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

-- Writes the response body to the file at `path` as it arrives and resolves once the file is whole.
function client.download(opts)
    return client.downloadRaw(buildOptions(opts))
end

-- Resolves to the response once its head arrives, whose body the caller reads one chunk at a time.
function client.open(opts)
    return client.openRaw(buildOptions(opts))
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
    client::HttpClientControllerLua::Controller* controller = client::HttpClientControllerLua::option(L, 1);
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

    client::HttpClientControllerLua::attach(controller, luaClient(L).send(std::move(request), std::move(callbacks)));
    Promise::push(L, promise);
    return 1;
}

int HttpClientModule::luaClientStream(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    client::HttpClientControllerLua::Controller* controller = client::HttpClientControllerLua::option(L, 1);
    client::Request request = HttpClientOptionsLua::read(L);

    Runtime& rt = luaRuntime(L);
    auto promise = std::make_shared<Promise>(rt);
    auto state = std::make_shared<StreamState>();

    // Hold the chunk callback and the optional response callback in the registry for the duration of the stream.
    lua_pushvalue(L, 2);
    state->onChunk = std::make_unique<client::HttpClientLuaFunction>(rt, luaL_ref(L, LUA_REGISTRYINDEX));
    if (lua_isfunction(L, 3))
    {
        lua_pushvalue(L, 3);
        state->onResponse = std::make_unique<client::HttpClientLuaFunction>(rt, luaL_ref(L, LUA_REGISTRYINDEX));
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
    client::HttpClientControllerLua::attach(controller, state->handle);
    Promise::push(L, promise);
    return 1;
}

int HttpClientModule::luaClientDownload(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    client::HttpClientControllerLua::Controller* controller = client::HttpClientControllerLua::option(L, 1);
    client::Request request = HttpClientOptionsLua::read(L);

    lua_getfield(L, 1, "path");
    if (lua_type(L, -1) != LUA_TSTRING)
    {
        luaL_error(L, "The option \"path\" must name the file of the download.");
    }
    std::filesystem::path path(lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "resume");
    const bool resume = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    Runtime& rt = luaRuntime(L);
    auto download = std::make_shared<client::HttpClientDownload>(request.url, std::move(path), resume);
    for (auto& header : download->prepare())
    {
        request.headers.push_back(std::move(header));
    }
    request.bodySink = download->sink();

    auto promise = std::make_shared<Promise>(rt);
    auto state = std::make_shared<DownloadState>();
    lua_getfield(L, 1, "onProgress");
    if (lua_isfunction(L, -1))
    {
        state->onProgress = std::make_unique<client::HttpClientLuaFunction>(rt, luaL_ref(L, LUA_REGISTRYINDEX));
    }
    else
    {
        lua_pop(L, 1);
    }

    client::HttpClient::Callbacks callbacks;
    Runtime* rtp = &rt;

    // clang-format off
    callbacks.onHead = [state](const client::ResponseHead& head)
    {
        state->head = head;
    };

    callbacks.onProgress = [rtp, state](const client::Progress& progress)
    {
        if (!state->onProgress)
        {
            return;
        }

        lua_State* lua = rtp->luaState();
        state->onProgress->push(lua);
        lua_pushinteger(lua, static_cast<lua_Integer>(progress.received));
        if (progress.total)
        {
            lua_pushinteger(lua, static_cast<lua_Integer>(*progress.total));
        }
        else
        {
            lua_pushnil(lua);
        }

        HttpClientStreamLua::call(lua, 2, state->failure);
        if (state->failure.failed)
        {
            state->handle.cancel();
        }
    };

    // The file is closed and moved, or removed, on the I/O pool before the promise settles, so a caller never finds a partial file in place of the one it asked for.
    callbacks.onComplete = [rtp, promise, state, download](const std::optional<client::Error>& error)
    {
        if (error || state->failure.failed)
        {
            rtp->ioPool().post([promise, state, download, error]
            {
                download->discard();
                HttpClientStreamLua::settle(*promise, state->failure, error);
            });
            return;
        }

        rtp->ioPool().post([promise, state, download]
        {
            try
            {
                const std::uint64_t size = download->finish();
                promise->resolveCustom([state, download, size](lua_State* lua)
                {
                    HttpClientDownloadLua::pushResult(lua, state->head, *download, size);
                });
            }
            catch (const std::exception& failure)
            {
                download->discard();
                promise->reject(failure.what());
            }
        });
    };
    // clang-format on

    state->handle = luaClient(L).send(std::move(request), std::move(callbacks));
    client::HttpClientControllerLua::attach(controller, state->handle);
    Promise::push(L, promise);
    return 1;
}

int HttpClientModule::luaClientOpen(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    client::HttpClientControllerLua::Controller* controller = client::HttpClientControllerLua::option(L, 1);
    client::Request request = HttpClientOptionsLua::read(L);

    Runtime& rt = luaRuntime(L);
    auto link = std::make_shared<OpenLink>();
    link->opening = std::make_shared<Promise>(rt);
    const std::shared_ptr<Promise> promise = link->opening;

    client::HttpClient::Callbacks callbacks;
    callbacks.releasesChunks = true;
    Runtime* rtp = &rt;

    // The response resolves the promise and only Lua keeps it alive from then on, so the request ends once nothing refers to it.
    // clang-format off
    callbacks.onHead = [rtp, link](const client::ResponseHead& head)
    {
        auto reader = std::make_shared<client::HttpClientReaderLua::Reader>(*rtp, link->handle, head);
        link->reader = reader;
        const std::shared_ptr<Promise> opened = std::exchange(link->opening, nullptr);
        opened->resolveCustom([reader](lua_State* lua) { client::HttpClientReaderLua::push(lua, reader); });
    };

    callbacks.onChunk = [link](std::string_view chunk)
    {
        if (const auto reader = link->reader.lock())
        {
            reader->take(std::string(chunk));
        }
    };

    callbacks.onComplete = [link](const std::optional<client::Error>& error)
    {
        if (link->opening)
        {
            link->opening->reject(error ? error->message : "The request ended without a response.");
            return;
        }

        if (const auto reader = link->reader.lock())
        {
            reader->end(error);
        }
    };
    // clang-format on

    link->handle = luaClient(L).send(std::move(request), std::move(callbacks));
    client::HttpClientControllerLua::attach(controller, link->handle);
    Promise::push(L, promise);
    return 1;
}

void HttpClientModule::installPrelude(lua_State* L)
{
    if (luaL_loadstring(L, kClientPrelude) != LUA_OK)
    {
        const char* message = lua_tostring(L, -1);
        luaL_error(L, "The client prelude failed to compile: %s", message ? message : "");
    }

    // Pass the client table currently on top of the stack to the prelude as its single argument.
    lua_pushvalue(L, -2);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK)
    {
        const char* message = lua_tostring(L, -1);
        luaL_error(L, "The client prelude failed to run: %s", message ? message : "");
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

    lua_pushcfunction(L, &HttpClientModule::luaClientDownload);
    lua_setfield(L, -2, "downloadRaw");

    lua_pushcfunction(L, &HttpClientModule::luaClientOpen);
    lua_setfield(L, -2, "openRaw");

    lua_pushcfunction(L, &client::HttpClientControllerLua::create);
    lua_setfield(L, -2, "controller");

    installPrelude(L);
    lua_setfield(L, -2, "client");
}

} // namespace varn::http
