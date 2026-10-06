#pragma once

#include <lua.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace varn::runtime
{
class Runtime;
}

namespace varn::async
{
class Promise;
}

namespace varn::socket
{

class TcpConnection;
class TcpListener;
class UdpSocket;
struct SocketEndpoint;
struct UdpDatagram;

class SocketModule
{
public:
    SocketModule() = delete;

    static void install(lua_State* L);

private:
    static varn::runtime::Runtime& luaRuntime(lua_State* L);

    static void pushTcpSocket(lua_State* L, std::shared_ptr<TcpConnection> conn);
    static void pushTcpListener(lua_State* L, std::shared_ptr<TcpListener> listener);
    static void pushUdpSocket(lua_State* L, std::shared_ptr<UdpSocket> socket);
    static void pushDatagram(lua_State* L, const UdpDatagram& datagram);
    static void pushEndpointField(lua_State* L, const SocketEndpoint& endpoint, bool port);

    static std::shared_ptr<TcpConnection>* checkTcpSocket(lua_State* L, int index);
    static std::shared_ptr<TcpListener>* checkTcpListener(lua_State* L, int index);
    static std::shared_ptr<UdpSocket>* checkUdpSocket(lua_State* L, int index);

    static int checkPort(lua_State* L, int index, lua_Integer lowest);
    static bool checkOptions(lua_State* L, int index);
    static lua_Integer integerOption(lua_State* L, int index, const char* name, lua_Integer fallback, lua_Integer lowest, lua_Integer highest);
    static bool booleanOption(lua_State* L, int index, const char* name, bool fallback);
    static std::size_t maxQueuedBytesOption(lua_State* L, int index);

    static void settleConnection(const std::shared_ptr<varn::async::Promise>& promise, varn::runtime::Runtime* runtime, std::shared_ptr<TcpConnection> conn, const std::string& error);
    static void settleSend(const std::shared_ptr<varn::async::Promise>& promise, varn::runtime::Runtime* runtime, bool ok, const std::string& error);

    static void createMetatables(lua_State* L);
    static void createMetatable(lua_State* L, const char* name, lua_CFunction gc, lua_CFunction index, const luaL_Reg* methods);

    static int luaTcpSocketGc(lua_State* L);
    static int luaTcpListenerGc(lua_State* L);
    static int luaUdpSocketGc(lua_State* L);
    static int luaTcpSocketIndex(lua_State* L);
    static int luaTcpListenerIndex(lua_State* L);
    static int luaUdpSocketIndex(lua_State* L);
    static int luaResolve(lua_State* L);
    static int luaTcpConnect(lua_State* L);
    static int luaTcpListen(lua_State* L);
    static int luaTlsConnect(lua_State* L);
    static int luaUnixConnect(lua_State* L);
    static int luaUnixListen(lua_State* L);
    static int luaUdpBind(lua_State* L);
    static int luaTcpSocketSend(lua_State* L);
    static int luaTcpSocketReceive(lua_State* L);
    static int luaTcpSocketStartTls(lua_State* L);
    static int luaTcpSocketShutdown(lua_State* L);
    static int luaTcpSocketSetNoDelay(lua_State* L);
    static int luaTcpSocketSetKeepAlive(lua_State* L);
    static int luaTcpSocketClose(lua_State* L);
    static int luaTcpListenerAccept(lua_State* L);
    static int luaTcpListenerClose(lua_State* L);
    static int luaUdpSocketSendTo(lua_State* L);
    static int luaUdpSocketRecvFrom(lua_State* L);
    static int luaUdpSocketSendMany(lua_State* L);
    static int luaUdpSocketRecvMany(lua_State* L);
    static int luaUdpSocketConnect(lua_State* L);
    static int luaUdpSocketSend(lua_State* L);
    static int luaUdpSocketSetBroadcast(lua_State* L);
    static int luaUdpSocketJoinGroup(lua_State* L);
    static int luaUdpSocketLeaveGroup(lua_State* L);
    static int luaUdpSocketClose(lua_State* L);
    static int luaOpen(lua_State* L);
};

} // namespace varn::socket
