#include "HttpText.h"

#include <cctype>
#include <string>

namespace varn::http
{

std::string HttpText::toLower(std::string value)
{
    for (char& c : value)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return value;
}

bool HttpText::iequals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
    {
        return false;
    }

    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
        {
            return false;
        }
    }

    return true;
}

// Answers whether the text is well-formed UTF-8, refusing overlong forms, surrogates and code points past U+10FFFF the way a browser does.
bool HttpText::validUtf8(const std::string& text)
{
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
    const std::size_t size = text.size();
    std::size_t i = 0;
    while (i < size)
    {
        const unsigned char lead = bytes[i];
        if (lead < 0x80)
        {
            ++i;
            continue;
        }

        std::size_t length = 0;
        unsigned char low = 0x80;
        unsigned char high = 0xBF;
        if (lead >= 0xC2 && lead <= 0xDF)
        {
            length = 2;
        }
        else if (lead >= 0xE0 && lead <= 0xEF)
        {
            length = 3;
            low = lead == 0xE0 ? 0xA0 : 0x80;
            high = lead == 0xED ? 0x9F : 0xBF;
        }
        else if (lead >= 0xF0 && lead <= 0xF4)
        {
            length = 4;
            low = lead == 0xF0 ? 0x90 : 0x80;
            high = lead == 0xF4 ? 0x8F : 0xBF;
        }
        else
        {
            return false;
        }

        if (size - i < length || bytes[i + 1] < low || bytes[i + 1] > high)
        {
            return false;
        }

        for (std::size_t k = 2; k < length; ++k)
        {
            if (bytes[i + k] < 0x80 || bytes[i + k] > 0xBF)
            {
                return false;
            }
        }

        i += length;
    }

    return true;
}

} // namespace varn::http
