#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <doctest.h>
#include <mold/mold.h>

// ---------------------------------------------------------------
// Test structs
// ---------------------------------------------------------------

struct empty_t {};

struct deepnest_t {
    mold::float16_t fp16;
    mold::float32_t fp32;
    mold::float64_t fp64;
    float regular_float;
    double regular_double;
};

struct inner_t {
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    deepnest_t deep;
};

struct outer_t {
    mold::null_t nullopt;
    mold::irange_t<0, 1000> fifth;
    mold::frange_t<0.0, 1e12> sixth;
    empty_t zeroth;
    bool first;
    char second;
    size_t third;
    inner_t fourth;
    char seventh;
    std::array<uint16_t, 10> eighth;
    std::array<deepnest_t, 3> ninth;
    std::tuple<int, float, double> tenth;
    std::string_view eleventh;
    std::array<std::array<int, 4>, 3> twelth;
    std::vector<int> thirteenth;
    std::string fourteenth;
    std::optional<int> optional_int;
    std::optional<deepnest_t> optional_deepnest_present;
    std::optional<deepnest_t> optional_deepnest_absent;
    std::optional<std::string> optional_string_absent;
    std::optional<std::string> optional_string_present;
    std::optional<std::vector<int>> optional_vector;
    mold::nullable_t<int> nullable_int;
    mold::nullable_t<std::string> nullable_string_absent;
    mold::nullable_t<std::string> nullable_string_present;
};

struct session_t {
    std::string_view name;
    mold::uuid_t uuid;
};

struct profile_t {
    mold::string_t<64> username;
    mold::string_t<256> bio;
    int age;
};

struct packet_t {
    mold::bytes_t<128> payload;
    int seq;
};

struct view_packet_t {
    mold::bytes_view_t payload;
    int seq;
};

struct tiny_profile_t {
    mold::string_t<8> username;
    mold::string_t<8> bio;
    int age;
};

// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------

template<class T>
mold::error_t json_roundtrip(const T& in, T& out)
{
    char buf[8192];
    std::span<char> text = buf;
    auto err = mold::json_from(in, text, 0);
    if (err != mold::error_t::ok) return err;
    std::string_view sv = {text.data(), text.size()};
    return mold::json_to(out, sv);
}

template<class T>
mold::error_t cbor_roundtrip(const T& in, T& out)
{
    uint8_t buf[4096];
    std::span<uint8_t> bytes = buf;
    auto err = mold::cbor_from(in, bytes);
    if (err != mold::error_t::ok) return err;
    std::span<const uint8_t> cbor_in = {bytes.data(), bytes.size()};
    return mold::cbor_to(out, cbor_in);
}

template<class T>
mold::error_t msgpack_roundtrip(const T& in, T& out)
{
    uint8_t buf[4096];
    std::span<uint8_t> bytes = buf;
    auto err = mold::msgpack_from(in, bytes);
    if (err != mold::error_t::ok) return err;
    std::span<const uint8_t> msgpack_in = {bytes.data(), bytes.size()};
    return mold::msgpack_to(out, msgpack_in);
}

inline outer_t make_outer_instance()
{
    outer_t instance = {};
    instance.first = true;
    instance.second = 65;
    instance.third = 123456789;
    instance.fourth.i8 = -128;
    instance.fourth.i16 = 32767;
    instance.fourth.i32 = 2147483647;
    instance.fourth.i64 = 9223372036854775807LL;
    instance.fourth.u8 = 255;
    instance.fourth.u16 = 65535;
    instance.fourth.u32 = 4294967295U;
    instance.fourth.u64 = 18446744073709551615ULL;
    instance.fourth.deep = {mold::float16_t(1.5), mold::float32_t(2.25), mold::float64_t(3.125), 4.5f, 5.75};
    instance.fifth = 999;
    instance.sixth = mold::float32_t(1234.5);
    instance.seventh = 66;
    instance.eighth = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    instance.ninth = {{
        {mold::float16_t(1.0), mold::float32_t(2.0), mold::float64_t(3.0), 4.0f, 5.0},
        {mold::float16_t(1.5), mold::float32_t(2.5), mold::float64_t(3.5), 4.5f, 5.5},
        {mold::float16_t(1.25), mold::float32_t(2.25), mold::float64_t(3.25), 4.25f, 5.25},
    }};
    instance.tenth = {42, 3.14f, 2.718};
    instance.eleventh = "Hello, world!";
    instance.twelth = {{{1,2,3,4}, {5,6,7,8}, {9,10,11,12}}};
    instance.thirteenth = {69, 70, 71, 100};
    instance.fourteenth = "CBOR string";
    instance.optional_int = 42;
    instance.optional_deepnest_present = deepnest_t{mold::float16_t(1.0), mold::float32_t(2.0), mold::float64_t(3.0), 4.0f, 5.0};
    instance.optional_string_present = "Hello Optional";
    instance.optional_vector = std::vector<int>{10, 20, 30};
    instance.nullable_int = 99;
    instance.nullable_string_present = "Hello Nullable";
    return instance;
}

#endif
