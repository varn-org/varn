#include "varn/dataview/DataViewJson.h"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace varn::dataview
{

nlohmann::json DataViewJson::write(const DataView& view)
{
    const DataView::Token token = view.token();
    return {{DataView::kTokenKey, token.id}, {DataView::kPointerKey, pointerText(token.pointer)}, {DataView::kSizeKey, token.size}};
}

// Answers nothing for a value that is no token, and refuses an object that holds the key of a token without being a whole one, so a malformed token never passes as plain data.
std::optional<DataView::Token> DataViewJson::read(const nlohmann::json& value)
{
    if (!value.is_object())
    {
        return std::nullopt;
    }

    const auto id = value.find(DataView::kTokenKey);
    if (id == value.end())
    {
        return std::nullopt;
    }

    const auto pointer = value.find(DataView::kPointerKey);
    const auto size = value.find(DataView::kSizeKey);
    const bool whole = value.size() == 3 && id->is_number_unsigned() && pointer != value.end() && pointer->is_string() && size != value.end() && size->is_number_unsigned();
    const std::optional<std::uintptr_t> address = whole ? parsePointer(pointer->get_ref<const std::string&>()) : std::nullopt;
    if (!address)
    {
        throw std::invalid_argument(kMalformed);
    }

    return DataView::Token{id->get<std::uint64_t>(), *address, size->get<std::size_t>()};
}

// The walk keeps its own stack, so JSON nested deeper than the native stack allows never overflows it.
std::vector<DataView> DataViewJson::acquire(const nlohmann::json& value)
{
    std::vector<DataView> views;
    std::vector<const nlohmann::json*> pending{&value};
    while (!pending.empty())
    {
        const nlohmann::json& current = *pending.back();
        pending.pop_back();

        if (const std::optional<DataView::Token> token = read(current))
        {
            DataView view = DataView::acquire(*token);
            if (!view)
            {
                throw std::invalid_argument("The JSON names the data view " + std::to_string(token->id) + ", which is no longer alive.");
            }

            views.push_back(std::move(view));
            continue;
        }

        if (current.is_structured())
        {
            for (const nlohmann::json& element : current)
            {
                pending.push_back(&element);
            }
        }
    }

    return views;
}

std::string DataViewJson::pointerText(std::uintptr_t pointer)
{
    char digits[2 + 2 * sizeof(std::uintptr_t)] = {'0', 'x'};
    const std::to_chars_result written = std::to_chars(digits + 2, digits + sizeof(digits), pointer, 16);
    return std::string(digits, written.ptr);
}

std::optional<std::uintptr_t> DataViewJson::parsePointer(std::string_view text)
{
    if (text.size() < 3 || text.substr(0, 2) != "0x")
    {
        return std::nullopt;
    }

    std::uintptr_t pointer = 0;
    const std::from_chars_result parsed = std::from_chars(text.data() + 2, text.data() + text.size(), pointer, 16);
    if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size())
    {
        return std::nullopt;
    }

    return pointer;
}

} // namespace varn::dataview
