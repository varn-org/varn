#pragma once

#include <string>

namespace varn::tls
{

class CaBundle
{
public:
    static const std::string& resolve();
    static std::string describeSearch();
};

} // namespace varn::tls
