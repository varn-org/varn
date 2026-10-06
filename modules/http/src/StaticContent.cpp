#include "StaticContent.h"

#include "HttpRange.h"
#include "HttpText.h"
#include "HttpUrl.h"
#include "varn/http/MimeTypes.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>

namespace varn::http
{

bool StaticContent::isHiddenComponent(const std::string& name)
{
    return !name.empty() && name.front() == '.' && name != "." && name != ".." && name != ".well-known";
}

std::string StaticContent::httpDate(std::filesystem::file_time_type fileTime)
{
    // Map the file clock onto the system clock, since not every standard library offers `clock_cast`.
    const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    const std::time_t seconds = std::chrono::system_clock::to_time_t(systemTime);

    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &seconds);
#else
    gmtime_r(&seconds, &tm);
#endif

    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &tm);
    return std::string(buffer);
}

std::string StaticContent::headerValue(const HttpRequest& request, const std::string& name)
{
    for (const auto& [key, value] : request.headers)
    {
        if (HttpText::iequals(key, name))
        {
            return value;
        }
    }

    return "";
}

std::string StaticContent::htmlEscape(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (char c : value)
    {
        switch (c)
        {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&#39;";
            break;
        default:
            out += c;
        }
    }

    return out;
}

std::string StaticContent::makeEtag(std::uintmax_t size, long long writeTime)
{
    std::ostringstream tag;
    tag << '"' << std::hex << size << '-' << writeTime << '"';
    return tag.str();
}

bool StaticContent::serveFile(const HttpRequest& request, HttpResponse& response, const std::filesystem::path& path)
{
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec)
    {
        response.setStatus(500);
        response.end("The file could not be read.");
        return true;
    }

    const auto fileTime = std::filesystem::last_write_time(path, ec);
    const long long writeTime = static_cast<long long>(fileTime.time_since_epoch().count());
    const std::string etag = makeEtag(size, writeTime);
    const std::string lastModified = httpDate(fileTime);

    response.setHeader("Cache-Control", "public, max-age=3600");
    response.setHeader("ETag", etag);
    response.setHeader("Last-Modified", lastModified);
    response.setHeader("Accept-Ranges", "bytes");
    // Static responses bypass the middleware chain, so set the sniffing guard here.
    response.setHeader("X-Content-Type-Options", "nosniff");

    // A matching validator means the client already holds this exact file, so send no representation, with the entity tag taking precedence over the modification date per RFC 9110.
    if (headerValue(request, "If-None-Match") == etag ||
        (headerValue(request, "If-None-Match").empty() && headerValue(request, "If-Modified-Since") == lastModified))
    {
        response.setStatus(304);
        response.end("");
        return true;
    }

    response.setHeader("Content-Type", MimeTypes::forPath(path.string()));

    // Only one byte range is served, so a header of several ranges or one the server cannot read is ignored and the whole file is sent, as RFC 9110 allows, while a range that misses the file answers 416.
    const std::string range = headerValue(request, "Range");
    if (HttpRange::isByteRange(range))
    {
        std::uintmax_t start = 0;
        std::uintmax_t end = 0;
        if (!HttpRange::parse(range, size, start, end))
        {
            response.setStatus(416);
            response.setHeader("Content-Range", "bytes */" + std::to_string(size));
            response.end("");
            return true;
        }

        response.setStatus(206);
        response.setHeader("Content-Range",
                           "bytes " + std::to_string(start) + "-" + std::to_string(end) + "/" + std::to_string(size));
        response.sendFile(path.string(), start, end - start + 1);
        return true;
    }

    response.setStatus(200);
    response.sendFile(path.string(), 0, size);
    return true;
}

void StaticContent::serveListing(HttpResponse& response, const std::filesystem::path& root, const std::filesystem::path& dir)
{
    std::error_code ec;
    const std::string relative = std::filesystem::relative(dir, root, ec).generic_string();
    const std::string title = ec || relative.empty() || relative == "." ? "/" : "/" + relative;

    std::ostringstream html;
    html << "<!doctype html><html><head><meta charset=\"utf-8\"><title>Index of " << htmlEscape(title)
         << "</title></head><body><h1>Index of " << htmlEscape(title) << "</h1><ul>";

    // Bound the listing so an enormous directory cannot stall the loop or balloon the response.
    // The iterator advances without throwing, so a folder that fails to read while it is listed answers 500 instead of leaving the request without an answer.
    constexpr std::size_t kMaxListingEntries = 10000;
    std::size_t listed = 0;
    bool truncated = false;
    std::error_code scan;
    for (std::filesystem::directory_iterator it(dir, scan), end; !scan && it != end; it.increment(scan))
    {
        const std::filesystem::directory_entry& entry = *it;
        const std::string name = entry.path().filename().string();
        // Keep hidden entries out of the listing for the same reason they are not served.
        if (isHiddenComponent(name))
        {
            continue;
        }

        if (listed >= kMaxListingEntries)
        {
            truncated = true;
            break;
        }
        ++listed;

        const std::string suffix = entry.is_directory(ec) ? "/" : "";
        // The visible label is HTML-escaped while the `href` is URL-encoded to survive special characters.
        const std::string label = htmlEscape(name + suffix);
        const std::string href = htmlEscape(HttpUrl::encode(name) + suffix);
        html << "<li><a href=\"" << href << "\">" << label << "</a></li>";
    }

    if (scan)
    {
        response.setStatus(500);
        response.setHeader("Content-Type", "text/plain; charset=utf-8");
        response.end("The folder could not be listed.");
        return;
    }

    html << "</ul>";
    if (truncated)
    {
        html << "<p>Listing truncated at " << kMaxListingEntries << " entries.</p>";
    }
    html << "</body></html>";

    response.setStatus(200);
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    response.setHeader("X-Content-Type-Options", "nosniff");
    response.end(html.str());
}

} // namespace varn::http
