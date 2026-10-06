#include "TlsServerContext.h"

#ifdef VARN_ENABLE_TLS

#include <Poco/Crypto/OpenSSLInitializer.h>
#include <Poco/Net/AcceptCertificateHandler.h>
#include <Poco/Net/KeyFileHandler.h>
#include <Poco/Net/PrivateKeyPassphraseHandler.h>
#include <Poco/Net/SSLManager.h>

#include <filesystem>
#include <mutex>
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

void TlsServerContext::initializeSslManager(Poco::Net::Context::Ptr context)
{
    // The SSL manager is a process-global singleton whose default handlers are initialized once so a second server start does not clobber the first server's configuration, while each server still binds its own context to its socket so the global default only supplies the passphrase and invalid-cert handlers.
    static std::once_flag onceFlag;
    // clang-format off
    std::call_once(onceFlag, [&context]
    {
        auto privateKeyHandler = Poco::SharedPtr<Poco::Net::PrivateKeyPassphraseHandler>(new Poco::Net::KeyFileHandler(false));
        auto invalidCertHandler = Poco::SharedPtr<Poco::Net::InvalidCertificateHandler>(new Poco::Net::AcceptCertificateHandler(false));

        Poco::Net::SSLManager::instance().initializeServer(privateKeyHandler, invalidCertHandler, context);
    });
    // clang-format on
}

void TlsServerContext::requireKeyMaterial(const HttpServerOptions& opts)
{
    if (opts.keyFile.empty() || opts.certFile.empty())
    {
        throw std::runtime_error("TLS is enabled but the key or certificate path is empty.");
    }

    for (const auto& path : {opts.keyFile, opts.certFile})
    {
        if (!std::filesystem::exists(path))
        {
            throw std::runtime_error("A TLS key or certificate file was not found.");
        }
    }
}

} // namespace varn::http

#endif
