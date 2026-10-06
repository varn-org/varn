#include "varn/crypto/CryptoPrimitives.h"

#include "varn/crypto/CryptoCodecs.h"

#include <charconv>
#include <chrono>
#include <climits>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(VARN_CRYPTO_DRIVER_OPENSSL) && VARN_CRYPTO_DRIVER_OPENSSL
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#endif

#if !defined(VARN_CRYPTO_DRIVER_OPENSSL) || !VARN_CRYPTO_DRIVER_OPENSSL
#error "The file \"OpenSslCryptoPrimitives.cpp\" is only built with \"VARN_CRYPTO_DRIVER_OPENSSL\"."
#endif

namespace varn::crypto
{

namespace
{
// Owns the objects of OpenSSL, so each one is freed on every path out of the function that made it, a thrown error included.
class OpenSslHandles
{
public:
    using DigestContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    using Digest = std::unique_ptr<EVP_MD, decltype(&EVP_MD_free)>;
    using Mac = std::unique_ptr<EVP_MAC, decltype(&EVP_MAC_free)>;
    using MacContext = std::unique_ptr<EVP_MAC_CTX, decltype(&EVP_MAC_CTX_free)>;
    using KeyContext = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
    using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
    using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
    using Bio = std::unique_ptr<BIO, decltype(&BIO_free)>;
};
} // namespace

std::string CryptoPrimitives::trimAlgo(std::string_view in)
{
    while (!in.empty() && (in.front() == ' ' || in.front() == '\t'))
    {
        in.remove_prefix(1);
    }
    while (!in.empty() && (in.back() == ' ' || in.back() == '\t'))
    {
        in.remove_suffix(1);
    }

    return std::string(in);
}

std::string CryptoPrimitives::toHex(const unsigned char* data, std::size_t len)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < len; ++i)
    {
        out << std::setw(2) << static_cast<int>(data[i]);
    }

    return out.str();
}

std::string CryptoPrimitives::digest(std::string_view algorithm, std::string_view data, bool outputHex)
{
    const std::string algo = trimAlgo(algorithm);
    if (algo.empty())
    {
        throw std::runtime_error("The hash algorithm name must not be empty.");
    }

    const OpenSslHandles::DigestContext ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!ctx)
    {
        throw std::runtime_error("The hash context could not be created.");
    }

    const OpenSslHandles::Digest fetched(EVP_MD_fetch(nullptr, algo.c_str(), nullptr), &EVP_MD_free);
    if (!fetched)
    {
        throw std::runtime_error("The requested hash algorithm is not known.");
    }

    if (EVP_DigestInit_ex2(ctx.get(), fetched.get(), nullptr) != 1)
    {
        throw std::runtime_error("The hash could not be initialized.");
    }

    if (EVP_DigestUpdate(ctx.get(), data.data(), data.size()) != 1)
    {
        throw std::runtime_error("The hash could not process the input.");
    }

    unsigned char buf[EVP_MAX_MD_SIZE];
    unsigned int outLen = 0;
    if (EVP_DigestFinal_ex(ctx.get(), buf, &outLen) != 1)
    {
        throw std::runtime_error("The hash could not be finalized.");
    }

    if (outputHex)
    {
        return toHex(buf, outLen);
    }

    return std::string(reinterpret_cast<const char*>(buf), static_cast<std::size_t>(outLen));
}

std::string CryptoPrimitives::hmac(std::string_view digestAlgorithm, std::string_view key, std::string_view data, bool outputHex)
{
    const std::string algo = trimAlgo(digestAlgorithm);
    if (algo.empty())
    {
        throw std::runtime_error("The keyed hash algorithm name must not be empty.");
    }

    const OpenSslHandles::Mac mac(EVP_MAC_fetch(nullptr, "HMAC", nullptr), &EVP_MAC_free);
    if (!mac)
    {
        throw std::runtime_error("The keyed hash backend could not be loaded.");
    }

    const OpenSslHandles::MacContext ctx(EVP_MAC_CTX_new(mac.get()), &EVP_MAC_CTX_free);
    if (!ctx)
    {
        throw std::runtime_error("The keyed hash context could not be created.");
    }

    std::vector<char> digestName(algo.begin(), algo.end());
    digestName.push_back('\0');
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, digestName.data(), 0),
        OSSL_PARAM_construct_end()};

    if (EVP_MAC_init(ctx.get(), reinterpret_cast<const unsigned char*>(key.data()), key.size(), params) != 1)
    {
        throw std::runtime_error("The keyed hash could not be initialized.");
    }

    if (EVP_MAC_update(ctx.get(), reinterpret_cast<const unsigned char*>(data.data()), data.size()) != 1)
    {
        throw std::runtime_error("The keyed hash could not process the input.");
    }

    const std::size_t macSize = EVP_MAC_CTX_get_mac_size(ctx.get());
    if (macSize == 0 || macSize > EVP_MAX_MD_SIZE)
    {
        throw std::runtime_error("The keyed hash produced an invalid output size.");
    }

    unsigned char buf[EVP_MAX_MD_SIZE];
    std::size_t outLen = sizeof(buf);
    if (EVP_MAC_final(ctx.get(), buf, &outLen, sizeof(buf)) != 1)
    {
        throw std::runtime_error("The keyed hash could not be finalized.");
    }

    if (outputHex)
    {
        return toHex(buf, outLen);
    }

    return std::string(reinterpret_cast<const char*>(buf), outLen);
}

std::string CryptoPrimitives::randomBytes(std::size_t count)
{
    // The only hard bound is the OpenSSL API taking an `int` length, so the caller decides how much it wants.
    if (count > static_cast<std::size_t>(INT_MAX))
    {
        throw std::runtime_error("The requested random byte count is too large.");
    }

    std::string bytes(count, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(bytes.data()), static_cast<int>(count)) != 1)
    {
        throw std::runtime_error("Random bytes could not be generated.");
    }

    return bytes;
}

namespace
{

class OpenSslCryptoHelpers
{
public:
    // Reads the "N,r,p" field of a stored hash as exactly three unsigned decimal numbers, so a sign, a space or a trailing byte is refused rather than read around.
    static bool parseScryptParams(std::string_view text, std::uint64_t& costN, std::uint64_t& blockR, std::uint64_t& parallelP)
    {
        const char* cursor = text.data();
        const char* const end = text.data() + text.size();
        std::uint64_t* const fields[] = {&costN, &blockR, &parallelP};
        for (std::size_t index = 0; index < 3; ++index)
        {
            if (index > 0)
            {
                if (cursor == end || *cursor != ',')
                {
                    return false;
                }

                ++cursor;
            }

            const auto [next, error] = std::from_chars(cursor, end, *fields[index]);
            if (error != std::errc() || next == cursor)
            {
                return false;
            }

            cursor = next;
        }

        return cursor == end;
    }

    static const EVP_MD* fetchDigest(const std::string& algo)
    {
        const EVP_MD* md = EVP_get_digestbyname(algo.c_str());
        if (md == nullptr)
        {
            throw std::runtime_error("The requested hash algorithm is not known.");
        }

        return md;
    }

    static std::uint64_t scryptMaxMem(std::uint64_t costN, std::uint32_t blockR)
    {
        // The scrypt algorithm needs roughly `128*N*r` bytes, so give it that plus headroom because the default ceiling rejects 32 MiB params.
        return 128ULL * costN * static_cast<std::uint64_t>(blockR) * 2ULL;
    }

    // The cost parameters of a stored hash decide how much memory verifying it allocates, so they are checked before scrypt is asked for anything.
    static bool scryptParamsAllowed(std::uint64_t costN, std::uint64_t blockR, std::uint64_t parallelP)
    {
        constexpr std::uint64_t kMaxScryptMemoryBytes = 1ULL << 30;
        constexpr std::uint64_t kMaxBlockR = 64;
        constexpr std::uint64_t kMaxParallelP = 16;

        if (costN < 2 || blockR == 0 || blockR > kMaxBlockR || parallelP == 0 || parallelP > kMaxParallelP)
        {
            return false;
        }

        // Divide rather than multiply so an oversized cost cannot overflow the estimate it is being checked against.
        return costN <= kMaxScryptMemoryBytes / (128ULL * blockR);
    }

    static bool constantTimeEqual(const unsigned char* a, const unsigned char* b, std::size_t len)
    {
        // Constant-time comparison so verifying a password or auth tag does not leak its bytes through timing.
        unsigned char diff = 0;
        for (std::size_t i = 0; i < len; ++i)
        {
            diff |= static_cast<unsigned char>(a[i] ^ b[i]);
        }

        return diff == 0;
    }
};

} // namespace

std::string CryptoPrimitives::base64Encode(std::string_view data, bool urlSafe, bool padding)
{
    return CryptoCodecs::base64Encode(data, urlSafe, padding);
}

std::string CryptoPrimitives::base64Decode(std::string_view data)
{
    return CryptoCodecs::base64Decode(data);
}

std::string CryptoPrimitives::hexEncode(std::string_view data)
{
    return CryptoCodecs::hexEncode(data);
}

std::string CryptoPrimitives::hexDecode(std::string_view data)
{
    return CryptoCodecs::hexDecode(data);
}

std::string CryptoPrimitives::uuidV4()
{
    unsigned char bytes[16];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1)
    {
        throw std::runtime_error("The UUID randomness could not be generated.");
    }

    // Version 4 in the high nibble of byte 6 and the RFC 4122 variant in the high bits of byte 8.
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);

    return CryptoCodecs::formatUuid(bytes);
}

std::string CryptoPrimitives::uuidV7()
{
    unsigned char bytes[16];
    if (RAND_bytes(bytes + 6, sizeof(bytes) - 6) != 1)
    {
        throw std::runtime_error("The UUID randomness could not be generated.");
    }

    // The first 48 bits hold a big-endian Unix millisecond timestamp so the ids sort by creation time.
    const std::uint64_t millis = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    bytes[0] = static_cast<unsigned char>((millis >> 40) & 0xFF);
    bytes[1] = static_cast<unsigned char>((millis >> 32) & 0xFF);
    bytes[2] = static_cast<unsigned char>((millis >> 24) & 0xFF);
    bytes[3] = static_cast<unsigned char>((millis >> 16) & 0xFF);
    bytes[4] = static_cast<unsigned char>((millis >> 8) & 0xFF);
    bytes[5] = static_cast<unsigned char>(millis & 0xFF);

    // Version 7 in the high nibble of byte 6 and the RFC 4122 variant in the high bits of byte 8.
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x70);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);

    return CryptoCodecs::formatUuid(bytes);
}

std::string CryptoPrimitives::pbkdf2(std::string_view password, std::string_view salt, std::size_t iterations, std::size_t keyLen, std::string_view algorithm)
{
    if (iterations == 0 || iterations > static_cast<std::size_t>(INT_MAX))
    {
        throw std::runtime_error("The PBKDF2 iteration count is out of range.");
    }

    if (keyLen == 0 || keyLen > static_cast<std::size_t>(INT_MAX))
    {
        throw std::runtime_error("The PBKDF2 key length is out of range.");
    }

    if (password.size() > static_cast<std::size_t>(INT_MAX) || salt.size() > static_cast<std::size_t>(INT_MAX))
    {
        throw std::runtime_error("The PBKDF2 input is too large.");
    }

    const std::string algo = trimAlgo(algorithm);
    const EVP_MD* md = OpenSslCryptoHelpers::fetchDigest(algo);

    std::string out(keyLen, '\0');
    if (PKCS5_PBKDF2_HMAC(password.data(),
                          static_cast<int>(password.size()),
                          reinterpret_cast<const unsigned char*>(salt.data()),
                          static_cast<int>(salt.size()),
                          static_cast<int>(iterations),
                          md,
                          static_cast<int>(keyLen),
                          reinterpret_cast<unsigned char*>(out.data())) != 1)
    {
        throw std::runtime_error("The PBKDF2 key could not be derived.");
    }

    return out;
}

std::string CryptoPrimitives::hkdf(std::string_view key, std::string_view salt, std::string_view info, std::size_t keyLen, std::string_view algorithm)
{
    if (keyLen == 0 || keyLen > static_cast<std::size_t>(INT_MAX))
    {
        throw std::runtime_error("The HKDF key length is out of range.");
    }

    const std::string algo = trimAlgo(algorithm);
    const EVP_MD* md = OpenSslCryptoHelpers::fetchDigest(algo);

    // The salt, the key and the info reach OpenSSL as `int` lengths, so an input past that bound is refused rather than truncated.
    if (key.size() > static_cast<std::size_t>(INT_MAX) || salt.size() > static_cast<std::size_t>(INT_MAX) || info.size() > static_cast<std::size_t>(INT_MAX))
    {
        throw std::runtime_error("The HKDF input is too large.");
    }

    const OpenSslHandles::KeyContext ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr), &EVP_PKEY_CTX_free);
    if (!ctx)
    {
        throw std::runtime_error("The HKDF context could not be created.");
    }

    std::string out(keyLen, '\0');
    std::size_t outLen = keyLen;
    bool ok = EVP_PKEY_derive_init(ctx.get()) == 1;
    ok = ok && EVP_PKEY_CTX_set_hkdf_md(ctx.get(), md) == 1;
    ok = ok && EVP_PKEY_CTX_set1_hkdf_salt(ctx.get(), reinterpret_cast<const unsigned char*>(salt.data()), static_cast<int>(salt.size())) == 1;
    ok = ok && EVP_PKEY_CTX_set1_hkdf_key(ctx.get(), reinterpret_cast<const unsigned char*>(key.data()), static_cast<int>(key.size())) == 1;
    ok = ok && EVP_PKEY_CTX_add1_hkdf_info(ctx.get(), reinterpret_cast<const unsigned char*>(info.data()), static_cast<int>(info.size())) == 1;
    ok = ok && EVP_PKEY_derive(ctx.get(), reinterpret_cast<unsigned char*>(out.data()), &outLen) == 1;

    if (!ok || outLen != keyLen)
    {
        throw std::runtime_error("The HKDF key could not be derived.");
    }

    return out;
}

std::string CryptoPrimitives::hashPassword(std::string_view password)
{
    // The scrypt cost parameters are chosen for an interactive login with N=2^15, r=8, p=1.
    const std::uint64_t costN = 1ULL << 15;
    const std::uint32_t blockR = 8;
    const std::uint32_t parallelP = 1;
    const std::size_t saltLen = 16;
    const std::size_t hashLen = 32;

    std::string salt(saltLen, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(salt.data()), static_cast<int>(saltLen)) != 1)
    {
        throw std::runtime_error("The password salt could not be generated.");
    }

    std::string hash(hashLen, '\0');
    if (EVP_PBE_scrypt(password.data(),
                       password.size(),
                       reinterpret_cast<const unsigned char*>(salt.data()),
                       saltLen,
                       costN,
                       blockR,
                       parallelP,
                       OpenSslCryptoHelpers::scryptMaxMem(costN, blockR),
                       reinterpret_cast<unsigned char*>(hash.data()),
                       hashLen) != 1)
    {
        throw std::runtime_error("The password hash could not be computed.");
    }

    // The self-describing format is `scrypt$N,r,p$base64Salt$base64Hash`, with URL-safe Base64 and no padding.
    std::ostringstream out;
    out << "scrypt$" << costN << ',' << blockR << ',' << parallelP << '$'
        << base64Encode(salt, true, false) << '$'
        << base64Encode(hash, true, false);

    return out.str();
}

bool CryptoPrimitives::verifyPassword(std::string_view password, std::string_view encoded)
{
    const std::string text(encoded);

    // Split the four dollar-delimited fields algorithm, parameters, salt, hash.
    const std::size_t p1 = text.find('$');
    const std::size_t p2 = p1 == std::string::npos ? p1 : text.find('$', p1 + 1);
    const std::size_t p3 = p2 == std::string::npos ? p2 : text.find('$', p2 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos || p3 == std::string::npos)
    {
        return false;
    }

    const std::string algo = text.substr(0, p1);
    const std::string paramStr = text.substr(p1 + 1, p2 - p1 - 1);
    const std::string saltStr = text.substr(p2 + 1, p3 - p2 - 1);
    const std::string hashStr = text.substr(p3 + 1);
    if (algo != "scrypt")
    {
        return false;
    }

    std::uint64_t costN = 0;
    std::uint64_t blockR = 0;
    std::uint64_t parallelP = 0;
    if (!OpenSslCryptoHelpers::parseScryptParams(paramStr, costN, blockR, parallelP))
    {
        return false;
    }

    if (!OpenSslCryptoHelpers::scryptParamsAllowed(costN, blockR, parallelP))
    {
        return false;
    }

    std::string salt;
    std::string expected;
    try
    {
        salt = base64Decode(saltStr);
        expected = base64Decode(hashStr);
    }
    catch (const std::exception&)
    {
        return false;
    }

    constexpr std::size_t kMaxPasswordHashBytes = 1024;
    if (expected.empty() || expected.size() > kMaxPasswordHashBytes)
    {
        return false;
    }

    std::string actual(expected.size(), '\0');
    if (EVP_PBE_scrypt(password.data(),
                       password.size(),
                       reinterpret_cast<const unsigned char*>(salt.data()),
                       salt.size(),
                       static_cast<std::uint64_t>(costN),
                       static_cast<std::uint32_t>(blockR),
                       static_cast<std::uint32_t>(parallelP),
                       OpenSslCryptoHelpers::scryptMaxMem(static_cast<std::uint64_t>(costN), static_cast<std::uint32_t>(blockR)),
                       reinterpret_cast<unsigned char*>(actual.data()),
                       actual.size()) != 1)
    {
        return false;
    }

    return OpenSslCryptoHelpers::constantTimeEqual(reinterpret_cast<const unsigned char*>(actual.data()),
                                                   reinterpret_cast<const unsigned char*>(expected.data()),
                                                   expected.size());
}

std::string CryptoPrimitives::aesGcmEncrypt(std::string_view key, std::string_view plaintext)
{
    const std::size_t ivLen = 12;
    const std::size_t tagLen = 16;

    if (key.size() != 32)
    {
        throw std::runtime_error("The encryption key must be 32 bytes.");
    }

    if (plaintext.size() > static_cast<std::size_t>(INT_MAX))
    {
        throw std::runtime_error("The plaintext is too large.");
    }

    unsigned char iv[12];
    if (RAND_bytes(iv, static_cast<int>(ivLen)) != 1)
    {
        throw std::runtime_error("The encryption IV could not be generated.");
    }

    const OpenSslHandles::CipherContext ctx(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!ctx)
    {
        throw std::runtime_error("The cipher context could not be created.");
    }

    std::string cipher(plaintext.size(), '\0');
    unsigned char tag[16];
    int len = 0;
    int cipherLen = 0;
    bool ok = EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1;
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(ivLen), nullptr) == 1;
    ok = ok && EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.data()), iv) == 1;
    if (ok && !plaintext.empty())
    {
        ok = EVP_EncryptUpdate(ctx.get(),
                               reinterpret_cast<unsigned char*>(cipher.data()),
                               &len,
                               reinterpret_cast<const unsigned char*>(plaintext.data()),
                               static_cast<int>(plaintext.size())) == 1;
        cipherLen = len;
    }

    ok = ok && EVP_EncryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(cipher.data()) + cipherLen, &len) == 1;
    cipherLen += len;
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(tagLen), tag) == 1;

    if (!ok)
    {
        throw std::runtime_error("The plaintext could not be encrypted.");
    }

    // The packed layout is `iv||tag||ciphertext`, so a single blob carries everything decrypt needs.
    std::string out;
    out.reserve(ivLen + tagLen + static_cast<std::size_t>(cipherLen));
    out.append(reinterpret_cast<const char*>(iv), ivLen);
    out.append(reinterpret_cast<const char*>(tag), tagLen);
    out.append(cipher.data(), static_cast<std::size_t>(cipherLen));

    return out;
}

std::string CryptoPrimitives::aesGcmDecrypt(std::string_view key, std::string_view blob)
{
    const std::size_t ivLen = 12;
    const std::size_t tagLen = 16;

    if (key.size() != 32)
    {
        throw std::runtime_error("The decryption key must be 32 bytes.");
    }

    if (blob.size() < ivLen + tagLen)
    {
        throw std::runtime_error("The encrypted blob is too short.");
    }

    const auto* raw = reinterpret_cast<const unsigned char*>(blob.data());
    const unsigned char* iv = raw;
    const unsigned char* tag = raw + ivLen;
    const unsigned char* cipher = raw + ivLen + tagLen;
    const std::size_t cipherLen = blob.size() - ivLen - tagLen;
    if (cipherLen > static_cast<std::size_t>(INT_MAX))
    {
        throw std::runtime_error("The encrypted blob is too large.");
    }

    const OpenSslHandles::CipherContext ctx(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!ctx)
    {
        throw std::runtime_error("The cipher context could not be created.");
    }

    std::string plain(cipherLen, '\0');
    int len = 0;
    int plainLen = 0;
    bool ok = EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1;
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(ivLen), nullptr) == 1;
    ok = ok && EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.data()), iv) == 1;
    if (ok && cipherLen > 0)
    {
        ok = EVP_DecryptUpdate(ctx.get(),
                               reinterpret_cast<unsigned char*>(plain.data()),
                               &len,
                               cipher,
                               static_cast<int>(cipherLen)) == 1;
        plainLen = len;
    }

    ok = ok && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(tagLen), const_cast<unsigned char*>(tag)) == 1;

    // A non-positive final result means the authentication tag did not verify so the blob is rejected.
    const bool verified = ok && EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(plain.data()) + plainLen, &len) > 0;
    plainLen += len;

    if (!verified)
    {
        throw std::runtime_error("The encrypted blob could not be authenticated.");
    }

    plain.resize(static_cast<std::size_t>(plainLen));
    return plain;
}

std::string CryptoPrimitives::rsaEncryptPublic(std::string_view pemPublicKey, std::string_view data)
{
    // The key reaches OpenSSL as an `int` length, so a longer one is refused rather than truncated.
    if (pemPublicKey.size() > static_cast<std::size_t>(INT_MAX))
    {
        throw std::runtime_error("The public key is too large.");
    }

    const OpenSslHandles::Bio bio(BIO_new_mem_buf(pemPublicKey.data(), static_cast<int>(pemPublicKey.size())), &BIO_free);
    if (!bio)
    {
        throw std::runtime_error("The public key buffer could not be read.");
    }

    const OpenSslHandles::Key key(PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr), &EVP_PKEY_free);
    if (!key)
    {
        throw std::runtime_error("The RSA public key could not be parsed.");
    }

    const OpenSslHandles::KeyContext ctx(EVP_PKEY_CTX_new(key.get(), nullptr), &EVP_PKEY_CTX_free);
    if (!ctx)
    {
        throw std::runtime_error("The RSA context could not be created.");
    }

    // OAEP padding with OpenSSL's default SHA-1 digest is what MySQL `caching_sha2` full auth expects.
    if (EVP_PKEY_encrypt_init(ctx.get()) <= 0 || EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) <= 0)
    {
        throw std::runtime_error("The RSA encryption could not be initialized.");
    }

    const auto* input = reinterpret_cast<const unsigned char*>(data.data());
    std::size_t outLen = 0;
    if (EVP_PKEY_encrypt(ctx.get(), nullptr, &outLen, input, data.size()) <= 0)
    {
        throw std::runtime_error("The RSA output size could not be determined.");
    }

    std::string out(outLen, '\0');
    if (EVP_PKEY_encrypt(ctx.get(), reinterpret_cast<unsigned char*>(out.data()), &outLen, input, data.size()) <= 0)
    {
        throw std::runtime_error("The RSA encryption failed.");
    }

    out.resize(outLen);
    return out;
}

} // namespace varn::crypto
