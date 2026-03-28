#ifndef MOLD_TYPES_FIELD_H
#define MOLD_TYPES_FIELD_H

/**
 * @file
 * @brief Transparent wrapper associating a compile-time CBOR map key with a struct member.
 */

#include <type_traits>
#include <bit>
#include "mold/refl/spec.h"
#include "mold/cbor/cbor_sink.h"

namespace mold {

/**
 * @brief Transparent wrapper carrying a compile-time CBOR map key.
 *
 * When CBOR serialization encounters a `field_t` member, it uses `Key` as
 * the map key instead of the member name string. JSON always ignores the
 * key and uses the reflected member name.
 *
 * @tparam Key Compile-time key value (integer, float, or structural type via `auto`)
 * @tparam T Wrapped value type
 */
template<auto Key, class T>
struct field_t {

    using value_type = T;

    static constexpr auto key = Key;

    T value{};

    constexpr field_t() = default;
    constexpr field_t(const T& v) : value(v) {}
    constexpr field_t(T&& v) : value(std::move(v)) {}

    constexpr operator T&()                 { return value; }
    constexpr operator const T&() const     { return value; }
    constexpr T& operator*()                { return value; }
    constexpr const T& operator*() const    { return value; }
};

namespace detail {

/**
 * @brief Detect `std::array` specializations for recursive key encoding.
 *
 */
template<class T>
struct is_std_array_impl : std::false_type {};

template<class V, size_t N>
struct is_std_array_impl<std::array<V, N>> : std::true_type {};

template<class T>
concept is_std_array = is_std_array_impl<std::remove_cvref_t<T>>::value;

/**
 * @brief Detect `std::array<char, N>` for CBOR text string encoding.
 *
 */
template<class T>
struct is_char_array_impl : std::false_type {};

template<size_t N>
struct is_char_array_impl<std::array<char, N>> : std::true_type {};

template<class T>
concept is_char_array = is_char_array_impl<std::remove_cvref_t<T>>::value;

/**
 * @brief Encode a compile-time value as CBOR into any byte-writer.
 *
 * Supports integers, floating-point, `std::array<char, N>` (encoded as CBOR
 * text string), generic `std::array` (encoded as CBOR array with each element
 * recursively encoded), and enums.  Delegates to the generic `cbor_write_*`
 * helpers from `cbor_sink.h`.
 *
 * @tparam Val Compile-time value to encode
 * @tparam W Byte-writer type
 * @param w Target writer
 */
template<auto Val, class W>
constexpr void cbor_encode_value(W& w)
{
    using K = std::remove_cvref_t<decltype(Val)>;
    if constexpr (std::is_floating_point_v<K>) {
        if constexpr (sizeof(K) <= 4) {
            cbor_write_base(w,
                uint8_t(+cbor_mt_t::simple | +cbor_simple_t::float32),
                std::bit_cast<uint32_t>(float(Val)), 4);
        } else {
            cbor_write_base(w,
                uint8_t(+cbor_mt_t::simple | +cbor_simple_t::float64),
                std::bit_cast<uint64_t>(double(Val)), 8);
        }
    } else if constexpr (is_char_array<K>) {
        constexpr auto N = std::tuple_size_v<K>;
        constexpr auto text_len = (N > 0 && Val[N - 1] == '\0') ? N - 1 : N;
        cbor_write_head(w, +cbor_mt_t::text, text_len);
        for (size_t i = 0; i < text_len; ++i) {
            w.write_byte(static_cast<uint8_t>(Val[i]));
        }
    } else if constexpr (is_std_array<K>) {
        constexpr auto N = std::tuple_size_v<K>;
        cbor_write_head(w, +cbor_mt_t::arr, N);
        [&]<size_t... I>(std::index_sequence<I...>) {
            (cbor_encode_value<Val[I]>(w), ...);
        }(std::make_index_sequence<N>());
    } else if constexpr (is_enumeration<K>) {
        cbor_encode_value<+Val>(w);
    } else if constexpr (std::is_signed_v<K>) {
        cbor_write_sint(w, int64_t(Val));
    } else {
        cbor_write_uint(w, uint64_t(Val));
    }
}

/**
 * @brief Static storage for the CBOR-encoded key of a given compile-time value.
 *
 * Two-pass constexpr: first counts bytes, then writes into an exactly-sized array.
 *
 * @tparam Key Compile-time key value
 */
template<auto Key>
struct cbor_key_storage_t {
    static constexpr size_t len = [] {
        struct counter_t {
            size_t n = 0;
            constexpr error_t write_byte(uint8_t) { ++n; return error_t::ok; }
        } c;
        cbor_encode_value<Key>(c);
        return c.n;
    }();
    static constexpr auto encoded = [] {
        struct writer_t {
            std::array<uint8_t, len> data{};
            size_t pos = 0;
            constexpr error_t write_byte(uint8_t b) { data[pos++] = b; return error_t::ok; }
        } w;
        cbor_encode_value<Key>(w);
        return w.data;
    }();
};

/**
 * @brief Detect whether a type is a `field_t` specialization.
 *
 */
template<class T>
struct is_field_impl : std::false_type {};

template<auto Key, class T>
struct is_field_impl<field_t<Key, T>> : std::true_type {};

}

/**
 * @brief True if `T` is a `field_t<Key, U>` specialization.
 *
 */
template<class T>
concept is_field = detail::is_field_impl<std::remove_cvref_t<T>>::value;

/**
 * @brief Strip `field_t` wrapper, returning the inner type (or `T` unchanged).
 *
 */
template<class T>
struct decay_field {
    using type = T;
};

template<auto Key, class T>
struct decay_field<field_t<Key, T>> {
    using type = T;
};

template<class T>
using decay_field_t = typename decay_field<std::remove_cvref_t<T>>::type;

/**
 * @brief Extract pre-encoded CBOR key bytes for a type.
 *
 * Returns a span over the CBOR-encoded key for `field_t` types,
 * or an empty span for non-field types.
 *
 * @tparam T Type to inspect (may or may not be a `field_t`)
 * @return Span of CBOR-encoded key bytes, or empty
 */
template<class T>
constexpr std::span<const uint8_t> extract_cbor_key()
{
    if constexpr (is_field<T>) {
        constexpr auto& s = detail::cbor_key_storage_t<std::remove_cvref_t<T>::key>::encoded;
        return {s.data(), s.size()};
    } else {
        return {};
    }
}

/**
 * @brief Specialization for `field_t` - delegates everything to `spec_t<T>`.
 *
 */
template<auto Key, class T>
struct spec_t<field_t<Key, T>> {

    static constexpr json_type_t json_type = spec_t<T>::json_type;
    static constexpr cbor_type_t cbor_type = spec_t<T>::cbor_type;
    static constexpr bool write_null = spec_write_null_v<T>;

    static void read(field_t<Key, T>& out, const io_value_t& val)
        requires has_spec_read<T>
    {
        spec_t<T>::read(out.value, val);
    }

    static void emit(const field_t<Key, T>& in, const io_sink_t& sink)
        requires has_spec_emit<T>
    {
        spec_t<T>::emit(in.value, sink);
    }

    static void* prepare(field_t<Key, T>& out, size_t slot_idx)
        requires has_spec_prepare<T>
    {
        return spec_t<T>::prepare(out.value, slot_idx);
    }

    static void* next(const field_t<Key, T>& in, const void* prev)
        requires has_spec_next<T>
    {
        return spec_t<T>::next(in.value, prev);
    }

    static void* nullable(field_t<Key, T>& out, size_t slot_idx)
        requires has_spec_nullable<T>
    {
        return spec_t<T>::nullable(out.value, slot_idx);
    }
};

}

#endif
