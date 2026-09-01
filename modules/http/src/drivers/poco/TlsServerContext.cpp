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

#if defined(_WIN32)
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>

#include <memory>
#include <vector>
#endif

namespace varn::http
{

#if defined(_WIN32)
namespace
{
class TlsServerHelpers
{
public:
    // bundle the pem certificate and private key into an in-memory pkcs12 with an empty passphrase, the only shape schannel imports
    static std::vector<char> buildPkcs12FromPem(const std::string& keyFile, const std::string& certFile)
    {
        std::unique_ptr<BIO, decltype(&BIO_free)> certBio(BIO_new_file(certFile.c_str(), "rb"), BIO_free);
        if (!certBio)
        {
            throw std::runtime_error("[TlsServerContext] The TLS certificate file could not be opened.");
        }

        std::unique_ptr<X509, decltype(&X509_free)> certificate(PEM_read_bio_X509(certBio.get(), nullptr, nullptr, nullptr), X509_free);
        if (!certificate)
        {
            throw std::runtime_error("[TlsServerContext] The TLS certificate is not valid PEM.");
        }

        std::unique_ptr<BIO, decltype(&BIO_free)> keyBio(BIO_new_file(keyFile.c_str(), "rb"), BIO_free);
        if (!keyBio)
        {
            throw std::runtime_error("[TlsServerContext] The TLS private key file could not be opened.");
        }

        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> privateKey(PEM_read_bio_PrivateKey(keyBio.get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
        if (!privateKey)
        {
            throw std::runtime_error("[TlsServerContext] The TLS private key is not valid PEM.");
        }

        // leave the certificate bag unencrypted since it is public, which also avoids the legacy cipher openssl 3 no longer enables by default
        std::unique_ptr<PKCS12, decltype(&PKCS12_free)> bundle(
            PKCS12_create("", "varn", privateKey.get(), certificate.get(), nullptr, 0, -1, 0, 0, 0), PKCS12_free);
        if (!bundle)
        {
            throw std::runtime_error("[TlsServerContext] The TLS key material could not be bundled for the platform.");
        }

        std::unique_ptr<BIO, decltype(&BIO_free)> sink(BIO_new(BIO_s_mem()), BIO_free);
        if (!sink || i2d_PKCS12_bio(sink.get(), bundle.get()) != 1)
        {
            throw std::runtime_error("[TlsServerContext] The TLS key material could not be serialized.");
        }

        char* data = nullptr;
        const long length = BIO_get_mem_data(sink.get(), &data);
        return std::vector<char>(data, data + length);
    }
};

// import the key material straight from memory so the private key never touches disk, unlike the file path schannel documents
class MemoryPkcs12Context : public Poco::Net::Context
{
public:
    explicit MemoryPkcs12Context(const std::vector<char>& bundle)
        : Poco::Net::Context(Poco::Net::Context::TLS_SERVER_USE, "", Poco::Net::Context::VERIFY_NONE,
                             Poco::Net::Context::OPT_DEFAULTS, Poco::Net::Context::CERT_STORE_MY)
    {
        importCertificate(bundle.data(), bundle.size());
    }
};
} // namespace
#endif

Poco::Net::Context::Ptr TlsServerContext::create(const HttpServerOptions& opts)
{
    requireKeyMaterial(opts);

    static Poco::Crypto::OpenSSLInitializer openSslInitializer;

#if defined(_WIN32)
    // schannel imports one pkcs12 blob, so bundle the same pem key and certificate in memory to keep the lua config identical across desktop os
    const std::vector<char> bundle = TlsServerHelpers::buildPkcs12FromPem(opts.keyFile, opts.certFile);
    Poco::Net::Context::Ptr context = new MemoryPkcs12Context(bundle);
#else
    // a modern suite of forward-secret aead ciphers, leaving tls 1.3 to negotiate its own
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
#endif

    // refuse the legacy protocols that modern deployments must not negotiate
    context->requireMinimumProtocol(Poco::Net::Context::PROTO_TLSV1_2);
    return context;
}

void TlsServerContext::initializeSslManager(Poco::Net::Context::Ptr context)
{
    // the ssl manager is a process-global singleton whose default handlers are initialized once so a second server start does not clobber the first server's configuration, while each server still binds its own context to its socket so the global default only supplies the passphrase and invalid-cert handlers
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
        throw std::runtime_error("[TlsServerContext] TLS is enabled but the key or certificate path is empty.");
    }

    for (const auto& path : {opts.keyFile, opts.certFile})
    {
        if (!std::filesystem::exists(path))
        {
            throw std::runtime_error("[TlsServerContext] A TLS key or certificate file was not found.");
        }
    }
}

} // namespace varn::http

#endif
