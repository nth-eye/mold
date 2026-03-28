#ifndef MOLD_CBOR_UTIL_H
#define MOLD_CBOR_UTIL_H

/**
 * @file
 * @brief CBOR type definitions, wire format helpers, and primitive parsers.
 */

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <limits>
#include <bit>
#include <span>
#include "mold/util/error.h"
#include "mold/util/enum.h"
#include "mold/util/common.h"

namespace mold {

/**
 * @brief Alias for immutable pointer to byte.
 * 
 */
using cbor_ptr_t = const uint8_t*;

/**
 * @brief Type for CBOR items (mapped to JSON-like categories).
 * 
 * CBOR has more types than JSON, but for struct reflection we map them
 * to JSON-compatible categories. String keys only, like JSON.
 */
enum class cbor_type_t : uint8_t {
    null        = 1u << 0,
    boolean     = 1u << 1,
    integer     = 1u << 2,  ///< Both uint and sint
    floating    = 1u << 3,
    string      = 1u << 4,  ///< Text string
    bytes       = 1u << 5,  ///< Byte string (CBOR-specific)
    object      = 1u << 6,  ///< Map
    array       = 1u << 7,
    primitive   = boolean | integer | floating | string | bytes,
};
MOLD_ENABLE_ENUM_BITWISE_OPERATORS(cbor_type_t)

/**
 * @brief Check if a CBOR type contains a specific mask.
 * 
 * @param type Type to check
 * @param mask Mask to check for
 * @return true if the type contains the mask
 */
constexpr bool cbor_type_has(cbor_type_t type, cbor_type_t mask) 
{
    return type & mask;
}

/**
 * @brief CBOR major type (3 bits).
 * 
 */
enum class cbor_mt_t : uint8_t {
    uint    = 0 << 5,
    nint    = 1 << 5,
    data    = 2 << 5,
    text    = 3 << 5,
    arr     = 4 << 5,
    map     = 5 << 5,
    tag     = 6 << 5,
    simple  = 7 << 5,
};

/**
 * @brief CBOR additional info (5 bits).
 * 
 */
enum class cbor_ai_t : uint8_t {
    _0      = 23,
    _1      = 24,
    _2      = 25,
    _4      = 26,
    _8      = 27,
    _indef  = 31,
};

/**
 * @brief CBOR simple values.
 * 
 */
enum class cbor_simple_t : uint8_t {
    bool_false  = 20,
    bool_true   = 21,
    null        = 22,
    undefined   = 23,
    float16     = 25,
    float32     = 26,
    float64     = 27,
};

/**
 * @brief Get human readable name for CBOR type.
 * 
 * @param type CBOR type
 * @return String representation
 */
constexpr auto cbor_type_str(cbor_type_t type) 
{
    switch (type) {
        case cbor_type_t::null: return "null";
        case cbor_type_t::boolean: return "boolean";
        case cbor_type_t::integer: return "integer";
        case cbor_type_t::floating: return "floating";
        case cbor_type_t::string: return "string";
        case cbor_type_t::bytes: return "bytes";
        case cbor_type_t::object: return "object";
        case cbor_type_t::array: return "array";
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
constexpr T cbor_getbe(cbor_ptr_t p)
{
    T val = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        val = (val << 8) | p[i];
    }
    return val;
}

/**
 * @brief Generic decoded CBOR primitive value.
 * 
 * Number (integer or floating), boolean, string, bytes, and null.
 */
struct cbor_primitive_t {
    constexpr cbor_primitive_t() : type_{cbor_type_t::null} {}
    constexpr cbor_primitive_t(is_boolean auto val) : boolean_{val}, type_{cbor_type_t::boolean} {}
    constexpr cbor_primitive_t(int64_t val) : integer_{val}, type_{cbor_type_t::integer} {}
    constexpr cbor_primitive_t(uint64_t val) : uinteger_{val}, type_{cbor_type_t::integer} {}
    constexpr cbor_primitive_t(is_floating auto val) : floating_{val}, type_{cbor_type_t::floating} {}
    constexpr cbor_primitive_t(std::string_view val) : string_{val}, type_{cbor_type_t::string} {}
    constexpr cbor_primitive_t(std::span<const uint8_t> val) : bytes_{val}, type_{cbor_type_t::bytes} {}
#define MOLD_CBOR_GET(t, v, d) MOLD_ASSERT(is(t)); return v;
    constexpr bool is(cbor_type_t t) const  { return type_ == t; }
    constexpr bool null() const             { return type_ == cbor_type_t::null; }
    constexpr auto type() const             { return type_; }
    constexpr auto boolean() const          { MOLD_CBOR_GET(cbor_type_t::boolean, boolean_, false); }
    constexpr auto integer() const          { MOLD_CBOR_GET(cbor_type_t::integer, integer_, int64_t(0)); }
    constexpr auto uinteger() const         { MOLD_CBOR_GET(cbor_type_t::integer, uint64_t(integer_), uint64_t(0)); }
    constexpr auto floating() const         { MOLD_CBOR_GET(cbor_type_t::floating, floating_, NAN); }
    constexpr auto string() const           { MOLD_CBOR_GET(cbor_type_t::string, string_, std::string_view{}); }
    constexpr auto bytes() const            { MOLD_CBOR_GET(cbor_type_t::bytes, bytes_, std::span<const uint8_t>{}); }
    constexpr auto number() const           { return is(cbor_type_t::integer) ? double(integer_) : floating(); }
#undef MOLD_CBOR_GET
private:
    union {
        bool boolean_;
        int64_t integer_;
        uint64_t uinteger_;
        double floating_;
        std::string_view string_;
        std::span<const uint8_t> bytes_;
    };
    cbor_type_t type_;
};

/**
 * @brief Result of CBOR primitive parsing.
 * 
 * Contains error code and pointer to next byte after last processed byte.
 */
struct cbor_result_t : cbor_primitive_t {
    constexpr cbor_result_t() = default;
    constexpr cbor_result_t(cbor_ptr_t ptr, error_t err) :
        cbor_primitive_t(), err_(err), ptr_(ptr) 
    {}
    constexpr cbor_result_t(cbor_ptr_t ptr, error_t err, auto&&... args) : 
        cbor_primitive_t(std::forward<decltype(args)>(args)...), err_(err), ptr_(ptr) 
    {}
    constexpr auto err() const { return err_; }
    constexpr auto end() const { return ptr_; }
private:
    error_t err_ = error_t::ok;
    cbor_ptr_t ptr_ = nullptr;
};

/**
 * @brief Decode CBOR additional info field.
 * 
 * @param ai Additional info byte (5 bits)
 * @param p Begin pointer (must be valid)
 * @param end End pointer (must be valid)
 * @return Tuple with error, decoded value, and pointer past interpreted bytes
 */
constexpr std::tuple<error_t, uint64_t, cbor_ptr_t> cbor_decode_ai(uint8_t ai, cbor_ptr_t p, const cbor_ptr_t end)
{
    if (ai < +cbor_ai_t::_1) {
        return {error_t::ok, ai, p};
    }
    if (ai > 27 && ai < 31) {
        return {error_t::reserved_ai, 0, p};
    }
    if (ai == +cbor_ai_t::_indef) {
        return {error_t::ok, 0, p};
    }
    size_t len = 1 << (ai - +cbor_ai_t::_1);

    if (p + len > end) {
        return {error_t::unexpected_eof, 0, p};
    }
    uint64_t val = 0;
    switch (ai) {
        case +cbor_ai_t::_1: val = *p; break;
        case +cbor_ai_t::_2: val = cbor_getbe<uint16_t>(p); break;
        case +cbor_ai_t::_4: val = cbor_getbe<uint32_t>(p); break;
        case +cbor_ai_t::_8: val = cbor_getbe<uint64_t>(p); break;
    }
    return {error_t::ok, val, p + len};
}

/**
 * @brief Parse a single CBOR primitive value.
 * 
 * @param ptr Start of the CBOR data
 * @param end End of the buffer
 * @return Result with primitive value
 */
constexpr cbor_result_t cbor_parse_primitive(cbor_ptr_t ptr, cbor_ptr_t end)
{
    if (ptr >= end) {
        return {ptr, error_t::unexpected_eof};
    }
    uint8_t mt = *ptr & 0xe0;
    uint8_t ai = *ptr++ & 0x1f;
    auto [err, val, next] = cbor_decode_ai(ai, ptr, end);
    
    if (err != error_t::ok) {
        return {next, err};
    }
    ptr = next;
    
    switch (mt) 
    {
    case +cbor_mt_t::uint:
        return {ptr, error_t::ok, val};
        
    case +cbor_mt_t::nint:
        return {ptr, error_t::ok, int64_t(~val)};
        
    case +cbor_mt_t::data:
        if (ai == +cbor_ai_t::_indef) {
            return {ptr, error_t::invalid_indef_mt}; // Not supporting indefinite bytes in primitives
        }
        if (ptr + val > end) {
            return {ptr, error_t::unexpected_eof};
        }
        return {ptr + val, error_t::ok, std::span<const uint8_t>{ptr, size_t(val)}};
        
    case +cbor_mt_t::text:
        if (ai == +cbor_ai_t::_indef) {
            return {ptr, error_t::invalid_indef_mt}; // Not supporting indefinite text in primitives
        }
        if (ptr + val > end) {
            return {ptr, error_t::unexpected_eof};
        }
        return {ptr + val, error_t::ok, std::string_view{reinterpret_cast<const char*>(ptr), size_t(val)}};
        
    case +cbor_mt_t::simple:
        switch (ai) 
        {
        case +cbor_simple_t::bool_false:
            return {ptr, error_t::ok, false};
        case +cbor_simple_t::bool_true:
            return {ptr, error_t::ok, true};
        case +cbor_simple_t::null:
        case +cbor_simple_t::undefined:
            return {ptr, error_t::ok}; // null
        case +cbor_simple_t::float16:
            return {ptr, error_t::ok, uint_to_float(uint16_t(val))};
        case +cbor_simple_t::float32:
            return {ptr, error_t::ok, uint_to_float(uint32_t(val))};
        case +cbor_simple_t::float64:
            return {ptr, error_t::ok, uint_to_float(uint64_t(val))};
        default: // Other simple values treated as null for now
            return {ptr, error_t::ok};
        }
        default: // Array, map, tag are not primitives
            return {ptr - 1, error_t::type_mismatch_structure};
    }
}

/**
 * @brief Skip over a complete CBOR value.
 * 
 * @param ptr Start of the value (updated to point past it)
 * @param end End of the buffer
 * @return Error code
 */
constexpr error_t cbor_skip_value(cbor_ptr_t& ptr, cbor_ptr_t end)
{
    if (ptr >= end) {
        return error_t::unexpected_eof;
    }
    auto mt = cbor_mt_t(*ptr & 0xe0);
    auto ai = uint8_t(*ptr++ & 0x1f);
    auto [err, val, next] = cbor_decode_ai(ai, ptr, end);

    if (err != error_t::ok) {
        return err;
    }
    ptr = next;
    
    switch (mt) 
    {
    case cbor_mt_t::uint:
    case cbor_mt_t::nint:
        return error_t::ok;
    
    case cbor_mt_t::data:
    case cbor_mt_t::text:
        if (ai == +cbor_ai_t::_indef) {
            // Skip indefinite string chunks
            while (ptr < end && *ptr != 0xff) {
                MOLD_TRY(cbor_skip_value(ptr, end));
            }
            if (ptr >= end) {
                return error_t::unexpected_eof;
            }
            ptr++; // skip break
        } else {
            if (ptr + val > end) {
                return error_t::unexpected_eof;
            }
            ptr += val;
        }
        return error_t::ok;
    
    case cbor_mt_t::arr:
        if (ai == +cbor_ai_t::_indef) {
            while (ptr < end && *ptr != 0xff) {
                MOLD_TRY(cbor_skip_value(ptr, end));
            }
            if (ptr >= end) {
                return error_t::unexpected_eof;
            }
            ptr++; // skip break
        } else {
            for (uint64_t i = 0; i < val; ++i) {
                MOLD_TRY(cbor_skip_value(ptr, end));
            }
        }
        return error_t::ok;
    
    case cbor_mt_t::map:
        if (ai == +cbor_ai_t::_indef) {
            while (ptr < end && *ptr != 0xff) {
                MOLD_TRY(cbor_skip_value(ptr, end));
                MOLD_TRY(cbor_skip_value(ptr, end));
            }
            if (ptr >= end) {
                return error_t::unexpected_eof;
            }
            ptr++; // skip break
        } else {
            for (uint64_t i = 0; i < val; ++i) {
                MOLD_TRY(cbor_skip_value(ptr, end));
                MOLD_TRY(cbor_skip_value(ptr, end));
            }
        }
        return error_t::ok;
    
    case cbor_mt_t::tag:
        return cbor_skip_value(ptr, end);
    
    case cbor_mt_t::simple:
        if (ai == +cbor_ai_t::_indef) {
            return error_t::invalid_break;
        }
        return error_t::ok;
    
    default:
        return error_t::internal_logic_error;
    }
}

}

#endif

