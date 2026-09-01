#include "HttpMultipart.h"

#include "HttpText.h"

#include <lua.hpp>

#include <string>

namespace varn::http
{

std::string HttpMultipart::extractBoundary(const std::string& contentType)
{
    // the parameter name is case-insensitive per rfc 2045, while the boundary value keeps its original case
    const std::size_t pos = HttpText::toLower(contentType).find("boundary=");
    if (pos == std::string::npos)
    {
        return "";
    }

    std::string boundary = contentType.substr(pos + 9);
    if (!boundary.empty() && boundary.front() == '"')
    {
        const std::size_t end = boundary.find('"', 1);
        return end == std::string::npos ? boundary.substr(1) : boundary.substr(1, end - 1);
    }

    const std::size_t semicolon = boundary.find(';');
    if (semicolon != std::string::npos)
    {
        boundary = boundary.substr(0, semicolon);
    }
    while (!boundary.empty() && (boundary.back() == ' ' || boundary.back() == '\r' || boundary.back() == '\n'))
    {
        boundary.pop_back();
    }

    return boundary;
}

std::string HttpMultipart::multipartAttribute(const std::string& headers, const std::string& key)
{
    std::size_t pos = 0;
    while ((pos = headers.find(key, pos)) != std::string::npos)
    {
        // the attribute is only accepted when it starts at a real boundary, which avoids matching inside another token such as filename= or a custom x-name=
        const char previous = pos == 0 ? ' ' : headers[pos - 1];
        if (pos == 0 || previous == ' ' || previous == ';')
        {
            const std::size_t start = pos + key.size();
            const std::size_t end = headers.find('"', start);
            return end == std::string::npos ? "" : headers.substr(start, end - start);
        }

        pos += key.size();
    }

    return "";
}

std::string HttpMultipart::multipartContentType(const std::string& headers)
{
    const std::size_t pos = HttpText::toLower(headers).find("content-type:");
    if (pos == std::string::npos)
    {
        return "";
    }

    const std::size_t start = pos + std::string("content-type:").size();
    const std::size_t end = headers.find("\r\n", start);
    std::string value = headers.substr(start, end == std::string::npos ? std::string::npos : end - start);

    const std::size_t firstReal = value.find_first_not_of(" \t");
    return firstReal == std::string::npos ? "" : value.substr(firstReal);
}

void HttpMultipart::pushMultipart(lua_State* L, const std::string& body, const std::string& boundary)
{
    lua_newtable(L);
    lua_newtable(L);
    lua_setfield(L, -2, "fields");
    lua_newtable(L);
    lua_setfield(L, -2, "files");

    const std::string delimiter = "--" + boundary;
    int fileIndex = 0;

    // bound the number of parts so a body full of boundaries cannot drive unbounded parse work
    constexpr int maxParts = 1000;
    int parts = 0;

    std::size_t pos = body.find(delimiter);
    while (pos != std::string::npos)
    {
        pos += delimiter.size();

        // a trailing "--" marks the final boundary
        if (pos + 1 < body.size() && body[pos] == '-' && body[pos + 1] == '-')
        {
            break;
        }

        if (pos + 1 < body.size() && body[pos] == '\r' && body[pos + 1] == '\n')
        {
            pos += 2;
        }

        const std::size_t headerEnd = body.find("\r\n\r\n", pos);
        if (headerEnd == std::string::npos)
        {
            break;
        }

        const std::string headers = body.substr(pos, headerEnd - pos);
        const std::size_t dataStart = headerEnd + 4;

        const std::size_t next = body.find(delimiter, dataStart);
        if (next == std::string::npos)
        {
            break;
        }

        std::size_t dataEnd = next;
        if (dataEnd >= dataStart + 2 && body[dataEnd - 2] == '\r' && body[dataEnd - 1] == '\n')
        {
            dataEnd -= 2;
        }

        const std::string data = body.substr(dataStart, dataEnd - dataStart);

        const std::string name = multipartAttribute(headers, "name=\"");
        const std::string filename = multipartAttribute(headers, "filename=\"");

        if (!filename.empty())
        {
            const std::string fileType = multipartContentType(headers);

            lua_getfield(L, -1, "files");
            lua_newtable(L);
            lua_pushlstring(L, name.data(), name.size());
            lua_setfield(L, -2, "field");
            lua_pushlstring(L, filename.data(), filename.size());
            lua_setfield(L, -2, "filename");
            lua_pushlstring(L, fileType.data(), fileType.size());
            lua_setfield(L, -2, "contentType");
            lua_pushlstring(L, data.data(), data.size());
            lua_setfield(L, -2, "data");
            lua_rawseti(L, -2, ++fileIndex);
            lua_pop(L, 1);
        }
        else if (!name.empty())
        {
            lua_getfield(L, -1, "fields");
            // a length-safe key so a field name with an embedded nul is not truncated
            lua_pushlstring(L, name.data(), name.size());
            lua_pushlstring(L, data.data(), data.size());
            lua_settable(L, -3);
            lua_pop(L, 1);
        }

        pos = next;
        if (++parts >= maxParts)
        {
            break;
        }
    }
}

} // namespace varn::http
