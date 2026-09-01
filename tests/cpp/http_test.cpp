#include "varn/http/MimeTypes.h"
#include "varn/http/Router.h"

#include "HttpMultipart.h"
#include "HttpRange.h"
#include "HttpText.h"
#include "HttpToken.h"
#include "HttpUrl.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace varn::http
{

TEST(Router, MatchesParamRouteAndCapturesValue)
{
    Router router;
    router.add("GET", "/users/:id");

    const MatchResult result = router.match("GET", "/users/42");
    ASSERT_EQ(result.status, MatchStatus::Found);
    ASSERT_EQ(result.params.size(), 1u);
    EXPECT_EQ(result.params[0].name, "id");
    EXPECT_EQ(result.params[0].value, "42");
}

TEST(Router, IntConstraintAcceptsDigitsAndRejectsLetters)
{
    Router router;
    const int route = router.add("GET", "/users/:id");
    router.setConstraint(route, "id", "int");

    EXPECT_EQ(router.match("GET", "/users/42").status, MatchStatus::Found);
    EXPECT_EQ(router.match("GET", "/users/abc").status, MatchStatus::NotFound);
}

TEST(Router, WildcardCapturesRemainingPathAsParam)
{
    Router router;
    router.add("GET", "/files/*");

    const MatchResult deep = router.match("GET", "/files/docs/2024/report.pdf");
    ASSERT_EQ(deep.status, MatchStatus::Found);
    ASSERT_EQ(deep.params.size(), 1u);
    EXPECT_EQ(deep.params[0].name, "wildcard");
    EXPECT_EQ(deep.params[0].value, "docs/2024/report.pdf");

    // the wildcard also matches an empty tail
    const MatchResult empty = router.match("GET", "/files");
    ASSERT_EQ(empty.status, MatchStatus::Found);
    ASSERT_EQ(empty.params.size(), 1u);
    EXPECT_EQ(empty.params[0].value, "");
}

TEST(Router, OptionalParamMatchesWithAndWithoutTheSegment)
{
    Router router;
    router.add("GET", "/posts/:id?");

    const MatchResult present = router.match("GET", "/posts/42");
    ASSERT_EQ(present.status, MatchStatus::Found);
    ASSERT_EQ(present.params.size(), 1u);
    EXPECT_EQ(present.params[0].name, "id");
    EXPECT_EQ(present.params[0].value, "42");

    const MatchResult absent = router.match("GET", "/posts");
    ASSERT_EQ(absent.status, MatchStatus::Found);
    EXPECT_TRUE(absent.params.empty());
}

TEST(Router, OptionalParamHonorsItsConstraint)
{
    Router router;
    const int route = router.add("GET", "/posts/:id?");
    router.setConstraint(route, "id", "int");

    EXPECT_EQ(router.match("GET", "/posts/7").status, MatchStatus::Found);
    EXPECT_EQ(router.match("GET", "/posts").status, MatchStatus::Found);

    // a present-but-invalid optional segment leaves a trailing part the route cannot consume
    EXPECT_EQ(router.match("GET", "/posts/abc").status, MatchStatus::NotFound);
}

TEST(Router, BuildsWildcardAndOptionalUrls)
{
    Router router;
    const int files = router.add("GET", "/files/*");
    router.setName(files, "files.show");
    const int posts = router.add("GET", "/posts/:id?");
    router.setName(posts, "posts.show");

    const auto wildcardUrl = router.buildUrl("files.show", {{"wildcard", "a/b c/d"}});
    ASSERT_TRUE(wildcardUrl.has_value());
    EXPECT_EQ(*wildcardUrl, "/files/a/b%20c/d");

    const auto withId = router.buildUrl("posts.show", {{"id", "9"}});
    ASSERT_TRUE(withId.has_value());
    EXPECT_EQ(*withId, "/posts/9");

    const auto withoutId = router.buildUrl("posts.show", {});
    ASSERT_TRUE(withoutId.has_value());
    EXPECT_EQ(*withoutId, "/posts");
}

TEST(Router, ReportsMethodNotAllowedForAKnownPath)
{
    Router router;
    router.add("GET", "/x");

    EXPECT_EQ(router.match("POST", "/x").status, MatchStatus::MethodNotAllowed);
}

TEST(Router, BuildsNamedUrlWithSegmentEncoding)
{
    Router router;
    const int route = router.add("GET", "/users/:id");
    router.setName(route, "users.show");

    const auto url = router.buildUrl("users.show", {{"id", "a b"}});
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(*url, "/users/a%20b");
}

TEST(MimeTypes, MapsKnownExtensionsToTheirType)
{
    EXPECT_NE(MimeTypes::forPath("page.html").find("text/html"), std::string::npos);
    EXPECT_NE(MimeTypes::forPath("data.json").find("application/json"), std::string::npos);
}

TEST(Base64Url, RoundTripsArbitraryBytes)
{
    const std::string input("\0\1\2hello-\xff_world", 16);
    std::string decoded;
    ASSERT_TRUE(HttpToken::base64UrlDecode(HttpToken::base64UrlEncode(input), decoded));
    EXPECT_EQ(decoded, input);
}

TEST(Base64Url, RejectsCharactersOutsideTheAlphabet)
{
    std::string out;
    EXPECT_FALSE(HttpToken::base64UrlDecode("abc*def", out));
}

TEST(Token, ConstantTimeEqualMatchesOnlyIdenticalStrings)
{
    EXPECT_TRUE(HttpToken::constantTimeEqual("secret", "secret"));
    EXPECT_FALSE(HttpToken::constantTimeEqual("secret", "secreT"));
    EXPECT_FALSE(HttpToken::constantTimeEqual("secret", "secre"));
}

TEST(Token, CsrfTokenIsBoundToSecretAndSession)
{
    const std::string token = HttpToken::makeCsrfToken("server-secret", "session-1");
    EXPECT_TRUE(HttpToken::validCsrfToken("server-secret", "session-1", token));
    EXPECT_FALSE(HttpToken::validCsrfToken("server-secret", "session-2", token));
    EXPECT_FALSE(HttpToken::validCsrfToken("other-secret", "session-1", token));
    EXPECT_FALSE(HttpToken::validCsrfToken("server-secret", "session-1", token + "x"));
}

TEST(UrlDecode, DecodesPercentAndPlusSequences)
{
    EXPECT_EQ(HttpUrl::decode("a+b"), "a b");
    EXPECT_EQ(HttpUrl::decode("%41%42"), "AB");
    EXPECT_EQ(HttpUrl::decode("100%25"), "100%");
}

TEST(UrlDecode, LeavesMalformedEscapesLiteral)
{
    EXPECT_EQ(HttpUrl::decode("%ZZ"), "%ZZ");
    EXPECT_EQ(HttpUrl::decode("trailing%4"), "trailing%4");
}

TEST(Multipart, ExtractsBoundaryIncludingQuotedAndTrailingForms)
{
    EXPECT_EQ(HttpMultipart::extractBoundary("multipart/form-data; boundary=xyz"), "xyz");
    EXPECT_EQ(HttpMultipart::extractBoundary("multipart/form-data; boundary=\"a b c\""), "a b c");
    EXPECT_EQ(HttpMultipart::extractBoundary("multipart/form-data; boundary=xyz; charset=utf-8"), "xyz");
    EXPECT_EQ(HttpMultipart::extractBoundary("text/plain"), "");
}

TEST(Multipart, ReadsQuotedAttributesAtTokenBoundary)
{
    const std::string headers = "Content-Disposition: form-data; name=\"field\"; filename=\"a.txt\"";
    EXPECT_EQ(HttpMultipart::multipartAttribute(headers, "name=\""), "field");
    EXPECT_EQ(HttpMultipart::multipartAttribute(headers, "filename=\""), "a.txt");
}

TEST(Multipart, DoesNotMatchAttributeInsideAnotherToken)
{
    // searching name=" must not match the name inside filename="
    const std::string headers = "Content-Disposition: form-data; filename=\"a.txt\"";
    EXPECT_EQ(HttpMultipart::multipartAttribute(headers, "name=\""), "");
}

TEST(Multipart, ExtractsPartContentType)
{
    const std::string headers = "Content-Disposition: form-data; name=\"f\"\r\nContent-Type: image/png\r\n";
    EXPECT_EQ(HttpMultipart::multipartContentType(headers), "image/png");
    EXPECT_EQ(HttpMultipart::multipartContentType("Content-Disposition: form-data"), "");
}

TEST(HttpByteRange, ParsesClosedOpenAndSuffixForms)
{
    std::uintmax_t start = 0;
    std::uintmax_t end = 0;
    ASSERT_TRUE(HttpRange::parse("bytes=0-99", 1000, start, end));
    EXPECT_EQ(start, 0u);
    EXPECT_EQ(end, 99u);
    ASSERT_TRUE(HttpRange::parse("bytes=500-", 1000, start, end));
    EXPECT_EQ(start, 500u);
    EXPECT_EQ(end, 999u);
    ASSERT_TRUE(HttpRange::parse("bytes=-100", 1000, start, end));
    EXPECT_EQ(start, 900u);
    EXPECT_EQ(end, 999u);
}

TEST(HttpByteRange, ClampsEndBeyondTheFile)
{
    std::uintmax_t start = 0;
    std::uintmax_t end = 0;
    ASSERT_TRUE(HttpRange::parse("bytes=0-99999", 1000, start, end));
    EXPECT_EQ(end, 999u);
}

TEST(HttpByteRange, RejectsInvalidInvertedAndOverflowing)
{
    std::uintmax_t start = 0;
    std::uintmax_t end = 0;
    EXPECT_FALSE(HttpRange::parse("bytes=abc", 1000, start, end));
    EXPECT_FALSE(HttpRange::parse("0-99", 1000, start, end));
    EXPECT_FALSE(HttpRange::parse("bytes=0-99", 0, start, end));
    EXPECT_FALSE(HttpRange::parse("bytes=900-100", 1000, start, end));
    EXPECT_FALSE(HttpRange::parse("bytes=99999999999999999999999999-", 1000, start, end));
    // non-digit endpoints must be rejected, not silently coerced by stoull
    EXPECT_FALSE(HttpRange::parse("bytes=+5-10", 1000, start, end));
    EXPECT_FALSE(HttpRange::parse("bytes= 5-10", 1000, start, end));
    EXPECT_FALSE(HttpRange::parse("bytes=5abc-10", 1000, start, end));
}

TEST(HttpTextCompare, CaseFoldsAndComparesInsensitively)
{
    EXPECT_EQ(HttpText::toLower("AbC-XYZ"), "abc-xyz");
    EXPECT_TRUE(HttpText::iequals("Content-Type", "content-type"));
    EXPECT_FALSE(HttpText::iequals("Content-Type", "content-length"));
}

} // namespace varn::http
