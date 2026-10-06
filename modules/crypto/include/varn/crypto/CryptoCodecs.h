#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace varn::crypto
{

class CryptoCodecs
{
public:
    CryptoCodecs() = delete;

    static std::string base64Encode(std::string_view data, bool urlSafe, bool padding);
    static std::string base64Decode(std::string_view data);
    static std::string hexEncode(std::string_view data);
    static std::string hexDecode(std::string_view data);
    static std::string formatUuid(const unsigned char bytes[16]);
};

} // namespace varn::crypto
