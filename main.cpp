#include <mold/mold.h>

struct empty_t {

};

struct float_nest_t {
    mold::float16_t fp16;
    mold::float32_t fp32;
    mold::float64_t fp64;
    float native_float;
    double native_double;
};

struct integer_nest_t {
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    float_nest_t floats;
};

struct outer_t {
    // Primitives
    mold::null_t null_val;
    bool bool_val;
    char char_val;
    size_t size_val;
    // Constrained types
    mold::irange_t<0, 1000> irange_val;
    mold::frange_t<0.0, 1e12> frange_val;
    // Nested structs
    empty_t empty_val;
    integer_nest_t integers;
    // Strings
    std::string_view string_view_val;
    std::string string_val;
    mold::string_t<64> fixed_string;
    // Containers
    std::array<uint16_t, 10> array_u16;
    std::array<float_nest_t, 3> array_struct;
    std::array<std::array<int, 4>, 3> nested_array;
    std::tuple<int, float, double> tuple_val;
    std::vector<int> vector_int;
    mold::vector_t<int, 8> static_vector;
    // Bytes
    mold::bytes_t<128> bytes_fixed;
    // Custom types
    mold::uuid_t uuid_val;
    // Optionals (skipped when absent)
    std::optional<int> opt_int;
    std::optional<float_nest_t> opt_struct_present;
    std::optional<std::string> opt_string_absent;
    std::optional<std::string> opt_string_present;
    std::optional<std::vector<int>> opt_vector;
    // Nullables (explicit null when absent)
    mold::nullable_t<int> nul_int;
    mold::nullable_t<std::string> nul_string_absent;
    mold::nullable_t<std::string> nul_string_present;
    // CBOR field_t keys (integer, negative, structural)
#if (MOLD_REFLECTION_CBOR_FIELD_KEYS)
    mold::field_t<1, int32_t> field_int_key;
    mold::field_t<-1, bool> field_neg_key;
    mold::field_t<99, std::string> field_string_val;
    mold::field_t<std::array<int,2>{10,20}, uint16_t> field_array_key;
#endif
};

// MessagePack test structs (must be at file scope for reflection)
struct msgpack_demo_t {
    bool bool_val;
    char char_val;
    size_t size_val;
    integer_nest_t integers;
    std::string string_val;
    std::array<uint16_t, 10> array_u16;
    std::tuple<int, float, double> tuple_val;
    std::vector<int> vector_int;
    mold::vector_t<int, 8> static_vector;
    std::optional<int> opt_int;
    std::optional<std::string> opt_string_present;
    std::optional<std::string> opt_string_absent;
    mold::nullable_t<int> nul_int;
    mold::nullable_t<std::string> nul_string_absent;
    mold::nullable_t<std::string> nul_string_present;
};


static outer_t make_instance()
{
    static uint8_t raw_bytes[] = { 0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe };

    outer_t instance = {};
    instance.bool_val = true;
    instance.char_val = 65;
    instance.size_val = 123456789;
    instance.irange_val = 999;
    instance.frange_val = mold::float32_t(1234.5);
    instance.integers.i8 = -128;
    instance.integers.i16 = 32767;
    instance.integers.i32 = 2147483647;
    instance.integers.i64 = 9223372036854775807LL;
    instance.integers.u8 = 255;
    instance.integers.u16 = 65535;
    instance.integers.u32 = 4294967295U;
    instance.integers.u64 = 18446744073709551615ULL;
    instance.integers.floats = {mold::float16_t(1.5), mold::float32_t(2.25), mold::float64_t(3.125), 4.5f, 5.75};
    instance.string_view_val = "Hello, world!";
    instance.string_val = "owned string";
    instance.fixed_string = "fixed";
    instance.array_u16 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    instance.array_struct = {{
        {mold::float16_t(1.0), mold::float32_t(2.0), mold::float64_t(3.0), 4.0f, 5.0},
        {mold::float16_t(1.5), mold::float32_t(2.5), mold::float64_t(3.5), 4.5f, 5.5},
        {mold::float16_t(1.25), mold::float32_t(2.25), mold::float64_t(3.25), 4.25f, 5.25},
    }};
    instance.nested_array = {{{1,2,3,4}, {5,6,7,8}, {9,10,11,12}}};
    instance.tuple_val = {42, 3.14f, 2.718};
    instance.vector_int = {69, 70, 71, 100};
    instance.static_vector = {42, 43, 44};
    instance.bytes_fixed = std::span<const uint8_t>(raw_bytes, sizeof(raw_bytes));
    instance.uuid_val = {{
        0xf8, 0x1d, 0x4f, 0xae, 0x7d, 0xec, 0x11, 0xd0,
        0xa7, 0x65, 0x00, 0xa0, 0xc9, 0x1e, 0x6b, 0xf6}};
    instance.opt_int = 42;
    instance.opt_struct_present = float_nest_t{mold::float16_t(1.0), mold::float32_t(2.0), mold::float64_t(3.0), 4.0f, 5.0};
    instance.opt_string_present = "Hello Optional";
    instance.opt_vector = std::vector<int>{10, 20, 30};
    instance.nul_int = 99;
    instance.nul_string_present = "Hello Nullable";
#if (MOLD_REFLECTION_CBOR_FIELD_KEYS)
    instance.field_int_key = -7;
    instance.field_neg_key = true;
    instance.field_string_val = {"keyed"};
    instance.field_array_key = 42;
#endif
    return instance;
}

int main()
{
    uint8_t byte_buf[4096];
    char text_buf[8192];
    mold::error_t status;

#if (MOLD_REFLECTION_JSON_ENABLED)
    printf("=== JSON ROUND-TRIP ===\n");
    {
        static const char json_data[] = R"({
            "null_val": null,
            "bool_val": true,
            "char_val": 65,
            "size_val": 123456789,
            "irange_val": 999,
            "frange_val": 1234.5,
            "empty_val": {},
            "integers": {
                "i8": -128, "i16": 32767, "i32": 2147483647, "i64": 9223372036854775807,
                "u8": 255, "u16": 65535, "u32": 4294967295, "u64": 18446744073709551615,
                "floats": { "fp16": 1.5, "fp32": 2.25, "fp64": 3.125, "native_float": 4.5, "native_double": 5.75 }
            },
            "string_view_val": "Hello, world!",
            "string_val": "owned string",
            "fixed_string": "fixed",
            "array_u16": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
            "array_struct": [
                { "fp16": 1.0, "fp32": 2.0, "fp64": 3.0, "native_float": 4.0, "native_double": 5.0 },
                { "fp16": 1.5, "fp32": 2.5, "fp64": 3.5, "native_float": 4.5, "native_double": 5.5 },
                { "fp16": 1.25, "fp32": 2.25, "fp64": 3.25, "native_float": 4.25, "native_double": 5.25 }
            ],
            "nested_array": [[1,2,3,4],[5,6,7,8],[9,10,11,12]],
            "tuple_val": [42, 3.14, 2.718],
            "vector_int": [69, 70, 71, 100],
            "static_vector": [42, 43, 44],
            "bytes_fixed": "3q2+78r+ur4=",
            "uuid_val": "f81d4fae-7dec-11d0-a765-00a0c91e6bf6",
            "opt_int": 42,
            "opt_struct_present": { "fp16": 1.0, "fp32": 2.0, "fp64": 3.0, "native_float": 4.0, "native_double": 5.0 },
            "opt_string_present": "Hello Optional",
            "opt_vector": [10, 20, 30],
            "nul_int": 99,
            "nul_string_absent": null,
            "nul_string_present": "Hello Nullable",
            "field_int_key": -7,
            "field_neg_key": true,
            "field_string_val": "keyed",
            "field_array_key": 42
        })";

        std::string_view json_sv = json_data;
        outer_t instance = {};
        status = mold::json_to(instance, json_sv);
        if (status != mold::error_t::ok) {
            printf("FAIL: json_to: %s\n", mold::error_str(status));
            return 1;
        }

        std::span<char> text_span = text_buf;
        status = mold::json_from(instance, text_span, 0);
        if (status != mold::error_t::ok) {
            printf("FAIL: json_from: %s\n", mold::error_str(status));
            return 1;
        }
        printf("json_from (%zu bytes):\n%.*s\n\n", text_span.size(), (int)text_span.size(), text_span.data());

        outer_t roundtrip = {};
        std::string_view rt_sv = {text_span.data(), text_span.size()};
        status = mold::json_to(roundtrip, rt_sv, true);
        printf("JSON round-trip: %s\n", status == mold::error_t::ok ? "OK" : "FAIL");
    }
#endif

#if (MOLD_REFLECTION_CBOR_ENABLED)
    printf("\n=== CBOR ROUND-TRIP ===\n");
    {
        outer_t instance = make_instance();

        std::span<uint8_t> byte_span = byte_buf;
        status = mold::cbor_from(instance, byte_span);
        if (status != mold::error_t::ok) {
            printf("FAIL: cbor_from: %s\n", mold::error_str(status));
            return 1;
        }
        printf("cbor_from: OK (%zu bytes)\n", byte_span.size());

        std::span<char> text_span = text_buf;
        mold::cbor_pretty(byte_span, text_span, 0);
        printf("%.*s\n", (int)text_span.size(), text_span.data());

        outer_t decoded = {};
        std::span<const uint8_t> cbor_in = {byte_span.data(), byte_span.size()};
        status = mold::cbor_to(decoded, cbor_in);
        if (status != mold::error_t::ok) {
            printf("FAIL: cbor_to: %s\n", mold::error_str(status));
            return 1;
        }

        std::span<uint8_t> byte_span2 = byte_buf;
        status = mold::cbor_from(decoded, byte_span2);
        if (status != mold::error_t::ok) {
            printf("FAIL: cbor_from (re-encode): %s\n", mold::error_str(status));
            return 1;
        }
        printf("CBOR round-trip: %s (orig=%zu re-encoded=%zu)\n",
            byte_span.size() == byte_span2.size() &&
            memcmp(byte_span.data(), byte_span2.data(), byte_span.size()) == 0
                ? "OK" : "MISMATCH",
            byte_span.size(), byte_span2.size());
    }
#endif

#if (MOLD_REFLECTION_MSGPACK_ENABLED)
    printf("\n=== MSGPACK ROUND-TRIP ===\n");
    {
        msgpack_demo_t instance = {};
        instance.bool_val = true;
        instance.char_val = 65;
        instance.size_val = 123456789;
        instance.integers.i8 = -128;
        instance.integers.i16 = 32767;
        instance.integers.i32 = 2147483647;
        instance.integers.i64 = 9223372036854775807LL;
        instance.integers.u8 = 255;
        instance.integers.u16 = 65535;
        instance.integers.u32 = 4294967295U;
        instance.integers.u64 = 18446744073709551615ULL;
        instance.integers.floats = {mold::float16_t(1.5), mold::float32_t(2.25), mold::float64_t(3.125), 4.5f, 5.75};
        instance.string_val = "owned string";
        instance.array_u16 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        instance.tuple_val = {42, 3.14f, 2.718};
        instance.vector_int = {69, 70, 71, 100};
        instance.static_vector = {42, 43, 44};
        instance.opt_int = 42;
        instance.opt_string_present = "Hello Optional";
        instance.nul_int = 99;
        instance.nul_string_present = "Hello Nullable";

        std::span<uint8_t> byte_span = byte_buf;
        status = mold::msgpack_from(instance, byte_span);
        if (status != mold::error_t::ok) {
            printf("FAIL: msgpack_from: %s\n", mold::error_str(status));
            return 1;
        }
        printf("msgpack_from: OK (%zu bytes)\n", byte_span.size());

        // Pretty-print encoded MessagePack
        printf("diagnostic: ");
        mold::msgpack_pretty(byte_span, 0);
        printf("\n");

        msgpack_demo_t decoded = {};
        std::span<const uint8_t> msgpack_in = {byte_span.data(), byte_span.size()};
        status = mold::msgpack_to(decoded, msgpack_in);
        if (status != mold::error_t::ok) {
            printf("FAIL: msgpack_to: %s\n", mold::error_str(status));
            return 1;
        }

        // Re-encode and compare bytes
        std::span<uint8_t> byte_span2 = byte_buf;
        status = mold::msgpack_from(decoded, byte_span2);
        if (status != mold::error_t::ok) {
            printf("FAIL: msgpack_from (re-encode): %s\n", mold::error_str(status));
            return 1;
        }
        printf("MessagePack round-trip: %s (orig=%zu re-encoded=%zu)\n",
            byte_span.size() == byte_span2.size() &&
            memcmp(byte_span.data(), byte_span2.data(), byte_span.size()) == 0
                ? "OK" : "MISMATCH",
            byte_span.size(), byte_span2.size());
    }

#endif
}
