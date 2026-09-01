#include "varn/platform/PlatformInfo.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace varn::platform
{

TEST(PlatformLibraryName, RejectsEmptyName)
{
    EXPECT_THROW(PlatformInfo::libraryFilenameForName(""), std::exception);
}

TEST(PlatformLibraryName, AppliesPlatformPrefixAndSuffix)
{
    const std::string name = PlatformInfo::libraryFilenameForName("foo");
    const std::string suffix = PlatformInfo::shlibSuffix();
    EXPECT_NE(name.find("foo"), std::string::npos);
    ASSERT_GE(name.size(), suffix.size());
    EXPECT_EQ(name.substr(name.size() - suffix.size()), suffix);
}

#ifndef _WIN32
// on posix an existing lib prefix or shared-object extension is normalized to a single canonical form
TEST(PlatformLibraryName, NormalizesExistingPrefixAndExtension)
{
    const std::string canonical = PlatformInfo::libraryFilenameForName("foo");
    EXPECT_EQ(PlatformInfo::libraryFilenameForName("libfoo"), canonical);
    EXPECT_EQ(PlatformInfo::libraryFilenameForName("foo" + PlatformInfo::shlibSuffix()), canonical);
}
#endif

} // namespace varn::platform
