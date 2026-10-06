#pragma once

#include "HttpText.h"
#include "varn/http/HttpClientTypes.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace varn::http::client
{

class HttpClientRules
{
public:
    HttpClientRules() = delete;

    static std::optional<Error> urlRefusal(const std::string& url)
    {
        if (!HttpText::validUtf8(url))
        {
            return Error{ErrorCode::Invalid, "The URL of the request is not valid UTF-8."};
        }

        if (!webScheme(url) || !printable(url))
        {
            return Error{ErrorCode::Invalid, "The URL \"" + url + "\" must be an \"http\" or \"https\" address."};
        }

        return std::nullopt;
    }

    static bool headerAllowed(const std::string& name, const std::string& value)
    {
        return token(name) && printable(value) && HttpText::validUtf8(value);
    }

    static bool token(std::string_view text)
    {
        // clang-format off
        return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isalnum(c) != 0 || std::string_view("!#$%&'*+-.^_`|~").find(static_cast<char>(c)) != std::string_view::npos; });
        // clang-format on
    }

private:
    static bool printable(std::string_view text)
    {
        // clang-format off
        return std::none_of(text.begin(), text.end(), [](unsigned char c) { return c < 0x20 || c == 0x7f; });
        // clang-format on
    }

    static bool webScheme(std::string_view url)
    {
        const std::size_t colon = url.find("://");
        if (colon == std::string_view::npos)
        {
            return false;
        }

        std::string scheme(url.substr(0, colon));
        // clang-format off
        std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        // clang-format on
        return scheme == "http" || scheme == "https";
    }
};

} // namespace varn::http::client
