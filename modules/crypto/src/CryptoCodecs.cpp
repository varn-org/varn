#include "varn/crypto/CryptoCodecs.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace varn::crypto
{

namespace
{
// fixed base64 alphabets indexed directly during encode without any per-character branching
constexpr char kBase64Std[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr char kBase64Url[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// reverse lookup tables map each input byte to its 6-bit value or to a sentinel for non-alphabet bytes
constexpr signed char kSkip = -1;
constexpr signed char kPad = -2;

class CryptoCodecsHelpers
{
public:
    static std::array<signed char, 256> makeBase64Reverse()
    {
        std::array<signed char, 256> table{};
        for (int i = 0; i < 256; ++i)
        {
            table[static_cast<std::size_t>(i)] = kSkip;
        }

        // accept both alphabets on decode so a standard or url-safe string is read interchangeably
        table[static_cast<unsigned char>('+')] = 62;
        table[static_cast<unsigned char>('/')] = 63;
        table[static_cast<unsigned char>('-')] = 62;
        table[static_cast<unsigned char>('_')] = 63;
        table[static_cast<unsigned char>('=')] = kPad;

        for (int i = 0; i < 26; ++i)
        {
            table[static_cast<unsigned char>('A' + i)] = static_cast<signed char>(i);
            table[static_cast<unsigned char>('a' + i)] = static_cast<signed char>(i + 26);
        }

        for (int i = 0; i < 10; ++i)
        {
            table[static_cast<unsigned char>('0' + i)] = static_cast<signed char>(i + 52);
        }

        return table;
    }

    static const std::array<signed char, 256>& base64Reverse()
    {
        static const std::array<signed char, 256> table = makeBase64Reverse();
        return table;
    }

    static int hexNibble(unsigned char c)
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f')
        {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F')
        {
            return c - 'A' + 10;
        }

        return -1;
    }
};
} // namespace

std::string CryptoCodecs::base64Encode(std::string_view data, bool urlSafe, bool padding)
{
    const char* alphabet = urlSafe ? kBase64Url : kBase64Std;
    const std::size_t fullGroups = data.size() / 3;
    const std::size_t remainder = data.size() % 3;

    std::string out;
    // reserve the exact output size up front so the tight loop never reallocates
    const std::size_t tailChars = remainder == 0 ? 0 : (padding ? 4 : remainder + 1);
    out.resize(fullGroups * 4 + tailChars);

    const auto* in = reinterpret_cast<const unsigned char*>(data.data());
    std::size_t si = 0;
    std::size_t di = 0;
    for (std::size_t g = 0; g < fullGroups; ++g)
    {
        const std::uint32_t triple = (static_cast<std::uint32_t>(in[si]) << 16) | (static_cast<std::uint32_t>(in[si + 1]) << 8) | static_cast<std::uint32_t>(in[si + 2]);
        out[di] = alphabet[(triple >> 18) & 0x3F];
        out[di + 1] = alphabet[(triple >> 12) & 0x3F];
        out[di + 2] = alphabet[(triple >> 6) & 0x3F];
        out[di + 3] = alphabet[triple & 0x3F];
        si += 3;
        di += 4;
    }

    if (remainder == 1)
    {
        const std::uint32_t triple = static_cast<std::uint32_t>(in[si]) << 16;
        out[di] = alphabet[(triple >> 18) & 0x3F];
        out[di + 1] = alphabet[(triple >> 12) & 0x3F];
        if (padding)
        {
            out[di + 2] = '=';
            out[di + 3] = '=';
        }
    }
    else if (remainder == 2)
    {
        const std::uint32_t triple = (static_cast<std::uint32_t>(in[si]) << 16) | (static_cast<std::uint32_t>(in[si + 1]) << 8);
        out[di] = alphabet[(triple >> 18) & 0x3F];
        out[di + 1] = alphabet[(triple >> 12) & 0x3F];
        out[di + 2] = alphabet[(triple >> 6) & 0x3F];
        if (padding)
        {
            out[di + 3] = '=';
        }
    }

    return out;
}

std::string CryptoCodecs::base64Decode(std::string_view data)
{
    const std::array<signed char, 256>& rev = CryptoCodecsHelpers::base64Reverse();

    std::string out;
    // four input characters become three output bytes so this never over-reserves by more than two bytes
    out.reserve((data.size() / 4) * 3 + 3);

    std::uint32_t accum = 0;
    int bits = 0;
    bool sawPad = false;
    for (unsigned char c : data)
    {
        const signed char v = rev[c];
        if (v == kSkip)
        {
            throw std::runtime_error("[CryptoPrimitives] The base64 input contains an invalid character.");
        }

        if (v == kPad)
        {
            sawPad = true;
            continue;
        }

        // any alphabet character after padding has begun is a malformed stream
        if (sawPad)
        {
            throw std::runtime_error("[CryptoPrimitives] The base64 input has data after padding.");
        }

        accum = (accum << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(static_cast<char>((accum >> bits) & 0xFF));
        }
    }

    // a valid stream leaves at most the discarded high bits of a partial group never a full unused byte
    if (bits >= 6)
    {
        throw std::runtime_error("[CryptoPrimitives] The base64 input has an invalid length.");
    }

    return out;
}

std::string CryptoCodecs::hexEncode(std::string_view data)
{
    static const char* digits = "0123456789abcdef";

    std::string out;
    out.resize(data.size() * 2);

    const auto* in = reinterpret_cast<const unsigned char*>(data.data());
    std::size_t di = 0;
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        out[di] = digits[in[i] >> 4];
        out[di + 1] = digits[in[i] & 0x0F];
        di += 2;
    }

    return out;
}

std::string CryptoCodecs::hexDecode(std::string_view data)
{
    if (data.size() % 2 != 0)
    {
        throw std::runtime_error("[CryptoPrimitives] The hex input must have an even length.");
    }

    std::string out;
    out.resize(data.size() / 2);

    const auto* in = reinterpret_cast<const unsigned char*>(data.data());
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        const int hi = CryptoCodecsHelpers::hexNibble(in[i * 2]);
        const int lo = CryptoCodecsHelpers::hexNibble(in[i * 2 + 1]);
        if (hi < 0 || lo < 0)
        {
            throw std::runtime_error("[CryptoPrimitives] The hex input contains an invalid character.");
        }

        out[i] = static_cast<char>((hi << 4) | lo);
    }

    return out;
}

std::string CryptoCodecs::formatUuid(const unsigned char bytes[16])
{
    static const char* digits = "0123456789abcdef";

    std::string out;
    out.resize(36);

    std::size_t di = 0;
    for (std::size_t i = 0; i < 16; ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
        {
            out[di++] = '-';
        }

        out[di++] = digits[bytes[i] >> 4];
        out[di++] = digits[bytes[i] & 0x0F];
    }

    return out;
}

} // namespace varn::crypto
