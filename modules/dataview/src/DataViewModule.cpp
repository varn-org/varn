#include "varn/dataview/DataViewModule.h"

#include <lua.hpp>

#include <cstddef>
#include <cstring>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace varn::dataview
{

namespace
{

class DataViewHelpers
{
public:
    // The metatable exists before any script requires the module, since the bridge of the host hands Lua views of its own.
    static void pushMetatable(lua_State* L)
    {
        if (luaL_newmetatable(L, DataViewModule::kMetatable) == 0)
        {
            return;
        }

        // The type reads as a data view in the messages of Lua, such as a value that JSON cannot encode.
        lua_pushliteral(L, "dataview");
        lua_setfield(L, -2, "__name");

        const luaL_Reg metamethods[] = {
            {"__index", &DataViewHelpers::index},
            {"__newindex", &DataViewHelpers::assign},
            {"__len", &DataViewHelpers::length},
            {"__eq", &DataViewHelpers::equal},
            {"__tostring", &DataViewHelpers::text},
            {"__gc", &DataViewHelpers::collect},
            {nullptr, nullptr},
        };
        luaL_setfuncs(L, metamethods, 0);
    }

    static DataView& slot(lua_State* L, int index)
    {
        auto* view = static_cast<DataView*>(luaL_testudata(L, index, DataViewModule::kMetatable));
        if (view == nullptr)
        {
            luaL_typeerror(L, index, "dataview");
        }

        return *view;
    }

    static const DataView& alive(lua_State* L, int index)
    {
        const DataView& view = slot(L, index);
        if (!view)
        {
            luaL_error(L, "The data view was released.");
        }

        return view;
    }

    // Reads an offset and a length that must lie inside the view, counted in bytes from zero as memory is.
    static std::pair<std::size_t, std::size_t> range(lua_State* L, const DataView& view, int offsetIndex, int lengthIndex, std::size_t wanted)
    {
        const lua_Integer offset = luaL_optinteger(L, offsetIndex, 0);
        if (offset < 0 || static_cast<std::size_t>(offset) > view.size())
        {
            luaL_error(L, "The offset %I lies outside the data view of %I bytes.", offset, static_cast<lua_Integer>(view.size()));
        }

        const std::size_t start = static_cast<std::size_t>(offset);
        const lua_Integer length = lengthIndex != 0 ? luaL_optinteger(L, lengthIndex, static_cast<lua_Integer>(view.size() - start)) : static_cast<lua_Integer>(wanted);
        if (length < 0 || static_cast<std::size_t>(length) > view.size() - start)
        {
            luaL_error(L, "The range of %I bytes at offset %I passes the end of the data view of %I bytes.", length, offset, static_cast<lua_Integer>(view.size()));
        }

        return {start, static_cast<std::size_t>(length)};
    }

    // Reads the offset of one byte, which lies inside the view, counted from zero as memory is.
    static std::size_t byteOffset(lua_State* L, const DataView& view)
    {
        const lua_Integer offset = luaL_checkinteger(L, 2);
        if (offset < 0 || static_cast<std::size_t>(offset) >= view.size())
        {
            luaL_error(L, "The offset %I lies outside the data view of %I bytes.", offset, static_cast<lua_Integer>(view.size()));
        }

        return static_cast<std::size_t>(offset);
    }

    static int index(lua_State* L)
    {
        if (lua_type(L, 2) == LUA_TNUMBER)
        {
            const DataView& view = alive(L, 1);
            lua_pushinteger(L, std::to_integer<lua_Integer>(view.data()[byteOffset(L, view)]));
            return 1;
        }

        const DataView& view = slot(L, 1);
        const std::string_view key = luaL_checkstring(L, 2);
        if (key == "released")
        {
            lua_pushboolean(L, view.empty() ? 1 : 0);
            return 1;
        }

        if (key == "read" || key == "write" || key == "release")
        {
            lua_pushcfunction(L, key == "read" ? &DataViewHelpers::read : key == "write" ? &DataViewHelpers::write
                                                                                         : &DataViewHelpers::release);
            return 1;
        }

        const DataView& live = alive(L, 1);
        if (key == "size")
        {
            lua_pushinteger(L, static_cast<lua_Integer>(live.size()));
            return 1;
        }

        if (key == "id")
        {
            lua_pushinteger(L, static_cast<lua_Integer>(live.id()));
            return 1;
        }

        if (key == "pointer")
        {
            lua_pushlightuserdata(L, live.data());
            return 1;
        }

        return luaL_error(L, "A data view has no field \"%s\".", std::string(key).c_str());
    }

    // Writes one byte in place, so Lua changes shared memory without copying it into a string and back.
    static int assign(lua_State* L)
    {
        const DataView& view = alive(L, 1);
        if (lua_type(L, 2) != LUA_TNUMBER)
        {
            return luaL_error(L, "A data view takes a byte at an offset, not the field \"%s\".", luaL_tolstring(L, 2, nullptr));
        }

        const std::size_t offset = byteOffset(L, view);
        const lua_Integer value = luaL_checkinteger(L, 3);
        if (value < 0 || value > 255)
        {
            return luaL_error(L, "A byte of a data view holds an integer from 0 to 255, not %I.", value);
        }

        view.data()[offset] = static_cast<std::byte>(value);
        return 0;
    }

    static int length(lua_State* L)
    {
        lua_pushinteger(L, static_cast<lua_Integer>(alive(L, 1).size()));
        return 1;
    }

    // Two values are equal when they hold the same view, such as one view that reached Lua twice.
    static int equal(lua_State* L)
    {
        const auto* left = static_cast<DataView*>(luaL_testudata(L, 1, DataViewModule::kMetatable));
        const auto* right = static_cast<DataView*>(luaL_testudata(L, 2, DataViewModule::kMetatable));
        lua_pushboolean(L, left != nullptr && right != nullptr && !left->empty() && *left == *right ? 1 : 0);
        return 1;
    }

    static int text(lua_State* L)
    {
        const DataView& view = slot(L, 1);
        if (!view)
        {
            lua_pushliteral(L, "dataview (released)");
            return 1;
        }

        lua_pushfstring(L, "dataview (%I bytes)", static_cast<lua_Integer>(view.size()));
        return 1;
    }

    static int collect(lua_State* L)
    {
        slot(L, 1).~DataView();
        return 0;
    }

    static int create(lua_State* L)
    {
        const lua_Integer size = luaL_checkinteger(L, 1);
        if (size < 0)
        {
            return luaL_error(L, "A data view needs a size of zero or more bytes, not %I.", size);
        }

        // A failed allocation leaves the view empty, and the error is raised outside the handler.
        DataView view;
        try
        {
            view = DataView::allocate(static_cast<std::size_t>(size));
        }
        catch (const std::bad_alloc&)
        {
        }

        if (!view)
        {
            return luaL_error(L, "A data view of %I bytes could not be allocated.", size);
        }

        DataViewModule::push(L, std::move(view));
        return 1;
    }

    static int from(lua_State* L)
    {
        std::size_t length = 0;
        const char* text = luaL_checklstring(L, 1, &length);

        DataView view;
        try
        {
            view = DataView::allocate(length);
        }
        catch (const std::bad_alloc&)
        {
        }

        if (!view)
        {
            return luaL_error(L, "A data view of %I bytes could not be allocated.", static_cast<lua_Integer>(length));
        }

        if (length != 0)
        {
            std::memcpy(view.data(), text, length);
        }

        DataViewModule::push(L, std::move(view));
        return 1;
    }

    static int is(lua_State* L)
    {
        lua_pushboolean(L, luaL_testudata(L, 1, DataViewModule::kMetatable) != nullptr ? 1 : 0);
        return 1;
    }

    static int read(lua_State* L)
    {
        const DataView& view = alive(L, 1);
        const auto [offset, length] = range(L, view, 2, 3, 0);
        lua_pushlstring(L, reinterpret_cast<const char*>(view.data()) + offset, length);
        return 1;
    }

    static int write(lua_State* L)
    {
        const DataView& view = alive(L, 1);
        std::size_t length = 0;
        const char* text = luaL_checklstring(L, 3, &length);
        const auto [offset, count] = range(L, view, 2, 0, length);
        if (count != 0)
        {
            std::memcpy(view.data() + offset, text, count);
        }

        return 0;
    }

    // Drops the reference of this value now instead of when it is collected, and releasing again does nothing.
    static int release(lua_State* L)
    {
        slot(L, 1).reset();
        return 0;
    }
};

} // namespace

void DataViewModule::install(lua_State* L)
{
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    lua_pushcfunction(L, &DataViewModule::luaOpen);
    lua_setfield(L, -2, "dataview");
    lua_pop(L, 1);
}

// The metatable is in place before the view is constructed, so nothing that can fail runs between the reference and its finalizer.
void DataViewModule::push(lua_State* L, DataView view)
{
    DataViewHelpers::pushMetatable(L);
    void* memory = lua_newuserdatauv(L, sizeof(DataView), 0);
    new (memory) DataView(std::move(view));
    lua_rotate(L, -2, 1);
    lua_setmetatable(L, -2);
}

// Answers the live view at `index`, or null for any other value and for a view that was released.
const DataView* DataViewModule::test(lua_State* L, int index)
{
    const auto* view = static_cast<DataView*>(luaL_testudata(L, index, kMetatable));
    return view != nullptr && !view->empty() ? view : nullptr;
}

const DataView& DataViewModule::check(lua_State* L, int index)
{
    return DataViewHelpers::alive(L, index);
}

int DataViewModule::luaOpen(lua_State* L)
{
    DataViewHelpers::pushMetatable(L);
    lua_pop(L, 1);

    const luaL_Reg functions[] = {
        {"new", &DataViewHelpers::create},
        {"from", &DataViewHelpers::from},
        {"is", &DataViewHelpers::is},
        {nullptr, nullptr},
    };
    luaL_newlib(L, functions);
    return 1;
}

} // namespace varn::dataview
