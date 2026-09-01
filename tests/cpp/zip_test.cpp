#include "ZipPath.h"

#include <gtest/gtest.h>

namespace varn::zip
{

TEST(ZipPath, EntrySafeAcceptsNormalRelativePaths)
{
    EXPECT_TRUE(ZipPath::entryPathSafe("file.txt"));
    EXPECT_TRUE(ZipPath::entryPathSafe("a/b/c.txt"));
    // dots inside a component are fine, only a whole ".." component is traversal
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

TEST(ZipPath, IsSubpathDetectsContainment)
{
    EXPECT_TRUE(ZipPath::isSubpath("/tmp/root", "/tmp/root/sub/file"));
    EXPECT_FALSE(ZipPath::isSubpath("/tmp/root", "/tmp/other"));
    EXPECT_FALSE(ZipPath::isSubpath("/tmp/root", "/tmp/root/../escape"));
}

} // namespace varn::zip
