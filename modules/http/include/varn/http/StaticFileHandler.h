#pragma once

#include "varn/http/HttpTypes.h"

#include <string>

namespace varn::http
{

class StaticFileHandler
{
public:
    StaticFileHandler(std::string publicDir, bool directoryListing);
    bool tryServe(const HttpRequest& request, HttpResponse& response) const;

private:
    std::string publicDir;
    bool directoryListing;
};

} // namespace varn::http
