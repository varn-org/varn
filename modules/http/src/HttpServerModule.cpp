#include "varn/http/HttpServerModule.h"
#include "varn/async/AsyncModule.h"
#include "varn/async/Promise.h"
#include "varn/http/HttpAppModule.h"
#include "varn/http/HttpClientModule.h"
#include "varn/http/HttpTypes.h"
#include "varn/http/drivers/reactor/ReactorHttpServer.h"
#include "varn/log/Log.h"
#if VARN_JSON_DRIVER_NLOHMANN
#include "varn/json/JsonSerializer.h"
#endif
#include "varn/lua/LuaHelpers.h"
#include "varn/runtime/App.h"
#include "varn/runtime/Runtime.h"
#if VARN_XML_DRIVER_PUGIXML
#include "varn/xml/XmlSerializer.h"
#endif

#include <climits>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace varn::http
{

using varn::runtime::Runtime;

namespace
{
constexpr const char* kRequestMeta = "varn.HttpRequest";

// The largest body a server may be told to hold in memory for one request.
constexpr long long kMaxRequestBodyBytes = 1024LL * 1024 * 1024;

class HttpRequestLua
{
public:
    static int luaRequestIndex(lua_State* L)
    {
        auto* request = static_cast<HttpRequest*>(luaL_checkudata(L, 1, kRequestMeta));
        const char* key = lua_tostring(L, 2);
        if (key == nullptr)
        {
            lua_pushnil(L);
            return 1;
        }

        if (std::strcmp(key, "path") == 0)
        {
            lua_pushlstring(L, request->path.data(), request->path.size());
            return 1;
        }

        if (std::strcmp(key, "method") == 0)
        {
            lua_pushlstring(L, request->method.data(), request->method.size());
            return 1;
        }

        if (std::strcmp(key, "host") == 0)
        {
            lua_pushlstring(L, request->host.data(), request->host.size());
            return 1;
        }

        if (std::strcmp(key, "target") == 0)
        {
            lua_pushlstring(L, request->target.data(), request->target.size());
            return 1;
        }

        if (std::strcmp(key, "queryString") == 0)
        {
            lua_pushlstring(L, request->queryString.data(), request->queryString.size());
            return 1;
        }

        if (std::strcmp(key, "body") == 0)
        {
            lua_pushlstring(L, request->body.data(), request->body.size());
            return 1;
        }

        if (std::strcmp(key, "remoteAddress") == 0)
        {
            lua_pushlstring(L, request->remoteAddress.data(), request->remoteAddress.size());
            return 1;
        }

        if (std::strcmp(key, "headers") == 0)
        {
            varn::lua::LuaHelpers::pushStringMap(L, request->headers);
            return 1;
        }

        if (std::strcmp(key, "cookies") == 0)
        {
            varn::lua::LuaHelpers::pushStringMap(L, request->cookies);
            return 1;
        }

        if (std::strcmp(key, "query") == 0)
        {
            varn::lua::LuaHelpers::pushStringMap(L, request->query);
            return 1;
        }

        lua_pushnil(L);
        return 1;
    }

    static int luaRequestGc(lua_State* L)
    {
        auto* request = static_cast<HttpRequest*>(lua_touserdata(L, 1));
        if (request != nullptr)
        {
            std::destroy_at(request);
        }

        return 0;
    }

    static void pushRequestUserdata(lua_State* L, HttpRequest&& request)
    {
        // The request is owned by this userdata so its fields are pushed only when the handler reads them, and it survives across an await.
        void* memory = lua_newuserdatauv(L, sizeof(HttpRequest), 0);
        new (memory) HttpRequest(std::move(request));
        luaL_getmetatable(L, kRequestMeta);
        lua_setmetatable(L, -2);
    }

    static void createRequestMetatable(lua_State* L)
    {
        if (luaL_newmetatable(L, kRequestMeta))
        {
            lua_pushcfunction(L, &HttpRequestLua::luaRequestIndex);
            lua_setfield(L, -2, "__index");
            lua_pushcfunction(L, &HttpRequestLua::luaRequestGc);
            lua_setfield(L, -2, "__gc");
        }

        lua_pop(L, 1);
    }
};
} // namespace

class HttpServerLuaBindings
{
public:
    static int luaOpen(lua_State* L);
    static void pushServer(lua_State* L, const std::shared_ptr<ReactorHttpServer>& server);

private:
    static constexpr const char* kServerMeta = "varn.HttpServerBuilder";
    static constexpr const char* kListeningMeta = "varn.HttpServer";
    static constexpr const char* kResponseMeta = "varn.HttpResponse";
    static constexpr long long kCloseTimeoutMs = 5000;

    struct ServerBuilderUserdata
    {
        Runtime* runtime = nullptr;
        int handlerRef = LUA_NOREF;
    };

    // A listening server seen from Lua, which never keeps the server alive, so collecting it leaves the server running until it is closed or the runtime stops.
    struct ListeningUserdata
    {
        std::weak_ptr<ReactorHttpServer> server;
        std::string host;
        int port = 0;
    };

    struct ResponseUserdata
    {
        std::shared_ptr<HttpResponse> response;
    };

    static Runtime& luaRuntime(lua_State* L);
    static void pushResponse(lua_State* L, std::shared_ptr<HttpResponse> response);
    static ResponseUserdata* checkResponse(lua_State* L);

    static int luaResponseGc(lua_State* L);
    static int luaResponseStatus(lua_State* L);
    static int luaResponseSetHeader(lua_State* L);
    static int luaResponseWrite(lua_State* L);
    static int luaResponseFinish(lua_State* L);
#if VARN_JSON_DRIVER_NLOHMANN
    static int luaResponseJson(lua_State* L);
#endif
#if VARN_XML_DRIVER_PUGIXML
    static int luaResponseXml(lua_State* L);
#endif
    static void finishResponse(HttpResponse& response, bool failed);
    static int handlerBody(lua_State* L);
    static int handlerContinuation(lua_State* L, int status, lua_KContext ctx);
    static int luaServerGc(lua_State* L);
    static int luaServerListen(lua_State* L);
    static int luaListeningIndex(lua_State* L);
    static int luaListeningClose(lua_State* L);
    static int luaListeningGc(lua_State* L);
    static int luaCreateServer(lua_State* L);
    static void createMetatables(lua_State* L);
};

Runtime& HttpServerLuaBindings::luaRuntime(lua_State* L)
{
    return *static_cast<Runtime*>(varn::lua::LuaHelpers::getRuntime(L));
}

void HttpServerLuaBindings::pushResponse(lua_State* L, std::shared_ptr<HttpResponse> response)
{
    void* memory = lua_newuserdatauv(L, sizeof(ResponseUserdata), 0);
    new (memory) ResponseUserdata{std::move(response)};

    luaL_getmetatable(L, kResponseMeta);
    lua_setmetatable(L, -2);
}

HttpServerLuaBindings::ResponseUserdata* HttpServerLuaBindings::checkResponse(lua_State* L)
{
    return static_cast<ResponseUserdata*>(luaL_checkudata(L, 1, kResponseMeta));
}

int HttpServerLuaBindings::luaResponseGc(lua_State* L)
{
    auto* userdata = checkResponse(L);
    userdata->response.~shared_ptr<HttpResponse>();
    return 0;
}

int HttpServerLuaBindings::luaResponseStatus(lua_State* L)
{
    auto* userdata = checkResponse(L);
    userdata->response->setStatus(static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

int HttpServerLuaBindings::luaResponseSetHeader(lua_State* L)
{
    auto* userdata = checkResponse(L);
    std::string name = varn::lua::LuaHelpers::checkString(L, 2);
    std::string value = varn::lua::LuaHelpers::checkString(L, 3);
    userdata->response->setHeader(name, value);
    return 0;
}

int HttpServerLuaBindings::luaResponseWrite(lua_State* L)
{
    auto* userdata = checkResponse(L);
    return HttpServerModule::pushWrite(L, *userdata->response, 2);
}

int HttpServerLuaBindings::luaResponseFinish(lua_State* L)
{
    auto* userdata = checkResponse(L);
    userdata->response->end(varn::lua::LuaHelpers::optionalString(L, 2, ""));
    return 0;
}

#if VARN_JSON_DRIVER_NLOHMANN
int HttpServerLuaBindings::luaResponseJson(lua_State* L)
{
    auto* userdata = checkResponse(L);
    std::string body = lua_istable(L, 2) ? json::JsonSerializer::serialize(L, 2) : "{}";

    userdata->response->setHeader("Content-Type", "application/json; charset=utf-8");
    userdata->response->end(std::move(body));
    return 0;
}
#endif

#if VARN_XML_DRIVER_PUGIXML
int HttpServerLuaBindings::luaResponseXml(lua_State* L)
{
    auto* userdata = checkResponse(L);
    std::string body =
        lua_istable(L, 2) ? xml::XmlSerializer::serialize(L, 2) : std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?><root/>\n");

    userdata->response->setHeader("Content-Type", "application/xml; charset=utf-8");
    userdata->response->end(std::move(body));
    return 0;
}
#endif

// Ends a response the handler left open, as a failure when the handler failed and with 204 or the end of its stream when it returned.
void HttpServerLuaBindings::finishResponse(HttpResponse& response, bool failed)
{
    if (failed)
    {
        response.fail();
        return;
    }

    if (response.ended())
    {
        return;
    }

    response.setStatus(204);
    response.end(std::string());
}

int HttpServerLuaBindings::handlerContinuation(lua_State* L, int status, lua_KContext ctx)
{
    (void)ctx;
    const bool failed = status != LUA_OK && status != LUA_YIELD;

    auto* userdata = static_cast<ResponseUserdata*>(luaL_checkudata(L, lua_upvalueindex(2), kResponseMeta));
    finishResponse(*userdata->response, failed);

    if (failed)
    {
        async::AsyncModule::report(L, "HttpServerLuaBindings");
    }

    return 0;
}

// Runs the handler of the server as a task that may await, with a handler that keeps the traceback of a failure, and ends the response however the handler ends.
int HttpServerLuaBindings::handlerBody(lua_State* L)
{
    lua_pushcfunction(L, &async::AsyncModule::capture);
    lua_insert(L, 1);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 2);
    const int status = lua_pcallk(L, 2, 0, 1, 0, &HttpServerLuaBindings::handlerContinuation);
    return handlerContinuation(L, status, 0);
}

int HttpServerLuaBindings::luaServerGc(lua_State* L)
{
    auto* userdata = static_cast<ServerBuilderUserdata*>(luaL_checkudata(L, 1, kServerMeta));
    if (userdata->runtime && userdata->handlerRef != LUA_NOREF)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, userdata->handlerRef);
        userdata->handlerRef = LUA_NOREF;
    }

    return 0;
}

// Starts the server on the port or the options given and answers the listening server.
int HttpServerLuaBindings::luaServerListen(lua_State* L)
{
    auto* builder = static_cast<ServerBuilderUserdata*>(luaL_checkudata(L, 1, kServerMeta));
    HttpServerOptions options = HttpServerModule::readListenOptions(L, 2);
    Runtime& rt = *builder->runtime;

    // Duplicate the registry ref for the native server lifetime because the builder userdata may be collected once the chunk returns.
    lua_rawgeti(L, LUA_REGISTRYINDEX, builder->handlerRef);
    const int persistedHandlerRef = luaL_ref(L, LUA_REGISTRYINDEX);

    Runtime* rtPtr = &rt;
    // clang-format off
    auto handler = [rtPtr, persistedHandlerRef](HttpRequest request, std::shared_ptr<HttpResponse> response)
    {
        lua_State* mainState = rtPtr->luaState();
        lua_State* thread = lua_newthread(mainState);
        const int threadRef = luaL_ref(mainState, LUA_REGISTRYINDEX);

        // The body keeps the handler and the response as its upvalues and takes the request and the response as its arguments.
        lua_rawgeti(thread, LUA_REGISTRYINDEX, persistedHandlerRef);
        HttpServerLuaBindings::pushResponse(thread, response);
        lua_pushvalue(thread, -1);
        lua_insert(thread, -3);
        lua_pushcclosure(thread, &HttpServerLuaBindings::handlerBody, 2);
        HttpRequestLua::pushRequestUserdata(thread, std::move(request));
        lua_rotate(thread, 1, -1);

        int nres = 0;
        const int status = lua_resume(thread, mainState, 2, &nres);
        if (status != LUA_OK && status != LUA_YIELD)
        {
            lua_xmove(thread, mainState, 1);
            HttpServerLuaBindings::finishResponse(*response, true);
            async::AsyncModule::report(mainState, "HttpServerLuaBindings");
        }

        // Once the handler yields the promise machinery holds its own ref to the coroutine, so release ours in every case.
        luaL_unref(mainState, LUA_REGISTRYINDEX, threadRef);
    };
    // clang-format on

    auto* engine = new ReactorHttpServer(rt, std::move(options), std::move(handler));

    // The runtime owns this `shared_ptr` and drops it inside `stop()`, which a host may call from another thread, so the registry is only touched while the Lua state is still driven by the loop.
    // clang-format off
    auto server = std::shared_ptr<ReactorHttpServer>(
        engine,
        [rtPtr, persistedHandlerRef](ReactorHttpServer* p)
        {
            if (persistedHandlerRef != LUA_NOREF && !rtPtr->stopped())
            {
                luaL_unref(rtPtr->luaState(), LUA_REGISTRYINDEX, persistedHandlerRef);
            }

            delete p;
        });
    // clang-format on

    HttpServerModule::startServer(L, server, "HttpServerLuaBindings");
    return 1;
}

void HttpServerLuaBindings::pushServer(lua_State* L, const std::shared_ptr<ReactorHttpServer>& server)
{
    void* memory = lua_newuserdatauv(L, sizeof(ListeningUserdata), 0);
    new (memory) ListeningUserdata{server, server->host(), server->port()};
    luaL_getmetatable(L, kListeningMeta);
    lua_setmetatable(L, -2);
}

int HttpServerLuaBindings::luaListeningIndex(lua_State* L)
{
    auto* userdata = static_cast<ListeningUserdata*>(luaL_checkudata(L, 1, kListeningMeta));
    const char* key = lua_tostring(L, 2);
    if (key == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }

    if (std::strcmp(key, "host") == 0)
    {
        lua_pushlstring(L, userdata->host.data(), userdata->host.size());
        return 1;
    }

    if (std::strcmp(key, "port") == 0)
    {
        lua_pushinteger(L, userdata->port);
        return 1;
    }

    if (std::strcmp(key, "close") == 0)
    {
        lua_pushcfunction(L, &HttpServerLuaBindings::luaListeningClose);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// Closes the server and answers a promise that resolves once its listener and every connection are closed and the runtime let it go.
int HttpServerLuaBindings::luaListeningClose(lua_State* L)
{
    auto* userdata = static_cast<ListeningUserdata*>(luaL_checkudata(L, 1, kListeningMeta));

    bool graceful = false;
    long long timeoutMs = kCloseTimeoutMs;
    if (!lua_isnoneornil(L, 2))
    {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, 2) != 0)
        {
            const std::string name = HttpServerModule::optionName(L, "HttpServer");
            if (name == "graceful")
            {
                graceful = HttpServerModule::optionBoolean(L, "HttpServer", name);
            }
            else if (name == "timeoutMs")
            {
                timeoutMs = HttpServerModule::optionInteger(L, "HttpServer", name, 1, HttpServerModule::kMaxDelayMs);
            }
            else
            {
                luaL_error(L, "[HttpServer] The close option \"%s\" is unknown.", name.c_str());
            }

            lua_pop(L, 1);
        }
    }

    Runtime& rt = luaRuntime(L);
    auto promise = std::make_shared<async::Promise>(rt);
    auto server = userdata->server.lock();

    // clang-format off
    auto settle = [promise]()
    {
        promise->resolveCustom([](lua_State* state)
        {
            lua_pushboolean(state, 1);
        });
    };
    // clang-format on

    if (!server)
    {
        settle();
        async::Promise::push(L, promise);
        return 1;
    }

    Runtime* rtPtr = &rt;
    const HttpServer* key = server.get();
    // clang-format off
    server->close(graceful, timeoutMs, [rtPtr, key, settle]()
    {
        rtPtr->removeServer(key);
        settle();
    });
    // clang-format on

    async::Promise::push(L, promise);
    return 1;
}

int HttpServerLuaBindings::luaListeningGc(lua_State* L)
{
    auto* userdata = static_cast<ListeningUserdata*>(luaL_checkudata(L, 1, kListeningMeta));
    std::destroy_at(userdata);
    return 0;
}

int HttpServerLuaBindings::luaCreateServer(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);

    auto& rt = luaRuntime(L);
    void* memory = lua_newuserdatauv(L, sizeof(ServerBuilderUserdata), 0);
    auto* userdata = new (memory) ServerBuilderUserdata();
    userdata->runtime = &rt;

    lua_pushvalue(L, 1);
    userdata->handlerRef = luaL_ref(L, LUA_REGISTRYINDEX);

    luaL_getmetatable(L, kServerMeta);
    lua_setmetatable(L, -2);

    return 1;
}

void HttpServerLuaBindings::createMetatables(lua_State* L)
{
    HttpRequestLua::createRequestMetatable(L);

    if (luaL_newmetatable(L, kResponseMeta))
    {
        lua_pushcfunction(L, &HttpServerLuaBindings::luaResponseGc);
        lua_setfield(L, -2, "__gc");

        lua_newtable(L);
        lua_pushcfunction(L, &HttpServerLuaBindings::luaResponseStatus);
        lua_setfield(L, -2, "status");
        lua_pushcfunction(L, &HttpServerLuaBindings::luaResponseSetHeader);
        lua_setfield(L, -2, "setHeader");
        lua_pushcfunction(L, &HttpServerLuaBindings::luaResponseWrite);
        lua_setfield(L, -2, "write");
        lua_pushcfunction(L, &HttpServerLuaBindings::luaResponseFinish);
        lua_setfield(L, -2, "finish");
#if VARN_JSON_DRIVER_NLOHMANN
        lua_pushcfunction(L, &HttpServerLuaBindings::luaResponseJson);
        lua_setfield(L, -2, "json");
#endif
#if VARN_XML_DRIVER_PUGIXML
        lua_pushcfunction(L, &HttpServerLuaBindings::luaResponseXml);
        lua_setfield(L, -2, "xml");
#endif
        lua_setfield(L, -2, "__index");
    }

    lua_pop(L, 1);

    if (luaL_newmetatable(L, kServerMeta))
    {
        lua_pushcfunction(L, &HttpServerLuaBindings::luaServerGc);
        lua_setfield(L, -2, "__gc");

        lua_newtable(L);
        lua_pushcfunction(L, &HttpServerLuaBindings::luaServerListen);
        lua_setfield(L, -2, "listen");
        lua_setfield(L, -2, "__index");
    }

    lua_pop(L, 1);

    if (luaL_newmetatable(L, kListeningMeta))
    {
        lua_pushcfunction(L, &HttpServerLuaBindings::luaListeningGc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, &HttpServerLuaBindings::luaListeningIndex);
        lua_setfield(L, -2, "__index");
    }

    lua_pop(L, 1);
}

int HttpServerLuaBindings::luaOpen(lua_State* L)
{
    createMetatables(L);

    lua_newtable(L);
    lua_pushcfunction(L, &HttpServerLuaBindings::luaCreateServer);
    lua_setfield(L, -2, "createServer");
    HttpAppModule::registerApp(L);
    HttpClientModule::registerClient(L);
    return 1;
}

void HttpServerModule::pushRequestTable(lua_State* L, const HttpRequest& request)
{
    lua_newtable(L);

    lua_pushlstring(L, request.host.data(), request.host.size());
    lua_setfield(L, -2, "host");

    lua_pushlstring(L, request.method.data(), request.method.size());
    lua_setfield(L, -2, "method");

    lua_pushlstring(L, request.path.data(), request.path.size());
    lua_setfield(L, -2, "path");

    lua_pushlstring(L, request.target.data(), request.target.size());
    lua_setfield(L, -2, "target");

    lua_pushlstring(L, request.queryString.data(), request.queryString.size());
    lua_setfield(L, -2, "queryString");

    lua_pushlstring(L, request.body.data(), request.body.size());
    lua_setfield(L, -2, "body");

    lua_pushlstring(L, request.remoteAddress.data(), request.remoteAddress.size());
    lua_setfield(L, -2, "remoteAddress");

    varn::lua::LuaHelpers::pushStringMap(L, request.headers);
    lua_setfield(L, -2, "headers");

    varn::lua::LuaHelpers::pushStringMap(L, request.cookies);
    lua_setfield(L, -2, "cookies");

    varn::lua::LuaHelpers::pushStringMap(L, request.query);
    lua_setfield(L, -2, "query");
}

// Streams the string at the index as the next part of the response and pushes a promise that resolves once it left for the client, so a writer that awaits it never runs ahead of the client.
int HttpServerModule::pushWrite(lua_State* L, HttpResponse& response, int index)
{
    std::size_t length = 0;
    const char* data = luaL_checklstring(L, index, &length);
    auto promise = std::make_shared<async::Promise>(*static_cast<Runtime*>(varn::lua::LuaHelpers::getRuntime(L)));

    // clang-format off
    response.write(std::string(data, length), [promise](std::string error)
    {
        if (error.empty())
        {
            promise->resolve("ok");
            return;
        }

        promise->reject(std::move(error));
    });
    // clang-format on

    async::Promise::push(L, promise);
    return 1;
}

// Starts the server, hands it to the runtime and pushes the listening server, or raises with the cause when the address cannot be taken.
void HttpServerModule::startServer(lua_State* L, const std::shared_ptr<ReactorHttpServer>& server, const char* tag)
{
    // The message is copied out of the exception first, since raising inside the handler would leave the exception live while Lua unwinds.
    std::string refused;
    try
    {
        server->start();
    }
    catch (const std::exception& ex)
    {
        refused = ex.what();
    }

    if (!refused.empty())
    {
        luaL_error(L, "%s", refused.c_str());
        return;
    }

    auto& rt = *static_cast<Runtime*>(varn::lua::LuaHelpers::getRuntime(L));
    rt.addServer(server);

    std::ostringstream started;
    started << "Listening on http" << (server->tls() ? "s" : "") << "://" << server->host() << ":" << server->port() << ".";
    log::Log::line(tag, started.str());

    HttpServerLuaBindings::pushServer(L, server);
}

// Reads the name of the option at the key slot of a table walk, which must be a string.
std::string HttpServerModule::optionName(lua_State* L, const char* tag)
{
    if (lua_type(L, -2) != LUA_TSTRING)
    {
        luaL_error(L, "[%s] An option must be named by a string.", tag);
    }

    return lua_tostring(L, -2);
}

bool HttpServerModule::optionBoolean(lua_State* L, const char* tag, const std::string& name)
{
    if (!lua_isboolean(L, -1))
    {
        luaL_error(L, "[%s] The option \"%s\" must be a boolean.", tag, name.c_str());
    }

    return lua_toboolean(L, -1) != 0;
}

std::string HttpServerModule::optionString(lua_State* L, const char* tag, const std::string& name)
{
    if (lua_type(L, -1) != LUA_TSTRING)
    {
        luaL_error(L, "[%s] The option \"%s\" must be a string.", tag, name.c_str());
    }

    std::size_t length = 0;
    const char* value = lua_tolstring(L, -1, &length);
    return std::string(value, length);
}

long long HttpServerModule::optionInteger(lua_State* L, const char* tag, const std::string& name, long long min, long long max)
{
    if (!lua_isinteger(L, -1) || lua_tointeger(L, -1) < min || lua_tointeger(L, -1) > max)
    {
        luaL_error(L, "[%s] The option \"%s\" must be an integer from %I to %I.", tag, name.c_str(), static_cast<lua_Integer>(min), static_cast<lua_Integer>(max));
    }

    return static_cast<long long>(lua_tointeger(L, -1));
}

void HttpServerModule::readListenOption(lua_State* L, const std::string& name, HttpServerOptions& options)
{
    constexpr const char* tag = "HttpServer";
    if (name == "host")
    {
        options.host = optionString(L, tag, name);
        return;
    }

    if (name == "port")
    {
        options.port = static_cast<int>(optionInteger(L, tag, name, 0, 65535));
        return;
    }

    if (name == "reusePort")
    {
        options.reusePort = optionBoolean(L, tag, name);
        return;
    }

    if (name == "publicDir")
    {
        options.publicDir = optionString(L, tag, name);
        return;
    }

    if (name == "servePublic")
    {
        options.servePublic = optionBoolean(L, tag, name);
        return;
    }

    if (name == "directoryListing")
    {
        options.directoryListing = optionBoolean(L, tag, name);
        return;
    }

    if (name == "tls")
    {
        options.tls = optionBoolean(L, tag, name);
        return;
    }

    if (name == "certFile")
    {
        options.certFile = optionString(L, tag, name);
        return;
    }

    if (name == "keyFile")
    {
        options.keyFile = optionString(L, tag, name);
        return;
    }

    if (name == "maxQueued")
    {
        options.maxQueued = static_cast<int>(optionInteger(L, tag, name, 1, 65535));
        return;
    }

    if (name == "requestTimeoutMs")
    {
        options.requestTimeoutMs = optionInteger(L, tag, name, 0, kMaxDelayMs);
        return;
    }

    if (name == "keepAliveTimeoutSeconds")
    {
        options.keepAliveTimeoutSeconds = static_cast<int>(optionInteger(L, tag, name, 0, INT_MAX / 1000));
        return;
    }

    if (name == "maxRequestBodyBytes")
    {
        options.maxRequestBodyBytes = optionInteger(L, tag, name, 1, kMaxRequestBodyBytes);
        return;
    }

    if (name == "compress")
    {
        options.compress = optionBoolean(L, tag, name);
        return;
    }

    luaL_error(L, "[HttpServer] The listen option \"%s\" is unknown.", name.c_str());
}

// Reads the port or the options table of `listen`, refusing a port outside its range, a value of the wrong type and an option it does not know, each by name.
HttpServerOptions HttpServerModule::readListenOptions(lua_State* L, int index)
{
    HttpServerOptions options;

    // Only the worker processes of `VARN_WORKERS` share their port, so a second server of one process fails on a busy address.
    options.reusePort = varn::runtime::App::workerCount() > 1;

    const char* envPort = std::getenv("VARN_PORT");
    if (envPort)
    {
        const int parsed = std::atoi(envPort);
        if (parsed >= 1 && parsed <= 65535)
        {
            options.port = parsed;
        }
        else
        {
            log::Log::line("HttpServerModule", "The variable \"VARN_PORT\" is not a valid port (1-65535) and was ignored.");
        }
    }

    const char* cert = std::getenv("VARN_TLS_CERT");
    const char* key = std::getenv("VARN_TLS_KEY");
    if (cert && key)
    {
        options.tls = true;
        options.certFile = cert;
        options.keyFile = key;
    }

    if (lua_isnoneornil(L, index))
    {
        return options;
    }

    if (lua_isinteger(L, index))
    {
        lua_pushvalue(L, index);
        options.port = static_cast<int>(optionInteger(L, "HttpServer", "port", 0, 65535));
        lua_pop(L, 1);
        return options;
    }

    if (!lua_istable(L, index))
    {
        luaL_error(L, "[HttpServer] The method \"listen\" takes a port or a table of options.");
    }

    const int table = lua_absindex(L, index);
    bool servePublicSet = false;
    lua_pushnil(L);
    while (lua_next(L, table) != 0)
    {
        const std::string name = optionName(L, "HttpServer");
        readListenOption(L, name, options);
        servePublicSet = servePublicSet || name == "servePublic";
        lua_pop(L, 1);
    }

    // Naming a folder is what turns static files on, unless the options turn them off.
    if (!servePublicSet && !options.publicDir.empty())
    {
        options.servePublic = true;
    }

    if (options.servePublic && options.publicDir.empty())
    {
        luaL_error(L, "[HttpServer] The option \"servePublic\" needs the option \"publicDir\".");
    }

    return options;
}

void HttpServerModule::install(lua_State* L)
{
    luaL_requiref(L, "http", &HttpServerLuaBindings::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::http
