#ifndef MOLD_TYPES_STD_H
#define MOLD_TYPES_STD_H

/**
 * @file
 * @brief Specializations for standard library and built-in types.
 */

#include <array>
#include <optional>
#include <string>
#include <vector>
#include "mold/refl/spec.h"

namespace mold {

/**
 * @brief Specialization for `bool`.
 *
 */
template<>
struct spec_t<bool> {

    static constexpr json_type_t json_type = json_type_t::boolean;
    static constexpr cbor_type_t cbor_type = cbor_type_t::boolean;

    static void read(bool& out, const io_value_t& val)
    {
        out = val.boolean();
    }

    static void emit(const bool& in, const io_sink_t& sink)
    {
        sink.write_bool(in);
    }
};

/**
 * @brief Specialization for `std::string_view` (non-owning string).
 *
 */
template<>
struct spec_t<std::string_view> {

    static constexpr json_type_t json_type = json_type_t::string;
    static constexpr cbor_type_t cbor_type = cbor_type_t::string;

    static void read(std::string_view& out, const io_value_t& val)
    {
        out = val.string();
    }

    static void emit(const std::string_view& in, const io_sink_t& sink)
    {
        sink.write_string(in);
    }
};

/**
 * @brief Specialization for `std::string` (owning string).
 *
 */
template<>
struct spec_t<std::string> {

    static constexpr json_type_t json_type = json_type_t::string;
    static constexpr cbor_type_t cbor_type = cbor_type_t::string;

    static void read(std::string& out, const io_value_t& val)
    {
        out = std::string(val.string());
    }

    static void emit(const std::string& in, const io_sink_t& sink)
    {
        sink.write_string(in);
    }
};

/**
 * @brief Specialization for all integer types (range-checked via `numeric_limits`).
 *
 * @tparam T Integer type satisfying is_integer
 */
template<is_integer T>
struct spec_t<T> {

    static constexpr json_type_t json_type = json_type_t::integer;
    static constexpr cbor_type_t cbor_type = cbor_type_t::integer;

    static error_t read(T& out, const io_value_t& val)
    {
        if constexpr (std::is_unsigned_v<T>) {
            uint64_t v = val.uinteger();
            if (v > std::numeric_limits<T>::max()) {
                MOLD_DEBUG_LOG("FAIL: Unsigned value out of range.");
                return error_t::handler_failure;
            }
            out = T(v);
        } else {
            int64_t v = val.integer();
            if (v < std::numeric_limits<T>::min() || v > std::numeric_limits<T>::max()) {
                MOLD_DEBUG_LOG("FAIL: Signed value out of range.");
                return error_t::handler_failure;
            }
            out = T(v);
        }
        return error_t::ok;
    }

    static void emit(const T& in, const io_sink_t& sink)
    {
        if constexpr (std::is_signed_v<T>) {
            sink.write_sint(int64_t(in));
        } else {
            sink.write_uint(uint64_t(in));
        }
    }
};

/**
 * @brief Specialization for enumeration types, serialized as their underlying integer.
 *
 * @tparam T Enumeration type satisfying `is_enumeration`
 */
template<is_enumeration T>
struct spec_t<T> {

    using underlying = std::underlying_type_t<T>;

    static constexpr json_type_t json_type = spec_t<underlying>::json_type;
    static constexpr cbor_type_t cbor_type = spec_t<underlying>::cbor_type;

    static error_t read(T& out, const io_value_t& val)
    {
        underlying v{};
        auto err = spec_t<underlying>::read(v, val);
        out = static_cast<T>(v);
        return err;
    }

    static void emit(const T& in, const io_sink_t& sink)
    {
        spec_t<underlying>::emit(static_cast<underlying>(in), sink);
    }
};

/**
 * @brief Specialization for all floating-point types.
 *
 * @tparam T Floating-point type satisfying `is_floating`
 */
template<is_floating T>
struct spec_t<T> {

    static constexpr json_type_t json_type = json_type_t::floating;
    static constexpr cbor_type_t cbor_type = cbor_type_t::floating;

    static void read(T& out, const io_value_t& val)
    {
        out = T(val.number());
    }

    static void emit(const T& in, const io_sink_t& sink)
    {
        sink.write_float(double(in));
    }
};

#if !defined(__STDCPP_FLOAT16_T__)

/**
 * @brief Specialization for `float16_t` when it does not satisfy `std::floating_point`.
 *
 * In C++20, `_Float16` and the software `half_t` fallback are not recognized by
 * `std::floating_point`, so the generic `is_floating` partial specialization above
 * does not match. This explicit specialization covers those cases.
 * In C++23+, `std::float16_t` / `_Float16` satisfy `std::floating_point` and the
 * generic one applies; the explicit specialization still wins but behaves identically.
 */
template<>
struct spec_t<float16_t> {

    static constexpr json_type_t json_type = json_type_t::floating;
    static constexpr cbor_type_t cbor_type = cbor_type_t::floating;

    static void read(float16_t& out, const io_value_t& val)
    {
        out = float16_t(float(val.number()));
    }

    static void emit(const float16_t& in, const io_sink_t& sink)
    {
        sink.write_float(double(float(in)));
    }
};

#endif

/**
 * @brief Specialization for tuple-like types (decoded as heterogeneous array).
 *
 * @tparam T Tuple type satisfying `is_tuple`
 */
template<is_tuple T>
struct spec_t<T> {
    static constexpr json_type_t json_type = json_type_t::array;
    static constexpr cbor_type_t cbor_type = cbor_type_t::array;
};

/**
 * @brief Specialization for aggregate types (decoded as object/map).
 *
 * @tparam T Aggregate type satisfying `is_aggregate`
 */
template<is_aggregate T>
struct spec_t<T> {
    static constexpr json_type_t json_type = json_type_t::object;
    static constexpr cbor_type_t cbor_type = cbor_type_t::object;
};

/**
 * @brief Specialization for `std::vector` (dynamic homogeneous container).
 *
 * @tparam T Element type
 */
template<class T>
struct spec_t<std::vector<T>> {

    static constexpr json_type_t json_type = json_type_t::array;
    static constexpr cbor_type_t cbor_type = cbor_type_t::array;

    static void* prepare(std::vector<T>& c, size_t /*slot_idx*/)
    {
        return &c.emplace_back();
    }

    static void* next(const std::vector<T>& c, const void* prev)
    {
        const T* begin = c.data();
        const T* end   = begin + c.size();
        const T* next  = prev ? static_cast<const T*>(prev) + 1 : begin;
        return const_cast<T*>((next >= begin && next < end) ? next : nullptr);
    }
};

/**
 * @brief Specialization for `std::array` (fixed-size homogeneous container).
 *
 * @tparam T Element type
 * @tparam N Element count
 */
template<class T, size_t N>
struct spec_t<std::array<T, N>> {

    static constexpr json_type_t json_type = json_type_t::array;
    static constexpr cbor_type_t cbor_type = cbor_type_t::array;

    static void* prepare(std::array<T, N>& c, size_t slot_idx)
    {
        return slot_idx < N ? &c[slot_idx] : nullptr;
    }

    static void* next(const std::array<T, N>& c, const void* prev)
    {
        const T* begin = c.data();
        const T* end   = begin + N;
        const T* next  = prev ? static_cast<const T*>(prev) + 1 : begin;
        return const_cast<T*>((next >= begin && next < end) ? next : nullptr);
    }
};

/**
 * @brief Specialization for `std::optional` (nullable wrapper).
 *
 * Conditionally provides read/emit/prepare/next depending on the inner
 * type's spec_t capabilities. Always provides nullable.
 *
 * @tparam T Inner type
 */
template<class T>
struct spec_t<std::optional<T>> {

    static constexpr json_type_t json_type = spec_t<T>::json_type | json_type_t::null;
    static constexpr cbor_type_t cbor_type = spec_t<T>::cbor_type | cbor_type_t::null;

    static error_t read(std::optional<T>& out, const io_value_t& val)
        requires has_spec_read<T>
    {
        if constexpr (!is_homogenous_container<T>) {
            if (val.null()) {
                out.reset();
                return error_t::ok;
            }
        }
        T& contained = out.has_value() ? out.value() : out.emplace();
        return error_t(reinterpret_cast<uintptr_t>(spec_invoke_read(contained, val)));
    }

    static void emit(const std::optional<T>& in, const io_sink_t& sink)
        requires has_spec_emit<T>
    {
        if (in.has_value()) {
            spec_t<T>::emit(*in, sink);
        }
    }

    static void* prepare(std::optional<T>& out, size_t slot_idx)
        requires has_spec_prepare<T>
    {
        T& contained = out.has_value() ? out.value() : out.emplace();
        return spec_t<T>::prepare(contained, slot_idx);
    }

    static void* next(const std::optional<T>& in, const void* prev)
        requires has_spec_next<T>
    {
        return in.has_value() ? spec_t<T>::next(*in, prev) : nullptr;
    }

    static void* nullable(std::optional<T>& out, size_t slot_idx)
    {
        if (slot_idx) {
            out.emplace();
        }
        return out.has_value() ? &out.value() : nullptr;
    }
};

}

#endif
