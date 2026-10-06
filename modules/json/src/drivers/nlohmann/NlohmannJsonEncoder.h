#pragma once

#include <lua.hpp>

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

namespace varn::json
{

class JsonEncoder
{
public:
    static void encode(lua_State* L, int index, int indent);

private:
    enum class Shape
    {
        Array,
        Object,
        SortedObject,
    };

    struct Frame
    {
        Shape shape;
        int slot;
        const void* table;
        lua_Integer position;
        lua_Integer length;
        std::size_t written;
        std::size_t sortedBegin;
    };

    struct SortedKey
    {
        std::string text;
        lua_Integer index;
        bool numeric;
    };

    JsonEncoder(lua_State* thread, luaL_Buffer& buffer, int indent, const void* arrayMetatable, const void* objectMetatable);

    void run();
    bool writeValue(int slot);
    void openTable(int slot);
    bool advance(Frame& frame);
    void closeTable();
    Shape classify(int slot, lua_Integer& length);
    bool isSequence(int slot, lua_Integer& length);
    lua_Integer arrayLength(int slot);
    void collectSortedKeys(int slot, int keysSlot);
    bool isOpen(const void* table) const;
    void writeKey(int slot);
    void writeSeparator(Frame& frame);
    void writeNewline(std::size_t depth);
    void writeString(const char* data, std::size_t length);
    void writeInteger(lua_Integer value);
    void writeFloat(lua_Number value);
    void writeLiteral(const char* text, std::size_t length);
    std::string keyText(int slot) const;
    std::string subject(const char* noun, std::size_t depth) const;

    static const void* metatablePointer(lua_State* L, const char* name);
    static std::size_t sequenceLength(const char* data, std::size_t available, std::size_t& skip);
    static std::string printable(std::string text);

    static constexpr std::size_t scannedDepth = 64;
    static constexpr std::size_t maxPathLength = 200;
    static const char threadKey;

    lua_State* m_thread;
    luaL_Buffer& m_buffer;
    int m_indent;
    const void* m_arrayMetatable;
    const void* m_objectMetatable;
    std::vector<Frame> m_frames;
    std::vector<SortedKey> m_sortedKeys;
    std::unordered_set<const void*> m_deepTables;
};

} // namespace varn::json
