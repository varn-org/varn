#pragma once

#include "varn/dataview/DataView.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace varn::dataview
{

class DataViewJson
{
public:
    DataViewJson() = delete;

    static constexpr const char* kMalformed = "A data view token holds exactly the id \"$view\", the pointer as a hexadecimal string and the size.";

    static nlohmann::json write(const DataView& view);
    static std::optional<DataView::Token> read(const nlohmann::json& value);
    static std::vector<DataView> acquire(const nlohmann::json& value);
    static std::string pointerText(std::uintptr_t pointer);
    static std::optional<std::uintptr_t> parsePointer(std::string_view text);
};

} // namespace varn::dataview
