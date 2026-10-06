#include "varn/http/HttpTypes.h"

#include <fstream>
#include <string>
#include <utility>

namespace varn::http
{

void HttpResponse::sendFile(const std::string& path, std::uint64_t start, std::uint64_t length, bool headersOnly)
{
    if (headersOnly)
    {
        end("");
        return;
    }

    // The buffering fallback reads the requested range up front, used only by transports that cannot stream.
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
    end(std::move(body));
}

// Answers whether a close frame may carry the code, which leaves out the codes the protocol reserves for a local report or never assigned.
bool WebSocketConnection::validCloseCode(int code)
{
    return (code >= 1000 && code <= 1003) || (code >= 1007 && code <= 1014) || (code >= 3000 && code <= 4999);
}

} // namespace varn::http
