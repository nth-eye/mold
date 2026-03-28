#ifndef MOLD_MSGPACK_UTIL_H
#define MOLD_MSGPACK_UTIL_H

/**
 * @file
 * @brief MessagePack type definitions, wire format helpers, and primitive parsers.
 */

#include <cstdint>
#include <cstring>
#include <limits>
#include <bit>
#include <span>
#include "mold/util/error.h"
#include "mold/util/common.h"

namespace mold {

/**
 * @brief Alias for immutable pointer to byte.
 *
 */
using msgpack_ptr_t = const uint8_t*;

/**
 * @brief Type categories for MessagePack items.
 *
 * Mirrors cbor_type_t for consistency with the shared reflection engine.
 */
enum class msgpack_type_t : uint8_t {
    null        = 1u << 0,
    boolean     = 1u << 1,
    integer     = 1u << 2,  ///< Both uint and sint
    floating    = 1u << 3,
    string      = 1u << 4,  ///< str format family
    bytes       = 1u << 5,  ///< bin format family
    object      = 1u << 6,  ///< map
    array       = 1u << 7,
    primitive   = boolean | integer | floating | string | bytes,
};
MOLD_ENABLE_ENUM_BITWISE_OPERATORS(msgpack_type_t)

/**
 * @brief Check if a MessagePack type contains a specific mask.
 *
 * @param type Type to check
 * @param mask Mask to check for
 * @return true if the type contains the mask
 */
constexpr bool msgpack_type_has(msgpack_type_t type, msgpack_type_t mask)
{
    return type & mask;
}

/**
 * @brief Get human readable name for MessagePack type.
 *
 * @param type MessagePack type
 * @return String representation
 */
constexpr auto msgpack_type_str(msgpack_type_t type)
{
    switch (type) {
    case msgpack_type_t::null:     return "null";
    case msgpack_type_t::boolean:  return "boolean";
    case msgpack_type_t::integer:  return "integer";
    case msgpack_type_t::floating: return "floating";
    case msgpack_type_t::string:   return "string";
    case msgpack_type_t::bytes:    return "bytes";
    case msgpack_type_t::object:   return "object";
    case msgpack_type_t::array:    return "array";
    default: return "<multi>";
    }
}

/**
 * @brief Read big-endian value from byte pointer.
 *
 * @tparam T Integer type to read
 * @param p Pointer to bytes
 * @return Value in host byte order
 */
template<class T>
constexpr T msgpack_getbe(msgpack_ptr_t p)
{
    T val = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        val = (val << 8) | p[i];
    }
    return val;
}

/**
 * @brief Generic decoded MessagePack primitive value.
 *
 * Number (integer or floating), boolean, string, bytes, and null.
 */
struct msgpack_primitive_t {
    constexpr msgpack_primitive_t() : type_{msgpack_type_t::null} {}
    constexpr msgpack_primitive_t(is_boolean auto val) : boolean_{val}, type_{msgpack_type_t::boolean} {}
    constexpr msgpack_primitive_t(int64_t val) : integer_{val}, type_{msgpack_type_t::integer} {}
    constexpr msgpack_primitive_t(uint64_t val) : uinteger_{val}, type_{msgpack_type_t::integer} {}
    constexpr msgpack_primitive_t(is_floating auto val) : floating_{val}, type_{msgpack_type_t::floating} {}
    constexpr msgpack_primitive_t(std::string_view val) : string_{val}, type_{msgpack_type_t::string} {}
    constexpr msgpack_primitive_t(std::span<const uint8_t> val) : bytes_{val}, type_{msgpack_type_t::bytes} {}
#define MOLD_MSGPACK_GET(t, v, d) MOLD_ASSERT(is(t)); return v;
    constexpr bool is(msgpack_type_t t) const  { return type_ == t; }
    constexpr bool null() const                { return type_ == msgpack_type_t::null; }
    constexpr auto type() const                { return type_; }
    constexpr auto boolean() const             { MOLD_MSGPACK_GET(msgpack_type_t::boolean, boolean_, false); }
    constexpr auto integer() const             { MOLD_MSGPACK_GET(msgpack_type_t::integer, integer_, int64_t(0)); }
    constexpr auto uinteger() const            { MOLD_MSGPACK_GET(msgpack_type_t::integer, uint64_t(integer_), uint64_t(0)); }
    constexpr auto floating() const            { MOLD_MSGPACK_GET(msgpack_type_t::floating, floating_, 0.0); }
    constexpr auto string() const              { MOLD_MSGPACK_GET(msgpack_type_t::string, string_, std::string_view{}); }
    constexpr auto bytes() const               { MOLD_MSGPACK_GET(msgpack_type_t::bytes, bytes_, std::span<const uint8_t>{}); }
    constexpr auto number() const              { return is(msgpack_type_t::integer) ? double(integer_) : floating(); }
#undef MOLD_MSGPACK_GET
private:
    union {
        bool boolean_;
        int64_t integer_;
        uint64_t uinteger_;
        double floating_;
        std::string_view string_;
        std::span<const uint8_t> bytes_;
    };
    msgpack_type_t type_;
};

/**
 * @brief Result of MessagePack primitive parsing.
 *
 * Contains error code and pointer to next byte after last processed byte.
 */
struct msgpack_result_t : msgpack_primitive_t {
    constexpr msgpack_result_t() = default;
    constexpr msgpack_result_t(msgpack_ptr_t ptr, error_t err) :
        msgpack_primitive_t(), err_(err), ptr_(ptr)
    {}
    constexpr msgpack_result_t(msgpack_ptr_t ptr, error_t err, auto&&... args) :
        msgpack_primitive_t(std::forward<decltype(args)>(args)...), err_(err), ptr_(ptr)
    {}
    constexpr auto err() const { return err_; }
    constexpr auto end() const { return ptr_; }
private:
    error_t err_ = error_t::ok;
    msgpack_ptr_t ptr_ = nullptr;
};

/**
 * @brief Parse a single MessagePack primitive value.
 *
 * Handles integers, floats, booleans, nil, strings, and bin.
 * Returns error for map/array headers (those are containers, not primitives).
 *
 * @param ptr Start of the MessagePack data
 * @param end End of the buffer
 * @return Result with primitive value
 */
constexpr msgpack_result_t msgpack_parse_primitive(msgpack_ptr_t ptr, msgpack_ptr_t end)
{
    if (ptr >= end) {
        return {ptr, error_t::unexpected_eof};
    }
    uint8_t b = *ptr++;

    // Positive fixint: 0x00-0x7f
    if (b <= 0x7f) {
        return {ptr, error_t::ok, uint64_t(b)};
    }
    // Negative fixint: 0xe0-0xff
    if (b >= 0xe0) {
        return {ptr, error_t::ok, int64_t(int8_t(b))};
    }
    // fixstr: 0xa0-0xbf
    if ((b & 0xe0) == 0xa0) {
        size_t len = b & 0x1f;
        if (ptr + len > end) {
            return {ptr, error_t::unexpected_eof};
        }
        return {ptr + len, error_t::ok, std::string_view{reinterpret_cast<const char*>(ptr), len}};
    }
    // fixmap/fixarray are containers, not primitives
    if ((b & 0xf0) == 0x80 || (b & 0xf0) == 0x90) {
        return {ptr - 1, error_t::type_mismatch_structure};
    }

    switch (b) {
    // nil
    case 0xc0:
        return {ptr, error_t::ok};
    // (unused) 0xc1
    case 0xc1:
        return {ptr, error_t::internal_logic_error};
    // false
    case 0xc2:
        return {ptr, error_t::ok, false};
    // true
    case 0xc3:
        return {ptr, error_t::ok, true};

    // bin8
    case 0xc4: {
        if (ptr >= end) { return {ptr, error_t::unexpected_eof}; }
        size_t len = *ptr++;
        if (ptr + len > end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + len, error_t::ok, std::span<const uint8_t>{ptr, len}};
    }
    // bin16
    case 0xc5: {
        if (ptr + 2 > end) { return {ptr, error_t::unexpected_eof}; }
        size_t len = msgpack_getbe<uint16_t>(ptr); ptr += 2;
        if (ptr + len > end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + len, error_t::ok, std::span<const uint8_t>{ptr, len}};
    }
    // bin32
    case 0xc6: {
        if (ptr + 4 > end) { return {ptr, error_t::unexpected_eof}; }
        size_t len = msgpack_getbe<uint32_t>(ptr); ptr += 4;
        if (ptr + len > end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + len, error_t::ok, std::span<const uint8_t>{ptr, len}};
    }

    // float32
    case 0xca: {
        if (ptr + 4 > end) { return {ptr, error_t::unexpected_eof}; }
        auto val = uint_to_float(msgpack_getbe<uint32_t>(ptr)); ptr += 4;
        return {ptr, error_t::ok, val};
    }
    // float64
    case 0xcb: {
        if (ptr + 8 > end) { return {ptr, error_t::unexpected_eof}; }
        auto val = uint_to_float(msgpack_getbe<uint64_t>(ptr)); ptr += 8;
        return {ptr, error_t::ok, val};
    }

    // uint8
    case 0xcc: {
        if (ptr >= end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + 1, error_t::ok, uint64_t(*ptr)};
    }
    // uint16
    case 0xcd: {
        if (ptr + 2 > end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + 2, error_t::ok, uint64_t(msgpack_getbe<uint16_t>(ptr))};
    }
    // uint32
    case 0xce: {
        if (ptr + 4 > end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + 4, error_t::ok, uint64_t(msgpack_getbe<uint32_t>(ptr))};
    }
    // uint64
    case 0xcf: {
        if (ptr + 8 > end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + 8, error_t::ok, uint64_t(msgpack_getbe<uint64_t>(ptr))};
    }

    // int8
    case 0xd0: {
        if (ptr >= end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + 1, error_t::ok, int64_t(int8_t(*ptr))};
    }
    // int16
    case 0xd1: {
        if (ptr + 2 > end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + 2, error_t::ok, int64_t(int16_t(msgpack_getbe<uint16_t>(ptr)))};
    }
    // int32
    case 0xd2: {
        if (ptr + 4 > end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + 4, error_t::ok, int64_t(int32_t(msgpack_getbe<uint32_t>(ptr)))};
    }
    // int64
    case 0xd3: {
        if (ptr + 8 > end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + 8, error_t::ok, int64_t(msgpack_getbe<uint64_t>(ptr))};
    }

    // str8
    case 0xd9: {
        if (ptr >= end) { return {ptr, error_t::unexpected_eof}; }
        size_t len = *ptr++;
        if (ptr + len > end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + len, error_t::ok, std::string_view{reinterpret_cast<const char*>(ptr), len}};
    }
    // str16
    case 0xda: {
        if (ptr + 2 > end) { return {ptr, error_t::unexpected_eof}; }
        size_t len = msgpack_getbe<uint16_t>(ptr); ptr += 2;
        if (ptr + len > end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + len, error_t::ok, std::string_view{reinterpret_cast<const char*>(ptr), len}};
    }
    // str32
    case 0xdb: {
        if (ptr + 4 > end) { return {ptr, error_t::unexpected_eof}; }
        size_t len = msgpack_getbe<uint32_t>(ptr); ptr += 4;
        if (ptr + len > end) { return {ptr, error_t::unexpected_eof}; }
        return {ptr + len, error_t::ok, std::string_view{reinterpret_cast<const char*>(ptr), len}};
    }

    // array16, array32, map16, map32: containers
    case 0xdc: case 0xdd: case 0xde: case 0xdf:
        return {ptr - 1, error_t::type_mismatch_structure};

    default:
        // ext types (0xc7-0xc9, 0xd4-0xd8): skip as unsupported
        return {ptr - 1, error_t::internal_logic_error};
    }
}

/**
 * @brief Skip over a complete MessagePack value.
 *
 * @param ptr Start of the value (updated to point past it)
 * @param end End of the buffer
 * @return Error code
 */
constexpr error_t msgpack_skip_value(msgpack_ptr_t& ptr, msgpack_ptr_t end)
{
    if (ptr >= end) {
        return error_t::unexpected_eof;
    }
    uint8_t b = *ptr++;

    // Positive fixint / negative fixint
    if (b <= 0x7f || b >= 0xe0) {
        return error_t::ok;
    }
    // fixstr
    if ((b & 0xe0) == 0xa0) {
        size_t len = b & 0x1f;
        if (ptr + len > end) { return error_t::unexpected_eof; }
        ptr += len;
        return error_t::ok;
    }
    // fixmap
    if ((b & 0xf0) == 0x80) {
        size_t n = b & 0x0f;
        for (size_t i = 0; i < n; ++i) {
            MOLD_TRY(msgpack_skip_value(ptr, end)); // key
            MOLD_TRY(msgpack_skip_value(ptr, end)); // value
        }
        return error_t::ok;
    }
    // fixarray
    if ((b & 0xf0) == 0x90) {
        size_t n = b & 0x0f;
        for (size_t i = 0; i < n; ++i) {
            MOLD_TRY(msgpack_skip_value(ptr, end));
        }
        return error_t::ok;
    }

    switch (b) {
    // nil, false, true
    case 0xc0: case 0xc2: case 0xc3:
        return error_t::ok;
    case 0xc1:
        return error_t::internal_logic_error;

    // bin8, str8
    case 0xc4: case 0xd9: {
        if (ptr >= end) { return error_t::unexpected_eof; }
        size_t len = *ptr++;
        if (ptr + len > end) { return error_t::unexpected_eof; }
        ptr += len;
        return error_t::ok;
    }
    // bin16, str16
    case 0xc5: case 0xda: {
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        size_t len = msgpack_getbe<uint16_t>(ptr); ptr += 2;
        if (ptr + len > end) { return error_t::unexpected_eof; }
        ptr += len;
        return error_t::ok;
    }
    // bin32, str32
    case 0xc6: case 0xdb: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        size_t len = msgpack_getbe<uint32_t>(ptr); ptr += 4;
        if (ptr + len > end) { return error_t::unexpected_eof; }
        ptr += len;
        return error_t::ok;
    }

    // ext8
    case 0xc7: {
        if (ptr >= end) { return error_t::unexpected_eof; }
        size_t len = *ptr++ + 1; // +1 for type byte
        if (ptr + len > end) { return error_t::unexpected_eof; }
        ptr += len;
        return error_t::ok;
    }
    // ext16
    case 0xc8: {
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        size_t len = msgpack_getbe<uint16_t>(ptr) + 1; ptr += 2;
        if (ptr + len > end) { return error_t::unexpected_eof; }
        ptr += len;
        return error_t::ok;
    }
    // ext32
    case 0xc9: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        size_t len = msgpack_getbe<uint32_t>(ptr) + 1; ptr += 4;
        if (ptr + len > end) { return error_t::unexpected_eof; }
        ptr += len;
        return error_t::ok;
    }

    // float32
    case 0xca: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        ptr += 4;
        return error_t::ok;
    }
    // float64
    case 0xcb: {
        if (ptr + 8 > end) { return error_t::unexpected_eof; }
        ptr += 8;
        return error_t::ok;
    }

    // uint8, int8
    case 0xcc: case 0xd0: {
        if (ptr >= end) { return error_t::unexpected_eof; }
        ptr += 1;
        return error_t::ok;
    }
    // uint16, int16
    case 0xcd: case 0xd1: {
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        ptr += 2;
        return error_t::ok;
    }
    // uint32, int32
    case 0xce: case 0xd2: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        ptr += 4;
        return error_t::ok;
    }
    // uint64, int64
    case 0xcf: case 0xd3: {
        if (ptr + 8 > end) { return error_t::unexpected_eof; }
        ptr += 8;
        return error_t::ok;
    }

    // fixext1..fixext16
    case 0xd4: { if (ptr + 2 > end) { return error_t::unexpected_eof; } ptr += 2; return error_t::ok; }
    case 0xd5: { if (ptr + 3 > end) { return error_t::unexpected_eof; } ptr += 3; return error_t::ok; }
    case 0xd6: { if (ptr + 5 > end) { return error_t::unexpected_eof; } ptr += 5; return error_t::ok; }
    case 0xd7: { if (ptr + 9 > end) { return error_t::unexpected_eof; } ptr += 9; return error_t::ok; }
    case 0xd8: { if (ptr + 17 > end) { return error_t::unexpected_eof; } ptr += 17; return error_t::ok; }

    // array16
    case 0xdc: {
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        size_t n = msgpack_getbe<uint16_t>(ptr); ptr += 2;
        for (size_t i = 0; i < n; ++i) {
            MOLD_TRY(msgpack_skip_value(ptr, end));
        }
        return error_t::ok;
    }
    // array32
    case 0xdd: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        size_t n = msgpack_getbe<uint32_t>(ptr); ptr += 4;
        for (size_t i = 0; i < n; ++i) {
            MOLD_TRY(msgpack_skip_value(ptr, end));
        }
        return error_t::ok;
    }
    // map16
    case 0xde: {
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        size_t n = msgpack_getbe<uint16_t>(ptr); ptr += 2;
        for (size_t i = 0; i < n; ++i) {
            MOLD_TRY(msgpack_skip_value(ptr, end));
            MOLD_TRY(msgpack_skip_value(ptr, end));
        }
        return error_t::ok;
    }
    // map32
    case 0xdf: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        size_t n = msgpack_getbe<uint32_t>(ptr); ptr += 4;
        for (size_t i = 0; i < n; ++i) {
            MOLD_TRY(msgpack_skip_value(ptr, end));
            MOLD_TRY(msgpack_skip_value(ptr, end));
        }
        return error_t::ok;
    }

    default:
        return error_t::internal_logic_error;
    }
}

}

#endif
