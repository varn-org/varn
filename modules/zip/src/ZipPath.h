#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace varn::zip
{

class ZipPath
{
public:
    ZipPath() = delete;

    static bool entryPathSafe(std::string_view entry);
    static std::string printable(std::string_view entry);
    static std::filesystem::path toPath(std::string_view utf8);
    static std::string fromPath(const std::filesystem::path& path);
};

} // namespace varn::zip
