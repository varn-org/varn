#include "NlohmannJsonEncoder.h"

#include "varn/json/JsonSerializer.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace varn::json
{

const char JsonEncoder::threadKey = 0;

JsonEncoder::JsonEncoder(lua_State* thread, luaL_Buffer& buffer, int indent, const void* arrayMetatable, const void* objectMetatable)
    : m_thread(thread)
    , m_buffer(buffer)
    , m_indent(indent)
    , m_arrayMetatable(arrayMetatable)
    , m_objectMetatable(objectMetatable)
{
}

// Pushes the JSON text of the value at `index`, or raises an error that names the key of the part that cannot be encoded.
void JsonEncoder::encode(lua_State* L, int index, int indent)
{
    index = lua_absindex(L, index);
    luaL_checkstack(L, 4, nullptr);
    const void* arrayMetatable = metatablePointer(L, JsonSerializer::arrayMetatable);
    const void* objectMetatable = metatablePointer(L, JsonSerializer::objectMetatable);

    // Walks the tables on a thread of its own so the buffer stays on top of this stack.
    // The cached thread leaves the registry while in use, so a finalizer that encodes meanwhile takes a fresh one.
    lua_State* thread = nullptr;
    if (lua_rawgetp(L, LUA_REGISTRYINDEX, &threadKey) == LUA_TTHREAD)
    {
        thread = lua_tothread(L, -1);
        lua_pushnil(L);
        lua_rawsetp(L, LUA_REGISTRYINDEX, &threadKey);
        lua_settop(thread, 0);
    }
    else
    {
        lua_pop(L, 1);
        thread = lua_newthread(L);
    }

    lua_pushvalue(L, index);
    lua_xmove(L, thread, 1);

    std::string failure;
    luaL_Buffer buffer;
    luaL_buffinit(L, &buffer);
    try
    {
        JsonEncoder encoder(thread, buffer, indent, arrayMetatable, objectMetatable);
        encoder.run();
    }
    catch (const std::runtime_error& ex)
    {
        failure = ex.what();
    }

    if (!failure.empty())
    {
        luaL_error(L, "[JsonModule] %s", failure.c_str());
        return;
    }

    luaL_pushresult(&buffer);
    lua_pushvalue(L, -2);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &threadKey);
    lua_remove(L, -2);
}

const void* JsonEncoder::metatablePointer(lua_State* L, const char* name)
{
    lua_getfield(L, LUA_REGISTRYINDEX, name);
    const void* pointer = lua_istable(L, -1) ? lua_topointer(L, -1) : nullptr;
    lua_pop(L, 1);
    return pointer;
}

void JsonEncoder::run()
{
    if (!writeValue(1))
    {
        return;
    }

    while (!m_frames.empty())
    {
        if (!advance(m_frames.back()))
        {
            closeTable();
            continue;
        }

        writeValue(lua_gettop(m_thread));
    }
}

// Writes the value at `slot` and pops it, or opens a frame for a table and answers true.
bool JsonEncoder::writeValue(int slot)
{
    switch (lua_type(m_thread, slot))
    {
    case LUA_TTABLE:
        openTable(slot);
        return true;
    case LUA_TSTRING:
    {
        std::size_t length = 0;
        const char* data = lua_tolstring(m_thread, slot, &length);
        writeString(data, length);
        break;
    }
    case LUA_TNUMBER:
        if (lua_isinteger(m_thread, slot))
        {
            writeInteger(lua_tointeger(m_thread, slot));
            break;
        }

        writeFloat(lua_tonumber(m_thread, slot));
        break;
    case LUA_TBOOLEAN:
        if (lua_toboolean(m_thread, slot) != 0)
        {
            writeLiteral("true", 4);
            break;
        }

        writeLiteral("false", 5);
        break;
    case LUA_TNIL:
        writeLiteral("null", 4);
        break;
    case LUA_TLIGHTUSERDATA:
        // Only the null pointer of `json.null` has a JSON meaning.
        if (lua_touserdata(m_thread, slot) != nullptr)
        {
            throw std::runtime_error(subject("value", m_frames.size()) + " is a light userdata and cannot be encoded.");
        }

        writeLiteral("null", 4);
        break;
    default:
        throw std::runtime_error(subject("value", m_frames.size()) + " is a " + luaL_typename(m_thread, slot) + " and cannot be encoded.");
    }

    lua_settop(m_thread, slot - 1);
    return false;
}

void JsonEncoder::openTable(int slot)
{
    const void* table = lua_topointer(m_thread, slot);
    if (isOpen(table))
    {
        throw std::runtime_error(subject("table", m_frames.size()) + " contains itself and cannot be encoded.");
    }

    // Reserves the slots of a key, a value and the sorted keys of the new level.
    if (lua_checkstack(m_thread, 4) == 0)
    {
        throw std::runtime_error(subject("table", m_frames.size()) + " nests too deeply to be encoded.");
    }

    lua_Integer length = 0;
    const Shape shape = classify(slot, length);
    const std::size_t sortedBegin = m_sortedKeys.size();
    if (shape == Shape::Object)
    {
        lua_pushnil(m_thread);
    }

    if (shape == Shape::SortedObject)
    {
        collectSortedKeys(slot, slot + 1);
        length = static_cast<lua_Integer>(m_sortedKeys.size() - sortedBegin);
    }

    luaL_addchar(&m_buffer, shape == Shape::Array ? '[' : '{');
    if (m_frames.size() >= scannedDepth)
    {
        m_deepTables.insert(table);
    }

    m_frames.push_back({shape, slot, table, 0, length, 0, sortedBegin});
}

// Moves the frame to its next entry and leaves the value on top, after writing the separator and the key.
bool JsonEncoder::advance(Frame& frame)
{
    if (frame.shape == Shape::Object)
    {
        if (lua_next(m_thread, frame.slot) == 0)
        {
            return false;
        }

        writeSeparator(frame);
        writeKey(frame.slot + 1);
        return true;
    }

    if (frame.position >= frame.length)
    {
        return false;
    }

    ++frame.position;
    writeSeparator(frame);
    if (frame.shape == Shape::Array)
    {
        lua_rawgeti(m_thread, frame.slot, frame.position);
        return true;
    }

    const SortedKey& key = m_sortedKeys[frame.sortedBegin + static_cast<std::size_t>(frame.position - 1)];
    writeString(key.text.data(), key.text.size());
    writeLiteral(": ", 2);
    lua_rawgeti(m_thread, frame.slot + 1, key.index);
    lua_rawget(m_thread, frame.slot);
    return true;
}

void JsonEncoder::closeTable()
{
    const Frame frame = m_frames.back();
    m_frames.pop_back();
    if (m_frames.size() >= scannedDepth)
    {
        m_deepTables.erase(frame.table);
    }

    if (frame.written > 0 && m_indent > 0)
    {
        writeNewline(m_frames.size());
    }

    luaL_addchar(&m_buffer, frame.shape == Shape::Array ? ']' : '}');
    m_sortedKeys.resize(frame.sortedBegin);
    lua_settop(m_thread, frame.slot - 1);
}

// Reads the marker of a table and otherwise tells a sequence from an object, which is sorted when the output is indented.
JsonEncoder::Shape JsonEncoder::classify(int slot, lua_Integer& length)
{
    const void* metatable = nullptr;
    if (lua_getmetatable(m_thread, slot) != 0)
    {
        metatable = lua_topointer(m_thread, -1);
        lua_pop(m_thread, 1);
    }

    if (metatable != nullptr && metatable == m_arrayMetatable)
    {
        length = arrayLength(slot);
        return Shape::Array;
    }

    const Shape object = m_indent > 0 ? Shape::SortedObject : Shape::Object;
    if (metatable != nullptr && metatable == m_objectMetatable)
    {
        return object;
    }

    return isSequence(slot, length) ? Shape::Array : object;
}

bool JsonEncoder::isSequence(int slot, lua_Integer& length)
{
    length = static_cast<lua_Integer>(lua_rawlen(m_thread, slot));
    if (length <= 0)
    {
        return false;
    }

    lua_Integer counted = 0;
    lua_pushnil(m_thread);
    while (lua_next(m_thread, slot) != 0)
    {
        lua_pop(m_thread, 1);
        ++counted;

        const bool inRange = lua_isinteger(m_thread, -1) && lua_tointeger(m_thread, -1) >= 1 && lua_tointeger(m_thread, -1) <= length;
        if (!inRange || counted > length)
        {
            lua_pop(m_thread, 1);
            return false;
        }
    }

    return counted == length;
}

// Answers the largest key of a table marked as an array, whose holes encode as null.
lua_Integer JsonEncoder::arrayLength(int slot)
{
    lua_Integer length = 0;
    lua_pushnil(m_thread);
    while (lua_next(m_thread, slot) != 0)
    {
        lua_pop(m_thread, 1);
        if (lua_isinteger(m_thread, -1) && lua_tointeger(m_thread, -1) >= 1)
        {
            length = std::max(length, lua_tointeger(m_thread, -1));
            continue;
        }

        const int type = lua_type(m_thread, -1);
        std::string held = std::string("a key of type ") + lua_typename(m_thread, type);
        if (type == LUA_TSTRING)
        {
            held = "the key \"" + printable(keyText(-1)) + "\"";
        }

        if (type == LUA_TNUMBER)
        {
            held = "the key " + keyText(-1);
        }

        throw std::runtime_error(subject("table", m_frames.size()) + " is marked as an array but holds " + held + ".");
    }

    return length;
}

// Copies the keys of the table at `slot` into a new table at `keysSlot` and records their text in sorted order.
void JsonEncoder::collectSortedKeys(int slot, int keysSlot)
{
    lua_newtable(m_thread);
    const std::size_t begin = m_sortedKeys.size();
    lua_Integer count = 0;
    lua_pushnil(m_thread);
    while (lua_next(m_thread, slot) != 0)
    {
        lua_pop(m_thread, 1);

        const int type = lua_type(m_thread, -1);
        const bool finite = type == LUA_TNUMBER && (lua_isinteger(m_thread, -1) || std::isfinite(lua_tonumber(m_thread, -1)));
        if (type != LUA_TSTRING && !finite)
        {
            throw std::runtime_error(subject("table", m_frames.size()) + " has a key of type " + lua_typename(m_thread, type) + " and cannot be encoded.");
        }

        m_sortedKeys.push_back({keyText(-1), ++count, type == LUA_TNUMBER});
        lua_pushvalue(m_thread, -1);
        lua_rawseti(m_thread, keysSlot, count);
    }

    // clang-format off
    std::sort(m_sortedKeys.begin() + static_cast<std::ptrdiff_t>(begin), m_sortedKeys.end(), [](const SortedKey& left, const SortedKey& right)
    {
        return left.text < right.text;
    });
    // clang-format on
}

bool JsonEncoder::isOpen(const void* table) const
{
    const std::size_t scanned = std::min(m_frames.size(), scannedDepth);
    for (std::size_t i = 0; i < scanned; ++i)
    {
        if (m_frames[i].table == table)
        {
            return true;
        }
    }

    return m_frames.size() > scannedDepth && m_deepTables.contains(table);
}

void JsonEncoder::writeKey(int slot)
{
    const int type = lua_type(m_thread, slot);
    const bool number = type == LUA_TNUMBER && (lua_isinteger(m_thread, slot) || std::isfinite(lua_tonumber(m_thread, slot)));
    if (type != LUA_TSTRING && !number)
    {
        throw std::runtime_error(subject("table", m_frames.size() - 1) + " has a key of type " + lua_typename(m_thread, type) + " and cannot be encoded.");
    }

    if (type == LUA_TSTRING)
    {
        std::size_t length = 0;
        const char* data = lua_tolstring(m_thread, slot, &length);
        writeString(data, length);
    }
    else
    {
        // Writes a number key as the string JSON requires.
        luaL_addchar(&m_buffer, '"');
        if (lua_isinteger(m_thread, slot))
        {
            writeInteger(lua_tointeger(m_thread, slot));
        }
        else
        {
            writeFloat(lua_tonumber(m_thread, slot));
        }

        luaL_addchar(&m_buffer, '"');
    }

    if (m_indent > 0)
    {
        writeLiteral(": ", 2);
        return;
    }

    luaL_addchar(&m_buffer, ':');
}

void JsonEncoder::writeSeparator(Frame& frame)
{
    if (frame.written > 0)
    {
        luaL_addchar(&m_buffer, ',');
    }

    ++frame.written;
    if (m_indent > 0)
    {
        writeNewline(m_frames.size());
    }
}

void JsonEncoder::writeNewline(std::size_t depth)
{
    const std::size_t spaces = static_cast<std::size_t>(m_indent) * depth;
    char* out = luaL_prepbuffsize(&m_buffer, spaces + 1);
    out[0] = '\n';
    std::memset(out + 1, ' ', spaces);
    luaL_addsize(&m_buffer, spaces + 1);
}

// Writes a quoted string, escaping what JSON requires and replacing each invalid UTF-8 sequence with U+FFFD.
void JsonEncoder::writeString(const char* data, std::size_t length)
{
    static constexpr char hex[] = "0123456789abcdef";
    luaL_addchar(&m_buffer, '"');

    std::size_t start = 0;
    std::size_t i = 0;
    while (i < length)
    {
        const auto byte = static_cast<unsigned char>(data[i]);
        if (byte >= 0x20 && byte < 0x80 && byte != '"' && byte != '\\')
        {
            ++i;
            continue;
        }

        if (byte >= 0x80)
        {
            std::size_t skip = 0;
            const std::size_t valid = sequenceLength(data + i, length - i, skip);
            if (valid > 0)
            {
                i += valid;
                continue;
            }

            luaL_addlstring(&m_buffer, data + start, i - start);
            writeLiteral("\xEF\xBF\xBD", 3);
            i += skip;
            start = i;
            continue;
        }

        luaL_addlstring(&m_buffer, data + start, i - start);
        char escape[6] = {'\\', 'u', '0', '0', hex[byte >> 4], hex[byte & 0x0F]};
        std::size_t escapeLength = 2;
        switch (byte)
        {
        case '"':
        case '\\':
            escape[1] = static_cast<char>(byte);
            break;
        case '\b':
            escape[1] = 'b';
            break;
        case '\f':
            escape[1] = 'f';
            break;
        case '\n':
            escape[1] = 'n';
            break;
        case '\r':
            escape[1] = 'r';
            break;
        case '\t':
            escape[1] = 't';
            break;
        default:
            escapeLength = 6;
            break;
        }

        luaL_addlstring(&m_buffer, escape, escapeLength);
        ++i;
        start = i;
    }

    luaL_addlstring(&m_buffer, data + start, length - start);
    luaL_addchar(&m_buffer, '"');
}

// Answers the length of the valid UTF-8 sequence at `data`, or zero with `skip` set to the length of its longest invalid prefix.
std::size_t JsonEncoder::sequenceLength(const char* data, std::size_t available, std::size_t& skip)
{
    const auto lead = static_cast<unsigned char>(data[0]);
    std::size_t continuation = 0;
    unsigned char low = 0x80;
    unsigned char high = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF)
    {
        continuation = 1;
    }
    else if (lead >= 0xE0 && lead <= 0xEF)
    {
        continuation = 2;
        low = lead == 0xE0 ? 0xA0 : 0x80;
        high = lead == 0xED ? 0x9F : 0xBF;
    }
    else if (lead >= 0xF0 && lead <= 0xF4)
    {
        continuation = 3;
        low = lead == 0xF0 ? 0x90 : 0x80;
        high = lead == 0xF4 ? 0x8F : 0xBF;
    }
    else
    {
        skip = 1;
        return 0;
    }

    for (std::size_t k = 1; k <= continuation; ++k)
    {
        if (k >= available || static_cast<unsigned char>(data[k]) < low || static_cast<unsigned char>(data[k]) > high)
        {
            skip = k;
            return 0;
        }

        low = 0x80;
        high = 0xBF;
    }

    return continuation + 1;
}

void JsonEncoder::writeInteger(lua_Integer value)
{
    constexpr std::size_t capacity = 24;
    char* out = luaL_prepbuffsize(&m_buffer, capacity);
    const std::to_chars_result result = std::to_chars(out, out + capacity, value);
    luaL_addsize(&m_buffer, static_cast<std::size_t>(result.ptr - out));
}

// Writes the shortest text that reads back as the same double, or null for a value JSON cannot represent.
void JsonEncoder::writeFloat(lua_Number value)
{
    if (!std::isfinite(value))
    {
        writeLiteral("null", 4);
        return;
    }

    constexpr std::size_t capacity = 64;
    char* out = luaL_prepbuffsize(&m_buffer, capacity);
    const char* end = nlohmann::detail::to_chars(out, out + capacity, static_cast<double>(value));
    luaL_addsize(&m_buffer, static_cast<std::size_t>(end - out));
}

void JsonEncoder::writeLiteral(const char* text, std::size_t length)
{
    luaL_addlstring(&m_buffer, text, length);
}

std::string JsonEncoder::keyText(int slot) const
{
    if (lua_type(m_thread, slot) == LUA_TSTRING)
    {
        std::size_t length = 0;
        const char* data = lua_tolstring(m_thread, slot, &length);
        return std::string(data, length);
    }

    if (lua_isinteger(m_thread, slot))
    {
        return std::to_string(lua_tointeger(m_thread, slot));
    }

    char text[64];
    const char* end = nlohmann::detail::to_chars(text, text + sizeof(text), static_cast<double>(lua_tonumber(m_thread, slot)));
    return std::string(text, static_cast<std::size_t>(end - text));
}

// Names a part of the value by the keys that lead to it through the first `depth` frames, such as `The value at key "items[2].name"`.
std::string JsonEncoder::subject(const char* noun, std::size_t depth) const
{
    std::string text = std::string("The ") + noun;
    if (depth == 0)
    {
        return text;
    }

    std::string path;
    for (std::size_t i = 0; i < depth; ++i)
    {
        const Frame& frame = m_frames[i];
        if (frame.shape == Shape::Array)
        {
            path += "[" + std::to_string(frame.position) + "]";
            continue;
        }

        bool numeric = frame.shape == Shape::Object && lua_type(m_thread, frame.slot + 1) == LUA_TNUMBER;
        std::string key;
        if (frame.shape == Shape::Object)
        {
            key = keyText(frame.slot + 1);
        }
        else
        {
            const SortedKey& sorted = m_sortedKeys[frame.sortedBegin + static_cast<std::size_t>(frame.position - 1)];
            key = sorted.text;
            numeric = sorted.numeric;
        }

        if (numeric)
        {
            path += "[" + key + "]";
            continue;
        }

        path += (path.empty() ? "" : ".") + key;
    }

    return text + " at key \"" + printable(std::move(path)) + "\"";
}

// Keeps the end of a long key or path and masks control characters, since keys come from the caller's data.
std::string JsonEncoder::printable(std::string text)
{
    if (text.size() > maxPathLength)
    {
        text = "..." + text.substr(text.size() - maxPathLength);
    }

    // clang-format off
    std::replace_if(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) < 0x20; }, '?');
    // clang-format on
    return text;
}

} // namespace varn::json
