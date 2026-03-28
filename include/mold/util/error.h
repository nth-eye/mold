#ifndef MOLD_UTIL_ERROR_H
#define MOLD_UTIL_ERROR_H

/**
 * @file
 * @brief Error codes and diagnostic utilities.
 */

#include <cstdint>

/**
 * @brief Early-return on error. Evaluates `expr` and returns the error if not `error_t::ok`.
 *
 * @param expr Expression returning `error_t`
 */
#define MOLD_TRY(expr) if (auto e = (expr); e != error_t::ok) { return e; }

namespace mold {

/**
 * @brief Common error codes within the library.
 *
 */
enum class error_t : uint8_t {
    // Common errors
    ok,                         ///< No error.
    unexpected_eof,             ///< Unexpected end of input or out of bounds write attempt.
    internal_logic_error,       ///< Internal logic error.
    invalid_argument,           ///< Invalid argument passed.
    handler_failure,            ///< User handler reported failure.
    missing_key,                ///< Missing required object key.
    duplicate_key,              ///< Duplicate object key.
    unexpected_key,             ///< Unexpected object key.
    type_mismatch_structure,    ///< Structure type mismatch (object vs array, etc.).
    type_mismatch_primitive,    ///< Primitive type mismatch (string vs number, etc.).
    array_size_mismatch,        ///< Array/tuple element count mismatch.
    trailing_data,              ///< Trailing bytes/characters after valid data.
    // JSON-specific errors
    invalid_literal,            ///< Invalid literal keyword (true/false/null).
    invalid_unicode,            ///< Invalid unicode escape sequence.
    invalid_escape,             ///< Invalid escape sequence.
    parse_error_key,            ///< Key parse error.
    // CBOR-specific errors
    reserved_ai,                ///< Reserved additional info value (28-30).
    invalid_break,              ///< Break without indefinite start.
    invalid_simple,             ///< Invalid simple value.
    invalid_indef_mt,           ///< Invalid indefinite major type.
    invalid_indef_string,       ///< Invalid indefinite string.
    key_not_string,             ///< Map key is not a text string.
};

/**
 * @brief Get human-readable string for error code.
 *
 * @param e Error code
 * @return String representation
 */
constexpr const char* error_str(error_t e)
{
    switch (e) {
        // Common
        case error_t::ok:                       return "ok";
        case error_t::unexpected_eof:           return "unexpected_eof";
        case error_t::internal_logic_error:     return "internal_logic_error";
        case error_t::invalid_argument:         return "invalid_argument";
        case error_t::handler_failure:          return "handler_failure";
        case error_t::missing_key:              return "missing_key";
        case error_t::duplicate_key:            return "duplicate_key";
        case error_t::unexpected_key:           return "unexpected_key";
        case error_t::type_mismatch_structure:  return "type_mismatch_structure";
        case error_t::type_mismatch_primitive:  return "type_mismatch_primitive";
        case error_t::array_size_mismatch:      return "array_size_mismatch";
        case error_t::trailing_data:            return "trailing_data";
        // JSON-specific
        case error_t::invalid_literal:          return "invalid_literal";
        case error_t::invalid_unicode:          return "invalid_unicode";
        case error_t::invalid_escape:           return "invalid_escape";
        case error_t::parse_error_key:          return "parse_error_key";
        // CBOR-specific
        case error_t::reserved_ai:              return "reserved_ai";
        case error_t::invalid_break:            return "invalid_break";
        case error_t::invalid_simple:           return "invalid_simple";
        case error_t::invalid_indef_mt:         return "invalid_indef_mt";
        case error_t::invalid_indef_string:     return "invalid_indef_string";
        case error_t::key_not_string:           return "key_not_string";
        default:                                return "<unknown_error>";
    }
}

}

#endif
