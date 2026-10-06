#include "PocoSocketBlocked.h"

#if defined(__ANDROID__)
#include <android/multinetwork.h>
#include <dlfcn.h>

#include <cstdint>
#endif

namespace varn::socket
{

// Android 17 records why its Local Network Protection dropped the packets of a socket of an app that may not reach the local network. The function exists only there, so it is looked up while the program runs, and every other platform answers that it blocked nothing.
std::string PocoSocketBlocked::reason([[maybe_unused]] const Poco::Net::Socket& socket, [[maybe_unused]] const std::string& target)
{
#if defined(__ANDROID__)
    using ReadBlockedReason = std::int32_t (*)(int socket);
    static const auto readBlockedReason = reinterpret_cast<ReadBlockedReason>(dlsym(RTLD_DEFAULT, "android_getnetworkblockedreason"));
    if (readBlockedReason == nullptr || readBlockedReason(static_cast<int>(socket.impl()->sockfd())) != ANDROID_NETWORK_BLOCKED_REASON_LNP)
    {
        return {};
    }

    return "Android blocked the connection to " + target + ", because the app may not reach the local network without the permission \"android.permission.ACCESS_LOCAL_NETWORK\", which the person grants while the app runs.";
#else
    return {};
#endif
}

} // namespace varn::socket
