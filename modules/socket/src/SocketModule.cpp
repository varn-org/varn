#include "varn/socket/SocketModule.h"

#include "varn/async/Promise.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/runtime/Runtime.h"
#include "varn/socket/SocketTransport.h"

#include <lua.hpp>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace varn::socket
{

using varn::async::Promise;
using varn::runtime::Runtime;

constexpr const char* kTcpSocketMeta = "varn.TcpSocket";
constexpr const char* kTcpListenerMeta = "varn.TcpListener";
constexpr const char* kUdpSocketMeta = "varn.UdpSocket";
constexpr lua_Integer kMaxBatchDatagrams = 1024;
constexpr lua_Integer kDefaultBacklog = 64;
constexpr lua_Integer kMaxBacklog = 4096;
constexpr lua_Integer kMaxTimeoutMs = 3600000;
constexpr lua_Integer kDefaultMaxQueuedBytes = 64 * 1024 * 1024;

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

// Pushes a received datagram as the table `{ data, host, port }`.
void SocketModule::pushDatagram(lua_State* L, const UdpDatagram& datagram)
{
    lua_createtable(L, 0, 3);
    lua_pushlstring(L, datagram.data.data(), datagram.data.size());
    lua_setfield(L, -2, "data");
    lua_pushlstring(L, datagram.host.data(), datagram.host.size());
    lua_setfield(L, -2, "host");
    lua_pushinteger(L, datagram.port);
    lua_setfield(L, -2, "port");
}

// Pushes the host of an endpoint, or its port, which is `nil` for a Unix-domain path.
void SocketModule::pushEndpointField(lua_State* L, const SocketEndpoint& endpoint, bool port)
{
    if (!port)
    {
        lua_pushlstring(L, endpoint.host.data(), endpoint.host.size());
        return;
    }

    if (!endpoint.port)
    {
        lua_pushnil(L);
        return;
    }

    lua_pushinteger(L, *endpoint.port);
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

// Checks the full integer of a port, so a value past the range cannot wrap into an accepted one.
int SocketModule::checkPort(lua_State* L, int index, lua_Integer lowest)
{
    const lua_Integer port = luaL_checkinteger(L, index);
    if (port < lowest || port > 65535)
    {
        luaL_error(L, "[SocketModule] Port must be between %d and 65535.", static_cast<int>(lowest));
    }

    return static_cast<int>(port);
}

// Answers whether an options table was given, raising for anything but a table or nothing.
bool SocketModule::checkOptions(lua_State* L, int index)
{
    if (lua_isnoneornil(L, index))
    {
        return false;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    return true;
}

lua_Integer SocketModule::integerOption(lua_State* L, int index, const char* name, lua_Integer fallback, lua_Integer lowest, lua_Integer highest)
{
    if (!checkOptions(L, index))
    {
        return fallback;
    }

    lua_getfield(L, index, name);
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        return fallback;
    }

    int isInteger = 0;
    const lua_Integer value = lua_tointegerx(L, -1, &isInteger);
    lua_pop(L, 1);
    if (!isInteger || value < lowest || value > highest)
    {
        luaL_error(L, "[SocketModule] The option \"%s\" must be an integer from %lld to %lld.", name, static_cast<long long>(lowest), static_cast<long long>(highest));
    }

    return value;
}

bool SocketModule::booleanOption(lua_State* L, int index, const char* name, bool fallback)
{
    if (!checkOptions(L, index))
    {
        return fallback;
    }

    lua_getfield(L, index, name);
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        return fallback;
    }

    if (!lua_isboolean(L, -1))
    {
        luaL_error(L, "[SocketModule] The option \"%s\" must be a boolean.", name);
    }

    const bool value = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return value;
}

// Reads the bound on the bytes a socket queues for sending, checked on its full width so it fits a size on every target.
std::size_t SocketModule::maxQueuedBytesOption(lua_State* L, int index)
{
    const auto highest = static_cast<lua_Integer>(std::min<std::uint64_t>(SIZE_MAX, static_cast<std::uint64_t>(LUA_MAXINTEGER)));
    return static_cast<std::size_t>(integerOption(L, index, "maxQueuedBytes", kDefaultMaxQueuedBytes, 1, highest));
}

// Settles the promise of an operation that answers a socket and lets the loop go of the operation.
void SocketModule::settleConnection(const std::shared_ptr<Promise>& promise, Runtime* runtime, std::shared_ptr<TcpConnection> conn, const std::string& error)
{
    if (conn)
    {
        // clang-format off
        promise->resolveCustom([conn = std::move(conn)](lua_State* lua)
        {
            pushTcpSocket(lua, conn);
        });
        // clang-format on
    }
    else
    {
        promise->reject(error);
    }

    runtime->releaseBackgroundDriver();
}

// Settles the promise of an operation that answers only whether it worked and lets the loop go of the operation.
void SocketModule::settleSend(const std::shared_ptr<Promise>& promise, Runtime* runtime, bool ok, const std::string& error)
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

// Answers the addresses of a socket as fields and every other key from its methods.
int SocketModule::luaTcpSocketIndex(lua_State* L)
{
    const auto& conn = *checkTcpSocket(L, 1);
    const char* key = lua_type(L, 2) == LUA_TSTRING ? lua_tostring(L, 2) : nullptr;
    if (key != nullptr && std::strcmp(key, "localHost") == 0)
    {
        pushEndpointField(L, conn->localEndpoint(), false);
        return 1;
    }

    if (key != nullptr && std::strcmp(key, "localPort") == 0)
    {
        pushEndpointField(L, conn->localEndpoint(), true);
        return 1;
    }

    if (key != nullptr && std::strcmp(key, "peerHost") == 0)
    {
        pushEndpointField(L, conn->peerEndpoint(), false);
        return 1;
    }

    if (key != nullptr && std::strcmp(key, "peerPort") == 0)
    {
        pushEndpointField(L, conn->peerEndpoint(), true);
        return 1;
    }

    if (key != nullptr && std::strcmp(key, "pendingBytes") == 0)
    {
        lua_pushinteger(L, static_cast<lua_Integer>(conn->pendingBytes()));
        return 1;
    }

    lua_pushvalue(L, 2);
    lua_rawget(L, lua_upvalueindex(1));
    return 1;
}

int SocketModule::luaTcpListenerIndex(lua_State* L)
{
    const auto& listener = *checkTcpListener(L, 1);
    const char* key = lua_type(L, 2) == LUA_TSTRING ? lua_tostring(L, 2) : nullptr;
    if (key != nullptr && (std::strcmp(key, "host") == 0 || std::strcmp(key, "port") == 0))
    {
        pushEndpointField(L, listener->endpoint(), std::strcmp(key, "port") == 0);
        return 1;
    }

    lua_pushvalue(L, 2);
    lua_rawget(L, lua_upvalueindex(1));
    return 1;
}

int SocketModule::luaUdpSocketIndex(lua_State* L)
{
    const auto& socket = *checkUdpSocket(L, 1);
    const char* key = lua_type(L, 2) == LUA_TSTRING ? lua_tostring(L, 2) : nullptr;
    if (key != nullptr && (std::strcmp(key, "host") == 0 || std::strcmp(key, "port") == 0))
    {
        pushEndpointField(L, socket->endpoint(), std::strcmp(key, "port") == 0);
        return 1;
    }

    lua_pushvalue(L, 2);
    lua_rawget(L, lua_upvalueindex(1));
    return 1;
}

// Resolves to the list of every address of the host, looked up on the I/O pool.
int SocketModule::luaResolve(lua_State* L)
{
    const std::string host = luaL_checkstring(L, 1);

    auto* runtime = &luaRuntime(L);
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    SocketTransport::resolveAsync(*runtime, host, [promise, runtime](bool ok, std::vector<std::string> addresses, const std::string& error)
    {
        if (ok)
        {
            auto found = std::make_shared<const std::vector<std::string>>(std::move(addresses));
            promise->resolveCustom([found](lua_State* lua)
            {
                lua_createtable(lua, static_cast<int>(found->size()), 0);
                for (std::size_t index = 0; index < found->size(); ++index)
                {
                    lua_pushlstring(lua, (*found)[index].data(), (*found)[index].size());
                    lua_rawseti(lua, -2, static_cast<lua_Integer>(index + 1));
                }
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

int SocketModule::luaTcpConnect(lua_State* L)
{
    const std::string host = luaL_checkstring(L, 1);
    const int port = checkPort(L, 2, 1);
    const auto timeoutMs = static_cast<int>(integerOption(L, 3, "timeoutMs", 0, 0, kMaxTimeoutMs));
    const std::size_t maxQueuedBytes = maxQueuedBytesOption(L, 3);

    auto* runtime = &luaRuntime(L);
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    SocketTransport::connectAsync(*runtime, host, port, timeoutMs, maxQueuedBytes, [promise, runtime](std::shared_ptr<TcpConnection> conn, const std::string& error)
    {
        settleConnection(promise, runtime, std::move(conn), error);
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaTcpListen(lua_State* L)
{
    const std::string host = luaL_checkstring(L, 1);
    const int port = checkPort(L, 2, 0);

    TcpListenOptions options;
    options.backlog = static_cast<int>(integerOption(L, 3, "backlog", kDefaultBacklog, 1, kMaxBacklog));
    options.reusePort = booleanOption(L, 3, "reusePort", false);
    options.maxQueuedBytes = maxQueuedBytesOption(L, 3);

    auto* runtime = &luaRuntime(L);
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    SocketTransport::listenAsync(*runtime, host, port, options, [promise, runtime](std::shared_ptr<TcpListener> listener, const std::string& error)
    {
        if (listener)
        {
            promise->resolveCustom([listener](lua_State* lua)
            {
                pushTcpListener(lua, listener);
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

int SocketModule::luaTlsConnect(lua_State* L)
{
    const std::string host = luaL_checkstring(L, 1);
    const int port = checkPort(L, 2, 1);
    const auto timeoutMs = static_cast<int>(integerOption(L, 3, "timeoutMs", 0, 0, kMaxTimeoutMs));
    const bool verify = !booleanOption(L, 3, "insecure", false);
    const std::size_t maxQueuedBytes = maxQueuedBytesOption(L, 3);

    auto* runtime = &luaRuntime(L);
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    SocketTransport::connectTlsAsync(*runtime, host, port, timeoutMs, verify, maxQueuedBytes, [promise, runtime](std::shared_ptr<TcpConnection> conn, const std::string& error)
    {
        settleConnection(promise, runtime, std::move(conn), error);
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUnixConnect(lua_State* L)
{
    const std::string path = luaL_checkstring(L, 1);
    const std::size_t maxQueuedBytes = maxQueuedBytesOption(L, 2);

    auto* runtime = &luaRuntime(L);
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    SocketTransport::connectUnixAsync(*runtime, path, maxQueuedBytes, [promise, runtime](std::shared_ptr<TcpConnection> conn, const std::string& error)
    {
        settleConnection(promise, runtime, std::move(conn), error);
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUnixListen(lua_State* L)
{
    const std::string path = luaL_checkstring(L, 1);
    const auto backlog = static_cast<int>(integerOption(L, 2, "backlog", kDefaultBacklog, 1, kMaxBacklog));
    const std::size_t maxQueuedBytes = maxQueuedBytesOption(L, 2);

    auto* runtime = &luaRuntime(L);
    auto promise = std::make_shared<Promise>(*runtime);

    // clang-format off
    SocketTransport::listenUnixAsync(*runtime, path, backlog, maxQueuedBytes, [promise](std::shared_ptr<TcpListener> listener, const std::string& error)
    {
        if (!listener)
        {
            promise->reject(error);
            return;
        }

        promise->resolveCustom([listener](lua_State* lua)
        {
            pushTcpListener(lua, listener);
        });
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUdpBind(lua_State* L)
{
    const std::string host = luaL_checkstring(L, 1);
    const int port = checkPort(L, 2, 0);

    auto* runtime = &luaRuntime(L);
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    SocketTransport::bindUdpAsync(*runtime, host, port, [promise, runtime](std::shared_ptr<UdpSocket> socket, const std::string& error)
    {
        if (socket)
        {
            promise->resolveCustom([socket](lua_State* lua)
            {
                pushUdpSocket(lua, socket);
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
        settleSend(promise, runtime, ok, error);
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaTcpSocketReceive(lua_State* L)
{
    auto* holder = checkTcpSocket(L, 1);
    const lua_Integer maxBytes = luaL_optinteger(L, 2, 65536);
    if (maxBytes <= 0 || maxBytes > INT_MAX)
    {
        return luaL_error(L, "[SocketModule] The maximum number of bytes to receive must be positive.");
    }

    auto* runtime = &luaRuntime(L);
    auto conn = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    conn->receiveAsync(static_cast<int>(maxBytes), [promise, runtime](bool ok, std::string data)
    {
        if (ok)
        {
            promise->resolve(std::move(data));
        }
        else
        {
            promise->reject(std::move(data));
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
    const bool verify = !booleanOption(L, 3, "insecure", false);

    auto* runtime = &luaRuntime(L);
    auto conn = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    conn->startTlsAsync(*runtime, host, verify, [promise, runtime](bool ok, const std::string& error)
    {
        settleSend(promise, runtime, ok, error);
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

// Shuts the sending side down after every send already queued, so the peer reads the end of the stream while this side still receives.
int SocketModule::luaTcpSocketShutdown(lua_State* L)
{
    auto* holder = checkTcpSocket(L, 1);
    const std::string how = luaL_checkstring(L, 2);
    if (how != "send")
    {
        return luaL_error(L, "[SocketModule] The method \"shutdown\" takes \"send\", not \"%s\".", how.c_str());
    }

    auto* runtime = &luaRuntime(L);
    auto conn = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    conn->shutdownSendAsync([promise, runtime](bool ok, const std::string& error)
    {
        settleSend(promise, runtime, ok, error);
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaTcpSocketSetNoDelay(lua_State* L)
{
    auto* holder = checkTcpSocket(L, 1);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    const bool enabled = lua_toboolean(L, 2) != 0;

    auto promise = std::make_shared<Promise>(luaRuntime(L));
    try
    {
        (*holder)->setNoDelay(enabled);
        promise->resolve("ok");
    }
    catch (const std::exception& ex)
    {
        promise->reject(ex.what());
    }

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaTcpSocketSetKeepAlive(lua_State* L)
{
    auto* holder = checkTcpSocket(L, 1);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    const bool enabled = lua_toboolean(L, 2) != 0;

    auto promise = std::make_shared<Promise>(luaRuntime(L));
    try
    {
        (*holder)->setKeepAlive(enabled);
        promise->resolve("ok");
    }
    catch (const std::exception& ex)
    {
        promise->reject(ex.what());
    }

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
        settleConnection(promise, runtime, std::move(conn), error);
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

int SocketModule::luaUdpSocketSendTo(lua_State* L)
{
    auto* holder = checkUdpSocket(L, 1);
    const std::string host = luaL_checkstring(L, 2);
    const int port = checkPort(L, 3, 1);
    size_t len = 0;
    const char* data = luaL_checklstring(L, 4, &len);

    auto* runtime = &luaRuntime(L);
    auto socket = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    socket->sendToAsync(host, port, std::string(data, len), [promise, runtime](bool ok, const std::string& error)
    {
        settleSend(promise, runtime, ok, error);
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUdpSocketRecvFrom(lua_State* L)
{
    auto* holder = checkUdpSocket(L, 1);
    const lua_Integer maxBytes = luaL_optinteger(L, 2, 65536);
    if (maxBytes <= 0 || maxBytes > INT_MAX)
    {
        return luaL_error(L, "[SocketModule] The maximum number of bytes to receive must be positive.");
    }

    auto* runtime = &luaRuntime(L);
    auto socket = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    socket->receiveFromAsync(static_cast<int>(maxBytes), [promise, runtime](bool ok, const UdpDatagram& datagram, const std::string& error)
    {
        if (ok)
        {
            UdpDatagram received = datagram;
            promise->resolveCustom([received](lua_State* lua)
            {
                pushDatagram(lua, received);
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

// Sends a list of `{ host, port, data }` datagrams in order, resolving to how many were sent once all of them left.
int SocketModule::luaUdpSocketSendMany(lua_State* L)
{
    auto* holder = checkUdpSocket(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const lua_Integer count = luaL_len(L, 2);
    if (count < 0 || count > kMaxBatchDatagrams)
    {
        return luaL_error(L, "[SocketModule] A batch holds at most %d datagrams.", static_cast<int>(kMaxBatchDatagrams));
    }

    std::vector<UdpDatagram> datagrams;
    datagrams.reserve(static_cast<std::size_t>(count));
    for (lua_Integer index = 1; index <= count; ++index)
    {
        if (lua_geti(L, 2, index) != LUA_TTABLE)
        {
            return luaL_error(L, "[SocketModule] The datagram at index %d must be a table.", static_cast<int>(index));
        }

        UdpDatagram& datagram = datagrams.emplace_back();
        if (lua_getfield(L, -1, "host") != LUA_TSTRING)
        {
            return luaL_error(L, "[SocketModule] The datagram at index %d needs a \"host\" string.", static_cast<int>(index));
        }

        datagram.host = lua_tostring(L, -1);
        lua_pop(L, 1);

        int isInteger = 0;
        lua_getfield(L, -1, "port");
        const lua_Integer port = lua_tointegerx(L, -1, &isInteger);
        if (!isInteger || port < 1 || port > 65535)
        {
            return luaL_error(L, "[SocketModule] The \"port\" of the datagram at index %d must be between 1 and 65535.", static_cast<int>(index));
        }

        datagram.port = static_cast<int>(port);
        lua_pop(L, 1);

        if (lua_getfield(L, -1, "data") != LUA_TSTRING)
        {
            return luaL_error(L, "[SocketModule] The datagram at index %d needs a \"data\" string.", static_cast<int>(index));
        }

        size_t len = 0;
        const char* data = lua_tolstring(L, -1, &len);
        datagram.data.assign(data, len);
        lua_pop(L, 2);
    }

    auto* runtime = &luaRuntime(L);
    auto socket = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    socket->sendManyAsync(std::move(datagrams), [promise, runtime, count](bool ok, const std::string& error)
    {
        if (ok)
        {
            promise->resolveCustom([count](lua_State* lua) { lua_pushinteger(lua, count); });
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

// Waits for a datagram and resolves to the list of every datagram already waiting, up to `maxDatagrams`.
int SocketModule::luaUdpSocketRecvMany(lua_State* L)
{
    auto* holder = checkUdpSocket(L, 1);
    const lua_Integer maxDatagrams = luaL_optinteger(L, 2, 64);
    if (maxDatagrams < 1 || maxDatagrams > kMaxBatchDatagrams)
    {
        return luaL_error(L, "[SocketModule] The maximum number of datagrams must be between 1 and %d.", static_cast<int>(kMaxBatchDatagrams));
    }

    const lua_Integer maxBytes = luaL_optinteger(L, 3, 65536);
    if (maxBytes <= 0 || maxBytes > INT_MAX)
    {
        return luaL_error(L, "[SocketModule] The maximum number of bytes to receive must be positive.");
    }

    auto* runtime = &luaRuntime(L);
    auto socket = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    socket->receiveManyAsync(static_cast<int>(maxDatagrams), static_cast<int>(maxBytes), [promise, runtime](bool ok, std::vector<UdpDatagram> datagrams, const std::string& error)
    {
        if (ok)
        {
            auto received = std::make_shared<const std::vector<UdpDatagram>>(std::move(datagrams));
            promise->resolveCustom([received](lua_State* lua)
            {
                lua_createtable(lua, static_cast<int>(received->size()), 0);
                for (std::size_t index = 0; index < received->size(); ++index)
                {
                    pushDatagram(lua, (*received)[index]);
                    lua_rawseti(lua, -2, static_cast<lua_Integer>(index + 1));
                }
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

// Sets the only peer the socket sends to with `send` and receives from.
int SocketModule::luaUdpSocketConnect(lua_State* L)
{
    auto* holder = checkUdpSocket(L, 1);
    const std::string host = luaL_checkstring(L, 2);
    const int port = checkPort(L, 3, 1);

    auto* runtime = &luaRuntime(L);
    auto socket = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    socket->connectAsync(host, port, [promise, runtime](bool ok, const std::string& error)
    {
        settleSend(promise, runtime, ok, error);
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUdpSocketSend(lua_State* L)
{
    auto* holder = checkUdpSocket(L, 1);
    size_t len = 0;
    const char* data = luaL_checklstring(L, 2, &len);

    auto* runtime = &luaRuntime(L);
    auto socket = *holder;
    auto promise = std::make_shared<Promise>(*runtime);

    runtime->retainBackgroundDriver();
    // clang-format off
    socket->sendAsync(std::string(data, len), [promise, runtime](bool ok, const std::string& error)
    {
        settleSend(promise, runtime, ok, error);
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUdpSocketSetBroadcast(lua_State* L)
{
    auto* holder = checkUdpSocket(L, 1);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    const bool enabled = lua_toboolean(L, 2) != 0;

    auto promise = std::make_shared<Promise>(luaRuntime(L));
    try
    {
        (*holder)->setBroadcast(enabled);
        promise->resolve("ok");
    }
    catch (const std::exception& ex)
    {
        promise->reject(ex.what());
    }

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUdpSocketJoinGroup(lua_State* L)
{
    auto* holder = checkUdpSocket(L, 1);
    const std::string group = luaL_checkstring(L, 2);

    auto promise = std::make_shared<Promise>(luaRuntime(L));
    try
    {
        (*holder)->joinGroup(group);
        promise->resolve("ok");
    }
    catch (const std::exception& ex)
    {
        promise->reject(ex.what());
    }

    Promise::push(L, promise);
    return 1;
}

int SocketModule::luaUdpSocketLeaveGroup(lua_State* L)
{
    auto* holder = checkUdpSocket(L, 1);
    const std::string group = luaL_checkstring(L, 2);

    auto promise = std::make_shared<Promise>(luaRuntime(L));
    try
    {
        (*holder)->leaveGroup(group);
        promise->resolve("ok");
    }
    catch (const std::exception& ex)
    {
        promise->reject(ex.what());
    }

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

// Creates a metatable whose `__index` answers the fields of the object and then its methods.
void SocketModule::createMetatable(lua_State* L, const char* name, lua_CFunction gc, lua_CFunction index, const luaL_Reg* methods)
{
    if (luaL_newmetatable(L, name))
    {
        lua_pushcfunction(L, gc);
        lua_setfield(L, -2, "__gc");

        lua_newtable(L);
        luaL_setfuncs(L, methods, 0);
        lua_pushcclosure(L, index, 1);
        lua_setfield(L, -2, "__index");
    }

    lua_pop(L, 1);
}

void SocketModule::createMetatables(lua_State* L)
{
    const luaL_Reg socketMethods[] = {
        {"send", &SocketModule::luaTcpSocketSend},
        {"receive", &SocketModule::luaTcpSocketReceive},
        {"startTls", &SocketModule::luaTcpSocketStartTls},
        {"shutdown", &SocketModule::luaTcpSocketShutdown},
        {"setNoDelay", &SocketModule::luaTcpSocketSetNoDelay},
        {"setKeepAlive", &SocketModule::luaTcpSocketSetKeepAlive},
        {"close", &SocketModule::luaTcpSocketClose},
        {nullptr, nullptr},
    };
    createMetatable(L, kTcpSocketMeta, &SocketModule::luaTcpSocketGc, &SocketModule::luaTcpSocketIndex, socketMethods);

    const luaL_Reg listenerMethods[] = {
        {"accept", &SocketModule::luaTcpListenerAccept},
        {"close", &SocketModule::luaTcpListenerClose},
        {nullptr, nullptr},
    };
    createMetatable(L, kTcpListenerMeta, &SocketModule::luaTcpListenerGc, &SocketModule::luaTcpListenerIndex, listenerMethods);

    const luaL_Reg udpMethods[] = {
        {"sendTo", &SocketModule::luaUdpSocketSendTo},
        {"recvFrom", &SocketModule::luaUdpSocketRecvFrom},
        {"sendMany", &SocketModule::luaUdpSocketSendMany},
        {"recvMany", &SocketModule::luaUdpSocketRecvMany},
        {"connect", &SocketModule::luaUdpSocketConnect},
        {"send", &SocketModule::luaUdpSocketSend},
        {"setBroadcast", &SocketModule::luaUdpSocketSetBroadcast},
        {"joinGroup", &SocketModule::luaUdpSocketJoinGroup},
        {"leaveGroup", &SocketModule::luaUdpSocketLeaveGroup},
        {"close", &SocketModule::luaUdpSocketClose},
        {nullptr, nullptr},
    };
    createMetatable(L, kUdpSocketMeta, &SocketModule::luaUdpSocketGc, &SocketModule::luaUdpSocketIndex, udpMethods);
}

int SocketModule::luaOpen(lua_State* L)
{
    createMetatables(L);

    lua_newtable(L);
    lua_pushcfunction(L, &SocketModule::luaResolve);
    lua_setfield(L, -2, "resolve");

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
