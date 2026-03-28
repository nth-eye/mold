#ifndef MOLD_CBOR_SINK_H
#define MOLD_CBOR_SINK_H

/**
 * @file
 * @brief CBOR encoding primitives (generic) and output sink.
 *
 * The `cbor_write_*` free function templates work with any type that provides
 * `constexpr error_t write_byte(uint8_t)`, enabling compile-time CBOR encoding
 * with `cbor_key_buf_t` and runtime encoding with `cbor_sink_t`.
 */

#include <bit>
#include "mold/util/sink.h"
#include "mold/cbor/cbor_util.h"

namespace mold {

/**
 * @brief Encode CBOR start byte + big-endian additional-info bytes.
 *
 * @tparam W Type with `write_byte(uint8_t) -> error_t`
 * @param w  Byte writer
 * @param start First byte (major type | additional info)
 * @param val Value whose bytes follow the start byte
 * @param len Number of additional-info bytes (0, 1, 2, 4, or 8)
 * @return `error_t::ok` on success
 */
template<class W>
constexpr error_t cbor_write_base(W& w, uint8_t start, uint64_t val, size_t len)
{
    MOLD_TRY(w.write_byte(start));
    for (int i = 8 * int(len) - 8; i >= 0; i -= 8)
        MOLD_TRY(w.write_byte(uint8_t(val >> i)));
    return error_t::ok;
}

/**
 * @brief Encode a CBOR head (major type + argument).
 *
 * @tparam W Type with `write_byte(uint8_t) -> error_t`
 * @param w Byte writer
 * @param mt Major type byte (already shifted to high 3 bits)
 * @param val Argument value
 * @return `error_t::ok` on success
 */
template<class W>
constexpr error_t cbor_write_head(W& w, uint8_t mt, uint64_t val)
{
    uint8_t ai;
    if (val <= +cbor_ai_t::_0) {
        ai = uint8_t(val);
    } else if (val <= 0xff) {
        ai = +cbor_ai_t::_1;
    } else if (val <= 0xffff) {
        ai = +cbor_ai_t::_2;
    } else if (val <= 0xffffffff) {
        ai = +cbor_ai_t::_4;
    } else {
        ai = +cbor_ai_t::_8;
    }
    size_t ai_len = (ai <= +cbor_ai_t::_0) ? 0 : (1 << (ai - +cbor_ai_t::_1));
    return cbor_write_base(w, mt | ai, val, ai_len);
}

/**
 * @brief Encode a CBOR unsigned integer.
 *
 * @tparam W Type with `write_byte(uint8_t) -> error_t`
 */
template<class W>
constexpr error_t cbor_write_uint(W& w, uint64_t val)
{
    return cbor_write_head(w, +cbor_mt_t::uint, val);
}

/**
 * @brief Encode a CBOR signed integer (positive or negative).
 *
 * @tparam W Type with `write_byte(uint8_t) -> error_t`
 */
template<class W>
constexpr error_t cbor_write_sint(W& w, int64_t val)
{
    uint64_t ui = val >> 63;
    return cbor_write_head(w, uint8_t(+cbor_mt_t(ui & 0x20)), ui ^ uint64_t(val));
}

/**
 * @brief Helper for writing CBOR data to a sink callback.
 *
 * Extends sink_t with CBOR-specific functionality.  Methods that have
 * generic counterparts delegate to the `cbor_write_*` free functions.
 */
struct cbor_sink_t : sink_t {

    using sink_t::sink_t;

    /**
     * @brief Write CBOR head (major type + additional info + value).
     *
     * @param mt Major type
     * @param val Value to encode
     * @return `error_t::ok` on success
     */
    constexpr error_t write_head(uint8_t mt, uint64_t val) const
    {
        return cbor_write_head(*this, mt, val);
    }

    /**
     * @brief Write an unsigned integer.
     *
     * @param val Value to emit
     * @return `error_t::ok` on success
     */
    constexpr error_t write_uint(uint64_t val) const
    {
        return cbor_write_uint(*this, val);
    }

    /**
     * @brief Write a signed integer (positive or negative).
     *
     * @param val Value to emit
     * @return `error_t::ok` on success
     */
    constexpr error_t write_sint(int64_t val) const
    {
        return cbor_write_sint(*this, val);
    }

    /**
     * @brief Write a boolean value.
     *
     * @param val Boolean value
     * @return `error_t::ok` on success
     */
    constexpr error_t write_bool(bool val) const
    {
        return write_byte(+cbor_mt_t::simple | (+cbor_simple_t::bool_false + val));
    }

    /**
     * @brief Write a null value.
     *
     * @return `error_t::ok` on success
     */
    constexpr error_t write_null() const
    {
        return write_byte(+cbor_mt_t::simple | +cbor_simple_t::null);
    }

    /**
     * @brief Write a half-precision float (16-bit).
     *
     * @param val Value as uint16_t bits
     * @return `error_t::ok` on success
     */
    constexpr error_t write_float16(mold::float16_t val) const
    {
        return cbor_write_base(*this, +cbor_mt_t::simple | +cbor_simple_t::float16,
            val != val ? 0x7e00 : std::bit_cast<uint16_t>(val), 2);
    }

    /**
     * @brief Write a single-precision float (32-bit).
     *
     * @param val Float value
     * @return `error_t::ok` on success
     */
    constexpr error_t write_float32(mold::float32_t val) const
    {
        auto fp16 = mold::float16_t(val);
        if (val == mold::float32_t(fp16)) {
            return write_float16(fp16);
        }
        return cbor_write_base(*this, +cbor_mt_t::simple | +cbor_simple_t::float32,
            std::bit_cast<uint32_t>(val), 4);
    }

    /**
     * @brief Write a double-precision float (64-bit).
     *
     * @param val Double value
     * @return `error_t::ok` on success
     */
    constexpr error_t write_float64(mold::float64_t val) const
    {
        auto fp32 = mold::float32_t(val);
        if (val == fp32) {
            return write_float32(fp32);
        }
        return cbor_write_base(*this, +cbor_mt_t::simple | +cbor_simple_t::float64,
            std::bit_cast<uint64_t>(val), 8);
    }

    /**
     * @brief Write a floating point value with automatic precision selection.
     *
     * @param val Double value
     * @return `error_t::ok` on success
     */
    constexpr error_t write_floating(double val) const
    {
        return write_float64(val);
    }

    /**
     * @brief Write a text string.
     *
     * @param sv String view to emit
     * @return `error_t::ok` on success
     */
    constexpr error_t write_text(std::string_view sv) const
    {
        MOLD_TRY(write_head(+cbor_mt_t::text, sv.size()));
        return write_str(sv);
    }

    /**
     * @brief Write a byte string.
     *
     * @param data Byte data to emit
     * @return `error_t::ok` on success
     */
    constexpr error_t write_data(std::span<const uint8_t> data) const
    {
        MOLD_TRY(write_head(+cbor_mt_t::data, data.size()));
        return write_bytes(data);
    }

    /**
     * @brief Write array header with given size.
     *
     * @param size Number of elements
     * @return `error_t::ok` on success
     */
    constexpr error_t write_array(size_t size) const
    {
        return write_head(+cbor_mt_t::arr, size);
    }

    /**
     * @brief Write map header with given size.
     *
     * @param size Number of key-value pairs
     * @return `error_t::ok` on success
     */
    constexpr error_t write_map(size_t size) const
    {
        return write_head(+cbor_mt_t::map, size);
    }


    /**
     * @brief Open an indefinite-length array (0x9F).
     *
     * Elements are written individually; close with `write_break()`.
     * @return `error_t::ok` on success
     */
    constexpr error_t write_indef_array() const
    {
        return write_byte(+cbor_mt_t::arr | +cbor_ai_t::_indef);
    }

    /**
     * @brief Open an indefinite-length map (0xBF).
     *
     * Key-value pairs are written individually; close with `write_break()`.
     * @return `error_t::ok` on success
     */
    constexpr error_t write_indef_map() const
    {
        return write_byte(+cbor_mt_t::map | +cbor_ai_t::_indef);
    }

    /**
     * @brief Write the CBOR break code (0xFF) to close an indefinite container.
     *
     * @return `error_t::ok` on success
     */
    constexpr error_t write_break() const
    {
        return write_byte(+cbor_mt_t::simple | +cbor_ai_t::_indef);
    }
};

}

#endif
