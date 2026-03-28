#ifndef MOLD_JSON_UTIL_H
#define MOLD_JSON_UTIL_H

/**
 * @file
 * @brief JSON type definitions, low-level tokenizer, and primitive parsers.
 */

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cassert>
#include "mold/util/error.h"
#include "mold/util/enum.h"

namespace mold {

/**
 * @brief JSON alias for immutable pointer to character.
 * 
 */
using json_ptr_t = const char*;

/**
 * @brief Type for JSON items.
 * 
 */
enum class json_type_t : uint8_t {
    null        = 1u << 0,
    boolean     = 1u << 1,
    integer     = 1u << 2,
    floating    = 1u << 3,
    string      = 1u << 4,
    object      = 1u << 5,
    array       = 1u << 6,
    primitive   = boolean | integer | floating | string,
};
MOLD_ENABLE_ENUM_BITWISE_OPERATORS(json_type_t)

/**
 * @brief Check if a JSON type contains a specific mask.
 * 
 * @param type Type to check
 * @param mask Mask to check for
 * @return true if the type contains the mask
 */
constexpr bool json_type_has(json_type_t type, json_type_t mask) 
{
    return type & mask;
}

/**
 * @brief Get human readable name for JSON type.
 * 
 * @param type JSON type
 * @return String representation
 */
constexpr auto json_type_str(json_type_t type) 
{
    switch (type) {
        case json_type_t::null: return "null";
        case json_type_t::boolean: return "boolean";
        case json_type_t::integer: return "integer";
        case json_type_t::floating: return "floating";
        case json_type_t::string: return "string";
        case json_type_t::object: return "object";
        case json_type_t::array: return "array";
        default: return "<multi>";
    }
}

/**
 * @brief Generic decoded JSON value for primitive types.
 * 
 * Number (integer or floating), boolean, string and null.
 * 
 */
struct json_primitive_t {
    constexpr json_primitive_t() : type_{json_type_t::null} {}
    constexpr json_primitive_t(is_boolean auto val) : boolean_{val}, type_{json_type_t::boolean} {}
    constexpr json_primitive_t(is_integer auto val) : integer_{val}, type_{json_type_t::integer} {}
    constexpr json_primitive_t(is_floating auto val) : floating_{val}, type_{json_type_t::floating} {}
    constexpr json_primitive_t(is_string auto val) : string_{val}, type_{json_type_t::string} {}
    constexpr bool operator==(is_boolean auto val) const    { return is(json_type_t::boolean) && boolean_ == val; }
    constexpr bool operator==(is_integer auto val) const    { return is(json_type_t::integer) && integer_ == val; }
    constexpr bool operator==(is_floating auto val) const   { return is(json_type_t::floating) && floating_ == val; }
    constexpr bool operator==(is_string auto val) const     { return is(json_type_t::string) && string_ == val; }
#define MOLD_JSON_GET(t, v, d) MOLD_ASSERT(is(t)); return v;
    constexpr bool is(json_type_t t) const  { return type_ == t; }
    constexpr bool null() const             { return type_ == json_type_t::null; }
    constexpr auto type() const             { return type_; }
    constexpr auto boolean() const          { MOLD_JSON_GET(json_type_t::boolean, boolean_, false); }
    constexpr auto integer() const          { MOLD_JSON_GET(json_type_t::integer, integer_, int64_t(0)); }
    constexpr auto uinteger() const         { MOLD_JSON_GET(json_type_t::integer, uint64_t(integer_), uint64_t(0)); }
    constexpr auto floating() const         { MOLD_JSON_GET(json_type_t::floating, floating_, NAN); }
    constexpr auto string() const           { MOLD_JSON_GET(json_type_t::string, string_, std::string_view{}); }
    constexpr auto number() const           { return is(json_type_t::integer) ? double(integer_) : floating(); }
#undef MOLD_JSON_GET
private:
    union {
        bool boolean_;
        int64_t integer_;
        double floating_;
        std::string_view string_;
    };
    json_type_t type_;
};

/**
 * @brief Result of JSON primitive parsing.
 * 
 * Additionally contains error code and pointer to next character after last processed character.
 * 
 */
struct json_result_t : json_primitive_t {
    constexpr json_result_t() = default;
    constexpr json_result_t(json_ptr_t ptr, error_t err) :
        json_primitive_t(), err_(err), ptr_(ptr) 
    {}
    constexpr json_result_t(json_ptr_t ptr, error_t err, auto&&... args) : 
        json_primitive_t(std::forward<decltype(args)>(args)...), err_(err), ptr_(ptr) 
    {}
    constexpr auto err() const { return err_; }
    constexpr auto end() const { return ptr_; }
private:
    error_t err_ = error_t::ok;
    json_ptr_t ptr_ = nullptr;
};

/**
 * @brief Check if a character is a hexadecimal digit.
 * 
 * @param c Character
 * @return true if hexadecimal
 */
constexpr bool json_is_xdigit(char c) 
{
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/**
 * @brief Skip whitespace characters.
 * 
 * @param ptr Pointer to the current position in the string
 * @param end End of the string
 * @return Pointer to the next non-whitespace character
 */
constexpr json_ptr_t json_parse_space(json_ptr_t ptr, json_ptr_t end)
{
#if (1)
    while (ptr < end && (*ptr == ' ' || *ptr == '\n' || *ptr == '\r' || *ptr == '\t')) {
        ++ptr;
    }
#else
    while (ptr < end) {
        switch (*ptr) {
            case ' ':
            case '\n':
            case '\r':
            case '\t': ++ptr; continue;
        }
        return ptr;
    }
#endif
    return ptr;
}

/**
 * @brief Parse a JSON string and check for valid escape sequences, including Unicode.
 * 
 * @param ptr Start of the string (past the initial `"` quote)
 * @param end End of the buffer containing JSON
 * @return Result with optional string value
 */
constexpr json_result_t json_parse_string(json_ptr_t ptr, json_ptr_t end) 
{
    json_ptr_t head = ptr;

    while (ptr < end) {
        if (*ptr == '\"') {
            return {ptr + 1, error_t::ok, std::string_view{head, ptr}};
        }
        if (*ptr++ == '\\') {
            if (ptr >= end) { 
                return {ptr, error_t::unexpected_eof}; // unfinished escape sequence
            }
            switch (*ptr) 
            {
            case '\"': 
            case '\\': 
            case '/': 
            case 'b': 
            case 'f':
            case 'n': 
            case 'r': 
            case 't': ++ptr; 
            break;
            case 'u':
                if (ptr + 4 >= end) { // unfinished unicode sequence
                    return {ptr, error_t::unexpected_eof};
                }
                if (!json_is_xdigit(ptr[1]) || !json_is_xdigit(ptr[2]) ||
                    !json_is_xdigit(ptr[3]) || !json_is_xdigit(ptr[4])) {
                    return {ptr, error_t::invalid_unicode};
                }
            ptr += 5; // skip over \uXXXX
            break;
            default:
                return {ptr, error_t::invalid_escape};
            }
        }
    }
    return {ptr, error_t::unexpected_eof};
}

/**
 * @brief Parse a number from a JSON string.
 * 
 * Handles optional leading `-` sign, decimal point, and exponential notation. The returned 
 * JSON value can be of either `json_type_t::integer` or `json_type_t::floating` type.
 * 
 * @param ptr Start of the number
 * @param end End of the buffer containing JSON
 * @return Result with integer or floating number value
 */
constexpr json_result_t json_parse_number(json_ptr_t ptr, json_ptr_t end) 
{
    if (ptr >= end) {
        return {ptr, error_t::unexpected_eof};
    }
    uint64_t integer_part = 0;
    int64_t decimal_part = 0;
    bool negative = false;
    int dot = 0;
    int exp = 0;
    bool is_floating = false;
    bool is_exponent = false;
    bool exp_negative = false;

    if (*ptr == '-') {
        ++ptr;
        negative = true;
    }
    while (ptr < end) {
        if (*ptr >= '0' && *ptr <= '9') {
            if (is_exponent) {
                exp = exp * 10 + (*ptr - '0');
            } else if (is_floating) {
                ++dot;
                decimal_part = decimal_part * 10 + (*ptr - '0');
            } else {
                integer_part = integer_part * 10 + (*ptr - '0');
            }
        } else if ('.' == *ptr && !is_floating && !is_exponent) {
            is_floating = true;
        } else if ('e' == (*ptr | 0x20) && !is_exponent) {
            is_exponent = true;
            if (++ptr < end && (*ptr == '+' || *ptr == '-')) {
                exp_negative = (*ptr == '-');
                ++ptr;
            }
            continue;
        } else {
            if (is_floating || is_exponent) {
                double floating_val = double(integer_part) + double(decimal_part) / std::pow(10, dot);
                if (is_exponent) {
                    floating_val *= std::pow(10, exp_negative ? -exp : exp);
                }
                return {ptr, error_t::ok, floating_val * (negative ? -1.0 : 1.0)};
            } else {
                return {ptr, error_t::ok, negative ? -int64_t(integer_part) : int64_t(integer_part)};
            }
        }
        ++ptr;
    }
    return {ptr, error_t::unexpected_eof};
}

/**
 * @brief Parse JSON primitive value.
 * 
 * @param ptr Start of the value
 * @param end End of the buffer containing JSON
 * @return Result with primitive value
 */
constexpr json_result_t json_parse_primitive(json_ptr_t ptr, json_ptr_t end) 
{
    while (ptr < end) {
        switch (*ptr) 
        {
        case '\n': case '\r': case '\t':
        case ' ':
            ++ptr;
        break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '-':
            return json_parse_number(ptr, end);
        break;
        case '"':
            return json_parse_string(++ptr, end);
        break;
        default:
            if (ptr + 4 <= end) {
                if (std::memcmp(ptr, "null", 4) == 0) {
                    return {ptr + 4, error_t::ok};
                }
                if (std::memcmp(ptr, "true", 4) == 0) {
                    return {ptr + 4, error_t::ok, true};
                }
            }
            if (ptr + 5 <= end) {
                if (std::memcmp(ptr, "false", 5) == 0) {
                    return {ptr + 5, error_t::ok, false};
                }
            }
            return {ptr, error_t::invalid_literal};
        }
    }
    return {ptr, error_t::unexpected_eof};
}

/**
 * @brief Skip over the next complete JSON value in the input stream.
 * 
 * This function advances the pointer past the next complete JSON value 
 * of any type without performing validation. It properly handles nested 
 * structures like objects and arrays using nesting level counting.
 * 
 * @param ptr Start of the value
 * @param end End of the buffer containing JSON
 * @return error_t::ok if successfully skipped a value, specific error on failure
 */
constexpr error_t json_skip_value(json_ptr_t& ptr, json_ptr_t end) 
{
    ptr = json_parse_space(ptr, end);

    if (ptr >= end) {
        return error_t::unexpected_eof; // EOF before value
    }
    // Handle primitives/strings directly (no nesting)
    if (*ptr != '{' && *ptr != '[') {
        json_result_t result = json_parse_primitive(ptr, end);
        ptr = result.end();
        return result.err();
    }
    // Handle Objects and Arrays using nesting level counting
    int nesting_level = 1;
    ptr++; // Consume the opening '{' or '['

    while (nesting_level > 0 && ptr < end) {

        ptr = json_parse_space(ptr, end);

        if (ptr >= end) {
            return error_t::unexpected_eof; // Unterminated structure (EOF while nested)
        }
        if (*ptr == '{' || *ptr == '[') { // Check for object or array start
            nesting_level++;
            ptr++;
        } else if (*ptr == '}' || *ptr == ']') { // Check for object or array end
            nesting_level--;
            ptr++;
        } else if (*ptr == '"') { // String value needs careful skipping
            json_result_t str_res = json_parse_string(ptr + 1, end);
            if (str_res.err() != error_t::ok) {
                ptr = str_res.end();
                return str_res.err(); // Unterminated or invalid string within structure
            }
            ptr = str_res.end(); // Advance pointer past the entire string
        } else { 
            // Other characters (comma, colon, primitives within structure)
            // These don't affect the nesting level tracking, just consume them.
            ptr++; 
        }
    }
    // If the loop finished, success means the nesting level returned to 0.
    // If loop exited due to ptr >= end while nesting_level > 0, this returns false.
    return nesting_level == 0 ? error_t::ok : error_t::unexpected_eof; 
}

}

#endif
