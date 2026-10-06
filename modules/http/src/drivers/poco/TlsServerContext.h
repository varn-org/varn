#pragma once

#ifdef VARN_ENABLE_TLS

#include "varn/http/HttpTypes.h"

#include <Poco/Net/Context.h>

namespace varn::http
{

class TlsServerContext
{
public:
    static Poco::Net::Context::Ptr create(const HttpServerOptions& opts);
    static void initializeSslManager(Poco::Net::Context::Ptr context);

private:
    static void requireKeyMaterial(const HttpServerOptions& opts);
};

} // namespace varn::http

#endif
