#include "varn/socket/SocketTransport.h"

#include "PocoSocketStream.h"

#include <string>

namespace varn::socket
{

void SocketTransport::connectTlsAsync(varn::runtime::Runtime& /*runtime*/, const std::string& /*host*/, int /*port*/, int /*timeoutMs*/, bool /*verify*/, std::size_t /*maxQueuedBytes*/, ConnectCallback callback)
{
    callback(nullptr, "TLS support was not enabled in this build.");
}

void PocoStreamConnection::startTlsAsync(varn::runtime::Runtime& /*runtime*/, std::string /*host*/, bool /*verify*/, SendCallback callback)
{
    callback(false, "TLS support was not enabled in this build.");
}

} // namespace varn::socket
