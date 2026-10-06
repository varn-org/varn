#include "varn/http/HttpTypes.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace varn::http
{

// Answers whether a close frame may carry the code, which leaves out the codes the protocol reserves for a local report or never assigned.
bool WebSocketConnection::validCloseCode(int code)
{
    return (code >= 1000 && code <= 1003) || (code >= 1007 && code <= 1014) || (code >= 3000 && code <= 4999);
}

// Splits a header such as `Accept` or `Accept-Encoding` into its lowercased values with their weight in thousandths, and leaves out an entry whose weight is not a valid `q`.
std::vector<HttpAcceptList::Entry> HttpAcceptList::parse(const std::string& header)
{
    std::vector<Entry> entries;
    for (const std::string& item : splitLowered(header, ','))
    {
        const std::vector<std::string> fields = splitLowered(item, ';');
        Entry entry;
        entry.value = fields[0];

        bool valid = !entry.value.empty();
        for (std::size_t i = 1; i < fields.size(); ++i)
        {
            if (fields[i].rfind("q=", 0) == 0 && !readWeight(fields[i].substr(2), entry.weight))
            {
                valid = false;
            }
        }

        if (valid)
        {
            entries.push_back(std::move(entry));
        }
    }

    return entries;
}

// Splits the text at every separator into parts without their surrounding blanks, lowercased, so the parts always number one more than the separators.
std::vector<std::string> HttpAcceptList::splitLowered(const std::string& text, char separator)
{
    std::vector<std::string> parts;
    std::size_t position = 0;
    for (;;)
    {
        const std::size_t stop = std::min(text.find(separator, position), text.size());
        const std::size_t begin = text.find_first_not_of(" \t", position);
        const std::size_t last = text.find_last_not_of(" \t", stop == 0 ? 0 : stop - 1);
        std::string part = begin < stop && last != std::string::npos && last >= begin ? text.substr(begin, last - begin + 1) : std::string();
        for (char& c : part)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        parts.push_back(std::move(part));
        if (stop == text.size())
        {
            return parts;
        }

        position = stop + 1;
    }
}

// Reads a weight of RFC 9110, a `0` or a `1` with at most three decimals that never take a `1` past one.
bool HttpAcceptList::readWeight(const std::string& text, int& weight)
{
    if (text.empty() || (text[0] != '0' && text[0] != '1'))
    {
        return false;
    }

    int value = (text[0] - '0') * 1000;
    if (text.size() > 1)
    {
        if (text[1] != '.' || text.size() > 5)
        {
            return false;
        }

        int scale = 100;
        for (std::size_t i = 2; i < text.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(text[i])))
            {
                return false;
            }

            value += (text[i] - '0') * scale;
            scale /= 10;
        }
    }

    if (value > 1000)
    {
        return false;
    }

    weight = value;
    return true;
}

} // namespace varn::http
