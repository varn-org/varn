#pragma once

#include <filesystem>
#include <string_view>

namespace varn::zip
{

class ZipPath
{
public:
    static bool entryPathSafe(std::string_view entry);
    static bool isSubpath(const std::filesystem::path& baseCanon, const std::filesystem::path& candidateCanon);
};

} // namespace varn::zip
