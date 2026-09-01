#include "varn/socket/SocketModule.h"

#include "varn/async/Promise.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/runtime/Runtime.h"
#include "varn/socket/SocketTransport.h"

#include <lua.hpp>

#include <memory>
#include <string>
#include <utility>

namespace varn::socket
{

using varn::async::Promise;
using varn::runtime::Runtime;

constexpr const char* kTcpSocketMeta = "varn.TcpSocket";
constexpr const char* kTcpListenerMeta = "varn.TcpListener";
constexpr const char* kUdpSocketMeta = "varn.UdpSocket";

Runtime& SocketModule::luaRuntime(lua_State* L)
{
    return *static_cast<Runtime*>(varn::lua::LuaHelpers::getRuntime(L));
}

void SocketModule::pushTcpSocket(lua_State* L, std::shared_ptr<TcpConnection> conn)
{
    void* memory = lua_newuserdatauv(L, sizeof(std::shared_ptr<TcpConnection>), 0);
    new (memory) std::shared_ptr<TcpConnection>(std::move(conn));
    luaL_getmetatable(L, kTcpSocketMeta);
    lua_setmetatable(L, -2);
}

void SocketModule::pushTcpListener(lua_State* L, std::shared_ptr<TcpListener> listener)
{
    void* memory = lua_newuserdatauv(L, sizeof(std::shared_ptr<TcpListener>), 0);
    new (memory) std::shared_ptr<TcpListener>(std::move(listener));
    luaL_getmetatable(L, kTcpListenerMeta);
    lua_setmetatable(L, -2);
    luaRuntime(L).retainBackgroundDriver();
}

void SocketModule::pushUdpSocket(lua_State* L, std::shared_ptr<UdpSocket> socket)
{
    void* memory = lua_newuserdatauv(L, sizeof(std::shared_ptr<UdpSocket>), 0);
    new (memory) std::shared_ptr<UdpSocket>(std::move(socket));
    luaL_getmetatable(L, kUdpSocketMeta);
    lua_setmetatable(L, -2);
    luaRuntime(L).retainBackgroundDriver();
}

std::shared_ptr<TcpConnection>* SocketModule::checkTcpSocket(lua_State* L, int index)
{
    return static_cast<std::shared_ptr<TcpConnection>*>(luaL_checkudata(L, index, kTcpSocketMeta));
}

std::shared_ptr<TcpListener>* SocketModule::checkTcpListener(lua_State* L, int index)
{
    return static_cast<std::shared_ptr<TcpListener>*>(luaL_checkudata(L, index, kTcpListenerMeta));
}

std::shared_ptr<UdpSocket>* SocketModule::checkUdpSocket(lua_State* L, int index)
{
    return static_cast<std::shared_ptr<UdpSocket>*>(luaL_checkudata(L, index, kUdpSocketMeta));
}

int SocketModule::luaTcpSocketGc(lua_State* L)
{
    auto* holder = static_cast<std::shared_ptr<TcpConnection>*>(lua_touserdata(L, 1));
    if (holder != nullptr)
    {
        if (*holder)
        {
            (*holder)->close();
        }

        std::destroy_at(holder);
    }

    return 0;
}

int SocketModule::luaTcpListenerGc(lua_State* L)
{
    auto* holder = static_cast<std::shared_ptr<TcpListener>*>(lua_touserdata(L, 1));
    if (holder != nullptr)
    {
        if (*holder)
        {
            (*holder)->close();
        }

        std::destroy_at(holder);
        luaRuntime(L).releaseBackgroundDriver();
    }

    return 0;
}

int SocketModule::luaUdpSocketGc(lua_State* L)
{
    auto* holder = static_cast<std::shared_ptr<UdpSocket>*>(lua_touserdata(L, 1));
    if (holder != nullptr)
    {
        if (*holder)
        {
            (*holder)->close();
        }

        std::destroy_at(holder);
        luaRuntime(L).releaseBackgroundDriver();
    }

    return 0;
}

int SocketModule::luaTcpConnect(lua_State* L)
{
    const char* host = luaL_checkstring(L, 1);
    const lua_Integer portArg = luaL_checkinteger(L, 2);
    if (portArg < 1 || portArg > 65535)
    {
        return luaL_error(L, "[SocketModule] Port must be between 1 and 65535.");
    }

    const int port = static_cast<int>(portArg);

    const lua_Integer timeoutArg = luaL_optinteger(L, 3, 0);
    int timeoutMs = 0;
    if (timeoutArg > 0)
    {
        timeoutMs = timeoutArg > 3600000 ? 3600000 : static_cast<int>(timeoutArg);
    }

    auto* runtime = &luaRuntime(L);
    std::string hostStr = host;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    SocketTransport::connectAsync(*runtime, hostStr, port, timeoutMs, [promise, runtime](std::shared_ptr<TcpConnection> conn, const std::string& error)
    {
        if (conn)
        {
            promise->resolveCustom([conn](lua_State* lua)
            {
                pushTcpSocket(lua, conn);
            });
        }
        else
        {
            promise->reject(error);
        }

        runtime->releaseBackgroundDriver();
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaTcpListen(lua_State* L)
{
    const char* host = luaL_checkstring(L, 1);
    const lua_Integer portArg = luaL_checkinteger(L, 2);
    const lua_Integer backlogArg = luaL_optinteger(L, 3, 64);
    if (portArg < 1 || portArg > 65535)
    {
        return luaL_error(L, "[SocketModule] Port must be between 1 and 65535.");
    }

    if (backlogArg < 1 || backlogArg > 4096)
    {
        return luaL_error(L, "[SocketModule] Backlog must be between 1 and 4096.");
    }

    auto& rt = luaRuntime(L);
    auto promise = std::make_shared<Promise>(rt);

    try
    {
        auto listener = SocketTransport::listen(rt, host, static_cast<int>(portArg), static_cast<int>(backlogArg));
        promise->resolveCustom([listener](lua_State* lua)
                               { pushTcpListener(lua, listener); });
    }
    catch (const std::exception& ex)
    {
        promise->reject(ex.what());
    }

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaTlsConnect(lua_State* L)
{
    const char* host = luaL_checkstring(L, 1);
    const lua_Integer portArg = luaL_checkinteger(L, 2);
    if (portArg < 1 || portArg > 65535)
    {
        return luaL_error(L, "[SocketModule] Port must be between 1 and 65535.");
    }

    const int port = static_cast<int>(portArg);

    int timeoutMs = 0;
    bool verify = true;
    if (lua_istable(L, 3))
    {
        lua_getfield(L, 3, "timeoutMs");
        if (lua_isinteger(L, -1))
        {
            const lua_Integer timeoutArg = lua_tointeger(L, -1);
            if (timeoutArg > 0)
            {
                timeoutMs = timeoutArg > 3600000 ? 3600000 : static_cast<int>(timeoutArg);
            }
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "insecure");
        verify = !lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    auto* runtime = &luaRuntime(L);
    std::string hostStr = host;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    SocketTransport::connectTlsAsync(*runtime, hostStr, port, timeoutMs, verify, [promise, runtime](std::shared_ptr<TcpConnection> conn, const std::string& error)
    {
        if (conn)
        {
            promise->resolveCustom([conn](lua_State* lua)
            {
                pushTcpSocket(lua, conn);
            });
        }
        else
        {
            promise->reject(error);
        }

        runtime->releaseBackgroundDriver();
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUnixConnect(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);

    auto* runtime = &luaRuntime(L);
    std::string pathStr = path;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    SocketTransport::connectUnixAsync(*runtime, pathStr, [promise, runtime](std::shared_ptr<TcpConnection> conn, const std::string& error)
    {
        if (conn)
        {
            promise->resolveCustom([conn](lua_State* lua)
            {
                pushTcpSocket(lua, conn);
            });
        }
        else
        {
            promise->reject(error);
        }

        runtime->releaseBackgroundDriver();
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUnixListen(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const lua_Integer backlogArg = luaL_optinteger(L, 2, 64);
    if (backlogArg < 1 || backlogArg > 4096)
    {
        return luaL_error(L, "[SocketModule] Backlog must be between 1 and 4096.");
    }

    auto& rt = luaRuntime(L);
    auto promise = std::make_shared<Promise>(rt);

    try
    {
        auto listener = SocketTransport::listenUnix(rt, path, static_cast<int>(backlogArg));
        promise->resolveCustom([listener](lua_State* lua)
                               { pushTcpListener(lua, listener); });
    }
    catch (const std::exception& ex)
    {
        promise->reject(ex.what());
    }

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaTcpSocketSend(lua_State* L)
{
    auto* holder = checkTcpSocket(L, 1);
    size_t len = 0;
    const char* data = luaL_checklstring(L, 2, &len);

    auto* runtime = &luaRuntime(L);
    auto conn = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    conn->sendAsync(std::string(data, len), [promise, runtime](bool ok, const std::string& error)
    {
        if (ok)
        {
            promise->resolve("ok");
        }
        else
        {
            promise->reject(error);
        }

        runtime->releaseBackgroundDriver();
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaTcpSocketReceive(lua_State* L)
{
    auto* holder = checkTcpSocket(L, 1);
    const int maxBytes = static_cast<int>(luaL_optinteger(L, 2, 65536));
    if (maxBytes <= 0)
    {
        return luaL_error(L, "[SocketModule] The maximum number of bytes to receive must be positive.");
    }

    auto* runtime = &luaRuntime(L);
    auto conn = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    conn->receiveAsync(maxBytes, [promise, runtime](bool ok, const std::string& data)
    {
        if (ok)
        {
            promise->resolve(data);
        }
        else
        {
            promise->reject(data);
        }

        runtime->releaseBackgroundDriver();
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaTcpSocketStartTls(lua_State* L)
{
    auto* holder = checkTcpSocket(L, 1);
    const std::string host = luaL_checkstring(L, 2);

    bool verify = true;
    if (lua_istable(L, 3))
    {
        lua_getfield(L, 3, "insecure");
        verify = !lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    auto* runtime = &luaRuntime(L);
    auto conn = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    conn->startTlsAsync(*runtime, host, verify, [promise, runtime](bool ok, const std::string& error)
    {
        if (ok)
        {
            promise->resolve("ok");
        }
        else
        {
            promise->reject(error);
        }

        runtime->releaseBackgroundDriver();
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaTcpSocketClose(lua_State* L)
{
    auto* holder = checkTcpSocket(L, 1);
    auto conn = *holder;
    auto promise = std::make_shared<Promise>(luaRuntime(L));

    conn->close();
    promise->resolve("ok");

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaTcpListenerAccept(lua_State* L)
{
    auto* holder = checkTcpListener(L, 1);
    auto* runtime = &luaRuntime(L);
    auto listener = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    listener->acceptAsync([promise, runtime](std::shared_ptr<TcpConnection> conn, const std::string& error)
    {
        if (conn)
        {
            promise->resolveCustom([conn](lua_State* lua)
            {
                pushTcpSocket(lua, conn);
            });
        }
        else
        {
            promise->reject(error);
        }

        runtime->releaseBackgroundDriver();
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaTcpListenerClose(lua_State* L)
{
    auto* holder = checkTcpListener(L, 1);
    auto listener = *holder;
    auto promise = std::make_shared<Promise>(luaRuntime(L));

    listener->close();
    promise->resolve("ok");

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUdpBind(lua_State* L)
{
    const char* host = luaL_checkstring(L, 1);
    const lua_Integer portArg = luaL_checkinteger(L, 2);
    if (portArg < 1 || portArg > 65535)
    {
        return luaL_error(L, "[SocketModule] Port must be between 1 and 65535.");
    }

    auto& rt = luaRuntime(L);
    auto promise = std::make_shared<Promise>(rt);

    try
    {
        auto socket = SocketTransport::bindUdp(rt, host, static_cast<int>(portArg));
        promise->resolveCustom([socket](lua_State* lua)
                               { pushUdpSocket(lua, socket); });
    }
    catch (const std::exception& ex)
    {
        promise->reject(ex.what());
    }

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUdpSocketSendTo(lua_State* L)
{
    auto* holder = checkUdpSocket(L, 1);
    const char* host = luaL_checkstring(L, 2);
    const lua_Integer portArg = luaL_checkinteger(L, 3);
    if (portArg < 1 || portArg > 65535)
    {
        return luaL_error(L, "[SocketModule] Port must be between 1 and 65535.");
    }

    size_t len = 0;
    const char* data = luaL_checklstring(L, 4, &len);

    auto* runtime = &luaRuntime(L);
    auto socket = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    socket->sendToAsync(host, static_cast<int>(portArg), std::string(data, len), [promise, runtime](bool ok, const std::string& error)
    {
        if (ok)
        {
            promise->resolve("ok");
        }
        else
        {
            promise->reject(error);
        }

        runtime->releaseBackgroundDriver();
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUdpSocketRecvFrom(lua_State* L)
{
    auto* holder = checkUdpSocket(L, 1);
    const int maxBytes = static_cast<int>(luaL_optinteger(L, 2, 65536));
    if (maxBytes <= 0)
    {
        return luaL_error(L, "[SocketModule] The maximum number of bytes to receive must be positive.");
    }

    auto* runtime = &luaRuntime(L);
    auto socket = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    socket->receiveFromAsync(maxBytes, [promise, runtime](bool ok, const UdpDatagram& datagram, const std::string& error)
    {
        if (ok)
        {
            UdpDatagram received = datagram;
            promise->resolveCustom([received](lua_State* lua)
            {
                lua_createtable(lua, 0, 3);
                lua_pushlstring(lua, received.data.data(), received.data.size());
                lua_setfield(lua, -2, "data");
                lua_pushlstring(lua, received.host.data(), received.host.size());
                lua_setfield(lua, -2, "host");
                lua_pushinteger(lua, received.port);
                lua_setfield(lua, -2, "port");
            });
        }
        else
        {
            promise->reject(error);
        }

        runtime->releaseBackgroundDriver();
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUdpSocketClose(lua_State* L)
{
    auto* holder = checkUdpSocket(L, 1);
    auto socket = *holder;
    auto promise = std::make_shared<Promise>(luaRuntime(L));

    socket->close();
    promise->resolve("ok");

    Promise::push(L, promise);
    return 1;
}

void SocketModule::createMetatables(lua_State* L)
{
    if (luaL_newmetatable(L, kTcpSocketMeta))
    {
        lua_pushcfunction(L, &SocketModule::luaTcpSocketGc);
        lua_setfield(L, -2, "__gc");

        lua_newtable(L);
        lua_pushcfunction(L, &SocketModule::luaTcpSocketSend);
        lua_setfield(L, -2, "send");
        lua_pushcfunction(L, &SocketModule::luaTcpSocketReceive);
        lua_setfield(L, -2, "receive");
        lua_pushcfunction(L, &SocketModule::luaTcpSocketStartTls);
        lua_setfield(L, -2, "startTls");
        lua_pushcfunction(L, &SocketModule::luaTcpSocketClose);
        lua_setfield(L, -2, "close");
        lua_setfield(L, -2, "__index");
    }

    lua_pop(L, 1);

    if (luaL_newmetatable(L, kTcpListenerMeta))
    {
        lua_pushcfunction(L, &SocketModule::luaTcpListenerGc);
        lua_setfield(L, -2, "__gc");

        lua_newtable(L);
        lua_pushcfunction(L, &SocketModule::luaTcpListenerAccept);
        lua_setfield(L, -2, "accept");
        lua_pushcfunction(L, &SocketModule::luaTcpListenerClose);
        lua_setfield(L, -2, "close");
        lua_setfield(L, -2, "__index");
    }

    lua_pop(L, 1);

    if (luaL_newmetatable(L, kUdpSocketMeta))
    {
        lua_pushcfunction(L, &SocketModule::luaUdpSocketGc);
        lua_setfield(L, -2, "__gc");

        lua_newtable(L);
        lua_pushcfunction(L, &SocketModule::luaUdpSocketSendTo);
        lua_setfield(L, -2, "sendTo");
        lua_pushcfunction(L, &SocketModule::luaUdpSocketRecvFrom);
        lua_setfield(L, -2, "recvFrom");
        lua_pushcfunction(L, &SocketModule::luaUdpSocketClose);
        lua_setfield(L, -2, "close");
        lua_setfield(L, -2, "__index");
    }

    lua_pop(L, 1);
}

int SocketModule::luaOpen(lua_State* L)
{
    createMetatables(L);

    lua_newtable(L);

    lua_newtable(L);
    lua_pushcfunction(L, &SocketModule::luaTcpConnect);
    lua_setfield(L, -2, "connect");
    lua_pushcfunction(L, &SocketModule::luaTcpListen);
    lua_setfield(L, -2, "listen");
    lua_setfield(L, -2, "tcp");

    lua_newtable(L);
    lua_pushcfunction(L, &SocketModule::luaTlsConnect);
    lua_setfield(L, -2, "connect");
    lua_setfield(L, -2, "tls");

    lua_newtable(L);
    lua_pushcfunction(L, &SocketModule::luaUnixConnect);
    lua_setfield(L, -2, "connect");
    lua_pushcfunction(L, &SocketModule::luaUnixListen);
    lua_setfield(L, -2, "listen");
    lua_setfield(L, -2, "unix");

    lua_newtable(L);
    lua_pushcfunction(L, &SocketModule::luaUdpBind);
    lua_setfield(L, -2, "bind");
    lua_setfield(L, -2, "udp");

    return 1;
}

void SocketModule::install(lua_State* L)
{
    luaL_requiref(L, "socket", &SocketModule::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::socket
