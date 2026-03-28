#include "test_common.h"

// ---------------------------------------------------------------
// MessagePack tests
// ---------------------------------------------------------------

TEST_CASE("MessagePack encode complex struct")
{
    auto instance = make_outer_instance();
    uint8_t buf[4096];
    std::span<uint8_t> bytes = buf;
    REQUIRE(mold::msgpack_from(instance, bytes) == mold::error_t::ok);
    CHECK(bytes.size() > 0);
}

TEST_CASE("MessagePack round-trip complex struct")
{
    auto instance = make_outer_instance();
    outer_t decoded = {};
    REQUIRE(msgpack_roundtrip(instance, decoded) == mold::error_t::ok);

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
// uint64_t max value (MessagePack)
// ---------------------------------------------------------------

struct msgpack_u64_struct_t {
    uint64_t val;
};

TEST_CASE("uint64_t max MessagePack round-trip")
{
    msgpack_u64_struct_t instance = { .val = 18446744073709551615ULL };
    msgpack_u64_struct_t decoded = {};
    REQUIRE(msgpack_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.val == 18446744073709551615ULL);
}

// ---------------------------------------------------------------
// Simple structs
// ---------------------------------------------------------------

struct msgpack_simple_t {
    int x;
    double y;
    std::string name;
    bool flag;
};

TEST_CASE("MessagePack round-trip simple struct")
{
    msgpack_simple_t instance = {42, 3.14, "hello", true};
    msgpack_simple_t decoded = {};
    REQUIRE(msgpack_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.x == 42);
    CHECK(decoded.y == doctest::Approx(3.14));
    CHECK(decoded.name == "hello");
    CHECK(decoded.flag == true);
}

// ---------------------------------------------------------------
// Nested structs
// ---------------------------------------------------------------

struct msgpack_inner_t {
    int a;
    int b;
};

struct msgpack_nested_t {
    msgpack_inner_t inner;
    std::string label;
};

TEST_CASE("MessagePack round-trip nested struct")
{
    msgpack_nested_t instance = {{10, 20}, "nested"};
    msgpack_nested_t decoded = {};
    REQUIRE(msgpack_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.inner.a == 10);
    CHECK(decoded.inner.b == 20);
    CHECK(decoded.label == "nested");
}

// ---------------------------------------------------------------
// Vector round-trip
// ---------------------------------------------------------------

struct msgpack_vec_t {
    std::vector<int> nums;
};

TEST_CASE("MessagePack round-trip vector")
{
    msgpack_vec_t instance = {{1, 2, 3, 4, 5}};
    msgpack_vec_t decoded = {};
    REQUIRE(msgpack_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.nums == instance.nums);
}

// ---------------------------------------------------------------
// Tuple round-trip
// ---------------------------------------------------------------

struct msgpack_tuple_t {
    std::tuple<int, float, std::string> data;
};

TEST_CASE("MessagePack round-trip tuple")
{
    msgpack_tuple_t instance = {{42, 1.5f, "tuple"}};
    msgpack_tuple_t decoded = {};
    REQUIRE(msgpack_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(std::get<0>(decoded.data) == 42);
    CHECK(std::get<1>(decoded.data) == doctest::Approx(1.5f));
    CHECK(std::get<2>(decoded.data) == "tuple");
}

// ---------------------------------------------------------------
// Negative integers
// ---------------------------------------------------------------

struct msgpack_negint_t {
    int8_t a;
    int16_t b;
    int32_t c;
    int64_t d;
};

TEST_CASE("MessagePack round-trip negative integers")
{
    msgpack_negint_t instance = {-1, -128, -32768, -2147483648LL};
    msgpack_negint_t decoded = {};
    REQUIRE(msgpack_roundtrip(instance, decoded) == mold::error_t::ok);
    CHECK(decoded.a == -1);
    CHECK(decoded.b == -128);
    CHECK(decoded.c == -32768);
    CHECK(decoded.d == -2147483648LL);
}

// ---------------------------------------------------------------
// Empty struct
// ---------------------------------------------------------------

TEST_CASE("MessagePack round-trip empty struct")
{
    empty_t instance = {};
    empty_t decoded = {};
    REQUIRE(msgpack_roundtrip(instance, decoded) == mold::error_t::ok);
}

// ---------------------------------------------------------------
// Allow unexpected keys
// ---------------------------------------------------------------

struct msgpack_partial_t {
    int x;
};

TEST_CASE("MessagePack skip unknown keys")
{
    // Encode a struct with more fields
    msgpack_simple_t full = {42, 3.14, "hello", true};
    uint8_t buf[1024];
    std::span<uint8_t> bytes = buf;
    REQUIRE(mold::msgpack_from(full, bytes) == mold::error_t::ok);

    // Decode into a struct with fewer fields (allow_unexpected = true)
    std::span<const uint8_t> msgpack_in = {bytes.data(), bytes.size()};
    msgpack_partial_t decoded = {};
    REQUIRE(mold::msgpack_to(decoded, msgpack_in, true) == mold::error_t::ok);
    CHECK(decoded.x == 42);
}

// ---------------------------------------------------------------
// msgpack_from_fields
// ---------------------------------------------------------------

struct msgpack_part_a_t { int32_t x; };
struct msgpack_part_b_t { std::string name; };
struct msgpack_combined_t { int32_t x; std::string name; };

TEST_CASE("MessagePack msgpack_from_fields composes structs")
{
    // Manually build a map: fixmap(2) + fields from a + fields from b
    std::vector<uint8_t> out;
    auto cb = [](uint8_t b, void* ctx) {
        static_cast<std::vector<uint8_t>*>(ctx)->push_back(b);
        return true;
    };

    // fixmap header for 2 entries
    mold::msgpack_sink_t sink(cb, &out);
    REQUIRE(sink.write_map(2) == mold::error_t::ok);

    msgpack_part_a_t a{42};
    REQUIRE(mold::msgpack_from_fields(a, cb, &out) == mold::error_t::ok);
    msgpack_part_b_t b{"hello"};
    REQUIRE(mold::msgpack_from_fields(b, cb, &out) == mold::error_t::ok);

    // Decode the composed map as a struct with both fields
    std::span<const uint8_t> input(out.data(), out.size());
    msgpack_combined_t decoded{};
    REQUIRE(mold::msgpack_to(decoded, input) == mold::error_t::ok);
    CHECK(decoded.x == 42);
    CHECK(decoded.name == "hello");
}