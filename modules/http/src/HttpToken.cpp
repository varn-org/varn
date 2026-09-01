#include "HttpToken.h"

#include "varn/crypto/CryptoPrimitives.h"
#include "varn/json/JsonSerializer.h"

#include <lua.hpp>

#include <ctime>
#include <exception>
#include <string>

namespace varn::http
{

bool HttpToken::constantTimeEqual(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
    {
        return false;
    }

    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }

    return diff == 0;
}

std::string HttpToken::base64UrlEncode(const std::string& input)
{
    return varn::crypto::CryptoPrimitives::base64Encode(input, true, false);
}

bool HttpToken::base64UrlDecode(const std::string& input, std::string& out)
{
    try
    {
        out = varn::crypto::CryptoPrimitives::base64Decode(input);
        return true;
    }
    catch (const std::exception&)
    {
        // reject any character outside the base64url alphabet so malformed segments cannot decode
        out.clear();
        return false;
    }
}

JwtVerifyOptions HttpToken::readJwtVerifyOptions(lua_State* L, int optsIdx)
{
    JwtVerifyOptions opts;
    if (!lua_istable(L, optsIdx))
    {
        return opts;
    }

    lua_getfield(L, optsIdx, "audience");
    if (lua_isstring(L, -1))
    {
        opts.hasAudience = true;
        opts.audience = lua_tostring(L, -1);
    }

    lua_pop(L, 1);

    lua_getfield(L, optsIdx, "issuer");
    if (lua_isstring(L, -1))
    {
        opts.hasIssuer = true;
        opts.issuer = lua_tostring(L, -1);
    }

    lua_pop(L, 1);

    lua_getfield(L, optsIdx, "leeway");
    if (lua_isnumber(L, -1))
    {
        const long long leeway = static_cast<long long>(lua_tointeger(L, -1));
        opts.leewaySeconds = leeway > 0 ? leeway : 0;
    }

    lua_pop(L, 1);

    return opts;
}

bool HttpToken::audienceMatches(lua_State* L, int claimsIdx, const std::string& expected)
{
    lua_getfield(L, claimsIdx, "aud");

    bool matched = false;
    if (lua_type(L, -1) == LUA_TSTRING)
    {
        matched = expected == lua_tostring(L, -1);
    }
    else if (lua_istable(L, -1))
    {
        const int count = static_cast<int>(lua_rawlen(L, -1));
        for (int i = 1; i <= count && !matched; ++i)
        {
            lua_rawgeti(L, -1, i);
            if (lua_type(L, -1) == LUA_TSTRING && expected == lua_tostring(L, -1))
            {
                matched = true;
            }

            lua_pop(L, 1);
        }
    }

    lua_pop(L, 1);
    return matched;
}

bool HttpToken::verifyJwt(lua_State* L, const std::string& token, const std::string& secret, const JwtVerifyOptions& opts, std::string& error)
{
    // refuse to verify without a configured secret so an empty key cannot be used to forge tokens
    if (secret.empty())
    {
        error = "missing secret";
        return false;
    }

    const std::size_t first = token.find('.');
    const std::size_t second = token.find('.', first == std::string::npos ? first : first + 1);
    if (first == std::string::npos || second == std::string::npos)
    {
        error = "malformed token";
        return false;
    }

    const std::string signingInput = token.substr(0, second);
    const std::string signature = token.substr(second + 1);

    // enforce the algorithm so a token with alg none or a different scheme cannot be accepted
    std::string header;
    if (!base64UrlDecode(token.substr(0, first), header))
    {
        error = "malformed header";
        return false;
    }

    bool headerDecoded = false;
    try
    {
        headerDecoded = json::JsonSerializer::deserialize(L, header);
    }
    catch (const std::exception& ex)
    {
        error = ex.what();
        return false;
    }

    bool algorithmOk = false;
    bool hasCrit = false;
    if (headerDecoded)
    {
        // a header that decodes to a non-object cannot be indexed, so guard before reading alg
        if (lua_istable(L, -1))
        {
            lua_getfield(L, -1, "alg");
            algorithmOk = lua_type(L, -1) == LUA_TSTRING && std::string(lua_tostring(L, -1)) == "HS256";
            lua_pop(L, 1);

            lua_getfield(L, -1, "crit");
            hasCrit = !lua_isnil(L, -1);
            lua_pop(L, 1);
        }

        lua_pop(L, 1);
    }

    if (!algorithmOk)
    {
        error = "unsupported algorithm";
        return false;
    }

    // a token demanding a critical extension this verifier cannot process must be rejected
    if (hasCrit)
    {
        error = "unsupported crit";
        return false;
    }

    std::string expected;
    try
    {
        expected = base64UrlEncode(varn::crypto::CryptoPrimitives::hmac("SHA256", secret, signingInput, false));
    }
    catch (const std::exception& ex)
    {
        error = ex.what();
        return false;
    }

    if (!constantTimeEqual(expected, signature))
    {
        error = "bad signature";
        return false;
    }

    std::string payload;
    if (!base64UrlDecode(token.substr(first + 1, second - first - 1), payload))
    {
        error = "malformed payload";
        return false;
    }

    bool decoded = false;
    try
    {
        decoded = json::JsonSerializer::deserialize(L, payload);
    }
    catch (const std::exception& ex)
    {
        error = ex.what();
        return false;
    }

    if (!decoded)
    {
        error = "bad payload";
        return false;
    }

    // a payload that decodes to a non-object cannot carry claims, so reject before indexing it
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        error = "bad payload";
        return false;
    }

    // a json array decodes to a table with a positive length, but claims must be a named object
    if (lua_rawlen(L, -1) > 0)
    {
        lua_pop(L, 1);
        error = "bad payload";
        return false;
    }

    const double now = static_cast<double>(std::time(nullptr));
    const double leeway = static_cast<double>(opts.leewaySeconds);

    // a present expiry must be numeric and must not be in the past beyond the allowed clock skew
    lua_getfield(L, -1, "exp");
    const int expType = lua_type(L, -1);
    if (expType != LUA_TNIL)
    {
        if (expType != LUA_TNUMBER)
        {
            lua_pop(L, 2);
            error = "invalid exp";
            return false;
        }

        if (now >= lua_tonumber(L, -1) + leeway)
        {
            lua_pop(L, 2);
            error = "token expired";
            return false;
        }
    }

    lua_pop(L, 1);

    // a present not-before must be numeric and must not be in the future beyond the allowed clock skew
    lua_getfield(L, -1, "nbf");
    const int nbfType = lua_type(L, -1);
    if (nbfType != LUA_TNIL)
    {
        if (nbfType != LUA_TNUMBER)
        {
            lua_pop(L, 2);
            error = "invalid nbf";
            return false;
        }

        if (now < lua_tonumber(L, -1) - leeway)
        {
            lua_pop(L, 2);
            error = "token not yet valid";
            return false;
        }
    }

    lua_pop(L, 1);

    // when an audience is expected the claim must be present and contain it
    if (opts.hasAudience && !audienceMatches(L, lua_gettop(L), opts.audience))
    {
        lua_pop(L, 1);
        error = "invalid audience";
        return false;
    }

    // when an issuer is expected the claim must be present and match exactly
    if (opts.hasIssuer)
    {
        lua_getfield(L, -1, "iss");
        const bool ok = lua_type(L, -1) == LUA_TSTRING && opts.issuer == lua_tostring(L, -1);
        lua_pop(L, 1);
        if (!ok)
        {
            lua_pop(L, 1);
            error = "invalid issuer";
            return false;
        }
    }

    return true;
}

std::string HttpToken::makeCsrfToken(const std::string& secret, const std::string& sessionId)
{
    const std::string nonce = base64UrlEncode(varn::crypto::CryptoPrimitives::randomBytes(16));
    const std::string mac = base64UrlEncode(varn::crypto::CryptoPrimitives::hmac("SHA256", secret, sessionId + ":" + nonce, false));
    return nonce + "." + mac;
}

bool HttpToken::validCsrfToken(const std::string& secret, const std::string& sessionId, const std::string& token)
{
    const std::size_t dot = token.find('.');
    if (dot == std::string::npos)
    {
        return false;
    }

    const std::string nonce = token.substr(0, dot);
    const std::string mac = token.substr(dot + 1);
    const std::string expected = base64UrlEncode(varn::crypto::CryptoPrimitives::hmac("SHA256", secret, sessionId + ":" + nonce, false));
    return constantTimeEqual(expected, mac);
}

} // namespace varn::http
