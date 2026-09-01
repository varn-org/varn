#include "varn/http/StaticFileHandler.h"

#include "StaticContent.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace varn::http
{

StaticFileHandler::StaticFileHandler(std::string publicDir, bool directoryListing)
    : publicDir(std::move(publicDir))
    , directoryListing(directoryListing)
{
}

bool StaticFileHandler::tryServe(const HttpRequest& request, HttpResponse& response) const
{
    if (request.method != "GET" && request.method != "HEAD")
    {
        return false;
    }

    const std::string requestPath = request.path.empty() ? "/" : request.path;

    // reject control characters and embedded null bytes that could truncate the resolved path
    for (char c : requestPath)
    {
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f)
        {
            response.setStatus(400);
            response.end("Bad Request");
            return true;
        }
    }

    // resolve without throwing so a path that triggers a filesystem error such as a symlink loop is treated as absent instead of abandoning the connection
    std::error_code ec;
    std::filesystem::path root = std::filesystem::weakly_canonical(publicDir, ec);
    if (ec)
    {
        return false;
    }

    std::filesystem::path candidate = std::filesystem::weakly_canonical(root / requestPath.substr(1), ec);
    if (ec)
    {
        return false;
    }

    // keep the resolved path inside the public directory tree, rejecting siblings and traversal
    const std::filesystem::path relative = candidate.lexically_relative(root);
    if (relative.empty() || *relative.begin() == "..")
    {
        response.setStatus(403);
        response.end("Forbidden");
        return true;
    }

    // hidden files such as .env or .git are never exposed, so treat them as absent
    for (const auto& component : relative)
    {
        if (StaticContent::isHiddenComponent(component.string()))
        {
            return false;
        }
    }

    if (std::filesystem::is_directory(candidate, ec))
    {
        // resolve the index through the same canonicalization so a symlinked index.html cannot escape the tree
        std::filesystem::path index = std::filesystem::weakly_canonical(candidate / "index.html", ec);
        const std::filesystem::path indexRelative = ec ? std::filesystem::path() : index.lexically_relative(root);
        bool indexInside = !indexRelative.empty() && *indexRelative.begin() != "..";
        for (const auto& component : indexRelative)
        {
            if (StaticContent::isHiddenComponent(component.string()))
            {
                indexInside = false;
                break;
            }
        }

        if (!ec && indexInside && std::filesystem::is_regular_file(index, ec))
        {
            candidate = index;
        }
        else if (directoryListing)
        {
            StaticContent::serveListing(response, root, candidate);
            return true;
        }
        else
        {
            return false;
        }
    }

    if (!std::filesystem::is_regular_file(candidate, ec))
    {
        return false;
    }

    return StaticContent::serveFile(request, response, candidate);
}

} // namespace varn::http
