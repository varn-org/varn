#include "varn/http/HttpClientHandle.h"

#include "HttpClientTransfer.h"

#include <utility>

namespace varn::http::client
{

Handle::Handle(std::weak_ptr<HttpClientTransfer> transfer)
    : transfer(std::move(transfer))
{
}

// Ends the request from any thread, after which its completion arrives with the code of a cancellation and no other callback runs.
void Handle::cancel() const
{
    if (const auto running = transfer.lock())
    {
        running->cancel();
    }
}

// Hands back to the window of the request the bytes of chunks its caller consumed, for a caller whose callbacks set `releasesChunks` and keep each chunk until they consume it.
void Handle::release(std::size_t bytes) const
{
    if (const auto running = transfer.lock())
    {
        running->release(bytes);
    }
}

// Answers whether the request has not ended yet.
bool Handle::active() const
{
    const auto running = transfer.lock();
    return running && !running->settled();
}

} // namespace varn::http::client
