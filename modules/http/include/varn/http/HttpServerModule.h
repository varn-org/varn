#pragma once

#include "varn/http/HttpTypes.h"

#include <memory>
#include <string>

struct lua_State;

namespace varn::http
{

class ReactorHttpServer;

class HttpServerModule
{
public:
    HttpServerModule() = delete;

    static constexpr long long kMaxDelayMs = 2147483647;

    static void install(struct lua_State* L);
    static HttpServerOptions readListenOptions(lua_State* L, int index);
    static void pushRequestTable(lua_State* L, const HttpRequest& request);
    static int pushWrite(lua_State* L, HttpResponse& response, int index);
    static void startServer(lua_State* L, const std::shared_ptr<ReactorHttpServer>& server, const char* tag);
    static std::string optionName(lua_State* L, const char* tag);
    static bool optionBoolean(lua_State* L, const char* tag, const std::string& name);
    static std::string optionString(lua_State* L, const char* tag, const std::string& name);
    static long long optionInteger(lua_State* L, const char* tag, const std::string& name, long long min, long long max);

private:
    static void readListenOption(lua_State* L, const std::string& name, HttpServerOptions& options);
};

} // namespace varn::http
