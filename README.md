# mold

A header-only C++ library that uses compile-time reflection to serialize and deserialize JSON, CBOR, and MessagePack into structs automatically, without any macros or boilerplate code. It is designed with simplicity in mind for performance-critical and resource-constrained environments, as it avoids dynamic memory allocations and recursion for its core parsing logic.

## Features

- __Header-only__ with compile-time reflection - no macros, no boilerplate, no code generation.
- __JSON, CBOR, and MessagePack__ serialization and deserialization through a shared reflection engine.
- __Extensible__ via `spec_t<T>` template specialization for custom types.
- __Small footprint__: template instantiation bloat is minimal and limited to reflection data.
- __High performance__: single-pass, non-recursive, stack-based parsers with zero-copy strings (`std::string_view`). No dynamic memory allocation in the parsing core - suitable for constrained environments.

## Requirements

- C++20 compiler (GCC 14+)
- CMake 3.20+

Compilers with C++26 structured binding packs (P1061R10) lift the 32-member-per-struct limit
in `to_tuple.h`.

## Binary size

Measured with GCC 15.2.0, `-Os -DNDEBUG -fdata-sections -ffunction-sections -Wl,--gc-sections`,
x86-64 Linux. The `main.cpp` binary exercises all supported types (primitives, integers, floats,
strings, arrays, tuples, vectors, optionals, bytes, UUID, `field_t` keys) with JSON, CBOR, and
MessagePack round-trips enabled.

Fixed overhead (parser/serializer core — paid once per function used):

| Function        | Overhead |
|-----------------|---------:|
| `json_to`       |   1949 B |
| `json_from`     |    911 B |
| `cbor_to`       |   1854 B |
| `cbor_from`     |    606 B |
| `msgpack_to`    |   1581 B |
| `msgpack_from`  |    653 B |

Total `main.cpp` binary:

| Metric                            | Size     |
|-----------------------------------|---------:|
| `.text + .rodata + .data.rel.ro`  | 40.4 KB  |
| Stripped ELF                      | 70.3 KB  |

Per-type cost (reflection metadata + handler instantiations): each struct contributes one
`reflection_t` root (80 B) plus one `reflection_t` per field (80 B each), field name strings
(one byte per character plus NUL), and one handler function per new primitive type (~37–237 B,
shared across all structs that use the same type).

## Quick start

Include `mold/mold.h` and call `mold::json_to`, `mold::cbor_to`, or `mold::msgpack_to` to populate your struct:

```cpp
#include <cstdio>
#include <mold/mold.h>

struct device_config_t {
    float offset;
    std::array<int16_t, 3> coeffs;
    bool enabled;
};

struct device_t {
    std::string_view name;
    uint8_t id;
    device_config_t config;
};

int main()
{
    std::string_view json_sv = R"({
        "name": "TemperatureSensor01",
        "id": 77,
        "config": {
            "offset": -4.2,
            "coeffs": [-1337, 0, 65535],
            "enabled": true
        }
    })";
    device_t device;

    auto err = mold::json_to(device, json_sv);
    if (err != mold::error_t::ok) {
        printf("parsing failure: %s\n", mold::error_str(err));
        printf("remaining JSON: %.*s\n", (int) json_sv.size(), json_sv.data());
        return 1;
    }
    printf("name:       %.*s\n", (int) device.name.size(), device.name.data());
    printf("id:         %d\n", device.id);
    printf("offset:     %f\n", device.config.offset);
    printf("coeffs:     [%d, %d, %d]\n", device.config.coeffs[0], device.config.coeffs[1], device.config.coeffs[2]);
    printf("enabled:    %s\n", device.config.enabled ? "true" : "false");
}
```

## JSON API

### Deserialization

```cpp
// Parse JSON string into a struct
mold::error_t mold::json_to(T& instance, std::string_view& json_data, bool allow_unexpected = false);
```

On error, `json_data` is advanced past the consumed prefix so you can inspect leftover input.

### Serialization

```cpp
// Write JSON to a callback
mold::error_t mold::json_from(const T& instance, sink_cb_t sink_fn, void* user_ctx, size_t indent = 0);

// Write JSON to a buffer
mold::error_t mold::json_from(const T& instance, std::span<char>& buffer, size_t indent = 0);

// Write JSON to stdout (uses MOLD_PUTCHAR)
mold::error_t mold::json_from(const T& instance, size_t indent = 0);
```

Pass `indent > 0` for pretty-printed output.

## CBOR API

### Deserialization

```cpp
// Parse CBOR binary data into a struct
mold::error_t mold::cbor_to(T& instance, std::span<const uint8_t>& cbor_data, bool allow_unexpected = false);
```

### Serialization

```cpp
// Write CBOR to a callback
mold::error_t mold::cbor_from(const T& instance, sink_cb_t sink_fn, void* user_ctx);

// Write CBOR to a span (advances past written bytes)
mold::error_t mold::cbor_from(const T& instance, std::span<uint8_t>& buffer);

// Write CBOR to a fixed array, reports written size
mold::error_t mold::cbor_from(const T& instance, std::array<uint8_t, N>& buffer, size_t& written_size);
```

### Writer

`cbor_writer_t<N>` is a fixed-capacity encoder combining buffer storage with the full
sink API and reflection-driven aggregate encoding. All primitive types are written via
a single concept-dispatched `write()` method. Aggregates and incremental field encoding
are also supported through `write()` and `write_fields()` overloads.

```cpp
mold::cbor_writer_t<256> w;

// Primitives — type-dispatched automatically
w.write(uint32_t(42));       // unsigned integer
w.write(int16_t(-7));        // signed integer
w.write(3.14);               // float
w.write(true);               // boolean
w.write("hello");            // text string
w.write(my_enum::value);     // enum as underlying integer

// Aggregate — full struct with map header
my_struct_t data{...};
w.write(data);

// Incremental — compose maps from multiple structs
w.write_indef_map();
w.write_fields(header);      // key-value pairs only (no map header)
w.write_fields(payload);
w.write("extra_key");        // manual key-value pair
w.write(42);
w.write_break();

std::span<const uint8_t> encoded = w;  // view the buffer
```

A matching `msgpack_writer_t<N>` is available for MessagePack.

### Diagnostic pretty-print

```cpp
// Pretty-print CBOR to a callback
mold::error_t mold::cbor_pretty(std::span<const uint8_t> cbor, sink_cb_t sink_fn, void* user_ctx, int indent = 0);

// Pretty-print CBOR to a buffer
mold::error_t mold::cbor_pretty(std::span<const uint8_t> cbor, std::span<char>& buffer, int indent = 0);

// Pretty-print CBOR to stdout (uses MOLD_PUTCHAR)
mold::error_t mold::cbor_pretty(std::span<const uint8_t> cbor, int indent = 0);
```

## MessagePack API

### Deserialization

```cpp
// Parse MessagePack binary data into a struct
mold::error_t mold::msgpack_to(T& instance, std::span<const uint8_t>& msgpack_data, bool allow_unexpected = false);
```

### Serialization

```cpp
// Write MessagePack to a callback
mold::error_t mold::msgpack_from(const T& instance, sink_cb_t sink_fn, void* user_ctx);

// Write MessagePack to a span (advances past written bytes)
mold::error_t mold::msgpack_from(const T& instance, std::span<uint8_t>& buffer);
```

### Diagnostic pretty-print

```cpp
// Pretty-print MessagePack to a callback
mold::error_t mold::msgpack_pretty(std::span<const uint8_t> msgpack, sink_cb_t sink_fn, void* user_ctx, int indent = 0);

// Pretty-print MessagePack to a buffer
mold::error_t mold::msgpack_pretty(std::span<const uint8_t> msgpack, std::span<char>& buffer, int indent = 0);

// Pretty-print MessagePack to stdout (uses MOLD_PUTCHAR)
mold::error_t mold::msgpack_pretty(std::span<const uint8_t> msgpack, int indent = 0);
```

## Supported types

### Built-in specializations

| Type                  | JSON representation      | CBOR representation       | MessagePack representation |
|-----------------------|--------------------------|---------------------------|----------------------------|
| `bool`                | `true` / `false`         | boolean                   | boolean                    |
| `char`, `size_t`      | number                   | unsigned integer          | unsigned integer           |
| `int8_t`..`int64_t`   | number                   | signed/unsigned integer   | signed/unsigned integer    |
| `uint8_t`..`uint64_t` | number                   | unsigned integer          | unsigned integer           |
| `float`, `double`     | number                   | float32 / float64         | float32 / float64          |
| `std::string`         | string (copied)          | text string (copied)      | str (copied)               |
| `std::string_view`    | string (zero-copy)       | text string (zero-copy)   | str (zero-copy)            |
| `std::array<T, N>`    | array (fixed size)       | array (fixed size)        | array (fixed size)         |
| `std::vector<T>`      | array (dynamic)          | array (dynamic)           | array (dynamic)            |
| `std::tuple<...>`     | array (heterogeneous)    | array (heterogeneous)     | array (heterogeneous)      |
| `std::optional<T>`    | value or skipped         | value or skipped          | value or skipped           |
| enum types            | number (underlying type) | integer (underlying type) | integer (underlying type)  |

### Library types

| Type                       | JSON representation                      | CBOR representation            | MessagePack representation     |
|----------------------------|------------------------------------------|--------------------------------|--------------------------------|
| `mold::null_t`             | `null`                                   | null                           | nil                            |
| `mold::uuid_t`             | `"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"` | 16-byte binary                 | 16-byte bin                    |
| `mold::bytes_view_t`       | base64 string (zero-copy decode)         | byte string (zero-copy)        | bin (zero-copy)                |
| `mold::bytes_t<N>`         | base64 string                            | byte string                    | bin                            |
| `mold::string_t<N>`        | string (fixed-capacity)                  | text string (fixed-capacity)   | str (fixed-capacity)           |
| `mold::nullable_t<T>`      | value or `null`                          | value or null                  | value or nil                   |
| `mold::vector_t<T, N>`     | dynamic array (fixed-capacity)           | dynamic array (fixed-capacity) | dynamic array (fixed-capacity) |
| `mold::irange_t<Min, Max>` | number (range-checked)                   | integer (range-checked)        | integer (range-checked)        |
| `mold::frange_t<Min, Max>` | number (range-checked)                   | float (range-checked)          | float (range-checked)          |
| `mold::float16_t`          | number                                   | float16                        | float32                        |
| `mold::field_t<Key, T>`    | uses member name (key ignored)           | uses `Key` as map key          | uses member name (key ignored) |

## Usage examples

### Bounded numerical ranges

Compile-time range-checked numbers. Parsing fails if the value is out of bounds.

```cpp
struct sensor_limits_t {
    mold::irange_t<0, 100> humidity;
    mold::frange_t<-10.0, 50.0> temperature;
} limits;

std::string_view json_sv_1 = R"({
    "humidity": -1,
    "temperature": 1000
})";
assert(mold::json_to(limits, json_sv_1) != mold::error_t::ok);

std::string_view json_sv_2 = R"({
    "humidity": 50,
    "temperature": 23.7
})";
assert(mold::json_to(limits, json_sv_2) == mold::error_t::ok);
```

### Trailing characters

Trailing input after a valid value is reported as `error_t::trailing_data`. The parser still advances the input past the consumed prefix, so you can inspect or ignore the leftover tail.

```cpp
std::string_view json_sv = R"({"ok":true}garbage)";
struct status_t { bool ok; } s{};

auto err = mold::json_to(s, json_sv);
assert(err == mold::error_t::trailing_data);
// json_sv now points at "garbage"
printf("trailing: %.*s\n", (int) json_sv.size(), json_sv.data());
// You may still use `s` if you decide the trailing data is acceptable.
```

### Arrays

Homogeneous arrays with fixed (`std::array<T, N>`), dynamic (`std::vector<T>`), and fixed-capacity dynamic (`mold::vector_t<T, N>`) size. 
Heterogeneous arrays via `std::tuple` (fixed size only). `mold::vector_t<T, N>` is a stack-allocated vector that holds up to N elements 
without heap allocation. It serializes identically to `std::vector<T>`.

```cpp
#include <mold/types/vector.h>

struct my_struct_t {
    std::vector<int> dynamic_array;
    std::array<float, 4> fixed_array;
    std::tuple<int, float, bool> tuple_array;
    mold::vector_t<int, 8> static_array;
} object;

std::string_view json_sv = R"({
    "dynamic_array": [10, 20, 30, 40, 50],
    "fixed_array": [1.0, 2.5, 3.0, 4.0],
    "tuple_array": [100, 3.14, true],
    "static_array": [1, 2, 3]
})";

assert(mold::json_to(object, json_sv) == mold::error_t::ok);

printf("dynamic_array: [ ");
for (int num : object.dynamic_array) {
    printf("%d ", num);
}
printf("]\n");
printf("fixed_array: [%f, %f, %f, %f]\n",
    object.fixed_array[0], object.fixed_array[1],
    object.fixed_array[2], object.fixed_array[3]);
printf("tuple_array: [int: %d, float: %f, bool: %s]\n",
    std::get<0>(object.tuple_array),
    std::get<1>(object.tuple_array),
    std::get<2>(object.tuple_array) ? "true" : "false");
printf("static_array: [ ");
for (int num : object.static_array) {
    printf("%d ", num);
}
printf("]\n");
```

### Optional fields

`std::optional<T>` fields are __skipped entirely__ during serialization when empty — no key or value
is written. During deserialization, absent fields keep their default value (`std::nullopt`), and
explicit `null` in the input resets the optional.

Use `mold::nullable_t<T>` when the protocol requires an __explicit null__ on the wire for absent values.

```cpp
struct my_struct_t {
    std::optional<int> opt_present;
    std::optional<int> opt_absent;      // Skipped during serialization
    mold::nullable_t<int> null_present;
    mold::nullable_t<int> null_absent;  // Serialized as null
} object;

object.opt_present = 42;
object.null_present = 10;

// Serializes as: {"opt_present":42,"null_present":10,"null_absent":null}
// Note: opt_absent is skipped, null_absent is explicit null
```

Deserialization accepts both absent keys and explicit `null` for either type:

```cpp
std::string_view json_sv = R"({
    "opt_present": "Hello",
    "null_present": null
})";

struct input_t {
    std::optional<std::string> opt_present;
    std::optional<std::string> opt_missing;
    mold::nullable_t<std::string> null_present;
} input;

assert(mold::json_to(input, json_sv) == mold::error_t::ok);

assert(input.opt_present);
assert(!input.opt_missing);
assert(!input.null_present);
```

### Handling unexpected fields

Set `allow_unexpected` to `true` to silently skip unknown fields:

```cpp
struct device_status_t {
    std::string_view last_check_time;
    bool online;
} object;

std::string_view json_sv = R"({
    "online": true,
    "last_check_time": "2025-07-30T10:30:00Z",
    "firmware_version": "1.0.1",
    "uptime_seconds": 3600
})";
assert(mold::json_to(object, json_sv, true) == mold::error_t::ok);

printf("online:             %s\n", object.online ? "true" : "false");
printf("last_check_time:    %.*s\n", (int) object.last_check_time.size(), object.last_check_time.data());
```

### CBOR non-string map keys

CBOR maps support any data item as a key, not just strings. Use `mold::field_t<Key, T>` to
assign a compile-time key to a struct member. `Key` can be an integer, float, or any C++20
structural type (e.g. `std::array`). CBOR serialization emits `Key` as the raw CBOR-encoded
map key; JSON and MessagePack ignore it and use the reflected member name as usual.

```cpp
struct cose_header_t {
    mold::field_t<1, int32_t> alg;
    mold::field_t<4, std::string_view> kid;
};

cose_header_t header;

// CBOR round-trip: map keys are integers 1 and 4
std::array<uint8_t, 64> buf{};
std::span<uint8_t> out = buf;
assert(mold::cbor_from(header, out) == mold::error_t::ok);

// JSON round-trip: keys are member names "alg" and "kid"
std::string_view json_sv = R"({"alg": 42, "kid": "my-key"})";
assert(mold::json_to(header, json_sv) == mold::error_t::ok);
```

Negative integer, floating-point, and structural keys are also supported:

```cpp
struct example_t {
    mold::field_t<-1, bool> flag;
    mold::field_t<3.14, int> pi_val;
    mold::field_t<std::array<int, 2>{10, 20}, uint8_t> composite;
};
```

### Empty structs and unspecialized types

Empty structures are parsed as empty objects (`{}`). Non-aggregate types without a custom `spec_t<T>` specialization default to `null`.

```cpp
struct empty_t {};

struct my_struct_t {
    empty_t empty;
    std::mt19937 rng;
} object;

std::string_view json_sv = R"({
    "empty": {},
    "rng": null
})";

assert(mold::json_to(object, json_sv) == mold::error_t::ok);
```

### Custom type specialization

Support custom types by specializing `mold::spec_t<T>`. Define the expected type mask and
whichever static callbacks apply:

- __`read(T&, const io_value_t&)`__ / __`emit(const T&, const io_sink_t&)`__: format-agnostic primitive read/write
- __`json_read`__ / __`json_emit`__, __`cbor_read`__ / __`cbor_emit`__, __`msgpack_read`__ / __`msgpack_emit`__: format-specific variants receiving raw `json_primitive_t` / `cbor_primitive_t` / `msgpack_primitive_t`
- __`prepare(T&, size_t)`__: allocate/return a container element by index
- __`next(const T&, const void*)`__: iterate container elements for serialization
- __`nullable(T&, size_t)`__: handle optional/nullable initialization and access

See `include/mold/types/uuid.h` for a complete example of a custom type with format-specific serialization (hex string in JSON, 16-byte binary in CBOR and MessagePack).

## Limitations

- __No C-style arrays__: Deserialization into raw C-style arrays (e.g., `int my_arr[5]`) is not supported. Use `std::array` instead.
- __No heterogeneous arrays of dynamic size__: Use `std::tuple` for heterogeneous arrays (fixed size only).
- __No JSON comments__: The parser adheres to strict JSON and does not support comments.
- __Limited Unicode support__: The parser validates `\uXXXX` escape sequences but does not decode them into UTF-8. The resulting string contains the original literal `\uXXXX` sequence.
- __Aggregate member limit__: Capped at 32 direct members per struct by default; extend by adding more cases in `to_tuple.h`. Compilers with C++26 structured binding packs (P1061R10) have no limit.

## TODO

- [ ] Typed union support (probably via `std::variant`).
- [ ] Heterogeneous arrays of dynamic size support.
- [ ] Full Unicode decoding for `\uXXXX` sequences.

## Architecture

The library is organized into several modules under `include/mold/`:

| Directory | Purpose |
|-----------|---------|
| `json/` | JSON parsing (`json_parse.h`), serialization (`json_write.h`), type specs (`json_spec.h`), sink (`json_sink.h`), utilities (`json_util.h`), debug output (`json_debug.h`) |
| `cbor/` | CBOR parsing (`cbor_parse.h`), serialization and writer (`cbor_write.h`), type specs (`cbor_spec.h`), sink (`cbor_sink.h`), utilities (`cbor_util.h`), debug/pretty-print (`cbor_debug.h`) |
| `msgpack/` | MessagePack parsing (`msgpack_parse.h`), serialization and writer (`msgpack_write.h`), type specs (`msgpack_spec.h`), sink (`msgpack_sink.h`), utilities (`msgpack_util.h`), debug/pretty-print (`msgpack_debug.h`) |
| `refl/` | Compile-time reflection (`reflection.h`), aggregate-to-tuple conversion (`to_tuple.h`), shared parsing logic (`parse_common.h`), unified spec interface (`spec.h`), I/O types (`io.h`) |
| `types/` | Built-in type specializations: `std.h` (standard types), `uuid.h`, `bytes.h`, `string.h`, `vector.h` (fixed-capacity vector), `nullable.h` (explicit-null optional), `range.h`, `null.h`, `field.h` (CBOR non-string map keys) |
| `util/` | Error codes (`error.h`), output sinks (`sink.h`), concepts (`concepts.h`), containers (`container.h`), half-float (`half.h`), base64 (`base64.h`), common macros (`common.h`), debug helpers (`debug.h`), enum support (`enum.h`) |

The core architecture works as follows:

1. __Reflection data generation__: `type_info_t<T>` builds a `reflection_t` tree with names, offsets, element counts, and format-specific info: type bitmask (e.g. `json_type_t`, `cbor_type_t`, `msgpack_type_t`) and handler function pointer (from `json_spec_t<T>` / `cbor_spec_t<T>` / `msgpack_spec_t<T>`). For containers, element type and count are derived via `container_traits_t`.
2. __Iterative parsing__: `json_to` / `cbor_to` / `msgpack_to` runs a non-recursive, stack-based state machine. A pre-allocated boolean array tracks required/seen keys with no dynamic allocation during parsing.
3. __Validation & population__: Input is validated against the reflection tree. Each node's type bitmask is checked, and if matched, the handler is invoked with events to write directly into the destination object.
4. __Serialization__: `json_from` / `cbor_from` / `msgpack_from` walks the same reflection tree in reverse, reading from the struct and emitting output through a sink abstraction that supports callbacks, buffers, and stdout.

This approach avoids the overhead of traditional virtual-function-based or map-based libraries and eliminates the risk of stack overflow for deeply nested structures.
