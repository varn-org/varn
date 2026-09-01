#include "varn/crypto/CryptoModule.h"

#include "varn/crypto/CryptoPrimitives.h"
#include "varn/lua/LuaHelpers.h"

#include <lua.hpp>

#include <string>
#include <string_view>

namespace varn::crypto
{

bool CryptoModule::parseDigestFormat(lua_State* L, int index, bool& hexOut)
{
    if (lua_gettop(L) < index || !lua_isstring(L, index))
    {
        return true;
    }

    const char* fmt = lua_tostring(L, index);
    if (fmt == nullptr)
    {
        return true;
    }

    if (std::string_view(fmt) == "hex")
    {
        hexOut = true;
        return true;
    }

    if (std::string_view(fmt) == "raw")
    {
        hexOut = false;
        return true;
    }

    return false;
}

int CryptoModule::luaDigest(lua_State* L)
{
    const std::string algorithm = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string data = varn::lua::LuaHelpers::checkString(L, 2);

    bool hexOut = true;
    if (!parseDigestFormat(L, 3, hexOut))
    {
        return luaL_error(L, "[CryptoModule] Output format must be hex or raw.");
    }

    try
    {
        const std::string out = CryptoPrimitives::digest(algorithm, data, hexOut);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaHmac(lua_State* L)
{
    const std::string digestAlgo = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string key = varn::lua::LuaHelpers::checkString(L, 2);
    const std::string data = varn::lua::LuaHelpers::checkString(L, 3);

    bool hexOut = true;
    if (!parseDigestFormat(L, 4, hexOut))
    {
        return luaL_error(L, "[CryptoModule] Output format must be hex or raw.");
    }

    try
    {
        const std::string out = CryptoPrimitives::hmac(digestAlgo, key, data, hexOut);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaRandomBytes(lua_State* L)
{
    const lua_Integer count = luaL_checkinteger(L, 1);
    if (count < 0)
    {
        return luaL_error(L, "[CryptoModule] Random byte count must not be negative.");
    }

    try
    {
        const std::string bytes = CryptoPrimitives::randomBytes(static_cast<std::size_t>(count));
        lua_pushlstring(L, bytes.data(), bytes.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaEquals(lua_State* L)
{
    std::size_t lenA = 0;
    std::size_t lenB = 0;
    const char* a = luaL_checklstring(L, 1, &lenA);
    const char* b = luaL_checklstring(L, 2, &lenB);

    // constant-time comparison with a length mismatch folded into the accumulator to report unequal
    std::size_t diff = lenA ^ lenB;
    const std::size_t common = lenA < lenB ? lenA : lenB;
    for (std::size_t i = 0; i < common; ++i)
    {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }

    lua_pushboolean(L, diff == 0 ? 1 : 0);
    return 1;
}

int CryptoModule::luaBase64Encode(lua_State* L)
{
    const std::string data = varn::lua::LuaHelpers::checkString(L, 1);

    try
    {
        const std::string out = CryptoPrimitives::base64Encode(data, false, true);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaBase64Decode(lua_State* L)
{
    const std::string data = varn::lua::LuaHelpers::checkString(L, 1);

    try
    {
        const std::string out = CryptoPrimitives::base64Decode(data);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaBase64UrlEncode(lua_State* L)
{
    const std::string data = varn::lua::LuaHelpers::checkString(L, 1);

    try
    {
        // url-safe alphabet without padding so the result is safe in urls and cookies
        const std::string out = CryptoPrimitives::base64Encode(data, true, false);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaBase64UrlDecode(lua_State* L)
{
    const std::string data = varn::lua::LuaHelpers::checkString(L, 1);

    try
    {
        const std::string out = CryptoPrimitives::base64Decode(data);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaHexEncode(lua_State* L)
{
    const std::string data = varn::lua::LuaHelpers::checkString(L, 1);

    try
    {
        const std::string out = CryptoPrimitives::hexEncode(data);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaHexDecode(lua_State* L)
{
    const std::string data = varn::lua::LuaHelpers::checkString(L, 1);

    try
    {
        const std::string out = CryptoPrimitives::hexDecode(data);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaUuidV4(lua_State* L)
{
    try
    {
        const std::string out = CryptoPrimitives::uuidV4();
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaUuidV7(lua_State* L)
{
    try
    {
        const std::string out = CryptoPrimitives::uuidV7();
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaHashPassword(lua_State* L)
{
    const std::string password = varn::lua::LuaHelpers::checkString(L, 1);

    try
    {
        const std::string out = CryptoPrimitives::hashPassword(password);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaVerifyPassword(lua_State* L)
{
    const std::string password = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string encoded = varn::lua::LuaHelpers::checkString(L, 2);

    try
    {
        const bool ok = CryptoPrimitives::verifyPassword(password, encoded);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaEncrypt(lua_State* L)
{
    const std::string key = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string plaintext = varn::lua::LuaHelpers::checkString(L, 2);

    try
    {
        const std::string out = CryptoPrimitives::aesGcmEncrypt(key, plaintext);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaRsaEncryptPublic(lua_State* L)
{
    const std::string pem = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string data = varn::lua::LuaHelpers::checkString(L, 2);

    try
    {
        const std::string out = CryptoPrimitives::rsaEncryptPublic(pem, data);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaDecrypt(lua_State* L)
{
    const std::string key = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string blob = varn::lua::LuaHelpers::checkString(L, 2);

    try
    {
        const std::string out = CryptoPrimitives::aesGcmDecrypt(key, blob);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaPbkdf2(lua_State* L)
{
    const std::string password = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string salt = varn::lua::LuaHelpers::checkString(L, 2);
    const lua_Integer iterations = luaL_checkinteger(L, 3);
    const lua_Integer keyLen = luaL_checkinteger(L, 4);
    const std::string algo = varn::lua::LuaHelpers::optionalString(L, 5, "SHA256");

    if (iterations <= 0)
    {
        return luaL_error(L, "[CryptoModule] The pbkdf2 iteration count must be positive.");
    }

    if (keyLen <= 0)
    {
        return luaL_error(L, "[CryptoModule] The pbkdf2 key length must be positive.");
    }

    try
    {
        const std::string out = CryptoPrimitives::pbkdf2(password, salt, static_cast<std::size_t>(iterations), static_cast<std::size_t>(keyLen), algo);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaHkdf(lua_State* L)
{
    const std::string key = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string salt = varn::lua::LuaHelpers::checkString(L, 2);
    const std::string info = varn::lua::LuaHelpers::checkString(L, 3);
    const lua_Integer keyLen = luaL_checkinteger(L, 4);
    const std::string algo = varn::lua::LuaHelpers::optionalString(L, 5, "SHA256");

    if (keyLen <= 0)
    {
        return luaL_error(L, "[CryptoModule] The hkdf key length must be positive.");
    }

    try
    {
        const std::string out = CryptoPrimitives::hkdf(key, salt, info, static_cast<std::size_t>(keyLen), algo);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int CryptoModule::luaOpen(lua_State* L)
{
    lua_newtable(L);

    lua_pushcfunction(L, &CryptoModule::luaDigest);
    lua_setfield(L, -2, "digest");

    lua_pushcfunction(L, &CryptoModule::luaHmac);
    lua_setfield(L, -2, "hmac");

    lua_pushcfunction(L, &CryptoModule::luaRandomBytes);
    lua_setfield(L, -2, "randomBytes");

    lua_pushcfunction(L, &CryptoModule::luaEquals);
    lua_setfield(L, -2, "equals");

    lua_pushcfunction(L, &CryptoModule::luaBase64Encode);
    lua_setfield(L, -2, "base64Encode");

    lua_pushcfunction(L, &CryptoModule::luaBase64Decode);
    lua_setfield(L, -2, "base64Decode");

    lua_pushcfunction(L, &CryptoModule::luaBase64UrlEncode);
    lua_setfield(L, -2, "base64UrlEncode");

    lua_pushcfunction(L, &CryptoModule::luaBase64UrlDecode);
    lua_setfield(L, -2, "base64UrlDecode");

    lua_pushcfunction(L, &CryptoModule::luaHexEncode);
    lua_setfield(L, -2, "hexEncode");

    lua_pushcfunction(L, &CryptoModule::luaHexDecode);
    lua_setfield(L, -2, "hexDecode");

    lua_pushcfunction(L, &CryptoModule::luaUuidV4);
    lua_setfield(L, -2, "uuidV4");

    lua_pushcfunction(L, &CryptoModule::luaUuidV7);
    lua_setfield(L, -2, "uuidV7");

    lua_pushcfunction(L, &CryptoModule::luaHashPassword);
    lua_setfield(L, -2, "hashPassword");

    lua_pushcfunction(L, &CryptoModule::luaVerifyPassword);
    lua_setfield(L, -2, "verifyPassword");

    lua_pushcfunction(L, &CryptoModule::luaEncrypt);
    lua_setfield(L, -2, "encrypt");

    lua_pushcfunction(L, &CryptoModule::luaDecrypt);
    lua_setfield(L, -2, "decrypt");

    lua_pushcfunction(L, &CryptoModule::luaRsaEncryptPublic);
    lua_setfield(L, -2, "rsaEncryptPublic");

    lua_pushcfunction(L, &CryptoModule::luaPbkdf2);
    lua_setfield(L, -2, "pbkdf2");

    lua_pushcfunction(L, &CryptoModule::luaHkdf);
    lua_setfield(L, -2, "hkdf");

    return 1;
}

void CryptoModule::install(lua_State* L)
{
    luaL_requiref(L, "crypto", &CryptoModule::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::crypto
