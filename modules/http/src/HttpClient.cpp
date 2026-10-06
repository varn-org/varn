#include "varn/http/HttpClient.h"

#include "HttpClientPerform.h"
#include "HttpClientTransfer.h"
#include "varn/runtime/Runtime.h"

#if !VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC
#include "HttpRedirects.h"
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>

namespace varn::http::client
{

namespace
{
// A deadline further away than this is no deadline, and refusing it keeps the time of the deadline within what the clock can count.
constexpr std::chrono::hours kLongestTimeout{24 * 365 * 100};

class RequestRules
{
public:
    RequestRules() = delete;

    // A header carries no control character, since one would let a value end its line and forge another header.
    static bool printable(std::string_view text)
    {
        // clang-format off
        return std::none_of(text.begin(), text.end(), [](unsigned char c) { return c < 0x20 || c == 0x7f; });
        // clang-format on
    }

    static bool token(std::string_view text)
    {
        // clang-format off
        return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isalnum(c) != 0 || std::string_view("!#$%&'*+-.^_`|~").find(static_cast<char>(c)) != std::string_view::npos; });
        // clang-format on
    }

    static bool webScheme(std::string_view url)
    {
        const std::size_t colon = url.find("://");
        if (colon == std::string_view::npos)
        {
            return false;
        }

        std::string scheme(url.substr(0, colon));
        // clang-format off
        std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        // clang-format on
        return scheme == "http" || scheme == "https";
    }
};
} // namespace

HttpClient::HttpClient(varn::runtime::Runtime& runtime)
    : runtime(runtime)
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
    auto transfer = std::make_shared<HttpClientTransfer>(runtime, std::move(request), std::move(callbacks));
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
    runtime.ioPool().post([transfer, shared = connections]
    {
        HttpRedirects::run(*shared, *transfer);
    });
    // clang-format on
#endif

    return Handle(transfer);
}

// Answers why a request cannot be sent, the same way on every target, before any transport sees it.
std::optional<Error> HttpClient::refusal(const Request& request)
{
    if (!RequestRules::token(request.method))
    {
        return Error{ErrorCode::Invalid, "[HttpClient] The method \"" + request.method + "\" is not a valid HTTP method."};
    }

    if (!RequestRules::webScheme(request.url) || !RequestRules::printable(request.url))
    {
        return Error{ErrorCode::Invalid, "[HttpClient] The URL \"" + request.url + "\" must be an \"http\" or \"https\" address."};
    }

    for (const auto& [name, value] : request.headers)
    {
        if (!RequestRules::token(name) || !RequestRules::printable(value))
        {
            return Error{ErrorCode::Invalid, "[HttpClient] A request header name or value contains invalid characters."};
        }
    }

    if (request.bodySource && !request.body.empty())
    {
        return Error{ErrorCode::Invalid, "[HttpClient] A request takes either \"body\" or \"bodySource\", not both."};
    }

    if (request.bodyLength && !request.bodySource)
    {
        return Error{ErrorCode::Invalid, "[HttpClient] The field \"bodyLength\" describes a \"bodySource\", which the request does not have."};
    }

    if (request.timeout.count() <= 0 || request.timeout > kLongestTimeout)
    {
        return Error{ErrorCode::Invalid, "[HttpClient] The field \"timeout\" must be a positive duration of at most a hundred years."};
    }

    if (request.maxRedirects < 0)
    {
        return Error{ErrorCode::Invalid, "[HttpClient] The field \"maxRedirects\" cannot be negative."};
    }

#if VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC
    // A browser follows a redirect itself and hands over only where it ended up, so a caller asking to see one is told it cannot be, rather than being handed the final answer as if it were the first.
    if (request.redirects != RedirectPolicy::Follow)
    {
        return Error{ErrorCode::Invalid, "[HttpClient] A browser follows redirects itself, so the redirect policy must be to follow them here."};
    }
#endif

    return std::nullopt;
}

} // namespace varn::http::client
