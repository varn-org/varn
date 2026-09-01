#pragma once

#include <string>

namespace varn::http
{

class MimeTypes
{
public:
    MimeTypes() = delete;

    static std::string forPath(const std::string& path);
};

} // namespace varn::http
