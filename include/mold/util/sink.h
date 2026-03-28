#ifndef MOLD_UTIL_SINK_H
#define MOLD_UTIL_SINK_H

/**
 * @file
 * @brief Byte-oriented output sink interface used by all serialization formats.
 */

#include <cstdarg>
#include <cstdint>
#include <span>
#include <string_view>
#include "mold/util/error.h"

namespace mold {

/**
 * @brief Universal byte sink callback type.
 *
 * @param b Byte to write
 * @param ctx User-defined context pointer
 * @return true on successful write
 */
using sink_cb_t = bool (*)(uint8_t b, void* ctx);

/**
 * @brief Common sink base with shared write functionality.
 *
 * Provides fundamental byte-oriented output operations used by
 * all serialization formats (JSON, CBOR, etc.).
 */
struct sink_t {

    /**
     * @brief Constructor with callback and context.
     *
     * @param put_cb Callback to output byte
     * @param user_ctx User context for the callback
     */
    constexpr sink_t(sink_cb_t put_cb, void* user_ctx) : put(put_cb), ctx(user_ctx)
    {}

    /**
     * @brief Write a single byte.
     *
     * @param b Byte to emit
     * @return `error_t::ok` on success
     */
    constexpr error_t write_byte(uint8_t b) const
    {
        if (!put) {
            return error_t::invalid_argument;
        }
        if (!put(b, ctx)) {
            return error_t::unexpected_eof;
        }
        return error_t::ok;
    }

    /**
     * @brief Write a span of bytes.
     *
     * @param bytes Bytes to emit
     * @return `error_t::ok` on success
     */
    constexpr error_t write_bytes(std::span<const uint8_t> bytes) const
    {
        for (uint8_t b : bytes) {
            MOLD_TRY(write_byte(b));
        }
        return error_t::ok;
    }

    /**
     * @brief Write a byte repeated N times.
     *
     * @param b Byte to emit
     * @param n Number of times to repeat
     * @return `error_t::ok` on success
     */
    constexpr error_t write_repeat(uint8_t b, size_t n) const
    {
        for (size_t i = 0; i < n; ++i) {
            MOLD_TRY(write_byte(b));
        }
        return error_t::ok;
    }

    /**
     * @brief Write a null-terminated C string.
     *
     * @param s C-string to emit
     * @return `error_t::ok` on success
     */
    constexpr error_t write_literal(const char* s) const
    {
        while (*s) {
            MOLD_TRY(write_byte(uint8_t(*s++)));
        }
        return error_t::ok;
    }

    /**
     * @brief Write a string view.
     *
     * @param sv String to emit
     * @return `error_t::ok` on success
     */
    constexpr error_t write_str(std::string_view sv) const
    {
        for (char c : sv) {
            MOLD_TRY(write_byte(uint8_t(c)));
        }
        return error_t::ok;
    }

    /**
     * @brief Format and write using vsnprintf.
     *
     * @param fmt Printf-style format string
     * @param ... Arguments to format
     * @return `error_t::ok` on success
     */
    error_t write_vfmt(const char* fmt, ...) const
    {
        char tmp[64];
        va_list args;
        va_start(args, fmt);
        int n = std::vsnprintf(tmp, sizeof(tmp), fmt, args);
        va_end(args);
        if (n < 0 || size_t(n) >= sizeof(tmp)) {
            return error_t::internal_logic_error;
        }
        return write_literal(tmp);
    }

    /**
     * @brief Write a signed integer using %lld format.
     *
     * @param value Number to emit
     * @return `error_t::ok` on success
     */
    error_t write_fmt_lld(long long value) const
    {
        return write_vfmt("%lld", value);
    }

    /**
     * @brief Write an unsigned integer using %llu format.
     *
     * @param value Number to emit
     * @return `error_t::ok` on success
     */
    error_t write_fmt_llu(unsigned long long value) const
    {
        return write_vfmt("%llu", value);
    }

    /**
     * @brief Write a floating-point number using %g format.
     *
     * @param value Number to emit
     * @return `error_t::ok` on success
     */
    error_t write_fmt_g(double value) const
    {
        return write_vfmt("%g", value);
    }

protected:
    sink_cb_t put = nullptr;    ///< Callback to output byte.
    void* ctx = nullptr;        ///< User context for the callback.
};

}

#endif
