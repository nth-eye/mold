#include "test_common.h"

// ---------------------------------------------------------------
// CBOR tests
// ---------------------------------------------------------------

TEST_CASE("CBOR encode complex struct")
{
    auto instance = make_outer_instance();
    uint8_t buf[2048];
    std::span<uint8_t> bytes = buf;
    REQUIRE(mold::cbor_from(instance, bytes) == mold::error_t::ok);
    CHECK(bytes.size() > 0);
}

TEST_CASE("CBOR round-trip complex struct")
{
    auto instance = make_outer_instance();
    outer_t decoded = {};
    REQUIRE(cbor_roundtrip(instance, decoded) == mold::error_t::ok);

    CHECK(decoded.first == instance.first);
    CHECK(decoded.second == instance.second);
    CHECK(decoded.third == instance.third);
    CHECK(decoded.fourth.i8 == instance.fourth.i8);
    CHECK(decoded.fourth.i32 == instance.fourth.i32);
    CHECK(decoded.fourth.u32 == instance.fourth.u32);
    CHECK(decoded.fourth.u64 == instance.fourth.u64);
    CHECK(decoded.fourth.deep.regular_double == instance.fourth.deep.regular_double);
    CHECK(decoded.eighth == instance.eighth);
    CHECK(decoded.thirteenth == instance.thirteenth);
    CHECK(decoded.fourteenth == instance.fourteenth);
    CHECK(decoded.optional_int.value() == 42);
    CHECK(decoded.optional_deepnest_absent.has_value() == false);
    CHECK(decoded.optional_string_present.value() == "Hello Optional");
    CHECK(decoded.optional_vector.value() == instance.optional_vector.value());
    CHECK(decoded.nullable_int.value() == 99);
    CHECK(decoded.nullable_string_absent.has_value() == false);
    CHECK(decoded.nullable_string_present.value() == "Hello Nullable");
}

// ---------------------------------------------------------------
// uint64_t max value (CBOR)
// ---------------------------------------------------------------

struct u64_struct_t {
    uint64_t val;
};

TEST_CASE("uint64_t max CBOR round-trip")
{
    u64_struct_t instance = { .val = 18446744073709551615ULL };
    u64_struct_t decoded = {};
    REQUIRE(cbor_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.val == 18446744073709551615ULL);
}

// ---------------------------------------------------------------
// Float16 precision selection
// ---------------------------------------------------------------

struct float_struct_t {
    float val;
};

TEST_CASE("CBOR encodes float16 when value fits")
{
    // 1.0 is exactly representable in float16
    float_struct_t instance = {1.0f};
    uint8_t buf[64];
    std::span<uint8_t> bytes = buf;
    REQUIRE(mold::cbor_from(instance, bytes) == mold::error_t::ok);

    // Map(1) key "val" then float16(1.0)
    // float16 marker is 0xf9, 1.0 in float16 is 0x3c00
    bool found_f9 = false;
    for (size_t i = 0; i + 2 < bytes.size(); ++i) {
        if (bytes[i] == 0xf9 && bytes[i+1] == 0x3c && bytes[i+2] == 0x00) {
            found_f9 = true;
            break;
        }
    }
    CHECK(found_f9);
}

TEST_CASE("CBOR encodes float32 when value doesn't fit float16")
{
    // 0.1 is not exactly representable in float16
    float_struct_t instance = {0.1f};
    uint8_t buf[64];
    std::span<uint8_t> bytes = buf;
    REQUIRE(mold::cbor_from(instance, bytes) == mold::error_t::ok);

    // float32 marker is 0xfa
    bool found_fa = false;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] == 0xfa) {
            found_fa = true;
            break;
        }
    }
    CHECK(found_fa);
}

TEST_CASE("CBOR float16 round-trips correctly")
{
    float_struct_t instance = {0.5f}; // exact in float16
    float_struct_t decoded = {};
    REQUIRE(cbor_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.val == 0.5f);
}

// ---------------------------------------------------------------
// field_t CBOR integer key tests
// ---------------------------------------------------------------

#if (MOLD_REFLECTION_CBOR_FIELD_KEYS)

struct cose_like_t {
    mold::field_t<1, int32_t> alg;
    mold::field_t<4, std::string> kid;
};

TEST_CASE("field_t CBOR round-trip with integer keys")
{
    cose_like_t instance = {.alg = -7, .kid = {"our-key"}};
    cose_like_t decoded = {};
    REQUIRE(cbor_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.alg.value == -7);
    CHECK(decoded.kid.value == "our-key");
}

TEST_CASE("field_t CBOR wire format uses integer keys")
{
    cose_like_t instance = {.alg = -7, .kid = {"our-key"}};
    uint8_t buf[128];
    std::span<uint8_t> bytes = buf;
    REQUIRE(mold::cbor_from(instance, bytes) == mold::error_t::ok);

    // Map of 2 items: a2
    CHECK(bytes[0] == 0xa2);
    // Key 1 (unsigned int): 01
    CHECK(bytes[1] == 0x01);
    // Key 4 (unsigned int): 04
    // Find key 4 after the value for key 1
    // Value -7 = nint(6) = 0x26
    CHECK(bytes[2] == 0x26);
    CHECK(bytes[3] == 0x04);
}

struct mixed_keys_t {
    mold::field_t<1, bool> enabled;
    std::string name;
    mold::field_t<3, uint16_t> version;
};

TEST_CASE("field_t CBOR mixed keys (integer + string)")
{
    mixed_keys_t instance = {.enabled = true, .name = "test", .version = 42};
    mixed_keys_t decoded = {};
    REQUIRE(cbor_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.enabled.value == true);
    CHECK(decoded.name == "test");
    CHECK(decoded.version.value == 42);
}

TEST_CASE("field_t CBOR wire format mixed keys")
{
    mixed_keys_t instance = {.enabled = true, .name = "test", .version = 42};
    uint8_t buf[128];
    std::span<uint8_t> bytes = buf;
    REQUIRE(mold::cbor_from(instance, bytes) == mold::error_t::ok);

    // Map of 3: a3
    CHECK(bytes[0] == 0xa3);
    // Key 1 (integer): 01
    CHECK(bytes[1] == 0x01);
    // Value true: f5
    CHECK(bytes[2] == 0xf5);
    // Key "name" (text string, 4 chars): 64 6e 61 6d 65
    CHECK(bytes[3] == 0x64);
    CHECK(bytes[4] == 'n');
}

struct negative_key_t {
    mold::field_t<-1, int32_t> val;
};

TEST_CASE("field_t CBOR negative integer key")
{
    negative_key_t instance = {.val = 100};
    negative_key_t decoded = {};
    REQUIRE(cbor_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.val.value == 100);
}

enum class cbor_key_e : int16_t { alg = 1, kid = 4 };

struct enum_key_t {
    mold::field_t<cbor_key_e::alg, int32_t> alg;
    mold::field_t<cbor_key_e::kid, std::string> kid;
};

TEST_CASE("field_t CBOR enum key")
{
    enum_key_t instance = {.alg = -7, .kid = {"our-key"}};
    enum_key_t decoded = {};
    REQUIRE(cbor_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.alg.value == -7);
    CHECK(decoded.kid.value == "our-key");
}

TEST_CASE("field_t JSON round-trip uses member names")
{
    cose_like_t instance = {.alg = -7, .kid = {"our-key"}};
    char buf[256];
    std::span<char> text = buf;
    REQUIRE(mold::json_from(instance, text, 0) == mold::error_t::ok);

    std::string_view json_sv = {text.data(), text.size()};
    // JSON should use member names "alg" and "kid", not integer keys
    CHECK(json_sv.find("\"alg\"") != std::string_view::npos);
    CHECK(json_sv.find("\"kid\"") != std::string_view::npos);

    cose_like_t decoded = {};
    std::string_view sv2 = {text.data(), text.size()};
    REQUIRE(mold::json_to(decoded, sv2) == mold::error_t::ok);
    CHECK(decoded.alg.value == -7);
    CHECK(decoded.kid.value == "our-key");
}

struct field_container_t {
    mold::field_t<1, std::vector<int>> items;
    mold::field_t<2, std::array<uint8_t, 3>> triple;
};

TEST_CASE("field_t CBOR with container types")
{
    field_container_t instance = {.items = std::vector<int>{10, 20, 30}, .triple = {std::array<uint8_t, 3>{1, 2, 3}}};
    field_container_t decoded = {};
    REQUIRE(cbor_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.items.value == instance.items.value);
    CHECK(decoded.triple.value == instance.triple.value);
}

struct array_key_t {
    mold::field_t<std::array<int, 3>{1, 2, 3}, bool> flag;
    mold::field_t<std::array<int, 2>{10, 20}, int32_t> val;
};

TEST_CASE("field_t CBOR with std::array as map key")
{
    array_key_t instance = {.flag = true, .val = 42};
    array_key_t decoded = {};
    REQUIRE(cbor_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.flag.value == true);
    CHECK(decoded.val.value == 42);
}

TEST_CASE("field_t CBOR wire format with array key")
{
    array_key_t instance = {.flag = true, .val = 42};
    uint8_t buf[128];
    std::span<uint8_t> bytes = buf;
    REQUIRE(mold::cbor_from(instance, bytes) == mold::error_t::ok);

    // Map of 2: a2
    CHECK(bytes[0] == 0xa2);
    // Key [1,2,3]: 83 01 02 03
    CHECK(bytes[1] == 0x83); // array(3)
    CHECK(bytes[2] == 0x01);
    CHECK(bytes[3] == 0x02);
    CHECK(bytes[4] == 0x03);
    // Value true: f5
    CHECK(bytes[5] == 0xf5);
    // Key [10,20]: 82 0a 14
    CHECK(bytes[6] == 0x82); // array(2)
    CHECK(bytes[7] == 0x0a); // 10
    CHECK(bytes[8] == 0x14); // 20
}

#endif

// ---------------------------------------------------------------
// Indefinite-length container tests
// ---------------------------------------------------------------

struct simple_pair_t {
    int32_t x;
    int32_t y;
};

static std::vector<uint8_t> sink_to_vec(auto fn)
{
    std::vector<uint8_t> out;
    mold::cbor_sink_t sink([](uint8_t b, void* ctx) {
        static_cast<std::vector<uint8_t>*>(ctx)->push_back(b);
        return true;
    }, &out);
    fn(sink);
    return out;
}

TEST_CASE("CBOR indefinite array wire format")
{
    auto bytes = sink_to_vec([](const mold::cbor_sink_t& s) {
        REQUIRE(s.write_indef_array() == mold::error_t::ok);
        REQUIRE(s.write_uint(1) == mold::error_t::ok);
        REQUIRE(s.write_uint(2) == mold::error_t::ok);
        REQUIRE(s.write_uint(3) == mold::error_t::ok);
        REQUIRE(s.write_break() == mold::error_t::ok);
    });
    // 9F 01 02 03 FF
    REQUIRE(bytes.size() == 5);
    CHECK(bytes[0] == 0x9f);
    CHECK(bytes[1] == 0x01);
    CHECK(bytes[2] == 0x02);
    CHECK(bytes[3] == 0x03);
    CHECK(bytes[4] == 0xff);
}

TEST_CASE("CBOR indefinite map wire format")
{
    auto bytes = sink_to_vec([](const mold::cbor_sink_t& s) {
        REQUIRE(s.write_indef_map() == mold::error_t::ok);
        REQUIRE(s.write_text("a") == mold::error_t::ok);
        REQUIRE(s.write_uint(1) == mold::error_t::ok);
        REQUIRE(s.write_break() == mold::error_t::ok);
    });
    // BF 61 61 01 FF
    REQUIRE(bytes.size() == 5);
    CHECK(bytes[0] == 0xbf);
    CHECK(bytes[4] == 0xff);
}

TEST_CASE("CBOR indefinite array with struct elements decodes correctly")
{
    auto bytes = sink_to_vec([](const mold::cbor_sink_t& s) {
        REQUIRE(s.write_indef_array() == mold::error_t::ok);

        simple_pair_t a{1, 2};
        simple_pair_t b{3, 4};
        REQUIRE(mold::cbor_from(a, [](uint8_t byte, void* ctx) {
            return static_cast<const mold::cbor_sink_t*>(ctx)->write_byte(byte) == mold::error_t::ok;
        }, const_cast<void*>(static_cast<const void*>(&s))) == mold::error_t::ok);
        REQUIRE(mold::cbor_from(b, [](uint8_t byte, void* ctx) {
            return static_cast<const mold::cbor_sink_t*>(ctx)->write_byte(byte) == mold::error_t::ok;
        }, const_cast<void*>(static_cast<const void*>(&s))) == mold::error_t::ok);

        REQUIRE(s.write_break() == mold::error_t::ok);
    });

    // Verify the reader can skip the entire indefinite array
    mold::cbor_ptr_t ptr = bytes.data();
    mold::cbor_ptr_t end = bytes.data() + bytes.size();
    CHECK(mold::cbor_skip_value(ptr, end) == mold::error_t::ok);
    CHECK(ptr == end);
}

struct cbor_part_a_t { int32_t x; };
struct cbor_part_b_t { std::string name; };

TEST_CASE("CBOR cbor_from_fields composes into indefinite map")
{
    auto bytes = sink_to_vec([](const mold::cbor_sink_t& s) {
        auto cb = [](uint8_t byte, void* ctx) {
            return static_cast<const mold::cbor_sink_t*>(ctx)->write_byte(byte) == mold::error_t::ok;
        };
        void* ctx = const_cast<void*>(static_cast<const void*>(&s));

        REQUIRE(s.write_indef_map() == mold::error_t::ok);
        cbor_part_a_t a{42};
        REQUIRE(mold::cbor_from_fields(a, cb, ctx) == mold::error_t::ok);
        cbor_part_b_t b{"hello"};
        REQUIRE(mold::cbor_from_fields(b, cb, ctx) == mold::error_t::ok);
        REQUIRE(s.write_break() == mold::error_t::ok);
    });

    // BF ... FF — indefinite map
    CHECK(bytes.front() == 0xbf);
    CHECK(bytes.back() == 0xff);

    // Decode: the reader should handle the composed indefinite map
    mold::cbor_ptr_t ptr = bytes.data();
    mold::cbor_ptr_t end = bytes.data() + bytes.size();
    CHECK(mold::cbor_skip_value(ptr, end) == mold::error_t::ok);
    CHECK(ptr == end);
}
