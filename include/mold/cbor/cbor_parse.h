#ifndef MOLD_CBOR_PARSE_H
#define MOLD_CBOR_PARSE_H

/**
 * @file
 * @brief Iterative CBOR parser with schema validation and struct population.
 */

#include "mold/refl/parse_common.h"

namespace mold {

#if (MOLD_REFLECTION_CBOR_ENABLED)

/**
 * @brief CBOR format policy for the shared parse stack.
 *
 */
struct cbor_policy_t {
    using type_t = cbor_type_t;
    static constexpr type_t object_type = cbor_type_t::object;
    static constexpr type_t array_type  = cbor_type_t::array;
    static constexpr type_t null_type   = cbor_type_t::null;
    static bool type_has(type_t mask, type_t bit) { return cbor_type_has(mask, bit); }
    static type_t get_type(const reflection_t& r) { return r.cbor_type; }
    static io_handler_t get_handler(const reflection_t& r) { return r.cbor_handler; }
};

/**
 * @brief Calculates the total number of members within T and its nested structures.
 *
 * @tparam T Aggregate or container type to count
 * @return Maximum number of found-flags needed at any nesting level
 */
template<class T>
constexpr size_t cbor_count_nodes()
{
    using U = decay_optional_t<T>;
    if constexpr (is_homogenous_container<U>) {
        return cbor_count_nodes<typename container_traits_t<U>::element_type>();
    }
    if constexpr (is_aggregate<U> || is_tuple<U>) {
        constexpr size_t direct = type_info_t<U>::members().size();
        constexpr size_t nested = [] <size_t... I> (std::index_sequence<I...>) {
            size_t m = 0;
            ((m = std::max(m, cbor_count_nodes<decay_optional_t<std::tuple_element_t<I, typename type_info_t<U>::tuple_type>>>())), ...);
            return m;
        }(std::make_index_sequence<direct>());
        return direct + nested;
    }
    return 0u;
}

/**
 * @brief Core CBOR validation and population logic.
 *
 * @param cbor_data CBOR input; prefix is consumed on return
 * @param root Reflection schema for the root type
 * @param base_ptr Pointer to the struct instance to populate
 * @param found_flags Pre-allocated buffer for required-field tracking
 * @param stack_buffer Pre-allocated stack frame buffer
 * @param allow_unexpected If true, silently skip unknown keys
 * @return `error_t::ok` on success, or a specific error code
 */
MOLD_NOINLINE inline error_t cbor_parse_impl(
    std::span<const uint8_t>& cbor_data,
    const reflection_t* root,
    void* base_ptr,
    std::span<bool> found_flags,
    std::span<parse_frame_t> stack_buffer,
    bool allow_unexpected)
{
    cbor_ptr_t ptr = cbor_data.data();
    cbor_ptr_t const end = cbor_data.data() + cbor_data.size();
    parse_stack_t<cbor_policy_t> stack { .frames = stack_buffer, .flags = found_flags };
    // RAII updater
    struct updater_t {
        std::span<const uint8_t>& ref; 
        cbor_ptr_t base; 
        cbor_ptr_t& cur;
        ~updater_t() {
            if (cur >= base) {
                ref = ref.subspan(cur - base);
            }
        }
    } updater { cbor_data, ptr, ptr };

    // Helper: check if CBOR container is done (definite count or break byte)
    auto check_done = [&](parse_frame_t& f) -> bool {
        if (f.is_indefinite) {
            if (ptr < end && *ptr == 0xff) {
                ptr++;
                return true;
            }
            return false;
        }
        return f.member_idx >= f.container_size;
    };

    // Helper: decode container size from ai
    auto decode_size = [&](uint8_t ai, parse_frame_t& f) -> error_t {
        if (ai == +cbor_ai_t::_indef) {
            f.is_indefinite = true;
            f.container_size = 0;
        } else {
            auto [err, val, next] = cbor_decode_ai(ai, ptr, end);
            if (err != error_t::ok) {
                return err;
            }
            ptr = next;
            f.is_indefinite = false;
            f.container_size = size_t(val);
        }
        return error_t::ok;
    };
    if (ptr >= end) {
        return error_t::invalid_argument;
    }
    if (!root || !cbor_type_has(root->cbor_type, cbor_type_t::object)) {
        return error_t::internal_logic_error;
    }
    if (!stack.push(root, base_ptr)) {
        return error_t::internal_logic_error;
    }
    while (stack.top > 0) {
        parse_frame_t& f = stack.current();
        const auto* refl = f.refl;
        const bool is_obj = cbor_type_has(refl->cbor_type, cbor_type_t::object);

        if (f.state == parse_state_t::post_item) {
            // --- Check completion or advance ---
            f.member_idx++;

            if (check_done(f)) {
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
            uint8_t mt = header & 0xe0;
            uint8_t ai = header & 0x1f;

            if (mt == +cbor_mt_t::map) {
                if (!cbor_type_has(refl->cbor_type, cbor_type_t::object)) {
                    return error_t::type_mismatch_structure;
                }
                ptr++;
                if (!stack.visit_nullable(f)) {
                    return error_t::handler_failure;
                }
                MOLD_TRY(decode_size(ai, f));
                f.member_idx = 0;
                f.state = parse_state_t::post_item;
                // Empty map: trigger done check
                if (check_done(f)) {
                    MOLD_TRY(stack.check_required(f));
                    if (!stack.pop()) {
                        return error_t::internal_logic_error;
                    }
                    continue;
                }
            } else if (mt == +cbor_mt_t::arr) {
                if (!cbor_type_has(refl->cbor_type, cbor_type_t::array)) {
                    return error_t::type_mismatch_structure;
                }
                ptr++;
                MOLD_TRY(decode_size(ai, f));
                f.member_idx = 0;
                f.state = parse_state_t::post_item;
                // Empty array: check done
                if (check_done(f)) {
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
                cbor_result_t result = cbor_parse_primitive(ptr, end);
                if (result.err() != error_t::ok) {
                    return result.err();
                }
                cbor_type_t actual = result.type();
                bool match = cbor_type_has(refl->cbor_type, actual);
                if (!match && actual == cbor_type_t::integer && cbor_type_has(refl->cbor_type, cbor_type_t::floating)) {
                    match = true;
                }
                if (!match) {
                    return error_t::type_mismatch_primitive;
                }
                ptr = result.end();
                if (f.base && refl->cbor_handler) {
                    io_value_t val{&detail::cbor_value_vtable, &result};
                    auto err_ptr = refl->cbor_handler(io_type_t::prepare_value, {
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
            size_t idx = size_t(-1);

#if (MOLD_REFLECTION_CBOR_FIELD_KEYS)
            {
                cbor_ptr_t key_start = ptr;
                MOLD_TRY(cbor_skip_value(ptr, end));
                idx = stack.find_member_by_cbor_key(f, {key_start, ptr});
            }
#elif (MOLD_REFLECTION_NAME_NEEDED)
            uint8_t key_mt = *ptr & 0xe0;
            if (key_mt == +cbor_mt_t::text) {
                cbor_result_t key = cbor_parse_primitive(ptr, end);
                if (key.err() != error_t::ok) {
                    return key.err();
                }
                std::string_view key_str = key.string();
                ptr = key.end();
                idx = stack.find_member(f, key_str);
            } else {
                return error_t::key_not_string;
            }
#else
            return error_t::key_not_string;
#endif
            if (idx == size_t(-2)) {
                return error_t::duplicate_key;
            }
            if (idx == size_t(-1)) {
                if (!allow_unexpected) {
                    return error_t::unexpected_key;
                }
                MOLD_TRY(cbor_skip_value(ptr, end));
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
 * @brief Validate CBOR against a compile-time generated schema and populate a struct directly.
 *
 * @tparam T Aggregate type to populate
 * @param instance Target struct instance
 * @param cbor_data CBOR input; prefix is consumed on return
 * @param allow_unexpected If true, silently skip unknown keys
 * @return `error_t::ok` on success, or a specific error code
 */
template<is_aggregate T>
error_t cbor_to(T& instance, std::span<const uint8_t>& cbor_data, bool allow_unexpected = false)
{
    static constexpr auto node_count = cbor_count_nodes<T>();
    static constexpr auto type_depth = type_info_t<T>::depth();
    static constexpr auto stack_depth = (type_depth == 0 && node_count > 0) ? 1 : type_depth + 1;

    std::array<bool, node_count> found_flags = {};
    std::array<parse_frame_t, stack_depth> stack_buffer;

    return cbor_parse_impl(cbor_data, &type_info_t<T>::self(), &instance,
        found_flags, stack_buffer, allow_unexpected);
}

#endif

}

#endif
