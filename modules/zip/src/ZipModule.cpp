#include "varn/zip/ZipModule.h"

#include "ZipPath.h"
#include "varn/async/AsyncTask.h"
#include "varn/async/Promise.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/runtime/Runtime.h"

#include <lua.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(VARN_HAVE_LIBZIP) && VARN_HAVE_LIBZIP
#include <zip.h>
#endif

namespace varn::zip
{

namespace fs = std::filesystem;

using varn::async::Promise;
using varn::runtime::Runtime;

#if defined(VARN_HAVE_LIBZIP) && VARN_HAVE_LIBZIP

namespace
{
struct ZipDiscarder
{
    void operator()(zip_t* archive) const { zip_discard(archive); }
};

struct ZipFileCloser
{
    void operator()(zip_file_t* file) const { zip_fclose(file); }
};

using ZipHandle = std::unique_ptr<zip_t, ZipDiscarder>;
using ZipFileHandle = std::unique_ptr<zip_file_t, ZipFileCloser>;

// Owns the folder an extraction writes into and removes it with whatever it holds unless the extraction moved it into place.
class StagingFolder
{
public:
    explicit StagingFolder(fs::path path)
        : m_path(std::move(path))
    {
    }

    ~StagingFolder()
    {
        std::error_code ec;
        if (!m_path.empty())
        {
            fs::remove_all(m_path, ec);
        }
    }

    StagingFolder(const StagingFolder&) = delete;
    StagingFolder& operator=(const StagingFolder&) = delete;

    const fs::path& path() const { return m_path; }
    void release() { m_path.clear(); }

private:
    fs::path m_path;
};

// Checks, writes and places the entries of one archive.
class ZipExtraction
{
public:
    struct Entry
    {
        zip_uint64_t index;
        std::string name;
        std::uint64_t size;
    };

    static std::vector<Entry> checkedEntries(zip_t* archive);
    static fs::path destinationPath(const std::string& destDir);
    static fs::path createStaging(const fs::path& destination);
    static void writeEntries(zip_t* archive, const std::vector<Entry>& entries, const fs::path& root);
    static void writeEntry(zip_t* archive, const Entry& entry, const fs::path& target);
    static void merge(const fs::path& staging, const fs::path& destination);

private:
    // Bounds the entry count and the total size the entries declare against zip bombs.
    static constexpr zip_uint64_t maxEntries = 100000;
    static constexpr std::uint64_t maxTotalBytes = 2ull * 1024 * 1024 * 1024;
};

// Answers every entry once its name, its containment and its declared size passed, so a refused archive writes nothing.
std::vector<ZipExtraction::Entry> ZipExtraction::checkedEntries(zip_t* archive)
{
    const zip_int64_t count = zip_get_num_entries(archive, 0);
    if (count < 0)
    {
        throw std::runtime_error("[ZipModule] The archive directory could not be read.");
    }

    if (static_cast<zip_uint64_t>(count) > maxEntries)
    {
        throw std::runtime_error("[ZipModule] The archive holds " + std::to_string(count) + " entries, more than the limit of " + std::to_string(maxEntries) + ".");
    }

    std::vector<Entry> entries;
    entries.reserve(static_cast<std::size_t>(count));
    std::uint64_t total = 0;
    for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(count); ++i)
    {
        zip_stat_t stat;
        zip_stat_init(&stat);
        const bool known = zip_stat_index(archive, i, ZIP_FL_ENC_GUESS, &stat) == 0 && (stat.valid & ZIP_STAT_NAME) != 0 && (stat.valid & ZIP_STAT_SIZE) != 0;
        if (!known)
        {
            throw std::runtime_error("[ZipModule] The entry at index " + std::to_string(i) + " could not be read.");
        }

        const std::string name(stat.name);
        if (name.empty())
        {
            throw std::runtime_error("[ZipModule] The archive holds an entry without a name.");
        }

        if (!ZipPath::entryPathSafe(name) || !ZipPath::staysInside(name))
        {
            throw std::runtime_error("[ZipModule] The entry \"" + ZipPath::printable(name) + "\" would leave the destination folder.");
        }

        if (stat.size > maxTotalBytes - total)
        {
            throw std::runtime_error("[ZipModule] The entries of the archive declare more than the limit of 2 GiB.");
        }

        total += stat.size;
        entries.push_back({i, name, stat.size});
    }

    return entries;
}

// Answers the absolute destination with its symbolic links resolved, creating the folder that will hold it.
fs::path ZipExtraction::destinationPath(const std::string& destDir)
{
    fs::path destination = fs::absolute(fs::path(destDir)).lexically_normal();
    if (!destination.has_filename())
    {
        destination = destination.parent_path();
    }

    if (fs::exists(destination))
    {
        destination = fs::canonical(destination);
        if (!fs::is_directory(destination))
        {
            throw std::runtime_error("[ZipModule] The destination \"" + destDir + "\" is not a folder.");
        }

        return destination;
    }

    fs::create_directories(destination.parent_path());
    return fs::canonical(destination.parent_path()) / destination.filename();
}

// Creates a new folder beside the destination, on the same file system, so it can be renamed into place.
fs::path ZipExtraction::createStaging(const fs::path& destination)
{
    std::random_device device;
    constexpr int attempts = 16;
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        std::ostringstream name;
        name << "." << destination.filename().string() << ".extracting-" << std::hex << device();
        const fs::path staging = destination.parent_path() / name.str();
        if (fs::create_directory(staging))
        {
            return staging;
        }
    }

    throw std::runtime_error("[ZipModule] A folder for the extraction could not be created beside the destination.");
}

void ZipExtraction::writeEntries(zip_t* archive, const std::vector<Entry>& entries, const fs::path& root)
{
    for (const Entry& entry : entries)
    {
        try
        {
            writeEntry(archive, entry, root / entry.name);
        }
        catch (const fs::filesystem_error&)
        {
            throw std::runtime_error("[ZipModule] The entry \"" + ZipPath::printable(entry.name) + "\" could not be written.");
        }
    }
}

void ZipExtraction::writeEntry(zip_t* archive, const Entry& entry, const fs::path& target)
{
    if (entry.name.back() == '/')
    {
        fs::create_directories(target);
        return;
    }

    fs::create_directories(target.parent_path());
    ZipFileHandle file(zip_fopen_index(archive, entry.index, 0));
    if (!file)
    {
        throw std::runtime_error("[ZipModule] The entry \"" + ZipPath::printable(entry.name) + "\" could not be opened.");
    }

    std::ofstream out(target, std::ios::binary);
    if (!out)
    {
        throw std::runtime_error("[ZipModule] The entry \"" + ZipPath::printable(entry.name) + "\" could not be created.");
    }

    // Holds each entry to the size it declared, which the archive as a whole was checked against.
    char buffer[8192];
    std::uint64_t written = 0;
    while (true)
    {
        const zip_int64_t read = zip_fread(file.get(), buffer, sizeof(buffer));
        if (read < 0)
        {
            throw std::runtime_error("[ZipModule] The entry \"" + ZipPath::printable(entry.name) + "\" could not be read from the archive.");
        }

        if (read == 0)
        {
            break;
        }

        written += static_cast<std::uint64_t>(read);
        if (written > entry.size)
        {
            throw std::runtime_error("[ZipModule] The entry \"" + ZipPath::printable(entry.name) + "\" expands beyond the size it declares.");
        }

        out.write(buffer, static_cast<std::streamsize>(read));
    }

    out.flush();
    if (!out)
    {
        throw std::runtime_error("[ZipModule] The entry \"" + ZipPath::printable(entry.name) + "\" could not be fully written.");
    }
}

// Moves the staged entries into an existing destination after checking that none replaces a symbolic link or changes a file into a folder.
void ZipExtraction::merge(const fs::path& staging, const fs::path& destination)
{
    std::vector<std::pair<fs::path, fs::path>> moves;
    for (auto it = fs::recursive_directory_iterator(staging); it != fs::recursive_directory_iterator(); ++it)
    {
        const fs::path relative = it->path().lexically_relative(staging);
        const fs::path target = destination / relative;
        const fs::file_status status = fs::symlink_status(target);
        if (!fs::exists(status))
        {
            moves.emplace_back(it->path(), target);
            it.disable_recursion_pending();
            continue;
        }

        const std::string name = ZipPath::printable(relative.generic_string());
        if (fs::is_symlink(status))
        {
            throw std::runtime_error("[ZipModule] The entry \"" + name + "\" would replace a symbolic link in the destination folder.");
        }

        const bool directory = it->is_directory();
        if (directory && fs::is_directory(status))
        {
            continue;
        }

        if (directory || fs::is_directory(status))
        {
            throw std::runtime_error("[ZipModule] The entry \"" + name + "\" would replace a " + (directory ? "file" : "folder") + " in the destination folder.");
        }

        moves.emplace_back(it->path(), target);
    }

    for (const auto& [from, to] : moves)
    {
        fs::rename(from, to);
    }
}
} // namespace

Runtime& ZipModule::luaRuntime(lua_State* L)
{
    return *static_cast<Runtime*>(varn::lua::LuaHelpers::getRuntime(L));
}

// Extracts into a new folder beside the destination and moves it into place only once every entry was written, so a failure leaves the destination as it was.
void ZipModule::performExtract(const std::string& zipPath, const std::string& destDir)
{
    int err = 0;
    ZipHandle za(zip_open(zipPath.c_str(), ZIP_RDONLY, &err));
    if (!za)
    {
        throw std::runtime_error("[ZipModule] The archive could not be opened.");
    }

    const std::vector<ZipExtraction::Entry> entries = ZipExtraction::checkedEntries(za.get());

    fs::path destination;
    fs::path stagingPath;
    try
    {
        destination = ZipExtraction::destinationPath(destDir);
        stagingPath = ZipExtraction::createStaging(destination);
    }
    catch (const fs::filesystem_error&)
    {
        throw std::runtime_error("[ZipModule] The destination \"" + destDir + "\" could not be prepared.");
    }

    StagingFolder staging(stagingPath);
    ZipExtraction::writeEntries(za.get(), entries, staging.path());

    try
    {
        if (!fs::exists(fs::symlink_status(destination)))
        {
            fs::rename(staging.path(), destination);
            staging.release();
            return;
        }

        ZipExtraction::merge(staging.path(), destination);
    }
    catch (const fs::filesystem_error&)
    {
        throw std::runtime_error("[ZipModule] The extracted entries could not be moved into \"" + destDir + "\".");
    }
}

void ZipModule::performCreate(const std::string& zipPath, const std::vector<std::pair<std::string, std::string>>& items)
{
    int err = 0;
    ZipHandle za(zip_open(zipPath.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err));
    if (!za)
    {
        throw std::runtime_error("[ZipModule] The archive could not be created.");
    }

    for (const auto& [localPath, entryName] : items)
    {
        if (!ZipPath::entryPathSafe(entryName))
        {
            throw std::runtime_error("[ZipModule] An entry name is unsafe.");
        }

        if (!fs::is_regular_file(localPath))
        {
            throw std::runtime_error("[ZipModule] A source path is not a regular file.");
        }

        zip_source_t* s = zip_source_file(za.get(), localPath.c_str(), 0, 0);
        if (!s)
        {
            throw std::runtime_error("[ZipModule] A source file could not be read.");
        }

        const zip_int64_t idx = zip_file_add(za.get(), entryName.c_str(), s, ZIP_FL_ENC_UTF_8);
        if (idx < 0)
        {
            zip_source_free(s);
            throw std::runtime_error("[ZipModule] An entry could not be added to the archive.");
        }
    }

    if (zip_close(za.get()) != 0)
    {
        throw std::runtime_error("[ZipModule] The archive could not be closed after creation.");
    }

    za.release();
}

// Answers every entry with its sizes and whether extracting it would be refused, without refusing the archive.
std::vector<ZipModule::ListedEntry> ZipModule::performList(const std::string& zipPath)
{
    int err = 0;
    ZipHandle za(zip_open(zipPath.c_str(), ZIP_RDONLY, &err));
    if (!za)
    {
        throw std::runtime_error("[ZipModule] The archive could not be opened.");
    }

    const zip_int64_t count = zip_get_num_entries(za.get(), 0);
    if (count < 0)
    {
        throw std::runtime_error("[ZipModule] The archive directory could not be read.");
    }

    std::vector<ListedEntry> entries;
    entries.reserve(static_cast<std::size_t>(count));
    for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(count); ++i)
    {
        zip_stat_t stat;
        zip_stat_init(&stat);
        if (zip_stat_index(za.get(), i, ZIP_FL_ENC_GUESS, &stat) != 0 || (stat.valid & ZIP_STAT_NAME) == 0)
        {
            throw std::runtime_error("[ZipModule] The entry at index " + std::to_string(i) + " could not be read.");
        }

        const std::string name(stat.name);
        const std::uint64_t size = (stat.valid & ZIP_STAT_SIZE) != 0 ? stat.size : 0;
        const std::uint64_t compressedSize = (stat.valid & ZIP_STAT_COMP_SIZE) != 0 ? stat.comp_size : 0;
        const bool unsafe = !ZipPath::entryPathSafe(name) || !ZipPath::staysInside(name);
        entries.push_back({name, size, compressedSize, unsafe});
    }

    return entries;
}

#endif

int ZipModule::luaExtract(lua_State* L)
{
#if defined(VARN_HAVE_LIBZIP) && VARN_HAVE_LIBZIP
    const std::string zipPath = varn::lua::LuaHelpers::checkString(L, 1);
    const std::string destDir = varn::lua::LuaHelpers::checkString(L, 2);

    auto& rt = luaRuntime(L);

    // clang-format off
    return varn::async::AsyncTask::runOnPool(L, rt, rt.taskPool(), "ZipModule", [zipPath, destDir](Promise& promise)
    {
        performExtract(zipPath, destDir);
        promise.resolve("ok");
    });
    // clang-format on
#else
    return luaL_error(L, "[ZipModule] The zip module is not available in this build.");
#endif
}

int ZipModule::luaCreate(lua_State* L)
{
#if defined(VARN_HAVE_LIBZIP) && VARN_HAVE_LIBZIP
    const std::string zipPath = varn::lua::LuaHelpers::checkString(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const int len = static_cast<int>(lua_rawlen(L, 2));
    if (len <= 0)
    {
        return luaL_error(L, "[ZipModule] The list of entries must not be empty.");
    }

    std::vector<std::pair<std::string, std::string>> items;
    items.reserve(static_cast<std::size_t>(len));

    for (int i = 1; i <= len; ++i)
    {
        lua_rawgeti(L, 2, i);
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            return luaL_error(L, "[ZipModule] Each entry must be a table.");
        }

        lua_getfield(L, -1, "file");
        const std::string file = varn::lua::LuaHelpers::checkString(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, -1, "entry");
        const std::string entry = varn::lua::LuaHelpers::checkString(L, -1);
        lua_pop(L, 1);
        lua_pop(L, 1);
        items.emplace_back(file, entry);
    }

    auto& rt = luaRuntime(L);

    // clang-format off
    return varn::async::AsyncTask::runOnPool(L, rt, rt.taskPool(), "ZipModule", [zipPath, items = std::move(items)](Promise& promise)
    {
        performCreate(zipPath, items);
        promise.resolve("ok");
    });
    // clang-format on
#else
    return luaL_error(L, "[ZipModule] The zip module is not available in this build.");
#endif
}

int ZipModule::luaList(lua_State* L)
{
#if defined(VARN_HAVE_LIBZIP) && VARN_HAVE_LIBZIP
    const std::string zipPath = varn::lua::LuaHelpers::checkString(L, 1);

    auto& rt = luaRuntime(L);

    // clang-format off
    return varn::async::AsyncTask::runOnPool(L, rt, rt.taskPool(), "ZipModule", [zipPath](Promise& promise)
    {
        std::vector<ListedEntry> entries = performList(zipPath);
        promise.resolveCustom([entries = std::move(entries)](lua_State* lua)
        {
            constexpr std::uint64_t maxSize = static_cast<std::uint64_t>(std::numeric_limits<lua_Integer>::max());
            lua_createtable(lua, static_cast<int>(entries.size()), 0);
            lua_Integer index = 0;
            for (const ListedEntry& entry : entries)
            {
                lua_createtable(lua, 0, 4);
                lua_pushlstring(lua, entry.name.data(), entry.name.size());
                lua_setfield(lua, -2, "name");
                lua_pushinteger(lua, static_cast<lua_Integer>(std::min(entry.size, maxSize)));
                lua_setfield(lua, -2, "size");
                lua_pushinteger(lua, static_cast<lua_Integer>(std::min(entry.compressedSize, maxSize)));
                lua_setfield(lua, -2, "compressedSize");
                lua_pushboolean(lua, entry.unsafe ? 1 : 0);
                lua_setfield(lua, -2, "unsafe");
                lua_rawseti(lua, -2, ++index);
            }
        });
    });
    // clang-format on
#else
    return luaL_error(L, "[ZipModule] The zip module is not available in this build.");
#endif
}

int ZipModule::luaOpen(lua_State* L)
{
    lua_newtable(L);
    lua_pushcfunction(L, &ZipModule::luaExtract);
    lua_setfield(L, -2, "extract");
    lua_pushcfunction(L, &ZipModule::luaCreate);
    lua_setfield(L, -2, "create");
    lua_pushcfunction(L, &ZipModule::luaList);
    lua_setfield(L, -2, "list");
    return 1;
}

void ZipModule::install(lua_State* L)
{
    luaL_requiref(L, "zip", &ZipModule::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::zip
