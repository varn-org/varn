#pragma once

#include <string>

struct lua_State;

namespace varn::http
{

struct JwtVerifyOptions
{
    bool hasAudience = false;
    std::string audience;
    bool hasIssuer = false;
    std::string issuer;
    long long leewaySeconds = 0;
};

class HttpToken
{
public:
    static bool constantTimeEqual(const std::string& a, const std::string& b);
    static std::string base64UrlEncode(const std::string& input);
    static bool base64UrlDecode(const std::string& input, std::string& out);
    static JwtVerifyOptions readJwtVerifyOptions(lua_State* L, int optsIdx);
    static bool audienceMatches(lua_State* L, int claimsIdx, const std::string& expected);
    static bool verifyJwt(lua_State* L, const std::string& token, const std::string& secret, const JwtVerifyOptions& opts, std::string& error);
    static std::string makeCsrfToken(const std::string& secret, const std::string& sessionId);
    static bool validCsrfToken(const std::string& secret, const std::string& sessionId, const std::string& token);
};

} // namespace varn::http
