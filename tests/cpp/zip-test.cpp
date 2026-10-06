#include "ZipPath.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace varn::zip
{

TEST(ZipPath, EntrySafeAcceptsNormalRelativePaths)
{
    EXPECT_TRUE(ZipPath::entryPathSafe("file.txt"));
    EXPECT_TRUE(ZipPath::entryPathSafe("a/b/c.txt"));
    // Dots inside a component are fine, only a whole ".." component is traversal.
    EXPECT_TRUE(ZipPath::entryPathSafe("a..b.txt"));
    EXPECT_TRUE(ZipPath::entryPathSafe("v1.2..3/data"));
}

TEST(ZipPath, EntrySafeRejectsTraversalAbsoluteAndEmpty)
{
    EXPECT_FALSE(ZipPath::entryPathSafe(""));
    EXPECT_FALSE(ZipPath::entryPathSafe("/etc/passwd"));
    EXPECT_FALSE(ZipPath::entryPathSafe("\\windows\\system32"));
    EXPECT_FALSE(ZipPath::entryPathSafe("C:\\evil"));
    EXPECT_FALSE(ZipPath::entryPathSafe("../escape"));
    EXPECT_FALSE(ZipPath::entryPathSafe("a/../../b"));
    EXPECT_FALSE(ZipPath::entryPathSafe(".."));
    EXPECT_FALSE(ZipPath::entryPathSafe("deep/path/../../../etc"));
}

TEST(ZipPath, EntrySafeAcceptsCurrentFolderComponents)
{
    EXPECT_TRUE(ZipPath::entryPathSafe("sub/./file"));
    EXPECT_TRUE(ZipPath::entryPathSafe("./"));
    EXPECT_FALSE(ZipPath::entryPathSafe("sub/../../escape"));
    EXPECT_FALSE(ZipPath::entryPathSafe("sub\\..\\..\\escape"));
}

TEST(ZipPath, PathsRoundTripTheirUtf8Names)
{
    // A name outside ASCII must reach the file system as the same characters on Windows, whose narrow paths use the active code page.
    const std::string name = "caf\xc3\xa9/\xe6\x97\xa5\xe6\x9c\xac.txt";
    const std::filesystem::path path = ZipPath::toPath(name);
    EXPECT_EQ(path.filename().u8string(), u8"\u65e5\u672c.txt");
    EXPECT_EQ(ZipPath::fromPath(path.generic_u8string()), name);
}

TEST(ZipPath, PrintableMasksControlCharactersAndShortensLongNames)
{
    EXPECT_EQ(ZipPath::printable("a\nb\x01"
                                 "c\x7F"),
              "a?b?c?");
    EXPECT_EQ(ZipPath::printable(std::string(300, 'x')), std::string(200, 'x') + "...");
}

} // namespace varn::zip
