#include "ZipPath.h"

namespace varn::zip
{

// Answers whether the entry, joined to any folder, stays inside it: it is relative on every platform and no component of it is "..", whichever separator it uses.
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

// Shortens an entry name for a message and masks its control characters, since the name comes from the archive.
std::string ZipPath::printable(std::string_view entry)
{
    constexpr std::size_t kMaxLength = 200;
    std::string text(entry.substr(0, kMaxLength));
    if (entry.size() > kMaxLength)
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

// Builds a path from UTF-8 code units, so Windows converts it to its wide encoding instead of reading it in the active code page.
std::filesystem::path ZipPath::toPath(std::string_view utf8)
{
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string ZipPath::fromPath(const std::filesystem::path& path)
{
    const std::u8string encoded = path.u8string();
    return std::string(encoded.begin(), encoded.end());
}

} // namespace varn::zip
