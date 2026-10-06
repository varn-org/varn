#include "HttpClientConnections.h"

#include <Poco/Exception.h>
#include <Poco/Net/Socket.h>
#include <Poco/Timespan.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace varn::http::client
{

namespace
{
// A server closes an idle connection after a while of its own, so one idle longer than this is closed here rather than tried.
constexpr std::chrono::seconds kIdleLifetime{30};

// The bounds keep a client that reached many hosts from holding a socket open for each of them.
constexpr std::size_t kIdlePerOrigin = 6;
constexpr std::size_t kIdleTotal = 64;
} // namespace

HttpClientConnections::~HttpClientConnections()
{
    for (Idle& entry : idle)
    {
        discard(std::move(entry.session));
    }
}

// Answers the newest idle connection to an origin that is still open, or nothing when there is none.
std::unique_ptr<Poco::Net::HTTPClientSession> HttpClientConnections::take(const std::string& origin)
{
    std::vector<std::unique_ptr<Poco::Net::HTTPClientSession>> expired;
    std::unique_ptr<Poco::Net::HTTPClientSession> found;
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto now = std::chrono::steady_clock::now();

        for (auto it = idle.begin(); it != idle.end();)
        {
            if (now - it->since <= kIdleLifetime)
            {
                ++it;
                continue;
            }

            expired.push_back(std::move(it->session));
            it = idle.erase(it);
        }

        // clang-format off
        const auto match = std::find_if(idle.rbegin(), idle.rend(), [&](const Idle& entry) { return entry.origin == origin; });
        // clang-format on
        if (match != idle.rend())
        {
            found = std::move(match->session);
            idle.erase(std::next(match).base());
        }
    }

    for (auto& session : expired)
    {
        discard(std::move(session));
    }

    if (found && stale(*found))
    {
        discard(std::move(found));
    }

    return found;
}

// Keeps a connection whose last response ended cleanly, dropping the oldest of its origin or of the client once a bound is reached.
void HttpClientConnections::give(const std::string& origin, std::unique_ptr<Poco::Net::HTTPClientSession> session)
{
    if (!session->connected())
    {
        discard(std::move(session));
        return;
    }

    std::vector<std::unique_ptr<Poco::Net::HTTPClientSession>> evicted;
    {
        std::lock_guard<std::mutex> lock(mutex);

        // clang-format off
        const auto sameOrigin = [&](const Idle& entry) { return entry.origin == origin; };
        // clang-format on
        if (static_cast<std::size_t>(std::count_if(idle.begin(), idle.end(), sameOrigin)) >= kIdlePerOrigin)
        {
            const auto oldest = std::find_if(idle.begin(), idle.end(), sameOrigin);
            evicted.push_back(std::move(oldest->session));
            idle.erase(oldest);
        }

        if (idle.size() >= kIdleTotal)
        {
            evicted.push_back(std::move(idle.front().session));
            idle.pop_front();
        }

        idle.push_back(Idle{origin, std::move(session), std::chrono::steady_clock::now()});
    }

    for (auto& dropped : evicted)
    {
        discard(std::move(dropped));
    }
}

// Closes a connection without waiting for the peer to answer the close of TLS, since nothing more is read from it.
void HttpClientConnections::discard(std::unique_ptr<Poco::Net::HTTPClientSession> session)
{
    if (!session)
    {
        return;
    }

    try
    {
        if (session->connected())
        {
            session->socket().setBlocking(false);
        }
    }
    catch (const Poco::Exception&)
    {
    }

    session.reset();
}

// A connection the server closed while it idled reads as ready, with its end or with bytes nobody asked for, and either way cannot carry a request.
bool HttpClientConnections::stale(Poco::Net::HTTPClientSession& session)
{
    try
    {
        return !session.connected() || session.socket().poll(Poco::Timespan(0), Poco::Net::Socket::SELECT_READ | Poco::Net::Socket::SELECT_ERROR);
    }
    catch (const Poco::Exception&)
    {
        return true;
    }
}

} // namespace varn::http::client
