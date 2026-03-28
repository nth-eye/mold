#ifndef MOLD_TYPES_NULLABLE_H
#define MOLD_TYPES_NULLABLE_H

/**
 * @file
 * @brief Explicit-null optional wrapper for serialization.
 *
 * Unlike `std::optional<T>` (which skips the field when absent),
 * `nullable_t<T>` always writes an explicit null value on the wire.
 */

#include <optional>
#include "mold/refl/spec.h"

namespace mold {

/**
 * @brief Optional wrapper that serializes as explicit null when absent.
 *
 * Behaves like `std::optional<T>` in C++ but differs in serialization:
 * absent `std::optional<T>` fields are skipped entirely, while absent
 * `nullable_t<T>` fields produce an explicit null (JSON `null`, CBOR null,
 * MessagePack nil).
 *
 * @tparam T Inner value type
 */
template<class T>
struct nullable_t {

    std::optional<T> inner{};

    constexpr nullable_t() = default;
    constexpr nullable_t(std::nullopt_t) noexcept {}
    constexpr nullable_t(const T& v) : inner(v) {}
    constexpr nullable_t(T&& v) : inner(std::move(v)) {}


    constexpr auto operator<=>(const nullable_t&) const = default;
    constexpr explicit operator bool() const noexcept   { return has_value(); }
    constexpr T& value()                                { return inner.value(); }
    constexpr const T& value() const                    { return inner.value(); }
    constexpr T& operator*()                            { return *inner; }
    constexpr const T& operator*() const                { return *inner; }
    constexpr T* operator->()                           { return inner.operator->(); }
    constexpr const T* operator->() const               { return inner.operator->(); }
    constexpr bool has_value() const noexcept           { return inner.has_value(); }
    constexpr void reset() noexcept                     { inner.reset(); }

    template<class... Args>
    constexpr T& emplace(Args&&... args)
    { 
        return inner.emplace(std::forward<Args>(args)...); 
    }

    constexpr nullable_t& operator=(std::nullopt_t) noexcept 
    { 
        inner.reset(); 
        return *this; 
    }

    constexpr nullable_t& operator=(const T& v)
    { 
        inner = v; 
        return *this; 
    }

    constexpr nullable_t& operator=(T&& v)
    { 
        inner = std::move(v); 
        return *this; 
    }
};

/**
 * @brief Specialization for `nullable_t` (explicit-null optional wrapper).
 *
 * Delegates all callbacks to `spec_t<std::optional<T>>` and sets
 * `write_null = true` so serializers emit null instead of skipping.
 *
 * @tparam T Inner type
 */
template<class T>
struct spec_t<nullable_t<T>> {

    static constexpr json_type_t json_type = spec_t<std::optional<T>>::json_type;
    static constexpr cbor_type_t cbor_type = spec_t<std::optional<T>>::cbor_type;
    static constexpr bool write_null = true;

    static error_t read(nullable_t<T>& out, const io_value_t& val)
        requires has_spec_read<std::optional<T>>
    {
        return spec_t<std::optional<T>>::read(out.inner, val);
    }

    static void emit(const nullable_t<T>& in, const io_sink_t& sink)
        requires has_spec_emit<std::optional<T>>
    {
        spec_t<std::optional<T>>::emit(in.inner, sink);
    }

    static void* prepare(nullable_t<T>& out, size_t slot_idx)
        requires has_spec_prepare<std::optional<T>>
    {
        return spec_t<std::optional<T>>::prepare(out.inner, slot_idx);
    }

    static void* next(const nullable_t<T>& in, const void* prev)
        requires has_spec_next<std::optional<T>>
    {
        return spec_t<std::optional<T>>::next(in.inner, prev);
    }

    static void* nullable(nullable_t<T>& out, size_t slot_idx)
    {
        return spec_t<std::optional<T>>::nullable(out.inner, slot_idx);
    }
};

}

#endif
