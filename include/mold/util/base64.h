#ifndef MOLD_UTIL_BASE64_H
#define MOLD_UTIL_BASE64_H

/**
 * @file
 * @brief Base64 encoding and decoding utilities (RFC 4648).
 */

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace mold {
namespace detail {

/**
 * @brief Standard base64 encoding alphabet (RFC 4648).
 *
 */
inline constexpr char base64_encode_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * @brief Lookup table mapping ASCII code points to 6-bit base64 values (255 = invalid).
 *
 */
inline constexpr uint8_t base64_decode_table[] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  62, 255, 255, 255,  63,
     52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255, 255,   0, 255, 255,
    255,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
     15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255, 255,
    255,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
     41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51, 255, 255, 255, 255, 255,
};

}

/**
 * @brief Compute the base64-encoded length for `n` raw bytes.

 * @param n Number of raw bytes to encode.
 * @return Length of the resulting base64 string (always a multiple of 4).
 */
constexpr size_t base64_encoded_size(size_t n)
{
    return (n + 2) / 3 * 4;
}

/**
 * @brief Compute the decoded byte length of a base64 string, accounting for padding.

 * @param sv Base64-encoded string (must be a multiple of 4 characters).
 * @return Number of decoded bytes, or 0 if `sv` is empty.
 */
constexpr size_t base64_decoded_size(std::string_view sv)
{
    return sv.empty() ? 0 : sv.size() / 4 * 3
        - (sv[sv.size() - 1] == '=')
        - (sv[sv.size() - 2] == '=');
}

/**
 * @brief Encode raw bytes to base64.
 *
 * @param src Pointer to source bytes.
 * @param len Number of bytes to encode.
 * @param out Destination buffer (must hold at least base64_encoded_size(len) chars).
 * @return Number of characters written to `out`.
 */
constexpr size_t base64_encode(const uint8_t* src, size_t len, char* out)
{
    using namespace detail;
    size_t pos = 0;
    size_t i = 0;
    for (; i + 2 < len; i += 3) {
        uint32_t v = 
            (uint32_t(src[i + 0]) << 16) | 
            (uint32_t(src[i + 1]) <<  8) | src[i + 2];
        out[pos++] = base64_encode_table[(v >> 18) & 0x3f];
        out[pos++] = base64_encode_table[(v >> 12) & 0x3f];
        out[pos++] = base64_encode_table[(v >>  6) & 0x3f];
        out[pos++] = base64_encode_table[ v        & 0x3f];
    }
    if (i < len) {
        uint32_t v = uint32_t(src[i]) << 16;
        if (i + 1 < len) {
            v |= uint32_t(src[i + 1]) << 8;
        }
        out[pos++] = base64_encode_table[(v >> 18) & 0x3f];
        out[pos++] = base64_encode_table[(v >> 12) & 0x3f];
        out[pos++] = (i + 1 < len) ? base64_encode_table[(v >> 6) & 0x3f] : '=';
        out[pos++] = '=';
    }
    return pos;
}

/**
 * @brief Decode a base64 string to raw bytes.

 * @param str Base64-encoded string (length must be a multiple of 4).
 * @param dst Destination buffer (must hold at least `base64_decoded_size(str)` bytes).
 * @return Number of bytes written to `dst`, or 0 on malformed input.
 */
constexpr size_t base64_decode(std::string_view str, uint8_t* dst)
{
    using namespace detail;

    if (str.size() % 4) {
        return 0;
    }
    size_t out = 0;

    for (size_t i = 0; i < str.size(); i += 4) {
        uint8_t a = base64_decode_table[uint8_t(str[i + 0])];
        uint8_t b = base64_decode_table[uint8_t(str[i + 1])];
        uint8_t c = base64_decode_table[uint8_t(str[i + 2])];
        uint8_t d = base64_decode_table[uint8_t(str[i + 3])];
        if (a == 255 || b == 255) { 
            return 0;
        }
        uint32_t v = (uint32_t(a) << 18) | (uint32_t(b) << 12);
        dst[out++] = v >> 16;
        if (str[i + 2] != '=') {
            if (c == 255) {
                return 0;
            }
            v |= uint32_t(c) << 6;
            dst[out++] = (v >> 8) & 0xff;
        }
        if (str[i + 3] != '=') {
            if (d == 255) {
                return 0;
            }
            v |= d;
            dst[out++] = v & 0xff;
        }
    }
    return out;
}

}

#endif
