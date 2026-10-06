#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace varn::crypto
{

class CryptoPrimitives final
{
public:
    CryptoPrimitives() = delete;

    static std::string digest(std::string_view algorithm, std::string_view data, bool outputHex);

    static std::string hmac(std::string_view digestAlgorithm, std::string_view key, std::string_view data, bool outputHex);

    static std::string randomBytes(std::size_t count);

    static std::string base64Encode(std::string_view data, bool urlSafe, bool padding);

    static std::string base64Decode(std::string_view data);

    static std::string hexEncode(std::string_view data);

    static std::string hexDecode(std::string_view data);

    static std::string uuidV4();

    static std::string uuidV7();

    static std::string hashPassword(std::string_view password);

    static bool verifyPassword(std::string_view password, std::string_view encoded);

    static std::string aesGcmEncrypt(std::string_view key, std::string_view plaintext);

    static std::string aesGcmDecrypt(std::string_view key, std::string_view blob);

    static std::string pbkdf2(std::string_view password, std::string_view salt, std::size_t iterations, std::size_t keyLen, std::string_view algorithm);

    static std::string hkdf(std::string_view key, std::string_view salt, std::string_view info, std::size_t keyLen, std::string_view algorithm);

    static std::string rsaEncryptPublic(std::string_view pemPublicKey, std::string_view data);

private:
    static std::string trimAlgo(std::string_view in);

    static std::string toHex(const unsigned char* data, std::size_t len);
};

} // namespace varn::crypto
