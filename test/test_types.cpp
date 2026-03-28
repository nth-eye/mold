#include "test_common.h"

// ---------------------------------------------------------------
// UUID tests
// ---------------------------------------------------------------

TEST_CASE("UUID JSON round-trip")
{
    session_t session = {
        .name = "My-Session",
        .uuid = {{0xf8, 0x1d, 0x4f, 0xae, 0x7d, 0xec, 0x11, 0xd0,
                   0xa7, 0x65, 0x00, 0xa0, 0xc9, 0x1e, 0x6b, 0xf6}}
    };

    char buf[1024];
    std::span<char> text = buf;
    REQUIRE(mold::json_from(session, text, 0) == mold::error_t::ok);

    std::string_view sv = {text.data(), text.size()};
    CHECK(sv.find("f81d4fae-7dec-11d0-a765-00a0c91e6bf6") != std::string_view::npos);

    session_t decoded = {};
    REQUIRE(mold::json_to(decoded, sv) == mold::error_t::ok);
    CHECK(decoded.uuid.bytes == session.uuid.bytes);
    CHECK(decoded.name == session.name);
}

TEST_CASE("UUID CBOR round-trip")
{
    session_t session = {
        .name = "My-Session",
        .uuid = {{0xf8, 0x1d, 0x4f, 0xae, 0x7d, 0xec, 0x11, 0xd0,
                   0xa7, 0x65, 0x00, 0xa0, 0xc9, 0x1e, 0x6b, 0xf6}}
    };

    session_t decoded = {};
    REQUIRE(cbor_roundtrip(session, decoded) == mold::error_t::ok);
    CHECK(decoded.uuid.bytes == session.uuid.bytes);
    CHECK(decoded.name == session.name);
}

struct uuid_holder_t {
    mold::uuid_t id;
};

TEST_CASE("UUID rejects invalid format")
{
    SUBCASE("wrong length")
    {
        static const char json[] = R"({"id": "too-short"})";
        std::string_view sv = json;
        uuid_holder_t instance = {};
        CHECK(mold::json_to(instance, sv) == mold::error_t::handler_failure);
    }
    SUBCASE("wrong separators")
    {
        static const char json[] = R"({"id": "f81d4fae_7dec_11d0_a765_00a0c91e6bf6"})";
        std::string_view sv = json;
        uuid_holder_t instance = {};
        CHECK(mold::json_to(instance, sv) == mold::error_t::handler_failure);
    }
}

// ---------------------------------------------------------------
// string_t tests
// ---------------------------------------------------------------

TEST_CASE("string_t JSON round-trip")
{
    profile_t profile = {
        .username = "john_doe",
        .bio = "Hello, I am John.",
        .age = 30,
    };

    profile_t decoded = {};
    REQUIRE(json_roundtrip(profile, decoded) == mold::error_t::ok);
    CHECK(decoded.username == profile.username);
    CHECK(decoded.bio == profile.bio);
    CHECK(decoded.age == profile.age);
}

TEST_CASE("string_t CBOR round-trip")
{
    profile_t profile = {
        .username = "john_doe",
        .bio = "Hello, I am John.",
        .age = 30,
    };

    profile_t decoded = {};
    REQUIRE(cbor_roundtrip(profile, decoded) == mold::error_t::ok);
    CHECK(decoded.username == profile.username);
    CHECK(decoded.bio == profile.bio);
    CHECK(decoded.age == profile.age);
}

TEST_CASE("string_t overflow truncates on construct")
{
    mold::string_t<4> s("hello world");
    CHECK(s.size() == 4);
    CHECK(std::string_view(s) == "hell");
}

TEST_CASE("string_t empty")
{
    mold::string_t<32> s;
    CHECK(s.empty());
    CHECK(s.size() == 0);
}

// ---------------------------------------------------------------
// bytes_t tests
// ---------------------------------------------------------------

TEST_CASE("bytes_t JSON round-trip (base64)")
{
    uint8_t raw[] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe, 0x01, 0x02, 0x03};
    packet_t pkt = {
        .payload = std::span<const uint8_t>(raw, sizeof(raw)),
        .seq = 42,
    };

    char buf[1024];
    std::span<char> text = buf;
    REQUIRE(mold::json_from(pkt, text, 0) == mold::error_t::ok);

    std::string_view sv = {text.data(), text.size()};
    CHECK(sv.find("3q2+78r+ur4BAgM=") != std::string_view::npos);

    packet_t decoded = {};
    REQUIRE(mold::json_to(decoded, sv) == mold::error_t::ok);
    CHECK(decoded.payload == pkt.payload);
    CHECK(decoded.seq == pkt.seq);
}

TEST_CASE("bytes_t CBOR round-trip")
{
    uint8_t raw[] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe, 0x01, 0x02, 0x03};
    packet_t pkt = {
        .payload = std::span<const uint8_t>(raw, sizeof(raw)),
        .seq = 42,
    };

    packet_t decoded = {};
    REQUIRE(cbor_roundtrip(pkt, decoded) == mold::error_t::ok);
    CHECK(decoded.payload == pkt.payload);
    CHECK(decoded.seq == pkt.seq);
}

TEST_CASE("bytes_t empty payload round-trip")
{
    packet_t pkt = { .payload = {}, .seq = 7 };

    packet_t json_decoded = {};
    REQUIRE(json_roundtrip(pkt, json_decoded) == mold::error_t::ok);
    CHECK(json_decoded.payload.size() == 0);
    CHECK(json_decoded.seq == 7);

    packet_t cbor_decoded = {};
    REQUIRE(cbor_roundtrip(pkt, cbor_decoded) == mold::error_t::ok);
    CHECK(cbor_decoded.payload.size() == 0);
    CHECK(cbor_decoded.seq == 7);
}

// ---------------------------------------------------------------
// bytes_view_t with writable buffer tests
// ---------------------------------------------------------------

TEST_CASE("bytes_view_t JSON round-trip with writable buffer")
{
    uint8_t raw[] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe};

    // Emit from a read-only view
    view_packet_t pkt = {
        .payload = mold::bytes_view_t(raw, sizeof(raw)),
        .seq = 99,
    };

    char buf[1024];
    std::span<char> text = buf;
    REQUIRE(mold::json_from(pkt, text, 0) == mold::error_t::ok);

    std::string_view sv = {text.data(), text.size()};
    CHECK(sv.find("3q2+78r+") != std::string_view::npos);

    // Decode into a writable view
    uint8_t decode_buf[64];
    view_packet_t decoded = {
        .payload = std::span<uint8_t>(decode_buf),
        .seq = 0,
    };
    REQUIRE(mold::json_to(decoded, sv) == mold::error_t::ok);
    CHECK(decoded.payload.size() == sizeof(raw));
    CHECK(memcmp(decoded.payload.ptr, raw, sizeof(raw)) == 0);
    CHECK(decoded.seq == 99);
}

TEST_CASE("bytes_view_t CBOR round-trip with writable buffer")
{
    uint8_t raw[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    view_packet_t pkt = {
        .payload = mold::bytes_view_t(raw, sizeof(raw)),
        .seq = 7,
    };

    uint8_t cbor_buf[256];
    std::span<uint8_t> bytes = cbor_buf;
    REQUIRE(mold::cbor_from(pkt, bytes) == mold::error_t::ok);

    uint8_t decode_buf[64];
    view_packet_t decoded = {
        .payload = std::span<uint8_t>(decode_buf),
        .seq = 0,
    };
    std::span<const uint8_t> cbor_in = {bytes.data(), bytes.size()};
    REQUIRE(mold::cbor_to(decoded, cbor_in) == mold::error_t::ok);
    CHECK(decoded.payload.size() == sizeof(raw));
    CHECK(memcmp(decoded.payload.ptr, raw, sizeof(raw)) == 0);
    CHECK(decoded.seq == 7);
}

TEST_CASE("bytes_view_t JSON read fails without capacity")
{
    static const char json[] = R"({"payload": "AQID", "seq": 1})";
    std::string_view sv = json;

    view_packet_t decoded = {}; // no buffer → cap=0
    CHECK(mold::json_to(decoded, sv) == mold::error_t::handler_failure);
}
