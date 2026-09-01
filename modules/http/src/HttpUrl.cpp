#include "HttpUrl.h"

#include <cstddef>

namespace varn::http
{

namespace
{
class HttpUrlHelpers
{
public:
    static int hexValue(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f')
        {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F')
        {
            return c - 'A' + 10;
        }

        return -1;
    }

    static bool unreserved(unsigned char c)
    {
        if (c >= 'A' && c <= 'Z')
        {
            return true;
        }
        if (c >= 'a' && c <= 'z')
        {
            return true;
        }
        if (c >= '0' && c <= '9')
        {
            return true;
        }

        return c == '-' || c == '_' || c == '.' || c == '~';
    }
};
} // namespace

std::string HttpUrl::encode(std::string_view input)
{
    static const char hex[] = "0123456789ABCDEF";

    std::string out;
    out.reserve(input.size());

    for (const char raw : input)
    {
        const unsigned char c = static_cast<unsigned char>(raw);

        if (HttpUrlHelpers::unreserved(c))
        {
            out += static_cast<char>(c);
            continue;
        }

        // anything outside the rfc 3986 unreserved set becomes a percent-escape so it survives in a url
        out += '%';
        out += hex[c >> 4];
        out += hex[c & 0x0F];
    }

    return out;
}

std::string HttpUrl::decode(std::string_view input)
{
    std::string out;
    out.reserve(input.size());

    for (std::size_t i = 0; i < input.size(); ++i)
    {
        const char c = input[i];

        if (c == '+')
        {
            out += ' ';
            continue;
        }

        if (c == '%' && i + 2 < input.size())
        {
            const int hi = HttpUrlHelpers::hexValue(input[i + 1]);
            const int lo = HttpUrlHelpers::hexValue(input[i + 2]);

            if (hi >= 0 && lo >= 0)
            {
                out += static_cast<char>(hi * 16 + lo);
                i += 2;
                continue;
            }
        }

        out += c;
    }

    return out;
}

} // namespace varn::http
