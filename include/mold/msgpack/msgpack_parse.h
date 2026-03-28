#ifndef MOLD_MSGPACK_PARSE_H
#define MOLD_MSGPACK_PARSE_H

/**
 * @file
 * @brief Iterative MessagePack parser with schema validation and struct population.
 */

#include "mold/refl/parse_common.h"

namespace mold {

#if (MOLD_REFLECTION_MSGPACK_ENABLED)

/**
 * @brief MessagePack format policy for the shared parse stack.
 *
 */
struct msgpack_policy_t {
    using type_t = msgpack_type_t;
    static constexpr type_t object_type = msgpack_type_t::object;
    static constexpr type_t array_type  = msgpack_type_t::array;
    static constexpr type_t null_type   = msgpack_type_t::null;
    static bool type_has(type_t mask, type_t bit) { return msgpack_type_has(mask, bit); }
    static type_t get_type(const reflection_t& r) { return r.msgpack_type; }
    static io_handler_t get_handler(const reflection_t& r) { return r.msgpack_handler; }
};

/**
 * @brief Calculates the total number of members within T and its nested structures.
 *
 * @tparam T Aggregate or container type to count
 * @return Maximum number of found-flags needed at any nesting level
 */
template<class T>
constexpr size_t msgpack_count_nodes()
{
    using U = decay_optional_t<T>;
    if constexpr (is_homogenous_container<U>) {
        return msgpack_count_nodes<typename container_traits_t<U>::element_type>();
    }
    if constexpr (is_aggregate<U> || is_tuple<U>) {
        constexpr size_t direct = type_info_t<U>::members().size();
        constexpr size_t nested = [] <size_t... I> (std::index_sequence<I...>) {
            size_t m = 0;
            ((m = std::max(m, msgpack_count_nodes<decay_optional_t<std::tuple_element_t<I, typename type_info_t<U>::tuple_type>>>())), ...);
            return m;
        }(std::make_index_sequence<direct>());
        return direct + nested;
    }
    return 0u;
}

/**
 * @brief Decode MessagePack container size from the header byte.
 *
 * @param b Header byte (already consumed)
 * @param ptr Current position (advanced past size bytes)
 * @param end End of buffer
 * @param size Output: number of elements (or key-value pairs)
 * @return `error_t::ok` on success
 */
constexpr error_t msgpack_decode_container_size(uint8_t b, msgpack_ptr_t& ptr, msgpack_ptr_t end, size_t& size)
{
    // fixmap: 0x80-0x8f
    if ((b & 0xf0) == 0x80) {
        size = b & 0x0f;
        return error_t::ok;
    }
    // fixarray: 0x90-0x9f
    if ((b & 0xf0) == 0x90) {
        size = b & 0x0f;
        return error_t::ok;
    }
    switch (b) 
    {
    case 0xdc: // array16
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        size = msgpack_getbe<uint16_t>(ptr); ptr += 2;
        return error_t::ok;
    case 0xdd: // array32
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        size = msgpack_getbe<uint32_t>(ptr); ptr += 4;
        return error_t::ok;
    case 0xde: // map16
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        size = msgpack_getbe<uint16_t>(ptr); ptr += 2;
        return error_t::ok;
    case 0xdf: // map32
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        size = msgpack_getbe<uint32_t>(ptr); ptr += 4;
        return error_t::ok;
    default:
        return error_t::internal_logic_error;
    }
}

/**
 * @brief Check if a MessagePack header byte starts a map.
 *
 * @param b Header byte
 * @return true if map (fixmap, map16, or map32)
 */
constexpr bool msgpack_is_map(uint8_t b)
{
    return (b & 0xf0) == 0x80 || b == 0xde || b == 0xdf;
}

/**
 * @brief Check if a MessagePack header byte starts an array.
 *
 * @param b Header byte
 * @return true if array (fixarray, array16, or array32)
 */
constexpr bool msgpack_is_array(uint8_t b)
{
    return (b & 0xf0) == 0x90 || b == 0xdc || b == 0xdd;
}

/**
 * @brief Core MessagePack validation and population logic.
 *
 * @param msgpack_data MessagePack input; prefix is consumed on return
 * @param root Reflection schema for the root type
 * @param base_ptr Pointer to the struct instance to populate
 * @param found_flags Pre-allocated buffer for required-field tracking
 * @param stack_buffer Pre-allocated stack frame buffer
 * @param allow_unexpected If true, silently skip unknown keys
 * @return `error_t::ok` on success, or a specific error code
 */
MOLD_NOINLINE inline error_t msgpack_parse_impl(
    std::span<const uint8_t>& msgpack_data,
    const reflection_t* root,
    void* base_ptr,
    std::span<bool> found_flags,
    std::span<parse_frame_t> stack_buffer,
    bool allow_unexpected)
{
    msgpack_ptr_t ptr = msgpack_data.data();
    msgpack_ptr_t const end = msgpack_data.data() + msgpack_data.size();
    parse_stack_t<msgpack_policy_t> stack { .frames = stack_buffer, .flags = found_flags };
    // RAII updater
    struct updater_t {
        std::span<const uint8_t>& ref;
        msgpack_ptr_t base;
        msgpack_ptr_t& cur;
        ~updater_t() {
            if (cur >= base) {
                ref = ref.subspan(cur - base);
            }
        }
    } updater { msgpack_data, ptr, ptr };

    if (ptr >= end) {
        return error_t::invalid_argument;
    }
    if (!root || !msgpack_type_has(root->msgpack_type, msgpack_type_t::object)) {
        return error_t::internal_logic_error;
    }
    if (!stack.push(root, base_ptr)) {
        return error_t::internal_logic_error;
    }
    while (stack.top > 0) {
        parse_frame_t& f = stack.current();
        const auto* refl = f.refl;
        const bool is_obj = msgpack_type_has(refl->msgpack_type, msgpack_type_t::object);

        if (f.state == parse_state_t::post_item) {
            // --- Check completion or advance ---
            f.member_idx++;

            if (f.member_idx >= f.container_size) {
                if (is_obj) {
                    MOLD_TRY(stack.check_required(f));
                } else {
                    const auto meta = stack.describe_array(refl);
                    if (meta.is_fixed && f.member_idx != meta.limit) {
                        return error_t::array_size_mismatch;
                    }
                }
                if (!stack.pop()) {
                    return error_t::internal_logic_error;
                }
                continue;
            }
            if (!is_obj) {
                const auto meta = stack.describe_array(refl);
                if (meta.is_fixed && f.member_idx >= meta.limit) {
                    return error_t::array_size_mismatch;
                }
            }
        } else {
            // --- Initial: open container or consume primitive ---
            if (ptr >= end) {
                return error_t::unexpected_eof;
            }
            uint8_t header = *ptr;

            if (msgpack_is_map(header)) {
                if (!msgpack_type_has(refl->msgpack_type, msgpack_type_t::object)) {
                    return error_t::type_mismatch_structure;
                }
                ptr++;
                if (!stack.visit_nullable(f)) {
                    return error_t::handler_failure;
                }
                MOLD_TRY(msgpack_decode_container_size(header, ptr, end, f.container_size));
                f.member_idx = 0;
                f.state = parse_state_t::post_item;
                // Empty map: trigger done check
                if (f.member_idx >= f.container_size) {
                    MOLD_TRY(stack.check_required(f));
                    if (!stack.pop()) {
                        return error_t::internal_logic_error;
                    }
                    continue;
                }
            } else if (msgpack_is_array(header)) {
                if (!msgpack_type_has(refl->msgpack_type, msgpack_type_t::array)) {
                    return error_t::type_mismatch_structure;
                }
                ptr++;
                MOLD_TRY(msgpack_decode_container_size(header, ptr, end, f.container_size));
                f.member_idx = 0;
                f.state = parse_state_t::post_item;
                // Empty array: check done
                if (f.member_idx >= f.container_size) {
                    const auto meta = stack.describe_array(refl);
                    if (meta.is_fixed && meta.limit != 0) {
                        return error_t::array_size_mismatch;
                    }
                    if (!stack.pop()) {
                        return error_t::internal_logic_error;
                    }
                    continue;
                }
            } else {
                // Primitive
                msgpack_result_t result = msgpack_parse_primitive(ptr, end);
                if (result.err() != error_t::ok) {
                    return result.err();
                }
                msgpack_type_t actual = result.type();
                bool match = msgpack_type_has(refl->msgpack_type, actual);
                if (!match && actual == msgpack_type_t::integer && msgpack_type_has(refl->msgpack_type, msgpack_type_t::floating)) {
                    match = true;
                }
                if (!match) {
                    return error_t::type_mismatch_primitive;
                }
                ptr = result.end();
                if (f.base && refl->msgpack_handler) {
                    io_value_t val{&detail::msgpack_value_vtable, &result};
                    auto err_ptr = refl->msgpack_handler(io_type_t::prepare_value, {
                        .mptr = f.base, .io_val = &val
                    });
                    if (err_ptr) {
                        return error_t(reinterpret_cast<uintptr_t>(err_ptr));
                    }
                }
                if (!stack.pop()) {
                    return error_t::internal_logic_error;
                }
                continue;
            }
        }
        // --- Item Processing ---
        if (is_obj) {
            if (ptr >= end) {
                return error_t::unexpected_eof;
            }
            // MessagePack map keys must be strings
            msgpack_result_t key = msgpack_parse_primitive(ptr, end);
            if (key.err() != error_t::ok) {
                return key.err();
            }
            if (!key.is(msgpack_type_t::string)) {
                return error_t::key_not_string;
            }
            std::string_view key_str = key.string();
            ptr = key.end();

            size_t idx = stack.find_member(f, key_str);
            if (idx == size_t(-2)) {
                return error_t::duplicate_key;
            }
            if (idx == size_t(-1)) {
                if (!allow_unexpected) {
                    return error_t::unexpected_key;
                }
                MOLD_TRY(msgpack_skip_value(ptr, end));
                continue;
            }
            if (!stack.push_member(f, idx)) {
                return error_t::internal_logic_error;
            }
        } else {
            MOLD_TRY(stack.push_array_element(f));
        }
    }
    if (ptr != end) {
        return error_t::trailing_data;
    }
    return error_t::ok;
}

/**
 * @brief Validate MessagePack against a compile-time generated schema and populate a struct.
 *
 * @tparam T Aggregate type to populate
 * @param instance Target struct instance
 * @param msgpack_data MessagePack input; prefix is consumed on return
 * @param allow_unexpected If true, silently skip unknown keys
 * @return `error_t::ok` on success, or a specific error code
 */
template<is_aggregate T>
error_t msgpack_to(T& instance, std::span<const uint8_t>& msgpack_data, bool allow_unexpected = false)
{
    static constexpr auto node_count = msgpack_count_nodes<T>();
    static constexpr auto type_depth = type_info_t<T>::depth();
    static constexpr auto stack_depth = (type_depth == 0 && node_count > 0) ? 1 : type_depth + 1;

    std::array<bool, node_count> found_flags = {};
    std::array<parse_frame_t, stack_depth> stack_buffer;

    return msgpack_parse_impl(msgpack_data, &type_info_t<T>::self(), &instance,
        found_flags, stack_buffer, allow_unexpected);
}

#endif

}

#endif
