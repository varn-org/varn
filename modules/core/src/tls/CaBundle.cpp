#include "varn/tls/CaBundle.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <climits>
#endif

namespace varn::tls
{

namespace
{

class CaBundleSearch
{
public:
    // an explicit setting always wins, since a host that ships its own roots knows better than any guess
    static constexpr const char* kOverride = "SSL_CERT_FILE";

    // openssl reads a bundle file or a directory of hashed certificates, so both forms belong here
    static constexpr const char* kCandidates[] = {
        "/etc/ssl/cert.pem",
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/ssl/ca-bundle.pem",
        "/opt/homebrew/etc/openssl@3/cert.pem",
        "/usr/local/etc/openssl@3/cert.pem",
        // android keeps its roots as a directory rather than one file, and moved them under apex in newer releases
        "/apex/com.android.conscrypt/cacerts",
        "/system/etc/security/cacerts",
    };

#if defined(__APPLE__)
    // an apple app has no readable trust store on disk, so a bundled cacert.pem is the conventional place to put one
    static std::string fromMainBundle()
    {
        CFBundleRef bundle = CFBundleGetMainBundle();
        if (bundle == nullptr)
        {
            return std::string();
        }

        CFURLRef url = CFBundleCopyResourceURL(bundle, CFSTR("cacert"), CFSTR("pem"), nullptr);
        if (url == nullptr)
        {
            return std::string();
        }

        char path[PATH_MAX] = {};
        const bool ok = CFURLGetFileSystemRepresentation(url, true, reinterpret_cast<UInt8*>(path), sizeof(path));
        CFRelease(url);

        return ok ? std::string(path) : std::string();
    }
#endif

    static std::string find()
    {
        if (const char* env = std::getenv(kOverride); env != nullptr && env[0] != '\0')
        {
            return env;
        }

#if defined(__APPLE__)
        if (std::string bundled = fromMainBundle(); !bundled.empty())
        {
            return bundled;
        }
#endif

        for (const char* candidate : kCandidates)
        {
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec))
            {
                return candidate;
            }
        }

        return std::string();
    }
};

} // namespace

const std::string& CaBundle::resolve()
{
    static const std::string bundle = CaBundleSearch::find();
    return bundle;
}

std::string CaBundle::describeSearch()
{
    std::string message = "set ";
    message += CaBundleSearch::kOverride;
    message += " to a CA bundle file or a directory of hashed certificates";

#if defined(__APPLE__)
    message += ", or ship cacert.pem as a resource of the application bundle";
#endif

    return message;
}

} // namespace varn::tls
