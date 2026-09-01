#include "Sha.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace varn::crypto::portable
{

namespace
{
const std::uint32_t kSha256K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

const std::uint64_t kSha512K[80] = {
    0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full, 0xe9b5dba58189dbbcull,
    0x3956c25bf348b538ull, 0x59f111f1b605d019ull, 0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull,
    0xd807aa98a3030242ull, 0x12835b0145706fbeull, 0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull,
    0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull, 0xc19bf174cf692694ull,
    0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull, 0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull,
    0x2de92c6f592b0275ull, 0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull,
    0x983e5152ee66dfabull, 0xa831c66d2db43210ull, 0xb00327c898fb213full, 0xbf597fc7beef0ee4ull,
    0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull, 0x06ca6351e003826full, 0x142929670a0e6e70ull,
    0x27b70a8546d22ffcull, 0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull, 0x53380d139d95b3dfull,
    0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull, 0x92722c851482353bull,
    0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull, 0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull,
    0xd192e819d6ef5218ull, 0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull,
    0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull, 0x2748774cdf8eeb99ull, 0x34b0bcb5e19b48a8ull,
    0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull, 0x5b9cca4f7763e373ull, 0x682e6ff3d6b2b8a3ull,
    0x748f82ee5defb2fcull, 0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
    0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull, 0xc67178f2e372532bull,
    0xca273eceea26619cull, 0xd186b8c721c0c207ull, 0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull,
    0x06f067aa72176fbaull, 0x0a637dc5a2c898a6ull, 0x113f9804bef90daeull, 0x1b710b35131c471bull,
    0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull, 0x431d67c49c100d4cull,
    0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull, 0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull};

class ShaOps
{
public:
    static std::uint32_t rotr32(std::uint32_t x, int n)
    {
        return (x >> n) | (x << (32 - n));
    }

    static std::uint32_t rotl32(std::uint32_t x, int n)
    {
        return (x << n) | (x >> (32 - n));
    }

    static std::uint64_t rotr64(std::uint64_t x, int n)
    {
        return (x >> n) | (x << (64 - n));
    }

    static void appendBigEndian32(std::string& out, std::uint32_t value)
    {
        out.push_back(static_cast<char>((value >> 24) & 0xFF));
        out.push_back(static_cast<char>((value >> 16) & 0xFF));
        out.push_back(static_cast<char>((value >> 8) & 0xFF));
        out.push_back(static_cast<char>(value & 0xFF));
    }

    static void appendBigEndian64(std::string& out, std::uint64_t value)
    {
        for (int shift = 56; shift >= 0; shift -= 8)
        {
            out.push_back(static_cast<char>((value >> shift) & 0xFF));
        }
    }

    // the sha-1 and sha-256 families pad to a multiple of 64 bytes with a 64-bit big-endian bit length
    static std::string pad64(std::string_view data)
    {
        std::string message(data);
        const std::uint64_t bitLength = static_cast<std::uint64_t>(data.size()) * 8;

        message.push_back(static_cast<char>(0x80));
        while (message.size() % 64 != 56)
        {
            message.push_back('\0');
        }
        appendBigEndian64(message, bitLength);

        return message;
    }

    // the sha-512 family pads to a multiple of 128 bytes with a 128-bit big-endian bit length
    static std::string pad128(std::string_view data)
    {
        std::string message(data);
        const std::uint64_t bitLength = static_cast<std::uint64_t>(data.size()) * 8;

        message.push_back(static_cast<char>(0x80));
        while (message.size() % 128 != 112)
        {
            message.push_back('\0');
        }
        appendBigEndian64(message, 0);
        appendBigEndian64(message, bitLength);

        return message;
    }

    static std::string sha1Impl(std::string_view data)
    {
        std::uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};

        const std::string message = pad64(data);
        const auto* bytes = reinterpret_cast<const unsigned char*>(message.data());

        for (std::size_t offset = 0; offset < message.size(); offset += 64)
        {
            std::uint32_t w[80];
            for (int t = 0; t < 16; ++t)
            {
                const std::size_t i = offset + static_cast<std::size_t>(t) * 4;
                w[t] = (static_cast<std::uint32_t>(bytes[i]) << 24) | (static_cast<std::uint32_t>(bytes[i + 1]) << 16) | (static_cast<std::uint32_t>(bytes[i + 2]) << 8) | static_cast<std::uint32_t>(bytes[i + 3]);
            }
            for (int t = 16; t < 80; ++t)
            {
                w[t] = rotl32(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
            }

            std::uint32_t a = h[0];
            std::uint32_t b = h[1];
            std::uint32_t c = h[2];
            std::uint32_t d = h[3];
            std::uint32_t e = h[4];

            for (int t = 0; t < 80; ++t)
            {
                std::uint32_t f = 0;
                std::uint32_t k = 0;
                if (t < 20)
                {
                    f = (b & c) | (~b & d);
                    k = 0x5A827999u;
                }
                else if (t < 40)
                {
                    f = b ^ c ^ d;
                    k = 0x6ED9EBA1u;
                }
                else if (t < 60)
                {
                    f = (b & c) | (b & d) | (c & d);
                    k = 0x8F1BBCDCu;
                }
                else
                {
                    f = b ^ c ^ d;
                    k = 0xCA62C1D6u;
                }

                const std::uint32_t temp = rotl32(a, 5) + f + e + k + w[t];
                e = d;
                d = c;
                c = rotl32(b, 30);
                b = a;
                a = temp;
            }

            h[0] += a;
            h[1] += b;
            h[2] += c;
            h[3] += d;
            h[4] += e;
        }

        std::string out;
        for (std::uint32_t word : h)
        {
            appendBigEndian32(out, word);
        }

        return out;
    }

    static std::string sha256Core(std::string_view data, const std::uint32_t initial[8], std::size_t outputBytes)
    {
        std::uint32_t h[8];
        for (int i = 0; i < 8; ++i)
        {
            h[i] = initial[i];
        }

        const std::string message = pad64(data);
        const auto* bytes = reinterpret_cast<const unsigned char*>(message.data());

        for (std::size_t offset = 0; offset < message.size(); offset += 64)
        {
            std::uint32_t w[64];
            for (int t = 0; t < 16; ++t)
            {
                const std::size_t i = offset + static_cast<std::size_t>(t) * 4;
                w[t] = (static_cast<std::uint32_t>(bytes[i]) << 24) | (static_cast<std::uint32_t>(bytes[i + 1]) << 16) | (static_cast<std::uint32_t>(bytes[i + 2]) << 8) | static_cast<std::uint32_t>(bytes[i + 3]);
            }
            for (int t = 16; t < 64; ++t)
            {
                const std::uint32_t s0 = rotr32(w[t - 15], 7) ^ rotr32(w[t - 15], 18) ^ (w[t - 15] >> 3);
                const std::uint32_t s1 = rotr32(w[t - 2], 17) ^ rotr32(w[t - 2], 19) ^ (w[t - 2] >> 10);
                w[t] = w[t - 16] + s0 + w[t - 7] + s1;
            }

            std::uint32_t a = h[0];
            std::uint32_t b = h[1];
            std::uint32_t c = h[2];
            std::uint32_t d = h[3];
            std::uint32_t e = h[4];
            std::uint32_t f = h[5];
            std::uint32_t g = h[6];
            std::uint32_t hh = h[7];

            for (int t = 0; t < 64; ++t)
            {
                const std::uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
                const std::uint32_t ch = (e & f) ^ (~e & g);
                const std::uint32_t temp1 = hh + S1 + ch + kSha256K[t] + w[t];
                const std::uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
                const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                const std::uint32_t temp2 = S0 + maj;

                hh = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }

            h[0] += a;
            h[1] += b;
            h[2] += c;
            h[3] += d;
            h[4] += e;
            h[5] += f;
            h[6] += g;
            h[7] += hh;
        }

        std::string full;
        for (std::uint32_t word : h)
        {
            appendBigEndian32(full, word);
        }

        return full.substr(0, outputBytes);
    }

    static std::string sha512Core(std::string_view data, const std::uint64_t initial[8], std::size_t outputBytes)
    {
        std::uint64_t h[8];
        for (int i = 0; i < 8; ++i)
        {
            h[i] = initial[i];
        }

        const std::string message = pad128(data);
        const auto* bytes = reinterpret_cast<const unsigned char*>(message.data());

        for (std::size_t offset = 0; offset < message.size(); offset += 128)
        {
            std::uint64_t w[80];
            for (int t = 0; t < 16; ++t)
            {
                const std::size_t i = offset + static_cast<std::size_t>(t) * 8;
                std::uint64_t value = 0;
                for (int b = 0; b < 8; ++b)
                {
                    value = (value << 8) | static_cast<std::uint64_t>(bytes[i + b]);
                }
                w[t] = value;
            }
            for (int t = 16; t < 80; ++t)
            {
                const std::uint64_t s0 = rotr64(w[t - 15], 1) ^ rotr64(w[t - 15], 8) ^ (w[t - 15] >> 7);
                const std::uint64_t s1 = rotr64(w[t - 2], 19) ^ rotr64(w[t - 2], 61) ^ (w[t - 2] >> 6);
                w[t] = w[t - 16] + s0 + w[t - 7] + s1;
            }

            std::uint64_t a = h[0];
            std::uint64_t b = h[1];
            std::uint64_t c = h[2];
            std::uint64_t d = h[3];
            std::uint64_t e = h[4];
            std::uint64_t f = h[5];
            std::uint64_t g = h[6];
            std::uint64_t hh = h[7];

            for (int t = 0; t < 80; ++t)
            {
                const std::uint64_t S1 = rotr64(e, 14) ^ rotr64(e, 18) ^ rotr64(e, 41);
                const std::uint64_t ch = (e & f) ^ (~e & g);
                const std::uint64_t temp1 = hh + S1 + ch + kSha512K[t] + w[t];
                const std::uint64_t S0 = rotr64(a, 28) ^ rotr64(a, 34) ^ rotr64(a, 39);
                const std::uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
                const std::uint64_t temp2 = S0 + maj;

                hh = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }

            h[0] += a;
            h[1] += b;
            h[2] += c;
            h[3] += d;
            h[4] += e;
            h[5] += f;
            h[6] += g;
            h[7] += hh;
        }

        std::string full;
        for (std::uint64_t word : h)
        {
            appendBigEndian64(full, word);
        }

        return full.substr(0, outputBytes);
    }

    static std::string normalize(std::string_view algorithm)
    {
        std::string out;
        for (char c : algorithm)
        {
            if (c >= 'a' && c <= 'z')
            {
                out.push_back(static_cast<char>(c - 'a' + 'A'));
            }
            else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            {
                out.push_back(c);
            }
        }

        return out;
    }
};
} // namespace

std::string Sha::sha1(std::string_view data)
{
    return ShaOps::sha1Impl(data);
}

std::string Sha::sha224(std::string_view data)
{
    static const std::uint32_t iv[8] = {0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939, 0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4};
    return ShaOps::sha256Core(data, iv, 28);
}

std::string Sha::sha256(std::string_view data)
{
    static const std::uint32_t iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    return ShaOps::sha256Core(data, iv, 32);
}

std::string Sha::sha384(std::string_view data)
{
    static const std::uint64_t iv[8] = {0xcbbb9d5dc1059ed8ull, 0x629a292a367cd507ull, 0x9159015a3070dd17ull, 0x152fecd8f70e5939ull, 0x67332667ffc00b31ull, 0x8eb44a8768581511ull, 0xdb0c2e0d64f98fa7ull, 0x47b5481dbefa4fa4ull};
    return ShaOps::sha512Core(data, iv, 48);
}

std::string Sha::sha512(std::string_view data)
{
    static const std::uint64_t iv[8] = {0x6a09e667f3bcc908ull, 0xbb67ae8584caa73bull, 0x3c6ef372fe94f82bull, 0xa54ff53a5f1d36f1ull, 0x510e527fade682d1ull, 0x9b05688c2b3e6c1full, 0x1f83d9abfb41bd6bull, 0x5be0cd19137e2179ull};
    return ShaOps::sha512Core(data, iv, 64);
}

std::size_t Sha::blockSize(std::string_view algorithm)
{
    const std::string name = ShaOps::normalize(algorithm);
    if (name == "SHA384" || name == "SHA512")
    {
        return 128;
    }

    return 64;
}

bool Sha::isSupported(std::string_view algorithm)
{
    const std::string name = ShaOps::normalize(algorithm);
    return name == "SHA1" || name == "SHA224" || name == "SHA256" || name == "SHA384" || name == "SHA512";
}

std::string Sha::hashByName(std::string_view algorithm, std::string_view data)
{
    const std::string name = ShaOps::normalize(algorithm);
    if (name == "SHA1")
    {
        return sha1(data);
    }
    if (name == "SHA224")
    {
        return sha224(data);
    }
    if (name == "SHA256")
    {
        return sha256(data);
    }
    if (name == "SHA384")
    {
        return sha384(data);
    }
    if (name == "SHA512")
    {
        return sha512(data);
    }

    throw std::runtime_error("[Sha] The hash algorithm is not supported.");
}

} // namespace varn::crypto::portable
