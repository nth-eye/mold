#ifndef MOLD_UTIL_COMMON_H
#define MOLD_UTIL_COMMON_H

/**
 * @file
 * @brief Common macros, compiler abstractions, and low-level helper functions.
 */

#include "mold/config.h"
#include "mold/util/concepts.h"
#include <cstdio>
#include <cctype>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <bit>

// Half-precision float type selection:
//  1. C++23 std::float16_t (__STDCPP_FLOAT16_T__)
//  2. _Float16 — available on Clang (all platforms) and GCC 13+ when __FLT16_MAX__ is defined.
//     GCC < 13 defines __FLT16_MAX__ on ARM with -mfp16-format=ieee but _Float16 is C-only.
//  3. Software half_t fallback
#ifdef __STDCPP_FLOAT16_T__
#include <stdfloat>
namespace mold {
    using float16_t = std::float16_t;
    using float32_t = std::float32_t;
    using float64_t = std::float64_t;
}
#elif defined(__FLT16_MAX__) && (defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 13))
namespace mold {
    using float16_t = _Float16;
    using float32_t = float;
    using float64_t = double;
}
#else
#include "mold/util/half.h"
namespace mold {
    using float16_t = half_t;
    using float32_t = float;
    using float64_t = double;
}
#endif
static_assert(sizeof(float) == 4,  "float must be 32-bit IEEE 754");
static_assert(sizeof(double) == 8, "double must be 64-bit IEEE 754");

#if (MOLD_PRINT_ENABLED)
#define MOLD_PUTCHAR(c)             std::putchar(c)
#define MOLD_PRINT(...)             std::printf(__VA_ARGS__)
#else
#define MOLD_PUTCHAR(c)
#define MOLD_PRINT(...)
#endif

#if (MOLD_DEBUG_ENABLED)
#define MOLD_DEBUG_LOG(fmt, ...)    MOLD_PRINT("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define MOLD_DEBUG_LOG(fmt, ...)
#endif

#if defined(__clang__) || defined(__GNUC__)
#define MOLD_PRETTY_FUNCTION        __PRETTY_FUNCTION__
#define MOLD_NOINLINE               __attribute__((noinline))
#elif defined(_MSC_VER)
#define MOLD_PRETTY_FUNCTION        __FUNCSIG__
#define MOLD_NOINLINE               __declspec(noinline)
#else
#error "Unsupported compiler."
#endif
#define MOLD_ASSERT(expr)           assert(expr)

namespace mold {
namespace detail {

/**
 * @brief Lowercase hex digit lookup table.
 * 
 */
inline constexpr char hex_digits[] = "0123456789abcdef";

}

/**
 * @brief Convert unsigned integer to floating-point representation via `bit_cast`.
 * ```
 * - uint16_t -> mold::float16_t
 * - uint32_t -> mold::float32_t
 * - uint64_t -> mold::float64_t
 * ```
 * @param bits Unsigned integer containing the bit representation
 * @return Floating-point value
 */
template<is_unsigned T>
constexpr double uint_to_float(T bits)
{
    static_assert(
        sizeof(T) == 2 || 
        sizeof(T) == 4 || 
        sizeof(T) == 8, "Unsupported integer size");

    if constexpr (sizeof(T) == 2) {
        return std::bit_cast<mold::float16_t>(bits);
    } else if constexpr (sizeof(T) == 4) {
        return std::bit_cast<mold::float32_t>(bits);
    } else if constexpr (sizeof(T) == 8) {
        return std::bit_cast<mold::float64_t>(bits);
    }
    return 0;
}

/**
 * @brief Print indentation spaces for pretty-printing.
 *
 * @param indent Spaces per indentation level (0 = no output, compact mode)
 * @param level Current nesting depth level
 */
MOLD_NOINLINE inline void print_indent(int indent, int level)
{
    if (indent) {
        for (int i = 0; i < indent * level; ++i) {
            MOLD_PUTCHAR(' ');
        }
    }
}

/**
 * @brief Print hex dump nicely with relevant ASCII representation.
 * 
 * @param dat Data to print
 * @param len Length in bytes
 */
MOLD_NOINLINE inline void print_hex(const void* dat, size_t len)
{
    if (!dat || !len) {
        return;
    }
    auto p = static_cast<const uint8_t*>(dat);

    for (size_t i = 0; i < len; ++i) {

        if (!(i & 15)) {
            MOLD_PUTCHAR('|');
            MOLD_PUTCHAR(' ');
        }
        MOLD_PUTCHAR(detail::hex_digits[p[i] >> 4]);
        MOLD_PUTCHAR(detail::hex_digits[p[i] & 0xf]);
        MOLD_PUTCHAR(' ');
        
        if ((i & 7) == 7) {
            MOLD_PUTCHAR(' ');
        }
        if ((i & 15) == 15) {
            MOLD_PUTCHAR('|');
            for (int j = 15; j >= 0; --j) {
                char c = p[i - j];
                MOLD_PUTCHAR(std::isprint(c) ? c : '.');
            }
            MOLD_PUTCHAR('|');
            MOLD_PUTCHAR('\n');
        }
    }
    int rem = len - ((len >> 4) << 4);
    if (rem) {
        for (int j = (16 - rem) * 3 + ((~rem & 8) >> 3); j >= 0; --j) {
            MOLD_PUTCHAR(' ');
        }
        MOLD_PUTCHAR('|');
        for (int j = rem; j; --j) {
            char c = p[len - j];
            MOLD_PUTCHAR(std::isprint(c) ? c : '.');
        }
        for (int j = 0; j < 16 - rem; ++j) {
            MOLD_PUTCHAR('.');
        }
        MOLD_PUTCHAR('|');
        MOLD_PUTCHAR('\n');
    }
}

}

#endif
