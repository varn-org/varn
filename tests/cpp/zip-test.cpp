#include "ZipPath.h"

#include <gtest/gtest.h>

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

TEST(ZipPath, StaysInsideResolvesDotsLexically)
{
    EXPECT_TRUE(ZipPath::staysInside("sub/file"));
    EXPECT_TRUE(ZipPath::staysInside("sub/./file"));
    EXPECT_TRUE(ZipPath::staysInside("./"));
    EXPECT_FALSE(ZipPath::staysInside("../escape"));
    EXPECT_FALSE(ZipPath::staysInside("sub/../../escape"));
    EXPECT_FALSE(ZipPath::staysInside("/etc/passwd"));
}

TEST(ZipPath, PrintableMasksControlCharactersAndShortensLongNames)
{
    EXPECT_EQ(ZipPath::printable("a\nb\x01"
                                 "c\x7F"),
              "a?b?c?");
    EXPECT_EQ(ZipPath::printable(std::string(300, 'x')), std::string(200, 'x') + "...");
}

} // namespace varn::zip
