#include "varn/platform/PlatformInfo.h"

#include <bit>
#include <cctype>
#include <stdexcept>
#include <string>
#include <thread>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace varn::platform
{

bool PlatformInfo::startsWith(std::string_view s, std::string_view p)
{
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

std::string PlatformInfo::toLower(std::string s)
{
    for (char& c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return s;
}

std::string PlatformInfo::osId()
{
#if defined(__EMSCRIPTEN__)
    return "wasm";
#elif defined(__ANDROID__)
    return "android";
#elif defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
#if TARGET_OS_IOS
#if TARGET_OS_SIMULATOR
    return "ios-simulator";
#else
    return "ios";
#endif
#elif TARGET_OS_TV
#if TARGET_OS_SIMULATOR
    return "tvos-simulator";
#else
    return "tvos";
#endif
#elif TARGET_OS_WATCH
#if TARGET_OS_SIMULATOR
    return "watchos-simulator";
#else
    return "watchos";
#endif
#elif defined(TARGET_OS_VISION) && TARGET_OS_VISION
#if TARGET_OS_SIMULATOR
    return "visionos-simulator";
#else
    return "visionos";
#endif
#elif TARGET_OS_OSX
    return "macos";
#else
    return "apple-unknown";
#endif
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

std::string PlatformInfo::archId()
{
#if defined(__EMSCRIPTEN__)
#if defined(__wasm64__)
    return "wasm64";
#else
    return "wasm32";
#endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#elif defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
    return "x86_64";
#elif defined(i386) || defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

unsigned PlatformInfo::cpuCount()
{
    const unsigned count = std::thread::hardware_concurrency();
    return count == 0 ? 1u : count;
}

std::size_t PlatformInfo::pointerSize()
{
    return sizeof(void*);
}

std::string PlatformInfo::endianness()
{
    if constexpr (std::endian::native == std::endian::little)
    {
        return "little";
    }

    if constexpr (std::endian::native == std::endian::big)
    {
        return "big";
    }

    return "mixed";
}

std::string PlatformInfo::libPrefix()
{
#if defined(_WIN32)
    return "";
#else
    return "lib";
#endif
}

std::string PlatformInfo::shlibSuffix()
{
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

std::string PlatformInfo::libraryFilenameForName(std::string_view logicalName)
{
    if (logicalName.empty())
    {
        throw std::invalid_argument("[PlatformInfo] The library name must not be empty.");
    }

    std::string s(logicalName);
    const std::string os = osId();

    // on windows keep an explicit extension and otherwise force a lowercase .dll
    if (os == "windows")
    {
        if (s.find('.') != std::string::npos)
        {
            return s;
        }

        return toLower(std::move(s)) + ".dll";
    }

    // reduce the name to its bare stem before applying the platform prefix and suffix
    std::string stem = s;
    if (startsWith(stem, "lib"))
    {
        stem = stem.substr(3);
    }

    const auto dot = stem.rfind('.');
    if (dot != std::string::npos)
    {
        const std::string ext = toLower(stem.substr(dot));
        if (ext == ".so" || ext == ".dylib" || ext == ".dll")
        {
            stem = stem.substr(0, dot);
        }
    }

    return libPrefix() + stem + shlibSuffix();
}

} // namespace varn::platform
