#ifndef MOLD_TYPES_BYTES_H
#define MOLD_TYPES_BYTES_H

/**
 * @file
 * @brief Fixed and view byte buffer types with base64 (JSON) / binary (CBOR/MessagePack) serialization.
 */

#include <algorithm>
#include "mold/refl/spec.h"
#include "mold/util/base64.h"

namespace mold {

/**
 * @brief Non-owning view over a byte buffer.
 *
 * Serializes as a base64 string in JSON and as a byte string in CBOR and MessagePack.
 *
 * When constructed from const data (or CBOR/MessagePack zero-copy read), `cap` is 0
 * and the view is read-only. When constructed from a mutable span, `cap`
 * reflects the buffer size and the view supports JSON/CBOR deserialization.
 */
struct bytes_view_t {

    constexpr bytes_view_t() = default;
    constexpr bytes_view_t(const uint8_t* p, size_t n)
        : ptr(const_cast<uint8_t*>(p)), len(n) {}
    constexpr bytes_view_t(std::span<const uint8_t> s)
        : ptr(const_cast<uint8_t*>(s.data())), len(s.size()) {}
    constexpr bytes_view_t(std::span<uint8_t> s)
        : ptr(s.data()), len(0), cap(s.size()) {}

    constexpr std::span<const uint8_t> span() const
    {
        return {ptr, len};
    }

    constexpr size_t size() const
    {
        return len;
    }

    constexpr size_t capacity() const
    {
        return cap;
    }

    constexpr bool empty() const
    {
        return len == 0;
    }

    uint8_t* ptr = nullptr;
    size_t len = 0;
    size_t cap = 0;
};

/**
 * @brief Specialization for `bytes_view_t` (base64 in JSON, raw bytes in CBOR).
 *
 */
template<>
struct spec_t<bytes_view_t> {

#if (MOLD_REFLECTION_JSON_ENABLED)
    static constexpr json_type_t json_type = json_type_t::string;
#endif
    static constexpr cbor_type_t cbor_type = cbor_type_t::bytes;

#if (MOLD_REFLECTION_JSON_ENABLED)

    static error_t json_read(bytes_view_t& out, const json_primitive_t& val)
    {
        if (!out.cap) {
            return error_t::handler_failure;
        }
        std::string_view sv = val.string();
        size_t decoded_size = base64_decoded_size(sv);

        if (decoded_size > out.cap) {
            return error_t::handler_failure;
        }
        size_t n = base64_decode(sv, out.ptr);

        if (n == 0 && !sv.empty()) {
            return error_t::handler_failure;
        }
        out.len = n;
        return error_t::ok;
    }

    static void json_emit(const bytes_view_t& in, const json_sink_t& sink)
    {
        constexpr size_t chunk = 48;
        sink.write_byte('"');
        char buf[64];
        for (size_t i = 0; i < in.len; i += chunk) {
            size_t n = std::min(chunk, in.len - i);
            size_t written = base64_encode(in.ptr + i, n, buf);
            sink.write_str({buf, written});
        }
        sink.write_byte('"');
    }

#endif

    static error_t cbor_read(bytes_view_t& out, const cbor_primitive_t& val)
    {
        auto bytes = val.bytes();
        if (out.cap) {
            if (bytes.size() > out.cap) {
                return error_t::handler_failure;
            }
            std::copy_n(bytes.data(), bytes.size(), out.ptr);
        } else {
            out.ptr = const_cast<uint8_t*>(bytes.data());
        }
        out.len = bytes.size();
        return error_t::ok;
    }

    static void cbor_emit(const bytes_view_t& in, const cbor_sink_t& sink)
    {
        sink.write_data({in.ptr, in.len});
    }

#if (MOLD_REFLECTION_MSGPACK_ENABLED)

    static error_t msgpack_read(bytes_view_t& out, const msgpack_primitive_t& val)
    {
        auto bytes = val.bytes();
        if (out.cap) {
            if (bytes.size() > out.cap) {
                return error_t::handler_failure;
            }
            std::copy_n(bytes.data(), bytes.size(), out.ptr);
        } else {
            out.ptr = const_cast<uint8_t*>(bytes.data());
        }
        out.len = bytes.size();
        return error_t::ok;
    }

    static void msgpack_emit(const bytes_view_t& in, const msgpack_sink_t& sink)
    {
        sink.write_data({in.ptr, in.len});
    }

#endif
};

/**
 * @brief In-place byte buffer of at most N bytes.
 *
 * Serializes as a base64 string in JSON and as a byte string in CBOR and MessagePack.
 *
 * @tparam N Maximum capacity in bytes
 */
template<size_t N>
struct bytes_t {

    constexpr bytes_t() = default;

    constexpr bytes_t(std::span<const uint8_t> s)
        : len(uint32_t(std::min(s.size(), N)))
    {
        std::copy_n(s.data(), len, data);
    }

    constexpr std::span<const uint8_t> span() const
    {
        return {data, len};
    }

    constexpr size_t size() const
    {
        return len;
    }

    constexpr size_t capacity() const
    {
        return N;
    }

    constexpr bool empty() const
    {
        return len == 0;
    }

    constexpr bool operator==(const bytes_t& o) const
    {
        if (len != o.len) {
            return false;
        }
        return std::equal(data, data + len, o.data);
    }

    uint8_t data[N] = {};
    uint32_t len = 0;
};

/**
 * @brief Specialization for `bytes_t` (base64 in JSON, raw bytes in CBOR).
 *
 * @tparam N Maximum capacity
 */
template<size_t N>
struct spec_t<bytes_t<N>> {

#if (MOLD_REFLECTION_JSON_ENABLED)
    static constexpr json_type_t json_type = json_type_t::string;
#endif
    static constexpr cbor_type_t cbor_type = cbor_type_t::bytes;

#if (MOLD_REFLECTION_JSON_ENABLED)

    static error_t json_read(bytes_t<N>& out, const json_primitive_t& val)
    {
        std::string_view sv = val.string();
        size_t decoded_size = base64_decoded_size(sv);
        if (decoded_size > N) {
            return error_t::handler_failure;
        }
        size_t n = base64_decode(sv, out.data);
        if (n == 0 && !sv.empty()) {
            return error_t::handler_failure;
        }
        out.len = uint32_t(n);
        return error_t::ok;
    }

    static void json_emit(const bytes_t<N>& in, const json_sink_t& sink)
    {
        char buf[base64_encoded_size(N)];
        size_t n = base64_encode(in.data, in.len, buf);
        sink.write_escaped_string({buf, n});
    }

#endif

    static error_t cbor_read(bytes_t<N>& out, const cbor_primitive_t& val)
    {
        auto bytes = val.bytes();
        if (bytes.size() > N) {
            return error_t::handler_failure;
        }
        out.len = uint32_t(bytes.size());
        std::copy_n(bytes.data(), bytes.size(), out.data);
        return error_t::ok;
    }

    static void cbor_emit(const bytes_t<N>& in, const cbor_sink_t& sink)
    {
        sink.write_data({in.data, in.len});
    }

#if (MOLD_REFLECTION_MSGPACK_ENABLED)

    static error_t msgpack_read(bytes_t<N>& out, const msgpack_primitive_t& val)
    {
        auto bytes = val.bytes();
        if (bytes.size() > N) {
            return error_t::handler_failure;
        }
        out.len = uint32_t(bytes.size());
        std::copy_n(bytes.data(), bytes.size(), out.data);
        return error_t::ok;
    }

    static void msgpack_emit(const bytes_t<N>& in, const msgpack_sink_t& sink)
    {
        sink.write_data({in.data, in.len});
    }

#endif
};

}

#endif
