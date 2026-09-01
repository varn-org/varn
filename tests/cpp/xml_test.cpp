#include "varn/xml/XmlSerializer.h"

#include <gtest/gtest.h>

#include <cctype>
#include <string>

namespace varn::xml
{

TEST(XmlSanitizeName, ReplacesUnsafeCharacters)
{
    EXPECT_EQ(XmlSerializer::sanitizeElementName("a b"), "a_b");
    EXPECT_EQ(XmlSerializer::sanitizeElementName("<script>"), "_script_");
    EXPECT_EQ(XmlSerializer::sanitizeElementName("a\"/>b"), "a___b");
}

TEST(XmlSanitizeName, GivesEmptyAndDigitLeadingNamesSafeForms)
{
    EXPECT_FALSE(XmlSerializer::sanitizeElementName("").empty());
    const std::string fromDigit = XmlSerializer::sanitizeElementName("1abc");
    ASSERT_FALSE(fromDigit.empty());
    EXPECT_FALSE(std::isdigit(static_cast<unsigned char>(fromDigit.front())));
}

TEST(XmlSanitizeName, PreservesUtf8Bytes)
{
    // a utf-8 name such as café must pass through rather than collapse to underscores
    EXPECT_EQ(XmlSerializer::sanitizeElementName("caf\xc3\xa9"), "caf\xc3\xa9");
}

TEST(XmlSanitizeText, StripsIllegalControlCharacters)
{
    const char raw[] = {'x', 0x01, 0x02, 0x08, 0x0b, 0x0c, 0x1f, 'y'};
    EXPECT_EQ(XmlSerializer::sanitizeText(raw, sizeof(raw)), "xy");

    const char embeddedNul[] = {'a', 0x00, 'b'};
    EXPECT_EQ(XmlSerializer::sanitizeText(embeddedNul, sizeof(embeddedNul)), "ab");
}

TEST(XmlSanitizeText, KeepsLegalWhitespaceAndUtf8)
{
    const std::string legal = "a\tb\nc\rd";
    EXPECT_EQ(XmlSerializer::sanitizeText(legal.data(), legal.size()), legal);

    const std::string utf8 = "caf\xc3\xa9";
    EXPECT_EQ(XmlSerializer::sanitizeText(utf8.data(), utf8.size()), utf8);
}

} // namespace varn::xml
