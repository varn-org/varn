#include "varn/fs/FsStorage.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace varn::fs
{

std::string FsStorage::readAll(const std::string& /*path*/)
{
    throw std::runtime_error("[FsStorage] The file system module is not available in this build.");
}

void FsStorage::writeAll(const std::string& /*path*/, const std::string& /*content*/)
{
    throw std::runtime_error("[FsStorage] The file system module is not available in this build.");
}

bool FsStorage::exists(const std::string& /*path*/)
{
    return false;
}

void FsStorage::mkdir(const std::string& /*path*/)
{
    throw std::runtime_error("[FsStorage] The file system module is not available in this build.");
}

void FsStorage::removeRecursive(const std::string& /*path*/)
{
    throw std::runtime_error("[FsStorage] The file system module is not available in this build.");
}

std::shared_ptr<FsHandle> FsStorage::open(const std::string& /*path*/, const std::string& /*mode*/)
{
    throw std::runtime_error("[FsStorage] The file system module is not available in this build.");
}

FsStat FsStorage::stat(const std::string& /*path*/)
{
    throw std::runtime_error("[FsStorage] The file system module is not available in this build.");
}

std::vector<std::string> FsStorage::readdir(const std::string& /*path*/)
{
    throw std::runtime_error("[FsStorage] The file system module is not available in this build.");
}

void FsStorage::rename(const std::string& /*from*/, const std::string& /*to*/)
{
    throw std::runtime_error("[FsStorage] The file system module is not available in this build.");
}

void FsStorage::copy(const std::string& /*from*/, const std::string& /*to*/)
{
    throw std::runtime_error("[FsStorage] The file system module is not available in this build.");
}

void FsStorage::append(const std::string& /*path*/, const std::string& /*data*/)
{
    throw std::runtime_error("[FsStorage] The file system module is not available in this build.");
}

std::string FsStorage::mkdtemp(const std::string& /*prefix*/)
{
    throw std::runtime_error("[FsStorage] The file system module is not available in this build.");
}

} // namespace varn::fs
