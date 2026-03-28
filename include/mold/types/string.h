#ifndef MOLD_TYPES_STRING_H
#define MOLD_TYPES_STRING_H

/**
 * @file
 * @brief Fixed-capacity string type with JSON/CBOR serialization.
 */

#include <algorithm>
#include "mold/refl/spec.h"

namespace mold {

/**
 * @brief In-place string of at most N bytes.
 *
 * Stores characters in a `char[N]` buffer with an explicit length.
 * Serializes as a string in both JSON and CBOR.
 *
 * @tparam N Maximum capacity in bytes (excluding null terminator)
 */
template<size_t N>
struct string_t {

    constexpr string_t() = default;
    constexpr string_t(const char* s) : string_t(std::string_view{s}) {}
    constexpr string_t(std::string_view sv) : len(uint32_t(std::min(sv.size(), N)))
    {
        std::copy_n(sv.data(), len, data);
    }

    constexpr operator std::string_view() const 
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

    constexpr bool operator==(const string_t& o) const
    {
        return std::string_view(*this) == std::string_view(o);
    }

    constexpr bool operator==(std::string_view sv) const
    {
        return std::string_view(*this) == sv;
    }

    char data[N] = {};
    uint32_t len = 0;
};

/**
 * @brief Specialization for `string_t` (serialized as string).
 *
 * @tparam N Maximum capacity
 */
template<size_t N>
struct spec_t<string_t<N>> {

    static constexpr json_type_t json_type = json_type_t::string;
    static constexpr cbor_type_t cbor_type = cbor_type_t::string;

    static error_t read(string_t<N>& out, const io_value_t& val)
    {
        std::string_view sv = val.string();
        if (sv.size() > N) {
            return error_t::handler_failure;
        }
        out.len = uint32_t(sv.size());
        std::copy_n(sv.data(), sv.size(), out.data);
        return error_t::ok;
    }

    static void emit(const string_t<N>& in, const io_sink_t& sink)
    {
        sink.write_string(std::string_view(in));
    }
};

}

#endif
