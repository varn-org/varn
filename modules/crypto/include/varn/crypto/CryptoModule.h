#pragma once

struct lua_State;

namespace varn::crypto
{

class CryptoModule
{
public:
    CryptoModule() = delete;

    static void install(lua_State* L);

private:
    static bool parseDigestFormat(lua_State* L, int index, bool& hexOut);

    static int luaDigest(lua_State* L);
    static int luaHmac(lua_State* L);
    static int luaRandomBytes(lua_State* L);
    static int luaEquals(lua_State* L);
    static int luaBase64Encode(lua_State* L);
    static int luaBase64Decode(lua_State* L);
    static int luaBase64UrlEncode(lua_State* L);
    static int luaBase64UrlDecode(lua_State* L);
    static int luaHexEncode(lua_State* L);
    static int luaHexDecode(lua_State* L);
    static int luaUuidV4(lua_State* L);
    static int luaUuidV7(lua_State* L);
    static int luaHashPassword(lua_State* L);
    static int luaVerifyPassword(lua_State* L);
    static int luaEncrypt(lua_State* L);
    static int luaDecrypt(lua_State* L);
    static int luaRsaEncryptPublic(lua_State* L);
    static int luaPbkdf2(lua_State* L);
    static int luaHkdf(lua_State* L);
    static int luaOpen(lua_State* L);
};

} // namespace varn::crypto
