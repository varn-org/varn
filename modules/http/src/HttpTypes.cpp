#include "varn/http/HttpTypes.h"

#include <fstream>
#include <string>

namespace varn::http
{

void HttpResponse::sendFile(const std::string& path, std::uint64_t start, std::uint64_t length, bool headersOnly)
{
    if (headersOnly)
    {
        end("");
        return;
    }

    // the buffering fallback reads the requested range up front, used only by transports that cannot stream
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        end("");
        return;
    }

    std::string body(static_cast<std::size_t>(length), '\0');
    file.seekg(static_cast<std::streamoff>(start));
    file.read(body.data(), static_cast<std::streamsize>(length));
    body.resize(static_cast<std::size_t>(file.gcount()));
    end(body);
}

void HttpResponse::beginChunked()
{
}

void HttpResponse::writeChunk(const std::string& chunk)
{
    write(chunk);
}

void HttpResponse::endChunked()
{
    end("");
}

} // namespace varn::http
