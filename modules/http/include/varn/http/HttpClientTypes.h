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

enum class RedirectPolicy
{
    Follow,
    Manual,
    Error,
};

using Headers = std::vector<std::pair<std::string, std::string>>;

using BodySource = std::function<std::size_t(char* buffer, std::size_t capacity)>;

struct ResponseHead;

struct BodySink
{
    std::function<void(const ResponseHead& head)> begin;
    std::function<void(std::string_view chunk)> write;
};

struct Request
{
    std::string method = "GET";
    std::string url;
    Headers headers;
    std::string body;
    BodySource bodySource;
    std::optional<std::uint64_t> bodyLength;
    BodySink bodySink;
    std::chrono::milliseconds timeout = std::chrono::seconds(60);
    std::uint64_t maxResponseBytes = 64u * 1024u * 1024u;
    bool verifyTls = true;
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
