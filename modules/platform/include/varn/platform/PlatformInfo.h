#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace varn::platform
{

class PlatformInfo final
{
public:
    PlatformInfo() = delete;

    static std::string osId();
    static std::string archId();

    static unsigned cpuCount();
    static std::size_t pointerSize();
    static std::string endianness();

    static std::string libPrefix();
    static std::string shlibSuffix();

    static std::string libraryFilenameForName(std::string_view logicalName);

private:
    static bool startsWith(std::string_view s, std::string_view p);
    static std::string toLower(std::string s);
};

} // namespace varn::platform
