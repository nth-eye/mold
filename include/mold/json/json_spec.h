#ifndef MOLD_JSON_SPEC_H
#define MOLD_JSON_SPEC_H

/**
 * @file
 * @brief JSON adapter vtables and `spec_t` specializations for built-in types.
 *
 * Defines constexpr `io_value_t` / `io_sink_t` vtables for JSON primitives,
 * concepts for format-specific callbacks (`json_read` / `json_emit`), and
 * `json_spec_t<T>` - auto-generated from `spec_t<T>`. Types with format-specific
 * callbacks get a dedicated handler accessing raw `json_primitive_t` / `json_sink_t`;
 * others delegate to `spec_handler_t<T>::handler` shared with CBOR.
 */

#include "mold/json/json_sink.h"
#include "mold/json/json_util.h"
#include "mold/refl/spec.h"

namespace mold {
namespace detail {

/**
 * @brief Vtable bridging `io_value_t` to `json_primitive_t` accessors.
 *
 */
inline constexpr io_value_t::vtable_t json_value_vtable = {
    [](const void* p) { return static_cast<const json_primitive_t*>(p)->null(); },
    [](const void* p) { return static_cast<const json_primitive_t*>(p)->boolean(); },
    [](const void* p) { return static_cast<const json_primitive_t*>(p)->integer(); },
    [](const void* p) { return static_cast<const json_primitive_t*>(p)->uinteger(); },
    [](const void* p) { return static_cast<const json_primitive_t*>(p)->number(); },
    [](const void* p) { return static_cast<const json_primitive_t*>(p)->string(); },
};

/**
 * @brief Vtable bridging `io_sink_t` to `json_sink_t` writers.
 *
 */
inline constexpr io_sink_t::vtable_t json_sink_vtable = {
    [](const void* p, bool v) {
        static_cast<const json_sink_t*>(p)->write_literal(v ? "true" : "false");
    },
    [](const void* p, int64_t v) {
        static_cast<const json_sink_t*>(p)->write_fmt_lld(static_cast<long long>(v));
    },
    [](const void* p, uint64_t v) {
        static_cast<const json_sink_t*>(p)->write_fmt_llu(static_cast<unsigned long long>(v));
    },
    [](const void* p, double v) {
        static_cast<const json_sink_t*>(p)->write_fmt_g(v);
    },
    [](const void* p, std::string_view v) {
        static_cast<const json_sink_t*>(p)->write_escaped_string(v);
    },
};

}

/**
 * @brief Check whether `spec_t<T>` provides a JSON-native read callback.
 *
 * @tparam T Value type
 */
template<class T>
concept has_spec_json_read = requires(T& t, const json_primitive_t& v) {
    spec_t<T>::json_read(t, v);
};

/**
 * @brief Check whether `spec_t<T>` provides a JSON-native emit callback.
 *
 * @tparam T Value type
 */
template<class T>
concept has_spec_json_emit = requires(const T& t, const json_sink_t& s) {
    spec_t<T>::json_emit(t, s);
};

/**
 * @brief Call `spec_t<T>::json_read` and normalise the return to `void*`.
 *
 * Handles both void-returning and `error_t`-returning overloads.
 *
 * @tparam T Value type
 * @param obj Target object to populate
 * @param val Parsed JSON primitive
 * @return nullptr on success, encoded `error_t` on failure
 */
template<class T>
inline void* spec_invoke_json_read(T& obj, const json_primitive_t& val)
{
    if constexpr (requires { { spec_t<T>::json_read(obj, val) } -> std::convertible_to<error_t>; }) {
        error_t e = spec_t<T>::json_read(obj, val);
        return e == error_t::ok ? nullptr : reinterpret_cast<void*>(static_cast<uintptr_t>(e));
    } else {
        spec_t<T>::json_read(obj, val);
        return nullptr;
    }
}

/**
 * @brief Auto-generated JSON handler from `spec_t<T>`.
 *
 * Types with format-specific callbacks (`json_read` / `json_emit`) get a
 * dedicated handler; others fall back to the shared `spec_handler_t<T>::handler`.
 *
 * @tparam T Value type
 */
template<class T>
struct json_spec_t {
private:
    using canonical = detail::io_canonical_t<T>;

    static constexpr bool is_canonical_ = std::is_same_v<T, canonical>;
    static constexpr bool has_format_specific_ = has_spec_json_read<T> || has_spec_json_emit<T>;

    static void* format_handler_fn(io_type_t type, io_data_t data)
    {
        switch (type) 
        {
        case io_type_t::prepare_value:
            if constexpr (has_spec_json_read<T>) {
                MOLD_ASSERT(data.io_val);
                return spec_invoke_json_read(
                    *static_cast<T*>(data.mptr),
                    *static_cast<const json_primitive_t*>(data.io_val->impl));
            } else if constexpr (has_spec_read<T>) {
                MOLD_ASSERT(data.io_val);
                return spec_invoke_read(*static_cast<T*>(data.mptr), *data.io_val);
            } else if constexpr (has_spec_prepare<T>) {
                return spec_t<T>::prepare(*static_cast<T*>(data.mptr), data.slot_idx);
            }
            return nullptr;

        case io_type_t::emit_or_next:
            if constexpr (has_spec_json_emit<T>) {
                MOLD_ASSERT(data.io_sink);
                spec_t<T>::json_emit(
                    *static_cast<const T*>(data.cptr),
                    *static_cast<const json_sink_t*>(data.io_sink->impl));
                return nullptr;
            } else if constexpr (has_spec_emit<T>) {
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
    static constexpr io_handler_t handler = [] {
        if constexpr (!is_canonical_) {
            return json_spec_t<canonical>::handler;
        } else if constexpr (has_format_specific_) {
            return &format_handler_fn;
        } else {
            return spec_handler_t<canonical>::handler;
        }
    }();
    static constexpr json_type_t expected = spec_t<T>::json_type;
};

}

#endif
