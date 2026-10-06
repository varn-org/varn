#include "ZipPath.h"

namespace varn::zip
{

bool ZipPath::entryPathSafe(std::string_view entry)
{
    if (entry.empty())
    {
        return false;
    }

    // Rejects POSIX/UNC absolute paths and Windows drive-qualified names.
    if (entry.front() == '/' || entry.front() == '\\')
    {
        return false;
    }

    if (entry.size() >= 2 && entry[1] == ':')
    {
        return false;
    }

    // Rejects `..` only as a whole path component.
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

// Answers whether the entry, joined to any folder, stays inside that folder once its dots are resolved.
bool ZipPath::staysInside(std::string_view entry)
{
    // Both separators count on every platform, so an archive is judged the same wherever it is read.
    if (!entry.empty() && (entry.front() == '/' || entry.front() == '\\'))
    {
        return false;
    }

    if (entry.size() >= 2 && entry[1] == ':')
    {
        return false;
    }

    long depth = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= entry.size(); ++i)
    {
        if (i != entry.size() && entry[i] != '/' && entry[i] != '\\')
        {
            continue;
        }

        const std::string_view part = entry.substr(start, i - start);
        start = i + 1;
        if (part.empty() || part == ".")
        {
            continue;
        }

        depth += part == ".." ? -1 : 1;
        if (depth < 0)
        {
            return false;
        }
    }

    return true;
}

// Shortens an entry name for a message and masks its control characters, since the name comes from the archive.
std::string ZipPath::printable(std::string_view entry)
{
    constexpr std::size_t maxLength = 200;
    std::string text(entry.substr(0, maxLength));
    if (entry.size() > maxLength)
    {
        text += "...";
    }

    for (char& c : text)
    {
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F)
        {
            c = '?';
        }
    }

    return text;
}

} // namespace varn::zip
