#include "varn/crypto/CryptoPrimitives.h"

#include <gtest/gtest.h>

#include <climits>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace varn::crypto
{

TEST(CryptoDigest, MatchesKnownSha256Vectors)
{
    EXPECT_EQ(CryptoPrimitives::digest("SHA256", "abc", true),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(CryptoPrimitives::digest("SHA256", "", true),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(CryptoDigest, RawOutputIsThirtyTwoBytes)
{
    EXPECT_EQ(CryptoPrimitives::digest("SHA256", "abc", false).size(), 32u);
}

TEST(CryptoDigest, RejectsUnknownAlgorithm)
{
    EXPECT_THROW(CryptoPrimitives::digest("NOPE-512", "x", true), std::exception);
    EXPECT_THROW(CryptoPrimitives::digest("", "x", true), std::exception);
}

TEST(CryptoHmac, MatchesKnownSha256Vector)
{
    EXPECT_EQ(CryptoPrimitives::hmac("SHA256", "key", "The quick brown fox jumps over the lazy dog", true),
              "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");
}

TEST(CryptoHmac, RejectsUnknownAlgorithm)
{
    EXPECT_THROW(CryptoPrimitives::hmac("NOPE", "key", "data", true), std::exception);
}

TEST(CryptoRandom, ReturnsTheRequestedLength)
{
    EXPECT_EQ(CryptoPrimitives::randomBytes(0).size(), 0u);
    EXPECT_EQ(CryptoPrimitives::randomBytes(16).size(), 16u);
}

TEST(CryptoRandom, OutputVariesAcrossCalls)
{
    EXPECT_NE(CryptoPrimitives::randomBytes(32), CryptoPrimitives::randomBytes(32));
}

TEST(CryptoRandom, AcceptsLargeInRangeCount)
{
    // a multi-megabyte request is honored because varn runs trusted local code
    EXPECT_EQ(CryptoPrimitives::randomBytes(4u * 1024u * 1024u).size(), 4u * 1024u * 1024u);
}

TEST(CryptoRandom, RejectsCountPastTheIntLimit)
{
    // the only hard bound is the underlying api's int length argument
    EXPECT_THROW(CryptoPrimitives::randomBytes(static_cast<std::size_t>(INT_MAX) + 1u), std::exception);
}

} // namespace varn::crypto
