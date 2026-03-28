#ifndef MOLD_MSGPACK_SINK_H
#define MOLD_MSGPACK_SINK_H

/**
 * @file
 * @brief MessagePack encoding primitives and output sink.
 *
 * The `msgpack_write_*` free function templates work with any type that provides
 * `constexpr error_t write_byte(uint8_t)`, mirroring the pattern from `cbor_sink.h`.
 */

#include <bit>
#include "mold/util/sink.h"
#include "mold/msgpack/msgpack_util.h"

namespace mold {

/**
 * @brief Write big-endian bytes of a value.
 *
 * @tparam W Type with `write_byte(uint8_t) -> error_t`
 * @param w Byte writer
 * @param val Value whose bytes to emit
 * @param len Number of bytes to write (1, 2, 4, or 8)
 * @return `error_t::ok` on success
 */
template<class W>
constexpr error_t msgpack_write_be(W& w, uint64_t val, size_t len)
{
    for (int i = 8 * int(len) - 8; i >= 0; i -= 8) {
        MOLD_TRY(w.write_byte(uint8_t(val >> i)));
    }
    return error_t::ok;
}

/**
 * @brief Encode a MessagePack unsigned integer.
 *
 * Uses the most compact encoding: positive fixint, uint8, uint16, uint32, or uint64.
 *
 * @tparam W Type with `write_byte(uint8_t) -> error_t`
 * @param w Byte writer
 * @param val Value to encode
 * @return `error_t::ok` on success
 */
template<class W>
constexpr error_t msgpack_write_uint(W& w, uint64_t val)
{
    if (val <= 0x7f) {
        return w.write_byte(uint8_t(val));
    }
    if (val <= 0xff) {
        MOLD_TRY(w.write_byte(0xcc));
        return w.write_byte(uint8_t(val));
    }
    if (val <= 0xffff) {
        MOLD_TRY(w.write_byte(0xcd));
        return msgpack_write_be(w, val, 2);
    }
    if (val <= 0xffffffff) {
        MOLD_TRY(w.write_byte(0xce));
        return msgpack_write_be(w, val, 4);
    }
    MOLD_TRY(w.write_byte(0xcf));
    return msgpack_write_be(w, val, 8);
}

/**
 * @brief Encode a MessagePack signed integer (positive or negative).
 *
 * Positive values delegate to `msgpack_write_uint`. Negative values use the
 * most compact encoding: negative fixint, int8, int16, int32, or int64.
 *
 * @tparam W Type with `write_byte(uint8_t) -> error_t`
 * @param w Byte writer
 * @param val Value to encode
 * @return `error_t::ok` on success
 */
template<class W>
constexpr error_t msgpack_write_sint(W& w, int64_t val)
{
    if (val >= 0) {
        return msgpack_write_uint(w, uint64_t(val));
    }
    // Negative fixint: -32..-1 -> 0xe0..0xff
    if (val >= -32) {
        return w.write_byte(uint8_t(int8_t(val)));
    }
    if (val >= -128) {
        MOLD_TRY(w.write_byte(0xd0));
        return w.write_byte(uint8_t(int8_t(val)));
    }
    if (val >= -32768) {
        MOLD_TRY(w.write_byte(0xd1));
        return msgpack_write_be(w, uint64_t(uint16_t(int16_t(val))), 2);
    }
    if (val >= int64_t(-2147483648LL)) {
        MOLD_TRY(w.write_byte(0xd2));
        return msgpack_write_be(w, uint64_t(uint32_t(int32_t(val))), 4);
    }
    MOLD_TRY(w.write_byte(0xd3));
    return msgpack_write_be(w, uint64_t(val), 8);
}

/**
 * @brief Helper for writing MessagePack data to a sink callback.
 *
 * Extends sink_t with MessagePack-specific encoding methods.
 */
struct msgpack_sink_t : sink_t {

    using sink_t::sink_t;

    /**
     * @brief Write an unsigned integer.
     *
     * @param val Value to emit
     * @return `error_t::ok` on success
     */
    constexpr error_t write_uint(uint64_t val) const
    {
        return msgpack_write_uint(*this, val);
    }

    /**
     * @brief Write a signed integer (positive or negative).
     *
     * @param val Value to emit
     * @return `error_t::ok` on success
     */
    constexpr error_t write_sint(int64_t val) const
    {
        return msgpack_write_sint(*this, val);
    }

    /**
     * @brief Write a boolean value.
     *
     * @param val Boolean value
     * @return `error_t::ok` on success
     */
    constexpr error_t write_bool(bool val) const
    {
        return write_byte(val ? 0xc3 : 0xc2);
    }

    /**
     * @brief Write a null value.
     *
     * @return `error_t::ok` on success
     */
    constexpr error_t write_null() const
    {
        return write_byte(0xc0);
    }

    /**
     * @brief Write a single-precision float (32-bit).
     *
     * @param val Float value
     * @return `error_t::ok` on success
     */
    constexpr error_t write_float32(float val) const
    {
        MOLD_TRY(write_byte(0xca));
        return msgpack_write_be(*this, std::bit_cast<uint32_t>(val), 4);
    }

    /**
     * @brief Write a double-precision float (64-bit).
     *
     * @param val Double value
     * @return `error_t::ok` on success
     */
    constexpr error_t write_float64(double val) const
    {
        auto fp32 = float(val);
        if (double(fp32) == val) {
            return write_float32(fp32);
        }
        MOLD_TRY(write_byte(0xcb));
        return msgpack_write_be(*this, std::bit_cast<uint64_t>(val), 8);
    }

    /**
     * @brief Write a floating-point value with automatic precision selection.
     *
     * @param val Double value
     * @return `error_t::ok` on success
     */
    constexpr error_t write_floating(double val) const
    {
        return write_float64(val);
    }

    /**
     * @brief Write a text string with length prefix.
     *
     * Uses the most compact encoding: fixstr, str8, str16, or str32.
     *
     * @param sv String view to emit
     * @return `error_t::ok` on success
     */
    constexpr error_t write_text(std::string_view sv) const
    {
        size_t len = sv.size();
        if (len <= 31) {
            MOLD_TRY(write_byte(0xa0 | uint8_t(len)));
        } else if (len <= 0xff) {
            MOLD_TRY(write_byte(0xd9));
            MOLD_TRY(write_byte(uint8_t(len)));
        } else if (len <= 0xffff) {
            MOLD_TRY(write_byte(0xda));
            MOLD_TRY(msgpack_write_be(*this, len, 2));
        } else {
            MOLD_TRY(write_byte(0xdb));
            MOLD_TRY(msgpack_write_be(*this, len, 4));
        }
        return write_str(sv);
    }

    /**
     * @brief Write a byte string (bin format).
     *
     * Uses the most compact encoding: bin8, bin16, or bin32.
     *
     * @param data Byte data to emit
     * @return `error_t::ok` on success
     */
    constexpr error_t write_data(std::span<const uint8_t> data) const
    {
        size_t len = data.size();
        if (len <= 0xff) {
            MOLD_TRY(write_byte(0xc4));
            MOLD_TRY(write_byte(uint8_t(len)));
        } else if (len <= 0xffff) {
            MOLD_TRY(write_byte(0xc5));
            MOLD_TRY(msgpack_write_be(*this, len, 2));
        } else {
            MOLD_TRY(write_byte(0xc6));
            MOLD_TRY(msgpack_write_be(*this, len, 4));
        }
        return write_bytes(data);
    }

    /**
     * @brief Write array header with given size.
     *
     * Uses the most compact encoding: fixarray, array16, or array32.
     *
     * @param size Number of elements
     * @return `error_t::ok` on success
     */
    constexpr error_t write_array(size_t size) const
    {
        if (size <= 15) {
            return write_byte(0x90 | uint8_t(size));
        }
        if (size <= 0xffff) {
            MOLD_TRY(write_byte(0xdc));
            return msgpack_write_be(*this, size, 2);
        }
        MOLD_TRY(write_byte(0xdd));
        return msgpack_write_be(*this, size, 4);
    }

    /**
     * @brief Write map header with given size.
     *
     * Uses the most compact encoding: fixmap, map16, or map32.
     *
     * @param size Number of key-value pairs
     * @return `error_t::ok` on success
     */
    constexpr error_t write_map(size_t size) const
    {
        if (size <= 15) {
            return write_byte(0x80 | uint8_t(size));
        }
        if (size <= 0xffff) {
            MOLD_TRY(write_byte(0xde));
            return msgpack_write_be(*this, size, 2);
        }
        MOLD_TRY(write_byte(0xdf));
        return msgpack_write_be(*this, size, 4);
    }
};

}

#endif
