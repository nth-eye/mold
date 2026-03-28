#ifndef MOLD_REFL_SPEC_H
#define MOLD_REFL_SPEC_H

/**
 * @file
 * @brief User-facing customization point for adding JSON + CBOR support to a type.
 */

#include "mold/json/json_util.h"
#include "mold/cbor/cbor_util.h"
#include "mold/refl/io.h"

namespace mold {

/**
 * @brief Customization point for adding JSON + CBOR serialization support to a type.
 *
 * Specialize this template and provide the callbacks needed for your type:
 * `read`/`emit` for primitives, `prepare`/`next` for containers, `nullable`
 * for optional wrappers. Also set `json_type` and `cbor_type` masks accordingly.
 *
 * @tparam T Type to provide serialization support for
 */
template<class T>
struct spec_t {
    static constexpr json_type_t json_type = json_type_t::null;
    static constexpr cbor_type_t cbor_type = cbor_type_t::null;
};

namespace detail {

/**
 * @brief Maps aliased types to a canonical representative for handler deduplication.
 *
 * @tparam T Original type
 */
template<class T> 
struct io_canonical_impl_t { 
    using type = T; 
};

template<> struct io_canonical_impl_t<signed char> { using type = char; };
template<> struct io_canonical_impl_t<mold::float32_t> { using type = float; };
template<> struct io_canonical_impl_t<mold::float64_t> { using type = double; };

template<class T>
using io_canonical_t = typename io_canonical_impl_t<T>::type;

}

/**
 * @brief True if `spec_t<T>` provides a `read` callback.
 *
 * @tparam T Type to check
 */
template<class T>
concept has_spec_read = requires(T& t, const io_value_t& v) { 
    spec_t<T>::read(t, v); 
};

/**
 * @brief True if `spec_t<T>` provides an `emit` callback.
 *
 * @tparam T Type to check
 */
template<class T>
concept has_spec_emit = requires(const T& t, const io_sink_t& s) { 
    spec_t<T>::emit(t, s); 
};

/**
 * @brief True if `spec_t<T>` provides a `prepare` callback.
 *
 * @tparam T Type to check
 */
template<class T>
concept has_spec_prepare = requires(T& t, size_t i) {
    { spec_t<T>::prepare(t, i) } -> std::convertible_to<void*>;
};

/**
 * @brief True if `spec_t<T>` provides a `next` callback.
 *
 * @tparam T Type to check
 */
template<class T>
concept has_spec_next = requires(const T& t, const void* p) {
    { spec_t<T>::next(t, p) } -> std::convertible_to<void*>;
};

/**
 * @brief True if `spec_t<T>` provides a `nullable` callback.
 *
 * @tparam T Type to check
 */
template<class T>
concept has_spec_nullable = requires(T& t, size_t i) {
    { spec_t<T>::nullable(t, i) } -> std::convertible_to<void*>;
};

/**
 * @brief True if `spec_t<T>` declares `write_null = true`.
 *
 * Types with this flag write explicit null when absent.
 * Types without it (e.g. std::optional) skip the field entirely.
 *
 * @tparam T Type to check
 */
namespace detail {

template<class T, class = void>
struct spec_write_null_impl : std::false_type {};

template<class T>
struct spec_write_null_impl<T, std::enable_if_t<spec_t<T>::write_null>> : std::true_type {};

}

template<class T>
constexpr bool spec_write_null_v = detail::spec_write_null_impl<T>::value;

/**
 * @brief Call `spec_t<T>::read` and normalise the return to `void*`.
 *
 * @tparam T Type being decoded
 * @param obj Target object
 * @param val Decoded primitive value
 * @return `nullptr` on success, or `error_t` cast to `void*` on failure.
 */
template<class T>
inline void* spec_invoke_read(T& obj, const io_value_t& val)
{
    if constexpr (requires { { spec_t<T>::read(obj, val) } -> std::convertible_to<error_t>; }) {
        error_t e = spec_t<T>::read(obj, val);
        return e == error_t::ok ? nullptr : reinterpret_cast<void*>(static_cast<uintptr_t>(e));
    } else {
        spec_t<T>::read(obj, val);
        return nullptr;
    }
}

/**
 * @brief Shared handler for types using only generic read/emit/prepare/next/nullable callbacks.
 *
 * Both `json_spec_t` and `cbor_spec_t` delegate to the same `handler_fn` for types
 * that do not provide format-specific callbacks, so the linker emits it once.
 *
 * @tparam T Type to generate a handler for
 */
template<class T>
struct spec_handler_t {
private:
    static constexpr bool has_any_ =
        has_spec_read<T>    || has_spec_emit<T>  ||
        has_spec_prepare<T> || has_spec_next<T>  ||
        has_spec_nullable<T>;

    static void* handler_fn(io_type_t type, io_data_t data)
    {
        switch (type)
        {
        case io_type_t::prepare_value:
            if constexpr (has_spec_read<T>) {
                MOLD_ASSERT(data.io_val);
                return spec_invoke_read(*static_cast<T*>(data.mptr), *data.io_val);
            } else if constexpr (has_spec_prepare<T>) {
                return spec_t<T>::prepare(*static_cast<T*>(data.mptr), data.slot_idx);
            }
            return nullptr;

        case io_type_t::emit_or_next:
            if constexpr (has_spec_emit<T>) {
                MOLD_ASSERT(data.io_sink);
                spec_t<T>::emit(*static_cast<const T*>(data.cptr), *data.io_sink);
                return nullptr;
            } else if constexpr (has_spec_next<T>) {
                return spec_t<T>::next(*static_cast<const T*>(data.cptr), data.previous_element);
            }
            return nullptr;

        case io_type_t::visit_nullable:
            if constexpr (has_spec_nullable<T>) {
                return spec_t<T>::nullable(*static_cast<T*>(data.mptr), data.slot_idx);
            }
            return nullptr;

        default:
            return nullptr;
        }
    }
public:
    static constexpr io_handler_t handler = has_any_ ? &handler_fn : nullptr;
};

}

#endif
