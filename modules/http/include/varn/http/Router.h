#pragma once

#include <cstddef>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

namespace varn::http
{

struct RouteParam
{
    std::string name;
    std::string value;
};

enum class MatchStatus
{
    Found,
    MethodNotAllowed,
    NotFound,
};

struct MatchResult
{
    MatchStatus status = MatchStatus::NotFound;
    int routeId = -1;
    std::vector<RouteParam> params;
    std::vector<std::string> allowedMethods;
};

class Router
{
public:
    int add(const std::string& method, const std::string& pattern);
    void setConstraint(int routeId, const std::string& param, const std::string& constraint);
    void setName(int routeId, const std::string& name);

    MatchResult match(const std::string& method, const std::string& path) const;
    std::optional<std::string> buildUrl(const std::string& name, const std::unordered_map<std::string, std::string>& params) const;

private:
    struct Segment
    {
        bool isParam = false;
        bool isOptional = false;
        bool isWildcard = false;
        std::string text;
    };

    struct Route
    {
        std::string method;
        std::vector<Segment> segments;
        std::unordered_map<std::string, std::regex> constraints;
        std::string name;
    };

    bool pathMatches(const Route& route, const std::vector<std::string>& parts, std::vector<RouteParam>& outParams) const;
    bool segmentMatchesPart(const Route& route, const Segment& segment, const std::string& part) const;

    static std::string urlEncodeSegment(const std::string& value);
    static std::vector<std::string> splitPath(const std::string& path);
    static std::string toUpper(std::string value);
    static std::regex compileConstraint(const std::string& spec);

    static constexpr std::size_t kMaxPathLength = 8192;
    static constexpr std::size_t kMaxSegmentLength = 1024;

    std::vector<Route> routes;
    std::unordered_map<std::string, int> namedRoutes;
};

} // namespace varn::http
