#include "varn/http/HttpServerModule.h"
#include "varn/http/HttpAppModule.h"
#include "varn/http/HttpClientModule.h"
#include "varn/http/HttpTypes.h"
#include "varn/http/drivers/reactor/ReactorHttpServer.h"
#include "varn/log/Log.h"
#if VARN_JSON_DRIVER_NLOHMANN
#include "varn/json/JsonSerializer.h"
#endif
#include "varn/lua/LuaHelpers.h"
#include "varn/runtime/Runtime.h"
#if VARN_XML_DRIVER_PUGIXML
#include "varn/xml/XmlSerializer.h"
#endif

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace varn::http
{

using varn::runtime::Runtime;

namespace
{
constexpr const char* kRequestMeta = "varn.HttpRequest";

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
        // the request is owned by this userdata so its fields are pushed only when the handler reads them, and it survives across an await
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

private:
    static constexpr const char* kServerMeta = "varn.HttpServerBuilder";
    static constexpr const char* kResponseMeta = "varn.HttpResponse";

    struct ServerBuilderUserdata
    {
        Runtime* runtime = nullptr;
        int handlerRef = LUA_NOREF;
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
    static int luaResponseFinish(lua_State* L);
#if VARN_JSON_DRIVER_NLOHMANN
    static int luaResponseJson(lua_State* L);
#endif
#if VARN_XML_DRIVER_PUGIXML
    static int luaResponseXml(lua_State* L);
#endif
    static int luaServerGc(lua_State* L);
    static int luaServerListen(lua_State* L);
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

int HttpServerLuaBindings::luaResponseFinish(lua_State* L)
{
    auto* userdata = checkResponse(L);
    std::string body = varn::lua::LuaHelpers::optionalString(L, 2, "");
    userdata->response->end(body);
    return 0;
}

#if VARN_JSON_DRIVER_NLOHMANN
int HttpServerLuaBindings::luaResponseJson(lua_State* L)
{
    auto* userdata = checkResponse(L);
    const std::string body = lua_istable(L, 2) ? json::JsonSerializer::serialize(L, 2) : "{}";

    userdata->response->setHeader("Content-Type", "application/json; charset=utf-8");
    userdata->response->end(body);
    return 0;
}
#endif

#if VARN_XML_DRIVER_PUGIXML
int HttpServerLuaBindings::luaResponseXml(lua_State* L)
{
    auto* userdata = checkResponse(L);
    const std::string body =
        lua_istable(L, 2) ? xml::XmlSerializer::serialize(L, 2) : std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?><root/>\n");

    userdata->response->setHeader("Content-Type", "application/xml; charset=utf-8");
    userdata->response->end(body);
    return 0;
}
#endif

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

int HttpServerLuaBindings::luaServerListen(lua_State* L)
{
    auto* builder = static_cast<ServerBuilderUserdata*>(luaL_checkudata(L, 1, kServerMeta));

    HttpServerOptions options;
    if (lua_isinteger(L, 2))
    {
        options.port = lua_tointeger(L, 2);
    }
    else if (lua_istable(L, 2))
    {
        options = HttpServerModule::readListenOptions(L, 2);
    }
    else
    {
        options = HttpServerModule::readListenOptions(L, 3);
    }

    Runtime& rt = *builder->runtime;

    // duplicate the registry ref for the native server lifetime because the builder userdata may be collected once the chunk returns
    lua_rawgeti(L, LUA_REGISTRYINDEX, builder->handlerRef);
    const int persistedHandlerRef = luaL_ref(L, LUA_REGISTRYINDEX);

    Runtime* rtPtr = &rt;
    // clang-format off
    auto handler = [rtPtr, persistedHandlerRef](HttpRequest request, std::shared_ptr<HttpResponse> response)
    {
        lua_State* mainState = rtPtr->luaState();
        lua_State* thread = lua_newthread(mainState);
        int threadRef = luaL_ref(mainState, LUA_REGISTRYINDEX);

        lua_rawgeti(mainState, LUA_REGISTRYINDEX, persistedHandlerRef);
        lua_xmove(mainState, thread, 1);

        HttpRequestLua::pushRequestUserdata(thread, std::move(request));
        HttpServerLuaBindings::pushResponse(thread, response);

        int nres = 0;
        int status = lua_resume(thread, mainState, 2, &nres);

        if (status != LUA_OK && status != LUA_YIELD)
        {
            const char* message = lua_tostring(thread, -1);
            std::string detail = message ? message : "A request handler failed without a message.";
            log::Log::error("HttpServerLuaBindings", detail);
            response->setStatus(500);
            response->end("Internal server error.");

            if (nres > 0)
            {
                lua_pop(thread, nres);
            }
        }
        else if (status == LUA_OK)
        {
            if (!response->ended())
            {
                response->setStatus(204);
                response->end("");
            }
        }

        // once the handler yields the promise machinery holds its own ref to the coroutine, so release ours in every case
        luaL_unref(mainState, LUA_REGISTRYINDEX, threadRef);
    };
    // clang-format on

    // the announcement reads the endpoint before the options are moved into the engine
    std::ostringstream started;
    started << "Listening on " << (options.tls ? "https" : "http") << "://" << options.host << ":" << options.port << ".";

    auto* engine = new ReactorHttpServer(rt, std::move(options), std::move(handler));

    // the runtime owns this shared_ptr and drops it inside stop(), which a host may call from another thread, so the registry is only touched while the lua state is still driven by the loop
    // clang-format off
    auto server = std::shared_ptr<HttpServer>(
        engine,
        [rtPtr, persistedHandlerRef](HttpServer* p)
        {
            if (persistedHandlerRef != LUA_NOREF && !rtPtr->stopped())
            {
                luaL_unref(rtPtr->luaState(), LUA_REGISTRYINDEX, persistedHandlerRef);
            }

            delete p;
        });
    // clang-format on
    server->start();
    rt.addServer(server);

    log::Log::line("HttpServerLuaBindings", started.str());

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

HttpServerOptions HttpServerModule::readListenOptions(lua_State* L, int index)
{
    HttpServerOptions options;

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
            log::Log::line("HttpServerModule", "VARN_PORT is not a valid port (1-65535) and was ignored.");
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

    if (!lua_istable(L, index))
    {
        return options;
    }

    lua_getfield(L, index, "host");
    options.host = varn::lua::LuaHelpers::optionalString(L, -1, options.host);
    lua_pop(L, 1);

    lua_getfield(L, index, "port");
    if (lua_isinteger(L, -1))
    {
        const lua_Integer portValue = lua_tointeger(L, -1);
        if (portValue >= 1 && portValue <= 65535)
        {
            options.port = static_cast<int>(portValue);
        }
    }

    lua_pop(L, 1);

    lua_getfield(L, index, "publicDir");
    options.publicDir = varn::lua::LuaHelpers::optionalString(L, -1, options.publicDir);
    lua_pop(L, 1);

    lua_getfield(L, index, "servePublic");
    if (lua_isboolean(L, -1))
        options.servePublic = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    lua_getfield(L, index, "directoryListing");
    if (lua_isboolean(L, -1))
        options.directoryListing = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    lua_getfield(L, index, "tls");
    if (lua_isboolean(L, -1))
        options.tls = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    lua_getfield(L, index, "certFile");
    options.certFile = varn::lua::LuaHelpers::optionalString(L, -1, options.certFile);
    lua_pop(L, 1);

    lua_getfield(L, index, "keyFile");
    options.keyFile = varn::lua::LuaHelpers::optionalString(L, -1, options.keyFile);
    lua_pop(L, 1);

    lua_getfield(L, index, "maxQueued");
    if (lua_isinteger(L, -1))
    {
        const auto value = static_cast<int>(lua_tointeger(L, -1));
        if (value > 0)
        {
            options.maxQueued = std::clamp(value, 1, 65535);
        }
    }

    lua_pop(L, 1);

    lua_getfield(L, index, "maxThreads");
    if (lua_isinteger(L, -1))
    {
        const auto value = static_cast<int>(lua_tointeger(L, -1));
        if (value > 0)
        {
            options.maxThreads = std::clamp(value, 1, 1024);
        }
    }

    lua_pop(L, 1);

    lua_getfield(L, index, "requestTimeoutMs");
    if (lua_isinteger(L, -1))
    {
        const long long value = lua_tointeger(L, -1);
        if (value >= 0)
        {
            options.requestTimeoutMs = value;
        }
    }

    lua_pop(L, 1);

    lua_getfield(L, index, "keepAliveTimeoutSeconds");
    if (lua_isinteger(L, -1))
    {
        const int value = static_cast<int>(lua_tointeger(L, -1));
        if (value >= 0)
        {
            options.keepAliveTimeoutSeconds = value;
        }
    }

    lua_pop(L, 1);

    lua_getfield(L, index, "compress");
    if (lua_isboolean(L, -1))
        options.compress = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    return options;
}

void HttpServerModule::install(lua_State* L)
{
    luaL_requiref(L, "http", &HttpServerLuaBindings::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::http
