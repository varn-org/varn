#include "varn/tls/SystemTrust.h"

#include "varn/tls/CaBundle.h"

// OpenSSL undefines the names of `wincrypt.h` that collide with its own only when it is included after it.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <wincrypt.h>
#endif

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace varn::tls
{

// The bundled OpenSSL ships no trust store, so a context verifies against the roots the system trusts, the bundle on POSIX or the root store of Windows.
#if defined(_WIN32)
void SystemTrust::install(ssl_ctx_st* context)
{
    HCERTSTORE system = CertOpenSystemStoreW(0, L"ROOT");
    if (system == nullptr)
    {
        throw std::runtime_error("The trusted root certificates of Windows could not be opened: " + std::system_category().message(static_cast<int>(GetLastError())) + ".");
    }

    X509_STORE* store = SSL_CTX_get_cert_store(context);
    int added = 0;
    for (PCCERT_CONTEXT certificate = CertEnumCertificatesInStore(system, nullptr); certificate != nullptr; certificate = CertEnumCertificatesInStore(system, certificate))
    {
        const unsigned char* der = certificate->pbCertEncoded;
        X509* parsed = d2i_X509(nullptr, &der, static_cast<long>(certificate->cbCertEncoded));
        if (parsed == nullptr)
        {
            continue;
        }

        if (X509_STORE_add_cert(store, parsed) == 1)
        {
            ++added;
        }

        X509_free(parsed);
    }

    CertCloseStore(system, 0);
    ERR_clear_error();

    if (added == 0)
    {
        throw std::runtime_error("No trusted root certificate was found in the system store of Windows.");
    }
}
#else
void SystemTrust::install(ssl_ctx_st* context)
{
    const std::string& bundle = CaBundle::resolve();
    if (bundle.empty())
    {
        throw std::runtime_error("No CA trust store was found for certificate verification: " + CaBundle::describeSearch() + ".");
    }

    std::error_code ec;
    const bool directory = std::filesystem::is_directory(bundle, ec);
    if (SSL_CTX_load_verify_locations(context, directory ? nullptr : bundle.c_str(), directory ? bundle.c_str() : nullptr) == 1)
    {
        return;
    }

    char reason[256] = {};
    ERR_error_string_n(ERR_get_error(), reason, sizeof(reason));
    ERR_clear_error();
    throw std::runtime_error("The CA trust store \"" + bundle + "\" could not be loaded: " + reason + ".");
}
#endif

} // namespace varn::tls
