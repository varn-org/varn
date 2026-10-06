#pragma once

#include <Poco/Net/HTTPClientSession.h>

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace varn::http::client
{

/// The idle connections of one client, kept per origin so a request to a host it already reached skips the TCP and TLS handshakes.
class HttpClientConnections
{
public:
    HttpClientConnections() = default;
    ~HttpClientConnections();

    HttpClientConnections(const HttpClientConnections&) = delete;
    HttpClientConnections& operator=(const HttpClientConnections&) = delete;

    std::unique_ptr<Poco::Net::HTTPClientSession> take(const std::string& origin);
    void give(const std::string& origin, std::unique_ptr<Poco::Net::HTTPClientSession> session);

    static void discard(std::unique_ptr<Poco::Net::HTTPClientSession> session);

private:
    struct Idle
    {
        std::string origin;
        std::unique_ptr<Poco::Net::HTTPClientSession> session;
        std::chrono::steady_clock::time_point since;
    };

    static bool stale(Poco::Net::HTTPClientSession& session);

    std::mutex mutex;
    std::deque<Idle> idle;
};

} // namespace varn::http::client
