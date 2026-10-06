#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace varn::crypto::portable
{

class Sha
{
public:
    Sha() = delete;

    static std::string sha1(std::string_view data);
    static std::string sha224(std::string_view data);
    static std::string sha256(std::string_view data);
    static std::string sha384(std::string_view data);
    static std::string sha512(std::string_view data);
    static std::size_t blockSize(std::string_view algorithm);
    static bool isSupported(std::string_view algorithm);
    static std::string hashByName(std::string_view algorithm, std::string_view data);
};

} // namespace varn::crypto::portable
