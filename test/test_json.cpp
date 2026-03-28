#include "test_common.h"

// ---------------------------------------------------------------
// JSON tests
// ---------------------------------------------------------------

static const char json_data[] = R"({
    "nullopt": null,
    "zeroth": {},
    "first": true,
    "second": 65,
    "third": 123456789,
    "fourth": {
        "i8": -128,
        "i16": 32767,
        "i32": 2147483647,
        "i64": 9223372036854775807,
        "u8": 255,
        "u16": 65535,
        "u32": 4294967295,
        "u64": 18446744073709551615,
        "deep": {
            "fp16": 1.5,
            "fp32": 2.25,
            "fp64": 3.125,
            "regular_float": 4.5,
            "regular_double": 5.75
        }
    },
    "fifth": 999,
    "sixth": 1234.5,
    "seventh": 66,
    "eighth": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
    "ninth": [
        { "fp16": 1.0, "fp32": 2.0, "fp64": 3.0, "regular_float": 4.0, "regular_double": 5.0 },
        { "fp16": 1.5, "fp32": 2.5, "fp64": 3.5, "regular_float": 4.5, "regular_double": 5.5 },
        { "fp16": 1.25, "fp32": 2.25, "fp64": 3.25, "regular_float": 4.25, "regular_double": 5.25 }
    ],
    "tenth": [42, 3.14, 2.718],
    "eleventh": "Hello, world!",
    "twelth": [
        [1, 2, 3, 4],
        [5, 6, 7, 8],
        [9, 10, 11, 12]
    ],
    "thirteenth": [69, 70, 71, 100],
    "fourteenth": "JSON string",
    "optional_int": 42,
    "optional_deepnest_present": {
        "fp16": 1.0, "fp32": 2.0, "fp64": 3.0,
        "regular_float": 4.0, "regular_double": 5.0
    },
    "optional_string_present": "Hello Optional",
    "optional_vector": [10, 20, 30],
    "nullable_int": 99,
    "nullable_string_absent": null,
    "nullable_string_present": "Hello Nullable"
})";

TEST_CASE("JSON parse complex struct")
{
    outer_t instance = {};
    std::string_view sv = json_data;
    REQUIRE(mold::json_to(instance, sv) == mold::error_t::ok);

    CHECK(instance.first == true);
    CHECK(instance.second == 65);
    CHECK(instance.fourth.i8 == -128);
    CHECK(instance.fourth.i16 == 32767);
    CHECK(instance.fourth.i32 == 2147483647);
    CHECK(instance.fourth.u8 == 255);
    CHECK(instance.fourth.u16 == 65535);
    CHECK(instance.fourth.u32 == 4294967295U);
    CHECK(instance.fourth.deep.regular_double == 5.75);
    CHECK(instance.eighth[0] == 1);
    CHECK(instance.eighth[9] == 10);
    CHECK(instance.eleventh == "Hello, world!");
    CHECK(instance.thirteenth.size() == 4);
    CHECK(instance.fourteenth == "JSON string");
    CHECK(instance.optional_int.value() == 42);
    CHECK(instance.optional_deepnest_absent.has_value() == false);
    CHECK(instance.optional_string_absent.has_value() == false);
    CHECK(instance.optional_string_present.value() == "Hello Optional");
    CHECK(instance.optional_vector.value().size() == 3);
    CHECK(instance.nullable_int.value() == 99);
    CHECK(instance.nullable_string_absent.has_value() == false);
    CHECK(instance.nullable_string_present.value() == "Hello Nullable");
}

TEST_CASE("JSON round-trip complex struct")
{
    outer_t instance = {};
    std::string_view sv = json_data;
    REQUIRE(mold::json_to(instance, sv) == mold::error_t::ok);

    char buf[8192];
    std::span<char> text = buf;
    REQUIRE(mold::json_from(instance, text, 0) == mold::error_t::ok);

    outer_t roundtrip = {};
    std::string_view rt_sv = {text.data(), text.size()};
    REQUIRE(mold::json_to(roundtrip, rt_sv, true) == mold::error_t::ok);
}

struct opt_null_test_t {
    int required;
    std::optional<int> opt_present;
    std::optional<int> opt_absent;
    mold::nullable_t<int> nul_present;
    mold::nullable_t<int> nul_absent;
};

TEST_CASE("JSON optional fields are skipped, nullable fields write null")
{
    opt_null_test_t instance = {};
    instance.required = 1;
    instance.opt_present = 42;
    instance.nul_present = 99;

    char buf[512];
    std::span<char> text = buf;
    REQUIRE(mold::json_from(instance, text, 0) == mold::error_t::ok);

    std::string_view json_sv = {text.data(), text.size()};

    // opt_absent should not appear at all
    CHECK(json_sv.find("opt_absent") == std::string_view::npos);
    // opt_present should appear with value
    CHECK(json_sv.find("\"opt_present\":42") != std::string_view::npos);
    // nul_absent should appear as explicit null
    CHECK(json_sv.find("\"nul_absent\":null") != std::string_view::npos);
    // nul_present should appear with value
    CHECK(json_sv.find("\"nul_present\":99") != std::string_view::npos);

    // Round-trip
    opt_null_test_t decoded = {};
    REQUIRE(mold::json_to(decoded, json_sv) == mold::error_t::ok);
    CHECK(decoded.required == 1);
    CHECK(decoded.opt_present.value() == 42);
    CHECK(decoded.opt_absent.has_value() == false);
    CHECK(decoded.nul_present.value() == 99);
    CHECK(decoded.nul_absent.has_value() == false);
}

// ---------------------------------------------------------------
// uint64_t max value (JSON)
// ---------------------------------------------------------------

struct u64_struct_t {
    uint64_t val;
};

TEST_CASE("uint64_t max JSON round-trip")
{
    static const char json[] = R"({"val": 18446744073709551615})";
    std::string_view sv = json;

    u64_struct_t instance = {};
    REQUIRE(mold::json_to(instance, sv) == mold::error_t::ok);
    CHECK(instance.val == 18446744073709551615ULL);

    u64_struct_t decoded = {};
    REQUIRE(json_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.val == 18446744073709551615ULL);
}

// ---------------------------------------------------------------
// Error propagation tests
// ---------------------------------------------------------------

struct ranged_t {
    mold::irange_t<0, 100> bounded;
};

TEST_CASE("handler error propagated on out-of-range value")
{
    SUBCASE("JSON rejects out-of-range integer")
    {
        static const char json[] = R"({"bounded": 999})";
        std::string_view sv = json;
        ranged_t instance = {};
        CHECK(mold::json_to(instance, sv) == mold::error_t::handler_failure);
    }
    SUBCASE("JSON rejects negative for unsigned range")
    {
        static const char json[] = R"({"bounded": -1})";
        std::string_view sv = json;
        ranged_t instance = {};
        CHECK(mold::json_to(instance, sv) == mold::error_t::handler_failure);
    }
}

TEST_CASE("JSON rejects unexpected keys in strict mode")
{
    static const char json[] = R"({"bounded": 50, "extra": 1})";
    std::string_view sv = json;
    ranged_t instance = {};
    CHECK(mold::json_to(instance, sv) == mold::error_t::unexpected_key);
}

TEST_CASE("JSON accepts unexpected keys in lenient mode")
{
    static const char json[] = R"({"bounded": 50, "extra": 1})";
    std::string_view sv = json;
    ranged_t instance = {};
    CHECK(mold::json_to(instance, sv, true) == mold::error_t::ok);
    CHECK(instance.bounded == 50);
}

TEST_CASE("JSON rejects duplicate keys")
{
    static const char json[] = R"({"bounded": 50, "bounded": 60})";
    std::string_view sv = json;
    ranged_t instance = {};
    CHECK(mold::json_to(instance, sv) == mold::error_t::duplicate_key);
}

TEST_CASE("JSON rejects missing required keys")
{
    static const char json[] = R"({})";
    std::string_view sv = json;
    ranged_t instance = {};
    CHECK(mold::json_to(instance, sv) == mold::error_t::missing_key);
}

TEST_CASE("JSON rejects type mismatch")
{
    static const char json[] = R"({"bounded": "not a number"})";
    std::string_view sv = json;
    ranged_t instance = {};
    CHECK(mold::json_to(instance, sv) == mold::error_t::type_mismatch_primitive);
}

// ---------------------------------------------------------------
// Enum round-trip
// ---------------------------------------------------------------

enum class color_t : uint8_t { red = 0, green = 1, blue = 2 };
enum signed_enum_t : int16_t { neg = -10, zero = 0, pos = 10 };

struct enum_struct_t {
    color_t color;
    signed_enum_t val;
};

TEST_CASE("JSON enum round-trip")
{
    enum_struct_t instance = {color_t::blue, signed_enum_t::neg};
    enum_struct_t decoded = {};
    REQUIRE(json_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.color == color_t::blue);
    CHECK(decoded.val == signed_enum_t::neg);
}

TEST_CASE("CBOR enum round-trip")
{
    enum_struct_t instance = {color_t::green, signed_enum_t::pos};
    enum_struct_t decoded = {};
    REQUIRE(cbor_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.color == color_t::green);
    CHECK(decoded.val == signed_enum_t::pos);
}

TEST_CASE("MessagePack enum round-trip")
{
    enum_struct_t instance = {color_t::red, signed_enum_t::zero};
    enum_struct_t decoded = {};
    REQUIRE(msgpack_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.color == color_t::red);
    CHECK(decoded.val == signed_enum_t::zero);
}
