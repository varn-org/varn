#include "TlsServerContext.h"

#ifdef VARN_ENABLE_TLS

#include <Poco/Crypto/OpenSSLInitializer.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace varn::http
{

Poco::Net::Context::Ptr TlsServerContext::create(const HttpServerOptions& opts)
{
    requireKeyMaterial(opts);

    static Poco::Crypto::OpenSSLInitializer openSslInitializer;

    // Use a modern suite of forward-secret AEAD ciphers, leaving TLS 1.3 to negotiate its own.
    constexpr const char* kCipherList =
        "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:"
        "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:"
        "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:"
        "DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384";

    Poco::Net::Context::Ptr context = new Poco::Net::Context(
        Poco::Net::Context::TLS_SERVER_USE,
        opts.keyFile,
        opts.certFile,
        "",
        Poco::Net::Context::VERIFY_NONE,
        9,
        true,
        kCipherList);

    // Refuse the legacy protocols that modern deployments must not negotiate.
    context->requireMinimumProtocol(Poco::Net::Context::PROTO_TLSV1_2);
    return context;
}

void TlsServerContext::requireKeyMaterial(const HttpServerOptions& opts)
{
    if (opts.keyFile.empty() || opts.certFile.empty())
    {
        throw std::runtime_error("The option \"tls\" needs both the options \"certFile\" and \"keyFile\".");
    }

    for (const auto& path : {opts.keyFile, opts.certFile})
    {
        if (!std::filesystem::exists(path))
        {
            throw std::runtime_error("The TLS file \"" + path + "\" was not found.");
        }
    }
}

} // namespace varn::http

#endif
