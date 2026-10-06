#pragma once

#include "../../HttpClientPerform.h"

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>

#include <chrono>
#include <istream>
#include <memory>
#include <ostream>
#include <string>

#if defined(VARN_ENABLE_TLS)
#include <Poco/Net/Context.h>
#endif

namespace varn::http::client
{

class PocoClientExchange
{
public:
    PocoClientExchange() = delete;

    static void perform(HttpClientConnections& connections, const Poco::URI& uri, const Hop& hop, HttpClientTransfer& transfer, const HeadFn& onHead);

private:
    class Deadline;
    class Interruption;

    static std::unique_ptr<Poco::Net::HTTPClientSession> open(const Poco::URI& uri, bool verifyTls);
    static bool exchange(Poco::Net::HTTPClientSession& session, const Poco::URI& uri, const Hop& hop, HttpClientTransfer& transfer, const HeadFn& onHead, bool& answered);
    static void sendSource(Poco::Net::HTTPClientSession& session, std::ostream& os, const Deadline& deadline, HttpClientTransfer& transfer);
    static bool readBody(Poco::Net::HTTPClientSession& session, std::istream& rs, const Deadline& deadline, HttpClientTransfer& transfer);
    static bool drain(Poco::Net::HTTPClientSession& session, std::istream& rs, const Deadline& deadline);
    static bool replayable(const Hop& hop, const HttpClientTransfer& transfer);
    static Headers collectHeaders(const Poco::Net::HTTPResponse& response);
#if defined(VARN_ENABLE_TLS)
    static Poco::Net::Context::Ptr tlsClientContext(bool verify);
    static void ensureTlsClientInitialized();
#endif
};

} // namespace varn::http::client
