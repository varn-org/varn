#pragma once

#include <string>
#include <string_view>

namespace varn::zip
{

class ZipPath
{
public:
    static bool entryPathSafe(std::string_view entry);
    static bool staysInside(std::string_view entry);
    static std::string printable(std::string_view entry);
};

} // namespace varn::zip
