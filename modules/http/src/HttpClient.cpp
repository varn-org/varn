#include "varn/http/HttpClient.h"

#include "HttpClientPerform.h"
#include "HttpClientRules.h"
#include "HttpClientTransfer.h"
#include "varn/runtime/Scheduler.h"

#if !VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC
#include "HttpRedirects.h"
#endif

#include <algorithm>
#include <chrono>
#include <utility>

namespace varn::http::client
{

namespace
{
// A deadline further away than this is no deadline, and refusing it keeps the time of the deadline within what the clock can count.
constexpr std::chrono::hours kLongestTimeout{24 * 365 * 100};
} // namespace

// Sends HTTP requests on every target and streams their responses to callbacks that run on the loop thread of its scheduler.
HttpClient::HttpClient(varn::runtime::Scheduler& scheduler)
    : scheduler(scheduler)
    , connections(HttpClientPerform::connections())
{
}

// Ends every request the client still runs without calling their callbacks, since a caller that destroys the client destroys what they reach too.
HttpClient::~HttpClient()
{
    std::vector<std::weak_ptr<HttpClientTransfer>> running;
    {
        std::lock_guard<std::mutex> lock(transfersMutex);
        running.swap(transfers);
    }

    for (const auto& weak : running)
    {
        if (const auto transfer = weak.lock())
        {
            transfer->abandon();
        }
    }
}

// Starts a request and answers its handle at once, and every callback, a refusal included, arrives later on the loop thread.
Handle HttpClient::send(Request request, Callbacks callbacks)
{
    const std::optional<Error> refused = refusal(request);
    auto transfer = std::make_shared<HttpClientTransfer>(scheduler, std::move(request), std::move(callbacks));
    {
        std::lock_guard<std::mutex> lock(transfersMutex);
        // clang-format off
        transfers.erase(std::remove_if(transfers.begin(), transfers.end(), [](const auto& weak) { return weak.expired(); }), transfers.end());
        // clang-format on
        transfers.push_back(transfer);
    }

    if (refused)
    {
        transfer->fail(*refused);
        return Handle(transfer);
    }

    transfer->begin();

#if VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC
    HttpClientPerform::start(*connections, transfer);
#else
    // clang-format off
    scheduler.ioPool().post([transfer, shared = connections]
    {
        HttpRedirects::run(*shared, *transfer);
    });
    // clang-format on
#endif

    return Handle(transfer);
}

// Answers why a request cannot be sent, the same way on every target, before any transport sees it, so no driver ever receives text its platform cannot carry.
std::optional<Error> HttpClient::refusal(const Request& request)
{
    if (!HttpClientRules::token(request.method))
    {
        return Error{ErrorCode::Invalid, "The method \"" + request.method + "\" is not a valid HTTP method."};
    }

    if (std::optional<Error> refused = HttpClientRules::urlRefusal(request.url))
    {
        return refused;
    }

    // A header carries no control character, since one would let a value end its line and forge another header, and only text every transport can carry.
    for (const auto& [name, value] : request.headers)
    {
        if (!HttpClientRules::headerAllowed(name, value))
        {
            return Error{ErrorCode::Invalid, "A request header name or value contains invalid characters."};
        }
    }

    if (request.bodySource && !request.body.empty())
    {
        return Error{ErrorCode::Invalid, "A request takes either \"body\" or \"bodySource\", not both."};
    }

    if (request.bodyLength && !request.bodySource)
    {
        return Error{ErrorCode::Invalid, "The field \"bodyLength\" describes a \"bodySource\", which the request does not have."};
    }

    if (request.timeout.count() <= 0 || request.timeout > kLongestTimeout)
    {
        return Error{ErrorCode::Invalid, "The field \"timeout\" must be a positive duration of at most a hundred years."};
    }

    if (request.maxRedirects < 0)
    {
        return Error{ErrorCode::Invalid, "The field \"maxRedirects\" cannot be negative."};
    }

#if VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC
    // A browser follows a redirect itself and hands over only where it ended up, so a caller asking to see one is told it cannot be, rather than being handed the final answer as if it were the first.
    if (request.redirects != RedirectPolicy::Follow)
    {
        return Error{ErrorCode::Invalid, "A browser follows redirects itself, so the redirect policy must be to follow them here."};
    }
#endif

    return std::nullopt;
}

} // namespace varn::http::client
