#include "varn/fs/FsModule.h"
#include "varn/async/AsyncTask.h"
#include "varn/async/Promise.h"
#include "varn/fs/FsStorage.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/runtime/Runtime.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace varn::fs
{

using varn::async::Promise;
using varn::runtime::Runtime;

namespace
{
constexpr const char* kFsHandleMeta = "varn.FsHandle";

class FsLua
{
public:
    static Runtime& fsRuntime(lua_State* L)
    {
        return *static_cast<Runtime*>(varn::lua::LuaHelpers::getRuntime(L));
    }

    static std::string checkPath(lua_State* L, int index)
    {
        std::string path = varn::lua::LuaHelpers::checkString(L, index);

        // reject a path containing a null byte
        if (path.find('\0') != std::string::npos)
        {
            luaL_error(L, "[FsModule] A path must not contain a null byte.");
        }

        return path;
    }

    static void pushHandle(lua_State* L, std::shared_ptr<FsHandle> handle)
    {
        void* memory = lua_newuserdatauv(L, sizeof(std::shared_ptr<FsHandle>), 0);
        new (memory) std::shared_ptr<FsHandle>(std::move(handle));
        luaL_getmetatable(L, kFsHandleMeta);
        lua_setmetatable(L, -2);
    }

    static std::shared_ptr<FsHandle>& checkHandle(lua_State* L)
    {
        return *static_cast<std::shared_ptr<FsHandle>*>(luaL_checkudata(L, 1, kFsHandleMeta));
    }

    static int luaHandleGc(lua_State* L)
    {
        auto* holder = static_cast<std::shared_ptr<FsHandle>*>(lua_touserdata(L, 1));
        if (holder != nullptr)
        {
            std::destroy_at(holder);
        }

        return 0;
    }

    static int luaHandleRead(lua_State* L)
    {
        auto handle = FsLua::checkHandle(L);
        const lua_Integer maxArg = luaL_optinteger(L, 2, 65536);
        if (maxArg <= 0)
        {
            return luaL_error(L, "[FsModule] The maximum number of bytes to read must be positive.");
        }

        const std::size_t maxBytes = static_cast<std::size_t>(maxArg);
        auto& rt = FsLua::fsRuntime(L);

        // clang-format off
        return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [handle, maxBytes](Promise& promise)
        {
            promise.resolve(handle->read(maxBytes));
        });
        // clang-format on
    }

    static int luaHandleWrite(lua_State* L)
    {
        auto handle = FsLua::checkHandle(L);
        std::size_t len = 0;
        const char* raw = luaL_checklstring(L, 2, &len);
        std::string data(raw, len);
        auto& rt = FsLua::fsRuntime(L);

        // clang-format off
        return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [handle, data = std::move(data)](Promise& promise)
        {
            handle->write(data);
            promise.resolve("ok");
        });
        // clang-format on
    }

    static int luaHandleClose(lua_State* L)
    {
        auto handle = FsLua::checkHandle(L);
        auto& rt = FsLua::fsRuntime(L);

        // clang-format off
        return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [handle](Promise& promise)
        {
            handle->close();
            promise.resolve("ok");
        });
        // clang-format on
    }

    static int luaFileOpen(lua_State* L)
    {
        std::string path = FsLua::checkPath(L, 1);
        std::string mode = varn::lua::LuaHelpers::optionalString(L, 2, "r");
        auto& rt = FsLua::fsRuntime(L);

        // clang-format off
        return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [path = std::move(path), mode = std::move(mode)](Promise& promise)
        {
            std::shared_ptr<FsHandle> handle = FsStorage::open(path, mode);
            promise.resolveCustom([handle](lua_State* lua)
                                  { FsLua::pushHandle(lua, handle); });
        });
        // clang-format on
    }

    static int luaStat(lua_State* L)
    {
        std::string path = FsLua::checkPath(L, 1);
        auto& rt = FsLua::fsRuntime(L);

        // clang-format off
        return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [path = std::move(path)](Promise& promise)
        {
            FsStat info = FsStorage::stat(path);
            promise.resolveCustom([info](lua_State* lua)
                                  {
                lua_createtable(lua, 0, 5);
                lua_pushinteger(lua, static_cast<lua_Integer>(info.size));
                lua_setfield(lua, -2, "size");
                lua_pushinteger(lua, static_cast<lua_Integer>(info.mtime));
                lua_setfield(lua, -2, "mtime");
                lua_pushboolean(lua, info.isDir);
                lua_setfield(lua, -2, "isDir");
                lua_pushboolean(lua, info.isFile);
                lua_setfield(lua, -2, "isFile");
                lua_pushboolean(lua, info.isSymlink);
                lua_setfield(lua, -2, "isSymlink"); });
        });
        // clang-format on
    }

    static int luaReaddir(lua_State* L)
    {
        std::string path = FsLua::checkPath(L, 1);
        auto& rt = FsLua::fsRuntime(L);

        // clang-format off
        return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [path = std::move(path)](Promise& promise)
        {
            std::vector<std::string> names = FsStorage::readdir(path);
            promise.resolveCustom([names = std::move(names)](lua_State* lua)
                                  {
                lua_createtable(lua, static_cast<int>(names.size()), 0);
                for (std::size_t i = 0; i < names.size(); ++i)
                {
                    lua_pushlstring(lua, names[i].data(), names[i].size());
                    lua_rawseti(lua, -2, static_cast<lua_Integer>(i + 1));
                } });
        });
        // clang-format on
    }

    static int luaRename(lua_State* L)
    {
        std::string from = FsLua::checkPath(L, 1);
        std::string to = FsLua::checkPath(L, 2);
        auto& rt = FsLua::fsRuntime(L);

        // clang-format off
        return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [from = std::move(from), to = std::move(to)](Promise& promise)
        {
            FsStorage::rename(from, to);
            promise.resolve("ok");
        });
        // clang-format on
    }

    static int luaCopy(lua_State* L)
    {
        std::string from = FsLua::checkPath(L, 1);
        std::string to = FsLua::checkPath(L, 2);
        auto& rt = FsLua::fsRuntime(L);

        // clang-format off
        return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [from = std::move(from), to = std::move(to)](Promise& promise)
        {
            FsStorage::copy(from, to);
            promise.resolve("ok");
        });
        // clang-format on
    }

    static int luaAppend(lua_State* L)
    {
        std::string path = FsLua::checkPath(L, 1);
        std::size_t len = 0;
        const char* raw = luaL_checklstring(L, 2, &len);
        std::string data(raw, len);
        auto& rt = FsLua::fsRuntime(L);

        // clang-format off
        return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [path = std::move(path), data = std::move(data)](Promise& promise)
        {
            FsStorage::append(path, data);
            promise.resolve("ok");
        });
        // clang-format on
    }

    static int luaMkdtemp(lua_State* L)
    {
        std::string prefix = FsLua::checkPath(L, 1);
        auto& rt = FsLua::fsRuntime(L);

        // clang-format off
        return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [prefix = std::move(prefix)](Promise& promise)
        {
            promise.resolve(FsStorage::mkdtemp(prefix));
        });
        // clang-format on
    }

    static void createHandleMetatable(lua_State* L)
    {
        if (luaL_newmetatable(L, kFsHandleMeta))
        {
            lua_pushcfunction(L, &FsLua::luaHandleGc);
            lua_setfield(L, -2, "__gc");

            lua_newtable(L);
            lua_pushcfunction(L, &FsLua::luaHandleRead);
            lua_setfield(L, -2, "read");
            lua_pushcfunction(L, &FsLua::luaHandleWrite);
            lua_setfield(L, -2, "write");
            lua_pushcfunction(L, &FsLua::luaHandleClose);
            lua_setfield(L, -2, "close");
            lua_setfield(L, -2, "__index");
        }

        lua_pop(L, 1);
    }
};
} // namespace

int FsModule::luaReadFile(lua_State* L)
{
    auto& rt = FsLua::fsRuntime(L);
    std::string path = FsLua::checkPath(L, 1);

    // clang-format off
    return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [path = std::move(path)](Promise& promise)
    {
        promise.resolve(FsStorage::readAll(path));
    });
    // clang-format on
}

int FsModule::luaWriteFile(lua_State* L)
{
    auto& rt = FsLua::fsRuntime(L);
    std::string path = FsLua::checkPath(L, 1);
    std::string content = varn::lua::LuaHelpers::checkString(L, 2);

    // clang-format off
    return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [path = std::move(path), content = std::move(content)](Promise& promise)
    {
        FsStorage::writeAll(path, content);
        promise.resolve("ok");
    });
    // clang-format on
}

int FsModule::luaExists(lua_State* L)
{
    std::string path = FsLua::checkPath(L, 1);
    lua_pushboolean(L, FsStorage::exists(path));
    return 1;
}

int FsModule::luaMkdir(lua_State* L)
{
    auto& rt = FsLua::fsRuntime(L);
    std::string path = FsLua::checkPath(L, 1);

    // clang-format off
    return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [path = std::move(path)](Promise& promise)
    {
        FsStorage::mkdir(path);
        promise.resolve("ok");
    });
    // clang-format on
}

int FsModule::luaRemoveRecursive(lua_State* L)
{
    auto& rt = FsLua::fsRuntime(L);
    std::string path = FsLua::checkPath(L, 1);

    // clang-format off
    return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "FsModule", [path = std::move(path)](Promise& promise)
    {
        FsStorage::removeRecursive(path);
        promise.resolve("ok");
    });
    // clang-format on
}

int FsModule::luaOpen(lua_State* L)
{
    FsLua::createHandleMetatable(L);

    lua_newtable(L);

    lua_pushcfunction(L, &FsModule::luaReadFile);
    lua_setfield(L, -2, "readFile");

    lua_pushcfunction(L, &FsModule::luaWriteFile);
    lua_setfield(L, -2, "writeFile");

    lua_pushcfunction(L, &FsModule::luaExists);
    lua_setfield(L, -2, "exists");

    lua_pushcfunction(L, &FsModule::luaMkdir);
    lua_setfield(L, -2, "mkdir");

    lua_pushcfunction(L, &FsModule::luaRemoveRecursive);
    lua_setfield(L, -2, "removeRecursive");

    lua_pushcfunction(L, &FsLua::luaFileOpen);
    lua_setfield(L, -2, "open");

    lua_pushcfunction(L, &FsLua::luaStat);
    lua_setfield(L, -2, "stat");

    lua_pushcfunction(L, &FsLua::luaReaddir);
    lua_setfield(L, -2, "readdir");

    lua_pushcfunction(L, &FsLua::luaRename);
    lua_setfield(L, -2, "rename");

    lua_pushcfunction(L, &FsLua::luaCopy);
    lua_setfield(L, -2, "copy");

    lua_pushcfunction(L, &FsLua::luaAppend);
    lua_setfield(L, -2, "append");

    lua_pushcfunction(L, &FsLua::luaMkdtemp);
    lua_setfield(L, -2, "mkdtemp");

    return 1;
}

void FsModule::install(lua_State* L)
{
    luaL_requiref(L, "fs", &FsModule::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::fs
