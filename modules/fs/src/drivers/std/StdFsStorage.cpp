#include "varn/fs/FsStorage.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace varn::fs
{

namespace
{
class StdFsPathHelpers
{
public:
    // build a path from utf-8 code units so windows converts to the native wide encoding instead of the active code page
    static std::filesystem::path toPath(const std::string& utf8)
    {
        return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
    }

    // return a native path as utf-8 so listings and generated names read the same across desktop os
    static std::string fromPath(const std::filesystem::path& path)
    {
        const std::u8string encoded = path.u8string();
        return std::string(encoded.begin(), encoded.end());
    }
};
} // namespace

std::string FsStorage::readAll(const std::string& path)
{
    const std::filesystem::path p = StdFsPathHelpers::toPath(path);

    // refuse endless streams like devices or fifos so a read into memory always terminates
    std::error_code ec;
    if (!std::filesystem::is_regular_file(std::filesystem::status(p, ec)) || ec)
    {
        throw std::runtime_error("[FsStorage] The path is not a readable regular file.");
    }

    std::ifstream file(p, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("[FsStorage] The file could not be opened for reading.");
    }

    // read the whole file into memory in chunks
    std::string out;
    char chunk[65536];
    while (file)
    {
        file.read(chunk, sizeof(chunk));
        const std::streamsize got = file.gcount();
        if (got <= 0)
        {
            break;
        }

        out.append(chunk, static_cast<std::size_t>(got));
    }

    // report a read fault instead of returning a truncated result
    if (file.bad())
    {
        throw std::runtime_error("[FsStorage] The file could not be read.");
    }

    return out;
}

void FsStorage::writeAll(const std::string& path, const std::string& content)
{
    const std::filesystem::path p = StdFsPathHelpers::toPath(path);
    if (p.has_parent_path())
    {
        std::filesystem::create_directories(p.parent_path());
    }

    std::ofstream file(p, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("[FsStorage] The file could not be opened for writing.");
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    file.flush();
    if (!file)
    {
        throw std::runtime_error("[FsStorage] The file could not be written.");
    }
}

bool FsStorage::exists(const std::string& path)
{
    return std::filesystem::exists(StdFsPathHelpers::toPath(path));
}

void FsStorage::mkdir(const std::string& path)
{
    std::error_code ec;
    std::filesystem::create_directories(StdFsPathHelpers::toPath(path), ec);
    if (ec)
    {
        throw std::runtime_error("[FsStorage] " + ec.message() + ".");
    }
}

void FsStorage::removeRecursive(const std::string& path)
{
    std::error_code ec;
    std::filesystem::remove_all(StdFsPathHelpers::toPath(path), ec);
    if (ec)
    {
        throw std::runtime_error("[FsStorage] " + ec.message() + ".");
    }
}

FsStat FsStorage::stat(const std::string& path)
{
    const std::filesystem::path p = StdFsPathHelpers::toPath(path);
    std::error_code ec;

    // read symlink status without following the target
    const std::filesystem::file_status linkStatus = std::filesystem::symlink_status(p, ec);
    if (ec || linkStatus.type() == std::filesystem::file_type::not_found)
    {
        throw std::runtime_error("[FsStorage] The path does not exist.");
    }

    const std::filesystem::file_status status = std::filesystem::status(p, ec);
    if (ec)
    {
        throw std::runtime_error("[FsStorage] " + ec.message() + ".");
    }

    FsStat result;
    result.isDir = std::filesystem::is_directory(status);
    result.isFile = std::filesystem::is_regular_file(status);
    result.isSymlink = std::filesystem::is_symlink(linkStatus);

    if (result.isFile)
    {
        result.size = std::filesystem::file_size(p, ec);
        if (ec)
        {
            result.size = 0;
        }
    }

    const auto fileTime = std::filesystem::last_write_time(p, ec);
    if (!ec)
    {
        // map the file clock onto the system clock to get epoch seconds
        const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        result.mtime = static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(systemTime));
    }

    return result;
}

std::vector<std::string> FsStorage::readdir(const std::string& path)
{
    std::error_code ec;
    std::filesystem::directory_iterator it(StdFsPathHelpers::toPath(path), ec);
    if (ec)
    {
        throw std::runtime_error("[FsStorage] " + ec.message() + ".");
    }

    std::vector<std::string> names;
    for (const auto& entry : it)
    {
        names.push_back(StdFsPathHelpers::fromPath(entry.path().filename()));
    }

    return names;
}

void FsStorage::rename(const std::string& from, const std::string& to)
{
    std::error_code ec;
    std::filesystem::rename(StdFsPathHelpers::toPath(from), StdFsPathHelpers::toPath(to), ec);
    if (ec)
    {
        throw std::runtime_error("[FsStorage] " + ec.message() + ".");
    }
}

void FsStorage::copy(const std::string& from, const std::string& to)
{
    std::error_code ec;
    std::filesystem::copy_file(StdFsPathHelpers::toPath(from), StdFsPathHelpers::toPath(to), std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        throw std::runtime_error("[FsStorage] " + ec.message() + ".");
    }
}

void FsStorage::append(const std::string& path, const std::string& data)
{
    const std::filesystem::path p = StdFsPathHelpers::toPath(path);
    if (p.has_parent_path())
    {
        std::filesystem::create_directories(p.parent_path());
    }

    std::ofstream file(p, std::ios::binary | std::ios::app);
    if (!file)
    {
        throw std::runtime_error("[FsStorage] The file could not be opened for appending.");
    }

    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    file.flush();
    if (!file)
    {
        throw std::runtime_error("[FsStorage] The file could not be appended.");
    }
}

std::string FsStorage::mkdtemp(const std::string& prefix)
{
    const std::filesystem::path base = StdFsPathHelpers::toPath(prefix);
    if (base.has_parent_path())
    {
        std::filesystem::create_directories(base.parent_path());
    }

    std::random_device device;
    std::mt19937_64 generator(device());
    std::uniform_int_distribution<std::uint64_t> distribution;

    // probe random suffixes until create_directory claims an unused name
    for (int attempt = 0; attempt < 256; ++attempt)
    {
        const std::string suffix = std::to_string(distribution(generator));
        std::filesystem::path candidate = base;
        candidate += suffix;

        std::error_code ec;
        if (std::filesystem::create_directory(candidate, ec) && !ec)
        {
            // restrict the directory to its owner like posix mkdtemp so its contents stay private to this user
            std::filesystem::permissions(candidate, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, ec);
            return StdFsPathHelpers::fromPath(candidate);
        }
    }

    throw std::runtime_error("[FsStorage] A unique temporary directory could not be created.");
}

namespace
{
class StdFsHandle : public FsHandle
{
public:
    explicit StdFsHandle(std::fstream stream)
        : stream(std::move(stream))
    {
    }

    std::string read(std::size_t maxBytes) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!stream.is_open())
        {
            throw std::runtime_error("[FsStorage] The file handle is closed.");
        }

        // grow the buffer in chunks so a large maxBytes never pre-allocates more than the file actually holds
        constexpr std::size_t kChunk = 1u << 16;
        std::string buffer;
        while (buffer.size() < maxBytes)
        {
            const std::size_t want = std::min<std::size_t>(kChunk, maxBytes - buffer.size());
            const std::size_t base = buffer.size();
            buffer.resize(base + want);
            stream.read(buffer.data() + base, static_cast<std::streamsize>(want));
            const std::streamsize got = stream.gcount();

            // report a read fault instead of returning a short read
            if (stream.bad())
            {
                throw std::runtime_error("[FsStorage] The file handle could not be read.");
            }

            buffer.resize(base + static_cast<std::size_t>(got));
            if (static_cast<std::size_t>(got) < want)
            {
                break;
            }
        }

        // clear eof and fail bits to keep the handle usable
        stream.clear();
        return buffer;
    }

    void write(const std::string& data) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!stream.is_open())
        {
            throw std::runtime_error("[FsStorage] The file handle is closed.");
        }

        stream.write(data.data(), static_cast<std::streamsize>(data.size()));
        stream.flush();
        if (!stream)
        {
            throw std::runtime_error("[FsStorage] The file handle could not be written.");
        }
    }

    void close() override
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stream.is_open())
        {
            stream.close();
        }
    }

private:
    std::fstream stream;
    std::mutex mutex;
};

class StdFsHelpers
{
public:
    static std::ios::openmode openmodeFor(const std::string& mode)
    {
        const std::ios::openmode binary = std::ios::binary;
        if (mode == "r")
        {
            return binary | std::ios::in;
        }
        if (mode == "w")
        {
            return binary | std::ios::out | std::ios::trunc;
        }
        if (mode == "a")
        {
            return binary | std::ios::out | std::ios::app;
        }
        if (mode == "r+")
        {
            return binary | std::ios::in | std::ios::out;
        }
        if (mode == "w+")
        {
            return binary | std::ios::in | std::ios::out | std::ios::trunc;
        }
        if (mode == "a+")
        {
            return binary | std::ios::in | std::ios::out | std::ios::app;
        }

        throw std::runtime_error("[FsStorage] The file mode is not one of r, w, a, r+, w+, a+.");
    }
};
} // namespace

std::shared_ptr<FsHandle> FsStorage::open(const std::string& path, const std::string& mode)
{
    const std::ios::openmode flags = StdFsHelpers::openmodeFor(mode);
    const std::filesystem::path p = StdFsPathHelpers::toPath(path);

    if (mode == "w" || mode == "a" || mode == "w+" || mode == "a+")
    {
        if (p.has_parent_path())
        {
            std::filesystem::create_directories(p.parent_path());
        }
    }

    std::fstream stream(p, flags);
    if (!stream.is_open())
    {
        throw std::runtime_error("[FsStorage] The file could not be opened.");
    }

    return std::make_shared<StdFsHandle>(std::move(stream));
}

} // namespace varn::fs
