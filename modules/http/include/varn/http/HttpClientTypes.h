#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace varn::http::client
{

/// What a caller wants done about a response that points somewhere else.
enum class RedirectPolicy
{
    Follow,
    Manual,
    Error,
};

using Headers = std::vector<std::pair<std::string, std::string>>;

/// Fills at most `capacity` bytes of the request body and answers how many it wrote, zero once the body ends.
using BodySource = std::function<std::size_t(char* buffer, std::size_t capacity)>;

struct Request
{
    std::string method = "GET";
    std::string url;
    Headers headers;
    std::string body;
    BodySource bodySource;
    std::optional<std::uint64_t> bodyLength;
    std::chrono::milliseconds timeout = std::chrono::seconds(60);
    std::uint64_t maxResponseBytes = 64u * 1024u * 1024u;
    bool verifyTls = true;

    // Following is what every client does unless it is told otherwise, and twenty is the limit the fetch standard sets, which is the one most of them took.
    RedirectPolicy redirects = RedirectPolicy::Follow;
    int maxRedirects = 20;
};

struct ContentRange
{
    std::uint64_t first = 0;
    std::uint64_t last = 0;
    std::optional<std::uint64_t> completeLength;
};

struct ResponseHead
{
    int status = 0;
    Headers headers;
    std::string url;
    std::optional<std::uint64_t> contentLength;

    std::optional<std::string_view> header(std::string_view name) const;
    std::optional<ContentRange> contentRange() const;
};

struct Progress
{
    std::uint64_t received = 0;
    std::optional<std::uint64_t> total;
};

enum class ErrorCode
{
    Invalid,
    Unavailable,
    Network,
    Tls,
    Timeout,
    Cancelled,
    TooLarge,
    Redirect,
    Callback,
};

struct Error
{
    ErrorCode code = ErrorCode::Network;
    std::string message;
};

} // namespace varn::http::client
