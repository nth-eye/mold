#ifndef MOLD_REFL_REFLECTION_H
#define MOLD_REFL_REFLECTION_H

/**
 * @file
 * @brief Compile-time schema generation for aggregate types.
 */

#include <span>
#include <utility>
#include <algorithm>
#include "mold/util/common.h"
#include "mold/util/container.h"
#include "mold/refl/to_tuple.h"
#if (MOLD_REFLECTION_JSON_ENABLED)
#include "mold/json/json_spec.h"
#endif
#if (MOLD_REFLECTION_CBOR_ENABLED)
#include "mold/cbor/cbor_spec.h"
#endif
#if (MOLD_REFLECTION_MSGPACK_ENABLED)
#include "mold/msgpack/msgpack_spec.h"
#endif
#include "mold/types/field.h"

namespace mold {

/**
 * @brief Get name of the given type as view to preprocessor string.
 *
 * @note Using this function directly increases overhead, because compiler has
 * to store whole pretty function name in memory, view to which is accessed.
 *
 * @tparam T Type
 * @return String view of the type name
 */
template<class T>
constexpr auto type_name_helper()
{
    std::string_view name = MOLD_PRETTY_FUNCTION;
#if defined(__clang__) || defined(__GNUC__)
    auto head = name.find_last_of('=') + 2;
    auto tail = name.find_last_of(']');
#elif defined(_MSC_VER)
    auto head = name.find_last_of('<') + 1;
    auto tail = name.find_last_of('>');
#else
    static_assert(false, "Unsupported compiler.");
#endif
    return name.substr(head, tail - head);
};

/**
 * @brief Get name of the member as view to preprocessor string.
 *
 * @note Using this function directly increases overhead, because compiler has
 * to store whole pretty function name in memory, view to which is accessed.
 *
 * @tparam P Pointer to member of fake object
 * @return String view of the member name
 */
template<auto P>
constexpr auto member_name_helper()
{
    std::string_view name = MOLD_PRETTY_FUNCTION;
#if defined(__clang__)
    auto head = name.find_last_of('.') + 1;
    auto tail = name.find_last_of(']');
#elif defined(__GNUC__)
    auto head = name.find_last_of(':') + 1;
    auto tail = name.find_last_of(')');
#else
    static_assert(false, "Unsupported compiler.");
#endif
    return name.substr(head, tail - head);
};

/**
 * @brief Fake object declaration of the given type.
 *
 * Symbolic object for compile-time reflection. Used to obtain a pointer-to-member
 * of the I-th field via `std::addressof(std::get<I>(tie_as_tuple(...)))`. This
 * pointer is then used to extract the member's name from the compiler's pretty
 * function signature. No definition is required as it's only used for type analysis.
 *
 * @tparam T Type
 */
template <class T>
extern const T fake_object;

/**
 * @brief Calculate offset of a member in aggregate object.
 *
 * @tparam TPtr Pointer to the aggregate
 * @tparam MPtr Pointer to the member of the aggregate
 * @return Offset of the member in the aggregate
 */
template<auto TPtr, auto MPtr>
constexpr auto offset_of()
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    struct offset_helper_t {
        char for_sizeof[
            (char*) MPtr -
            (char*) TPtr
        ];
    };
#pragma GCC diagnostic pop
    return sizeof(offset_helper_t::for_sizeof);
}

/**
 * @brief Get address of a member in aggregate object.
 *
 * @tparam T Aggregate type
 * @tparam I Index of the member
 * @param obj Aggregate object
 * @return Address of the member in the aggregate object
 */
template<class T, size_t I>
constexpr auto member_address(const T& obj)
{
    return std::addressof(std::get<I>(tie_as_tuple(obj)));
}

/**
 * @brief Calculate offset of the I-th member in aggregate object.
 *
 * @tparam T Aggregate type
 * @tparam I Index of the member
 * @return Offset of the member in the aggregate
 */
template<class T, size_t I>
constexpr auto member_offset()
{
    return offset_of<std::addressof(fake_object<T>), member_address<T, I>(fake_object<T>)>();
}

template<class T> struct nullable_t;

/**
 * @brief Strip std::optional wrapper from a type, leaving the inner type.
 *
 * @tparam T Input type
 */
template<class T>
struct decay_optional_helper_t {
    using type = std::remove_cvref_t<T>;
};

/**
 * @brief Specialization that unwraps `std::optional<T>` to its inner type.
 *
 */
template<class T>
struct decay_optional_helper_t<std::optional<T>> {
    using type = std::remove_cvref_t<T>;
};

/**
 * @brief Specialization that unwraps `nullable_t<T>` to its inner type.
 *
 */
template<class T>
struct decay_optional_helper_t<nullable_t<T>> {
    using type = std::remove_cvref_t<T>;
};

/**
 * @brief Alias that strips `std::optional` from a type, leaving the inner type.
 *
 */
template<class T>
using decay_optional_t = typename decay_optional_helper_t<std::remove_cvref_t<T>>::type;

/**
 * @brief Map an aggregate type to its tuple representation.
 *
 * For non-aggregates the result is simply std::decay_t<T>.
 *
 * @tparam T Input type
 */
template<class T, class = void>
struct decay_aggregate_helper_t {
    using type = std::decay_t<T>;
};

/**
 * @brief Specialization that maps aggregate types to their tuple representation.
 *
 */
template<class T>
struct decay_aggregate_helper_t<T, std::enable_if_t<is_aggregate<std::decay_t<T>>>> {
    using type = decltype(to_tuple(std::declval<T>()));
};

/**
 * @brief Alias that maps an aggregate type to its tuple representation.
 *
 */
template<class T>
using decay_aggregate_t = typename decay_aggregate_helper_t<T>::type;

/**
 * @brief Compile-time metadata for the I-th member of aggregate T.
 *
 * @tparam T Aggregate type
 * @tparam I Zero-based member index
 */
template<class T, size_t I>
struct member_info_t {
    using tuple_type = decay_aggregate_t<T>;
    using field_type = std::tuple_element_t<I, tuple_type>;
    static constexpr auto name()        { return std::string_view{name_.data(), name_.size() - 1}; }
    static constexpr auto& name_array() { return name_; }
    static constexpr auto size()        { return sizeof(field_type); }
    static constexpr auto offset()      { return member_offset<T, I>(); }
private:
    static constexpr auto name_ = [] {
        constexpr auto name = member_name_helper<member_address<T, I>(fake_object<T>)>();
        std::array<char, name.size() + 1> arr = {};
        std::copy(name.begin(), name.end(), arr.begin());
        return arr;
    }();
};

struct reflection_t;

/**
 * @brief Span of `reflection_t` objects.
 *
 */
using reflection_span_t = std::span<const reflection_t>;

/**
 * @brief Detailed reflection information for a single aggregate member.
 *
 * Stores the compile-time schema for one node in the type tree: its child
 * members (if composite), name, layout offsets, and per-format handler +
 * type-mask pairs used by the iterative parser and writer.
 */
struct reflection_t {

    /**
     * @brief Dynamic element count value.
     *
     * Used to indicate that the element count is not known at compile time.
     */
    static constexpr uint32_t dynamic_count = std::numeric_limits<uint32_t>::max();

    reflection_span_t members = {};     ///< Direct child reflections, or single element type for arrays.
#if (MOLD_REFLECTION_TYPE_NAME_ENABLED)
    std::string_view type = {};         ///< Member type name (debug/diagnostic only).
#endif
#if (MOLD_REFLECTION_NAME_NEEDED)
    std::string_view name = {};         ///< Member name for aggregates; element type name for arrays.
#endif
#if (MOLD_REFLECTION_CBOR_ENABLED)
#if (MOLD_REFLECTION_CBOR_FIELD_KEYS)
    std::span<const uint8_t> cbor_key;  ///< Pre-encoded CBOR key bytes (empty = use string name).
#endif
#endif
#if (MOLD_REFLECTION_SIZE_ENABLED)
    uint32_t size = 0;                  ///< Size of the member in bytes.
#endif
    uint32_t offset = 0;                ///< Byte offset of the member in the parent object.
    uint32_t element_count = 0;         ///< Array element count: 0 for non-arrays, UINT32_MAX for dynamic.
#if (MOLD_REFLECTION_JSON_ENABLED)
    io_handler_t json_handler;          ///< JSON unified callback handler for decode/encode events.
#endif
#if (MOLD_REFLECTION_CBOR_ENABLED)
    io_handler_t cbor_handler;          ///< CBOR unified callback handler for decode/encode events.
#endif
#if (MOLD_REFLECTION_MSGPACK_ENABLED)
    io_handler_t msgpack_handler;       ///< MessagePack unified callback handler for decode/encode events.
#endif
#if (MOLD_REFLECTION_JSON_ENABLED)
    json_type_t json_type;              ///< Bitmask of allowed JSON types.
#endif
#if (MOLD_REFLECTION_CBOR_ENABLED)
    cbor_type_t cbor_type;              ///< Bitmask of allowed CBOR types.
#endif
#if (MOLD_REFLECTION_MSGPACK_ENABLED)
    msgpack_type_t msgpack_type;        ///< Bitmask of allowed MessagePack types.
#endif
    bool skip_null = false;             ///< True = skip field when absent (std::optional); false = write null.
};

/**
 * @brief Calculate the depth of the reflection span.
 *
 * @param children Reflection span of children
 * @return Depth of the reflection span
 */
constexpr size_t reflection_depth(reflection_span_t children)
{
    size_t max_depth = 0;
    for (auto& refl : children) {
        auto depth = reflection_depth(refl.members);
        if (max_depth < depth) {
            max_depth = depth;
        }
    }
    return !children.empty() + max_depth;
}

/**
 * @brief Check if a member field should be skipped during encoding.
 *
 * Returns true for absent optional fields marked with `skip_null`.
 *
 * @param m Member reflection
 * @param base Pointer to the parent struct instance
 * @param handler_ptr Pointer-to-member for the format-specific handler
 * @return True if the field should be omitted from output
 */
inline bool should_skip(const reflection_t& m, const void* base, io_handler_t reflection_t::*handler_ptr)
{
    if (!m.skip_null) {
        return false;
    }
    io_handler_t mh = m.*handler_ptr;
    return !mh || !mh(io_type_t::visit_nullable, {.cptr = static_cast<const char*>(base) + m.offset, .slot_idx = 0});
}

/**
 * @brief Count object members that should not be skipped during encoding.
 *
 * @param refl Reflection of the object
 * @param base Pointer to the struct instance
 * @param handler_ptr Pointer-to-member for the format-specific handler
 * @return Number of non-skipped members
 */
inline size_t count_non_skipped(const reflection_t* refl, const void* base, io_handler_t reflection_t::*handler_ptr)
{
    size_t count = 0;
    for (size_t i = 0; i < refl->members.size(); ++i) {
        if (!should_skip(refl->members[i], base, handler_ptr)) {
            ++count;
        }
    }
    return count;
}

template<class T> struct type_info_t;

/**
 * @brief Populate a `reflection_t` node for a given type.
 *
 * Centralises the per-format handler and type-mask wiring that is
 * shared between `type_info_t::members_info_` and `type_info_t::self_`.
 *
 * @tparam Field   The (possibly optional-wrapped) type of the node
 * @param members  Pre-resolved child reflections (avoids recursive instantiation)
 * @param name     Member name (empty for root / container element nodes)
 * @param offset   Byte offset within the parent aggregate (0 for root)
 * @return Fully populated `reflection_t`
 */
template<class Field>
constexpr reflection_t make_reflection(reflection_span_t members, std::string_view name = {}, uint32_t offset = 0)
{
    reflection_t info = {};
    info.members = members;
#if (MOLD_REFLECTION_TYPE_NAME_ENABLED)
    info.type = type_info_t<Field>::name();
#endif
#if (MOLD_REFLECTION_NAME_NEEDED)
    info.name = name;
#endif
#if (MOLD_REFLECTION_SIZE_ENABLED)
    info.size = sizeof(Field);
#endif
    info.offset = offset;
    info.element_count = container_traits_t<decay_optional_t<decay_field_t<Field>>>::element_count;
    info.skip_null = has_spec_nullable<Field> && !spec_write_null_v<Field>;
#if (MOLD_REFLECTION_JSON_ENABLED)
    info.json_handler = json_spec_t<Field>::handler;
    info.json_type = json_spec_t<Field>::expected;

    if constexpr (is_homogenous_container<Field>) {
        static_assert(
            json_spec_t<Field>::expected == json_type_t::array ||
            json_spec_t<Field>::expected == json_type_t::string,
            "Homogeneous container must report json_type_t::array (or string).");
    }
    if constexpr (json_type_has(json_spec_t<Field>::expected, json_type_t::primitive)) {
        static_assert(requires { io_handler_t{ json_spec_t<Field>::handler }; },
            "Primitive type requires a JSON IO handler.");
    }
#endif
#if (MOLD_REFLECTION_CBOR_ENABLED)
    info.cbor_handler = cbor_spec_t<Field>::handler;
    info.cbor_type = cbor_spec_t<Field>::expected;
#if (MOLD_REFLECTION_CBOR_FIELD_KEYS)
    info.cbor_key = extract_cbor_key<Field>();
#endif
    if constexpr (is_homogenous_container<Field>) {
        static_assert(
            cbor_spec_t<Field>::expected == cbor_type_t::array ||
            cbor_spec_t<Field>::expected == cbor_type_t::string ||
            cbor_spec_t<Field>::expected == cbor_type_t::bytes,
            "Homogeneous container must report cbor_type_t::array (or string/bytes).");
    }
    if constexpr (cbor_type_has(cbor_spec_t<Field>::expected, cbor_type_t::primitive)) {
        static_assert(requires { io_handler_t{ cbor_spec_t<Field>::handler }; },
            "Primitive type requires a CBOR IO handler.");
    }
#endif
#if (MOLD_REFLECTION_MSGPACK_ENABLED)
    info.msgpack_handler = msgpack_spec_t<Field>::handler;
    info.msgpack_type = msgpack_spec_t<Field>::expected;

    if constexpr (is_homogenous_container<Field>) {
        static_assert(
            msgpack_spec_t<Field>::expected == msgpack_type_t::array ||
            msgpack_spec_t<Field>::expected == msgpack_type_t::string ||
            msgpack_spec_t<Field>::expected == msgpack_type_t::bytes,
            "Homogeneous container must report msgpack_type_t::array (or string/bytes).");
    }
    if constexpr (msgpack_type_has(msgpack_spec_t<Field>::expected, msgpack_type_t::primitive)) {
        static_assert(requires { io_handler_t{ msgpack_spec_t<Field>::handler }; },
            "Primitive type requires a MessagePack IO handler.");
    }
#endif
    return info;
}

/**
 * @brief Reflection information storage for a given type.
 *
 * @tparam T Type
 */
template<class T>
struct type_info_t {
    using tuple_type = decay_aggregate_t<T>;
    static constexpr auto name()    { return std::string_view{name_.data(), name_.size() - 1}; }
    static constexpr auto size()    { return sizeof(T); }
    static constexpr auto members() { return std::span{members_info_}; }
    static constexpr auto depth()   { return depth_; }
    static constexpr auto& self()   { return self_; }
private:
    static constexpr auto name_ = [] {
        constexpr auto name = type_name_helper<T>();
        std::array<char, name.size() + 1> arr = {};
        std::copy(name.begin(), name.end(), arr.begin());
        return arr;
    }();
    static constexpr auto direct_members_ = count_members<T>();
    static constexpr auto members_info_ = [] {
        if constexpr (is_homogenous_container<T>) {
            using Element = typename container_traits_t<T>::element_type;
            using ElementStripped = decay_optional_t<Element>;
            std::array<reflection_t, 1> arr = {};
            arr[0] = make_reflection<Element>(type_info_t<ElementStripped>::members());
            return arr;
        } else if constexpr (direct_members_) {
            std::array<reflection_t, direct_members_> arr = {};
            [&] <size_t... I> (std::index_sequence<I...>) {
                ((arr[I] = make_reflection<std::tuple_element_t<I, tuple_type>>(
                    type_info_t<decay_field_t<decay_optional_t<std::tuple_element_t<I, tuple_type>>>>::members(),
                    member_info_t<T, I>::name(),
                    member_info_t<T, I>::offset())), ...);
#if (MOLD_REFLECTION_CBOR_ENABLED && MOLD_REFLECTION_CBOR_FIELD_KEYS)
                ((arr[I].cbor_key.empty() ? (void)(arr[I].cbor_key = [] {
                    constexpr auto& s = detail::cbor_key_storage_t<member_info_t<T, I>::name_array()>::encoded;
                    return std::span{s.data(), s.size()};
                }()) : (void)0), ...);
#endif
            }(std::make_index_sequence<direct_members_>());
            return arr;
        } else {
            return reflection_span_t{};
        }
    }();
    static constexpr auto depth_ = reflection_depth(members());
    static constexpr auto self_ = make_reflection<T>(members());
};

/**
 * @brief Get the name of the type of the given value.
 *
 * @param value Value
 * @return String view of the type name
 */
constexpr auto type_name(auto&& value)
{
    return type_info_t<decltype(value)>::name();
}

/**
 * @brief Get name of the given type.
 *
 * @tparam T Type
 * @return String view of the type name
 */
template<class T>
constexpr auto type_name()
{
    return type_info_t<T>::name();
}

/**
 * @brief Get name of the N-th member of the given aggregate type.
 *
 * @tparam T Aggregate type
 * @param I Index of the member
 * @return String view of the member name
 */
template<class T, size_t I>
constexpr auto member_name()
{
    return type_info_t<T>::members()[I].name;
}

}

#endif
