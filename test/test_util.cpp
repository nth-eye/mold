#include "test_common.h"
#include <mold/util/half.h>
#include <cmath>

// ---------------------------------------------------------------
// half_t unit tests
// ---------------------------------------------------------------

TEST_CASE("half_t bit patterns")
{
    using mold::half_t;

    SUBCASE("known IEEE 754 binary16 bit patterns")
    {
        CHECK(std::bit_cast<uint16_t>(half_t(0.0f))   == 0x0000);
        CHECK(std::bit_cast<uint16_t>(half_t(1.0f))   == 0x3c00);
        CHECK(std::bit_cast<uint16_t>(half_t(1.5f))   == 0x3e00);
        CHECK(std::bit_cast<uint16_t>(half_t(2.0f))   == 0x4000);
        CHECK(std::bit_cast<uint16_t>(half_t(2.25f))  == 0x4080);
        CHECK(std::bit_cast<uint16_t>(half_t(-1.0f))  == 0xbc00);
        CHECK(std::bit_cast<uint16_t>(half_t(0.5f))   == 0x3800);
        CHECK(std::bit_cast<uint16_t>(half_t(0.25f))  == 0x3400);
    }

    SUBCASE("round-trip float->half->float")
    {
        float values[] = {0.0f, 1.0f, -1.0f, 1.5f, 2.25f, 3.125f, 0.5f, 100.0f};
        for (float v : values) {
            float rt = float(half_t(v));
            CHECK(rt == v);
        }
    }

    SUBCASE("double construction")
    {
        half_t h(3.125);
        CHECK(float(h) == 3.125f);
        CHECK(double(h) == 3.125);
    }
}

TEST_CASE("half_t special values")
{
    using mold::half_t;

    SUBCASE("positive and negative zero")
    {
        half_t pz(0.0f);
        half_t nz(-0.0f);
        CHECK(std::bit_cast<uint16_t>(pz) == 0x0000);
        CHECK(std::bit_cast<uint16_t>(nz) == 0x8000);
        CHECK(pz == nz); // IEEE: +0 == -0
    }

    SUBCASE("infinity")
    {
        half_t inf = std::numeric_limits<half_t>::infinity();
        CHECK(std::bit_cast<uint16_t>(inf) == 0x7c00);
        CHECK(std::isinf(float(inf)));

        half_t from_float(std::numeric_limits<float>::infinity());
        CHECK(std::bit_cast<uint16_t>(from_float) == 0x7c00);
    }

    SUBCASE("numeric_limits min/max/lowest")
    {
        CHECK(std::bit_cast<uint16_t>(std::numeric_limits<half_t>::min())    == 0x0400);
        CHECK(std::bit_cast<uint16_t>(std::numeric_limits<half_t>::max())    == 0x7bff);
        CHECK(std::bit_cast<uint16_t>(std::numeric_limits<half_t>::lowest()) == 0xfbff);

        CHECK(float(std::numeric_limits<half_t>::max()) == doctest::Approx(65504.0f));
    }
}

TEST_CASE("half_t comparisons")
{
    using mold::half_t;

    CHECK(half_t(1.0f) == half_t(1.0f));
    CHECK(half_t(1.0f) != half_t(2.0f));
    CHECK(half_t(1.0f) < half_t(2.0f));
    CHECK(half_t(2.0f) > half_t(1.0f));
    CHECK(half_t(-1.0f) < half_t(1.0f));
    CHECK(half_t(1.0f) <= half_t(1.0f));
    CHECK(half_t(1.0f) >= half_t(1.0f));
}

TEST_CASE("half_t overflow produces non-finite value")
{
    using mold::half_t;

    half_t big(100000.0f);
    CHECK_FALSE(std::isfinite(float(big)));
}

// ---------------------------------------------------------------
// Base64 unit tests
// ---------------------------------------------------------------

TEST_CASE("base64 encode/decode")
{
    SUBCASE("empty")
    {
        char out[4];
        CHECK(mold::base64_encode(nullptr, 0, out) == 0);
    }
    SUBCASE("1 byte")
    {
        uint8_t src[] = {0x61}; // 'a'
        char out[8];
        size_t n = mold::base64_encode(src, 1, out);
        CHECK(std::string_view(out, n) == "YQ==");
    }
    SUBCASE("3 bytes")
    {
        uint8_t src[] = {0x61, 0x62, 0x63}; // "abc"
        char out[8];
        size_t n = mold::base64_encode(src, 3, out);
        CHECK(std::string_view(out, n) == "YWJj");
    }
    SUBCASE("round-trip")
    {
        uint8_t src[] = {0x00, 0xff, 0x80, 0x7f, 0x01};
        char encoded[12];
        size_t enc_n = mold::base64_encode(src, 5, encoded);

        uint8_t decoded[8];
        size_t dec_n = mold::base64_decode({encoded, enc_n}, decoded);
        CHECK(dec_n == 5);
        CHECK(memcmp(src, decoded, 5) == 0);
    }
    SUBCASE("invalid input")
    {
        CHECK(mold::base64_decode("abc", nullptr) == 0); // not multiple of 4
    }
}
