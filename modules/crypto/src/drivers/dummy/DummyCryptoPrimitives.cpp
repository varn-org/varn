#include "varn/crypto/CryptoPrimitives.h"

#include <stdexcept>
#include <string>
#include <string_view>

#if !defined(VARN_CRYPTO_DRIVER_DUMMY) || !VARN_CRYPTO_DRIVER_DUMMY
#error "DummyCryptoPrimitives.cpp is only built with VARN_CRYPTO_DRIVER_DUMMY"
#endif

namespace varn::crypto
{

std::string CryptoPrimitives::digest(std::string_view /*algorithm*/, std::string_view /*data*/, bool /*outputHex*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::hmac(std::string_view /*digestAlgorithm*/, std::string_view /*key*/, std::string_view /*data*/, bool /*outputHex*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::randomBytes(std::size_t /*count*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::base64Encode(std::string_view /*data*/, bool /*urlSafe*/, bool /*padding*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::base64Decode(std::string_view /*data*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::hexEncode(std::string_view /*data*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::hexDecode(std::string_view /*data*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::uuidV4()
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::uuidV7()
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::hashPassword(std::string_view /*password*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

bool CryptoPrimitives::verifyPassword(std::string_view /*password*/, std::string_view /*encoded*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::aesGcmEncrypt(std::string_view /*key*/, std::string_view /*plaintext*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::aesGcmDecrypt(std::string_view /*key*/, std::string_view /*blob*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::pbkdf2(std::string_view /*password*/, std::string_view /*salt*/, std::size_t /*iterations*/, std::size_t /*keyLen*/, std::string_view /*algorithm*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::hkdf(std::string_view /*key*/, std::string_view /*salt*/, std::string_view /*info*/, std::size_t /*keyLen*/, std::string_view /*algorithm*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

std::string CryptoPrimitives::rsaEncryptPublic(std::string_view /*pemPublicKey*/, std::string_view /*data*/)
{
    throw std::runtime_error("[CryptoPrimitives] The crypto module is not available in this build.");
}

} // namespace varn::crypto
