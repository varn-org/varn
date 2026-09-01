#include "ZipPath.h"

namespace varn::zip
{

namespace fs = std::filesystem;

bool ZipPath::entryPathSafe(std::string_view entry)
{
    if (entry.empty())
    {
        return false;
    }

    // rejects posix/unc absolute paths and windows drive-qualified names
    if (entry.front() == '/' || entry.front() == '\\')
    {
        return false;
    }

    if (entry.size() >= 2 && entry[1] == ':')
    {
        return false;
    }

    // rejects ".." only as a whole path component
    std::size_t start = 0;
    for (std::size_t i = 0; i <= entry.size(); ++i)
    {
        if (i == entry.size() || entry[i] == '/' || entry[i] == '\\')
        {
            if (entry.substr(start, i - start) == "..")
            {
                return false;
            }

            start = i + 1;
        }
    }

    return true;
}

bool ZipPath::isSubpath(const fs::path& baseCanon, const fs::path& candidateCanon)
{
    std::error_code ec;
    const fs::path rel = fs::relative(candidateCanon, baseCanon, ec);

    // treats an unrelatable path (different root or mount) as unsafe
    if (ec || rel.empty())
    {
        return false;
    }

    for (const auto& part : rel)
    {
        if (part == "..")
        {
            return false;
        }
    }

    return true;
}

} // namespace varn::zip
