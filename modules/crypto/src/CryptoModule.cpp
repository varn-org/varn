#include "varn/crypto/CryptoModule.h"

#include "varn/async/AsyncTask.h"
#include "varn/async/Promise.h"
#include "varn/crypto/CryptoPrimitives.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/runtime/Runtime.h"

#include <lua.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace varn::crypto
{

namespace
{
class CryptoLua
{
public:
    // Pushes the bytes a primitive answers, and turns a failure it reports into a Lua error with its sentence.
    template <typename Produce>
    static int pushResult(lua_State* L, Produce produce)
    {
        std::string out;
        try
        {
            out = produce();
        }
        catch (const std::runtime_error& ex)
        {
            return luaL_error(L, "%s", ex.what());
        }

        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
};
} // namespace

// Answers whether a digest is written as hex, which it is when the format is absent, and refuses any format that is not "hex" or "raw".
bool CryptoModule::hexOutput(lua_State* L, int index)
{
    if (lua_isnoneornil(L, index))
    {
        return true;
    }

    const std::string_view format = lua_type(L, index) == LUA_TSTRING ? std::string_view(lua_tostring(L, index)) : std::string_view();
    if (format == "hex")
    {
        return true;
    }

    if (format != "raw")
    {
        luaL_error(L, "Output format must be \"hex\" or \"raw\".");
    }

    return false;
}

int CryptoModule::luaDigest(lua_State* L)
{
    const std::string algorithm = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string data = varn::lua::LuaHelpers::checkString(L, 2);

    const bool hexOut = hexOutput(L, 3);

    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::digest(algorithm, data, hexOut); });
    // clang-format on
}

int CryptoModule::luaHmac(lua_State* L)
{
    const std::string digestAlgo = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string key = varn::lua::LuaHelpers::checkString(L, 2);
    const std::string data = varn::lua::LuaHelpers::checkString(L, 3);

    const bool hexOut = hexOutput(L, 4);

    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::hmac(digestAlgo, key, data, hexOut); });
    // clang-format on
}

int CryptoModule::luaRandomBytes(lua_State* L)
{
    const lua_Integer count = luaL_checkinteger(L, 1);
    if (count < 0)
    {
        return luaL_error(L, "Random byte count must not be negative.");
    }

    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::randomBytes(static_cast<std::size_t>(count)); });
    // clang-format on
}

int CryptoModule::luaEquals(lua_State* L)
{
    std::size_t lenA = 0;
    std::size_t lenB = 0;
    const char* a = luaL_checklstring(L, 1, &lenA);
    const char* b = luaL_checklstring(L, 2, &lenB);

    // Constant-time comparison with a length mismatch folded into the accumulator to report unequal.
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

    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::base64Encode(data, false, true); });
    // clang-format on
}

int CryptoModule::luaBase64Decode(lua_State* L)
{
    const std::string data = varn::lua::LuaHelpers::checkString(L, 1);

    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::base64Decode(data); });
    // clang-format on
}

int CryptoModule::luaBase64UrlEncode(lua_State* L)
{
    const std::string data = varn::lua::LuaHelpers::checkString(L, 1);

    // The URL-safe alphabet without padding keeps the result safe in URLs and cookies.
    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::base64Encode(data, true, false); });
    // clang-format on
}

int CryptoModule::luaHexEncode(lua_State* L)
{
    const std::string data = varn::lua::LuaHelpers::checkString(L, 1);

    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::hexEncode(data); });
    // clang-format on
}

int CryptoModule::luaHexDecode(lua_State* L)
{
    const std::string data = varn::lua::LuaHelpers::checkString(L, 1);

    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::hexDecode(data); });
    // clang-format on
}

int CryptoModule::luaUuidV4(lua_State* L)
{
    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::uuidV4(); });
    // clang-format on
}

int CryptoModule::luaUuidV7(lua_State* L)
{
    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::uuidV7(); });
    // clang-format on
}

// A password hash is slow by design, so it runs on the task pool instead of holding the loop.
int CryptoModule::luaHashPassword(lua_State* L)
{
    std::string password = varn::lua::LuaHelpers::checkString(L, 1);
    auto& rt = *static_cast<varn::runtime::Runtime*>(varn::lua::LuaHelpers::getRuntime(L));

    // clang-format off
    return varn::async::AsyncTask::runOnPool(L, rt, rt.scheduler().taskPool(), [password = std::move(password)](varn::async::Promise& promise)
    {
        promise.resolve(CryptoPrimitives::hashPassword(password));
    });
    // clang-format on
}

int CryptoModule::luaVerifyPassword(lua_State* L)
{
    std::string password = varn::lua::LuaHelpers::checkString(L, 1);
    std::string encoded = varn::lua::LuaHelpers::checkString(L, 2);
    auto& rt = *static_cast<varn::runtime::Runtime*>(varn::lua::LuaHelpers::getRuntime(L));

    // clang-format off
    return varn::async::AsyncTask::runOnPool(L, rt, rt.scheduler().taskPool(), [password = std::move(password), encoded = std::move(encoded)](varn::async::Promise& promise)
    {
        const bool matches = CryptoPrimitives::verifyPassword(password, encoded);
        promise.resolveCustom([matches](lua_State* lua)
        {
            lua_pushboolean(lua, matches ? 1 : 0);
        });
    });
    // clang-format on
}

int CryptoModule::luaEncrypt(lua_State* L)
{
    const std::string key = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string plaintext = varn::lua::LuaHelpers::checkString(L, 2);

    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::aesGcmEncrypt(key, plaintext); });
    // clang-format on
}

int CryptoModule::luaRsaEncryptPublic(lua_State* L)
{
    const std::string pem = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string data = varn::lua::LuaHelpers::checkString(L, 2);

    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::rsaEncryptPublic(pem, data); });
    // clang-format on
}

int CryptoModule::luaDecrypt(lua_State* L)
{
    const std::string key = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string blob = varn::lua::LuaHelpers::checkString(L, 2);

    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::aesGcmDecrypt(key, blob); });
    // clang-format on
}

// The iterations make a derivation as slow as its caller asks, so it runs on the task pool.
int CryptoModule::luaPbkdf2(lua_State* L)
{
    std::string password = varn::lua::LuaHelpers::checkString(L, 1);
    std::string salt = varn::lua::LuaHelpers::checkString(L, 2);
    const lua_Integer iterations = luaL_checkinteger(L, 3);
    const lua_Integer keyLen = luaL_checkinteger(L, 4);
    std::string algo = varn::lua::LuaHelpers::optionalString(L, 5, "SHA256");

    if (iterations <= 0)
    {
        return luaL_error(L, "The PBKDF2 iteration count must be positive.");
    }

    if (keyLen <= 0)
    {
        return luaL_error(L, "The PBKDF2 key length must be positive.");
    }

    auto& rt = *static_cast<varn::runtime::Runtime*>(varn::lua::LuaHelpers::getRuntime(L));
    const auto rounds = static_cast<std::size_t>(iterations);
    const auto length = static_cast<std::size_t>(keyLen);

    // clang-format off
    return varn::async::AsyncTask::runOnPool(L, rt, rt.scheduler().taskPool(), [password = std::move(password), salt = std::move(salt), rounds, length, algo = std::move(algo)](varn::async::Promise& promise)
    {
        promise.resolve(CryptoPrimitives::pbkdf2(password, salt, rounds, length, algo));
    });
    // clang-format on
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
        return luaL_error(L, "The HKDF key length must be positive.");
    }

    // clang-format off
    return CryptoLua::pushResult(L, [&] { return CryptoPrimitives::hkdf(key, salt, info, static_cast<std::size_t>(keyLen), algo); });
    // clang-format on
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

    // Decoding accepts either alphabet and optional padding, so one function serves both names.
    lua_pushcfunction(L, &CryptoModule::luaBase64Decode);
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
