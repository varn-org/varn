#include "varn/http/MimeTypes.h"

#include <algorithm>
#include <filesystem>
#include <map>

namespace varn::http
{

std::string MimeTypes::forPath(const std::string& path)
{
    static const std::map<std::string, std::string> types = {
        {".html", "text/html; charset=utf-8"},
        {".htm", "text/html; charset=utf-8"},
        {".css", "text/css; charset=utf-8"},
        {".js", "application/javascript; charset=utf-8"},
        {".mjs", "application/javascript; charset=utf-8"},
        {".json", "application/json; charset=utf-8"},
        {".txt", "text/plain; charset=utf-8"},
        {".xml", "application/xml; charset=utf-8"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".webp", "image/webp"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".pdf", "application/pdf"},
        {".zip", "application/zip"},
        {".wasm", "application/wasm"},
        {".mp3", "audio/mpeg"},
        {".mp4", "video/mp4"},
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf", "font/ttf"}};

    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });

    auto it = types.find(ext);
    if (it != types.end())
    {
        return it->second;
    }

    return "application/octet-stream";
}

} // namespace varn::http
