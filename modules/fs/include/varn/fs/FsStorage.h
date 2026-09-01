#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace varn::fs
{

struct FsStat
{
    std::uintmax_t size = 0;
    std::int64_t mtime = 0;
    bool isDir = false;
    bool isFile = false;
    bool isSymlink = false;
};

class FsHandle
{
public:
    virtual ~FsHandle() = default;

    virtual std::string read(std::size_t maxBytes) = 0;
    virtual void write(const std::string& data) = 0;
    virtual void close() = 0;
};

class FsStorage
{
public:
    FsStorage() = delete;

    static std::string readAll(const std::string& path);
    static void writeAll(const std::string& path, const std::string& content);
    static bool exists(const std::string& path);
    static void mkdir(const std::string& path);
    static void removeRecursive(const std::string& path);
    static std::shared_ptr<FsHandle> open(const std::string& path, const std::string& mode);
    static FsStat stat(const std::string& path);
    static std::vector<std::string> readdir(const std::string& path);
    static void rename(const std::string& from, const std::string& to);
    static void copy(const std::string& from, const std::string& to);
    static void append(const std::string& path, const std::string& data);
    static std::string mkdtemp(const std::string& prefix);
};

} // namespace varn::fs
