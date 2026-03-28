#ifndef MOLD_TYPES_UUID_H
#define MOLD_TYPES_UUID_H

/**
 * @file
 * @brief 128-bit UUID type with hex-string (JSON) and byte-string (CBOR/MessagePack) serialization.
 */

#include <charconv>
#include <system_error>
#include "mold/refl/spec.h"

namespace mold {

/**
 * @brief 128-bit UUID stored as raw bytes.
 *
 */
struct uuid_t {
    std::array<uint8_t, 16> bytes; ///< Raw UUID bytes in network byte order.
};

/**
 * @brief Specialization for `uuid_t`.
 *
 * Serializes as a hex string "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
 * (lowercase) in JSON and as a 16-byte byte string in CBOR and MessagePack.
 */
template<>
struct spec_t<uuid_t> {

#if (MOLD_REFLECTION_JSON_ENABLED)
    static constexpr json_type_t json_type = json_type_t::string;
#endif
    static constexpr cbor_type_t cbor_type = cbor_type_t::bytes;

#if (MOLD_REFLECTION_JSON_ENABLED)

    static error_t json_read(uuid_t& out, const json_primitive_t& val)
    {
        std::string_view sv = val.string();
        if (sv.length() != 36 ||
            sv[8]  != '-' || sv[13] != '-' ||
            sv[18] != '-' || sv[23] != '-')
        {
            return error_t::handler_failure;
        }
        auto b = out.bytes.data();
        auto s = sv.data();
        auto parse = [&](const char* from, const char* to, uint8_t* dst) {
            while (from < to) {
                auto res = std::from_chars(from, from + 2, *dst++, 16);
                if (res.ec != std::errc()) {
                    return false;
                }
                from += 2;
            }
            return true;
        };
        if (!(parse(s,      s + 8,  b)      &&
              parse(s + 9,  s + 13, b + 4)  &&
              parse(s + 14, s + 18, b + 6)  &&
              parse(s + 19, s + 23, b + 8)  &&
              parse(s + 24, s + 36, b + 10)))
        {
            return error_t::handler_failure;
        }
        return error_t::ok;
    }

    static void json_emit(const uuid_t& in, const json_sink_t& sink)
    {
        char buf[37];
        int pos = 0;
        for (int i = 0; i < 16; ++i) {
            unsigned b = in.bytes[i];
            buf[pos++] = detail::hex_digits[(b >> 4) & 0xf];
            buf[pos++] = detail::hex_digits[b & 0xf];
            if (i == 3 || i == 5 || i == 7 || i == 9) {
                buf[pos++] = '-';
            }
        }
        sink.write_escaped_string({buf, 36});
    }

#endif

    static error_t cbor_read(uuid_t& out, const cbor_primitive_t& val)
    {
        auto bytes = val.bytes();
        if (bytes.size() != 16) {
            return error_t::handler_failure;
        }
        std::copy(bytes.begin(), bytes.end(), out.bytes.begin());
        return error_t::ok;
    }

    static void cbor_emit(const uuid_t& in, const cbor_sink_t& sink)
    {
        sink.write_data({in.bytes.data(), 16});
    }

#if (MOLD_REFLECTION_MSGPACK_ENABLED)

    static error_t msgpack_read(uuid_t& out, const msgpack_primitive_t& val)
    {
        auto bytes = val.bytes();
        if (bytes.size() != 16) {
            return error_t::handler_failure;
        }
        std::copy(bytes.begin(), bytes.end(), out.bytes.begin());
        return error_t::ok;
    }

    static void msgpack_emit(const uuid_t& in, const msgpack_sink_t& sink)
    {
        sink.write_data({in.bytes.data(), 16});
    }

#endif
};

}

#endif
