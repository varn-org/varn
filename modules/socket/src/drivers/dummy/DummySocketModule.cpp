#include "varn/socket/SocketTransport.h"

#include <string>

namespace varn::socket
{

void SocketTransport::connectAsync(varn::runtime::Runtime& /*runtime*/, const std::string& /*host*/, int /*port*/, int /*timeoutMs*/, std::size_t /*maxQueuedBytes*/, ConnectCallback callback)
{
    callback(nullptr, "The socket module is not available in this build.");
}

void SocketTransport::connectTlsAsync(varn::runtime::Runtime& /*runtime*/, const std::string& /*host*/, int /*port*/, int /*timeoutMs*/, bool /*verify*/, std::size_t /*maxQueuedBytes*/, ConnectCallback callback)
{
    callback(nullptr, "The socket module is not available in this build.");
}

void SocketTransport::connectUnixAsync(varn::runtime::Runtime& /*runtime*/, const std::string& /*path*/, std::size_t /*maxQueuedBytes*/, ConnectCallback callback)
{
    callback(nullptr, "The socket module is not available in this build.");
}

void SocketTransport::listenAsync(varn::runtime::Runtime& /*runtime*/, const std::string& /*host*/, int /*port*/, const TcpListenOptions& /*options*/, ListenCallback callback)
{
    callback(nullptr, "The socket module is not available in this build.");
}

void SocketTransport::listenUnixAsync(varn::runtime::Runtime& /*runtime*/, const std::string& /*path*/, int /*backlog*/, std::size_t /*maxQueuedBytes*/, ListenCallback callback)
{
    callback(nullptr, "The socket module is not available in this build.");
}

void SocketTransport::bindUdpAsync(varn::runtime::Runtime& /*runtime*/, const std::string& /*host*/, int /*port*/, BindCallback callback)
{
    callback(nullptr, "The socket module is not available in this build.");
}

void SocketTransport::resolveAsync(varn::runtime::Runtime& /*runtime*/, const std::string& /*host*/, ResolveCallback callback)
{
    callback(false, {}, "The socket module is not available in this build.");
}

} // namespace varn::socket
