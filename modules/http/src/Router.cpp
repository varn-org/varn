#include "varn/http/Router.h"

#include <algorithm>
#include <cctype>

namespace varn::http
{

std::string Router::urlEncodeSegment(const std::string& value)
{
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value)
    {
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                                c == '-' || c == '_' || c == '.' || c == '~';
        if (unreserved)
        {
            out += static_cast<char>(c);
            continue;
        }

        out += '%';
        out += hex[c >> 4];
        out += hex[c & 0x0F];
    }

    return out;
}

std::vector<std::string> Router::splitPath(const std::string& path)
{
    std::vector<std::string> parts;
    std::string current;

    for (char c : path)
    {
        if (c != '/')
        {
            current += c;
            continue;
        }

        if (!current.empty())
        {
            parts.push_back(current);
            current.clear();
        }
    }

    if (!current.empty())
    {
        parts.push_back(current);
    }

    return parts;
}

std::string Router::toUpper(std::string value)
{
    for (char& c : value)
    {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    return value;
}

std::regex Router::compileConstraint(const std::string& spec)
{
    if (spec == "int")
        return std::regex("[0-9]+");
    if (spec == "alpha")
        return std::regex("[A-Za-z]+");
    if (spec == "alnum")
        return std::regex("[A-Za-z0-9]+");
    if (spec == "slug")
        return std::regex("[A-Za-z0-9_-]+");
    if (spec == "uuid")
        return std::regex("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}");
    return std::regex(spec);
}

int Router::add(const std::string& method, const std::string& pattern)
{
    Route route;
    route.method = toUpper(method);

    for (const std::string& token : splitPath(pattern))
    {
        Segment segment;

        // a bare '*' captures the remaining path and surfaces it as the 'wildcard' param
        if (token == "*")
        {
            segment.isParam = true;
            segment.isWildcard = true;
            segment.text = "wildcard";
            route.segments.push_back(std::move(segment));
            continue;
        }

        segment.isParam = token.front() == ':';
        if (!segment.isParam)
        {
            segment.text = token;
            route.segments.push_back(std::move(segment));
            continue;
        }

        // a trailing '?' on a named param makes that segment optional
        segment.isOptional = token.back() == '?';
        const std::size_t length = token.size() - 1 - (segment.isOptional ? 1 : 0);
        segment.text = token.substr(1, length);
        route.segments.push_back(std::move(segment));
    }

    routes.push_back(std::move(route));
    return static_cast<int>(routes.size()) - 1;
}

void Router::setConstraint(int routeId, const std::string& param, const std::string& constraint)
{
    routes.at(routeId).constraints[param] = compileConstraint(constraint);
}

void Router::setName(int routeId, const std::string& name)
{
    const auto existing = namedRoutes.find(name);
    if (existing != namedRoutes.end())
    {
        throw std::runtime_error("[Router] The route name '" + name + "' is already in use.");
    }

    routes.at(routeId).name = name;
    namedRoutes[name] = routeId;
}

bool Router::segmentMatchesPart(const Route& route, const Segment& segment, const std::string& part) const
{
    const auto constraint = route.constraints.find(segment.text);
    if (constraint == route.constraints.end())
    {
        return true;
    }

    // cap the matched length so a crafted segment cannot trigger catastrophic backtracking
    if (part.size() > kMaxSegmentLength)
    {
        return false;
    }

    try
    {
        return std::regex_match(part, constraint->second);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool Router::pathMatches(const Route& route, const std::vector<std::string>& parts, std::vector<RouteParam>& outParams) const
{
    std::size_t partIndex = 0;

    for (std::size_t i = 0; i < route.segments.size(); ++i)
    {
        const Segment& segment = route.segments[i];

        // a wildcard is the terminal segment and captures every remaining part joined by '/'
        if (segment.isWildcard)
        {
            std::string tail;
            for (std::size_t j = partIndex; j < parts.size(); ++j)
            {
                if (j > partIndex)
                {
                    tail += '/';
                }

                tail += parts[j];
            }

            outParams.push_back(RouteParam{segment.text, tail});
            return true;
        }

        // an optional param with no part left is simply skipped, leaving its value absent
        if (segment.isOptional && partIndex >= parts.size())
        {
            continue;
        }

        if (partIndex >= parts.size())
        {
            return false;
        }

        const std::string& part = parts[partIndex];

        if (!segment.isParam)
        {
            if (segment.text != part)
            {
                return false;
            }

            ++partIndex;
            continue;
        }

        // an optional param yields its segment when the value fails its constraint rather than failing the route
        if (!segmentMatchesPart(route, segment, part))
        {
            if (segment.isOptional)
            {
                continue;
            }

            return false;
        }

        outParams.push_back(RouteParam{segment.text, part});
        ++partIndex;
    }

    return partIndex == parts.size();
}

MatchResult Router::match(const std::string& method, const std::string& path) const
{
    const std::string wanted = toUpper(method);

    MatchResult result;

    // bound the total path length before any matching so constraint regexes stay cheap
    if (path.size() > kMaxPathLength)
    {
        result.status = MatchStatus::NotFound;
        return result;
    }

    // reject control characters that desync segment matching and leak into params, logs and headers
    for (char c : path)
    {
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f)
        {
            result.status = MatchStatus::NotFound;
            return result;
        }
    }

    const std::vector<std::string> parts = splitPath(path);
    std::vector<std::string> allowed;
    int headFallback = -1;
    std::vector<RouteParam> headParams;

    for (std::size_t i = 0; i < routes.size(); ++i)
    {
        const Route& route = routes[i];

        std::vector<RouteParam> params;
        if (!pathMatches(route, parts, params))
        {
            continue;
        }

        if (route.method == wanted || route.method == "ALL")
        {
            result.status = MatchStatus::Found;
            result.routeId = static_cast<int>(i);
            result.params = std::move(params);
            return result;
        }

        // a HEAD request falls back to the first matching GET handler, whose body the transport drops
        if (wanted == "HEAD" && route.method == "GET" && headFallback < 0)
        {
            headFallback = static_cast<int>(i);
            headParams = params;
        }

        if (std::find(allowed.begin(), allowed.end(), route.method) == allowed.end())
        {
            allowed.push_back(route.method);
        }
    }

    if (headFallback >= 0)
    {
        result.status = MatchStatus::Found;
        result.routeId = headFallback;
        result.params = std::move(headParams);
        return result;
    }

    if (!allowed.empty())
    {
        result.status = MatchStatus::MethodNotAllowed;
        result.allowedMethods = std::move(allowed);
        return result;
    }

    result.status = MatchStatus::NotFound;
    return result;
}

std::optional<std::string> Router::buildUrl(const std::string& name, const std::unordered_map<std::string, std::string>& params) const
{
    const auto entry = namedRoutes.find(name);
    if (entry == namedRoutes.end())
    {
        return std::nullopt;
    }

    std::string url;
    for (const Segment& segment : routes[entry->second].segments)
    {
        if (!segment.isParam)
        {
            url += '/';
            url += segment.text;
            continue;
        }

        const auto value = params.find(segment.text);

        // an optional or wildcard param simply drops out of the url when no value is supplied
        if (value == params.end())
        {
            if (segment.isOptional || segment.isWildcard)
            {
                continue;
            }

            return std::nullopt;
        }

        // a wildcard value carries its own '/' separators, so each part is encoded independently
        if (segment.isWildcard)
        {
            for (const std::string& part : splitPath(value->second))
            {
                url += '/';
                url += urlEncodeSegment(part);
            }

            continue;
        }

        url += '/';
        url += urlEncodeSegment(value->second);
    }

    return url.empty() ? std::string("/") : url;
}

} // namespace varn::http
