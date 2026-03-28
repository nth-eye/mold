#ifndef MOLD_JSON_PARSE_H
#define MOLD_JSON_PARSE_H

/**
 * @file
 * @brief Iterative JSON parser with schema validation and struct population.
 */

#include "mold/refl/parse_common.h"

namespace mold {

#if (MOLD_REFLECTION_JSON_ENABLED)

/**
 * @brief JSON format policy for the shared parse stack.
 *
 */
struct json_policy_t {
    using type_t = json_type_t;
    static constexpr type_t object_type = json_type_t::object;
    static constexpr type_t array_type  = json_type_t::array;
    static constexpr type_t null_type   = json_type_t::null;
    static bool type_has(type_t mask, type_t bit) { return json_type_has(mask, bit); }
    static type_t get_type(const reflection_t& r) { return r.json_type; }
    static io_handler_t get_handler(const reflection_t& r) { return r.json_handler; }
};

/**
 * @brief Calculates the total number of members within T and its nested structures.
 *
 * @tparam T Aggregate or container type to count
 * @return Maximum number of found-flags needed at any nesting level
 */
template<class T>
constexpr size_t json_count_nodes()
{
    using U = decay_optional_t<T>;
    if constexpr (is_homogenous_container<U>) {
        return json_count_nodes<typename container_traits_t<U>::element_type>();
    }
    if constexpr (is_aggregate<U> || is_tuple<U>) {
        constexpr size_t direct = type_info_t<U>::members().size();
        constexpr size_t nested = [] <size_t... I> (std::index_sequence<I...>) {
            size_t m = 0;
            ((m = std::max(m, json_count_nodes<decay_optional_t<std::tuple_element_t<I, typename type_info_t<U>::tuple_type>>>())), ...);
            return m;
        }(std::make_index_sequence<direct>());
        return direct + nested;
    }
    return 0u;
}

/**
 * @brief Core JSON validation and population logic.
 *
 * @param json_data JSON input; prefix is consumed on return
 * @param root Reflection schema for the root type
 * @param base_ptr Pointer to the struct instance to populate
 * @param found_flags Pre-allocated buffer for required-field tracking
 * @param stack_buffer Pre-allocated stack frame buffer
 * @param allow_unexpected If true, silently skip unknown keys
 * @return `error_t::ok` on success, or a specific error code
 */
MOLD_NOINLINE inline error_t json_parse_impl(
    std::string_view& json_data,
    const reflection_t* root,
    void* base_ptr,
    std::span<bool> found_flags,
    std::span<parse_frame_t> stack_buffer,
    bool allow_unexpected)
{
    json_ptr_t ptr = json_data.data();
    json_ptr_t const end = json_data.data() + json_data.size();
    parse_stack_t<json_policy_t> stack { .frames = stack_buffer, .flags = found_flags };
    // RAII updater
    struct updater_t {
        std::string_view& ref; 
        json_ptr_t base; 
        json_ptr_t& cur;
        ~updater_t() {
            if (cur >= base) {
                ref.remove_prefix(cur - base);
            }
        }
    } updater { json_data, ptr, ptr };

    auto eat_space = [&] { ptr = json_parse_space(ptr, end); };

    eat_space();
    if (ptr >= end) {
        return error_t::invalid_argument;
    }
    if (!root || !json_type_has(root->json_type, json_type_t::object)) {
        return error_t::internal_logic_error;
    }
    if (!stack.push(root, base_ptr)) {
        return error_t::internal_logic_error;
    }
    while (stack.top > 0) {
        parse_frame_t& f = stack.current();
        const auto* refl = f.refl;
        const bool is_obj = json_type_has(refl->json_type, json_type_t::object);

        if (f.state == parse_state_t::post_item) {
            // --- Check completion or advance to next item ---
            eat_space();
            if (ptr >= end) {
                return error_t::unexpected_eof;
            }
            if (is_obj) {
                if (*ptr == '}') {
                    ptr++;
                    MOLD_TRY(stack.check_required(f));
                    if (!stack.pop()) {
                        return error_t::internal_logic_error;
                    }
                    continue;
                }
                if (*ptr != ',') {
                    return error_t::parse_error_key;
                }
                ptr++;
            } else {
                f.member_idx++;
                const auto meta = stack.describe_array(refl);
                if (*ptr == ']') {
                    ptr++;
                    if (meta.is_fixed && f.member_idx != meta.limit) {
                        return error_t::array_size_mismatch;
                    }
                    if (!stack.pop()) {
                        return error_t::internal_logic_error;
                    }
                    continue;
                }
                if (*ptr != ',') {
                    return error_t::type_mismatch_structure;
                }
                ptr++;
                if (meta.is_fixed && f.member_idx >= meta.limit) {
                    return error_t::array_size_mismatch;
                }
            }
        } else {
            // --- Initial: open container or consume primitive ---
            eat_space();
            if (ptr >= end) {
                return error_t::unexpected_eof;
            }
            if (*ptr == '{') {
                if (!json_type_has(refl->json_type, json_type_t::object)) {
                    return error_t::type_mismatch_structure;
                }
                if (!stack.visit_nullable(f)) {
                    return error_t::handler_failure;
                }
                ptr++;
                eat_space();
                if (ptr >= end) {
                    return error_t::unexpected_eof;
                }
                f.state = parse_state_t::post_item;
                if (*ptr == '}') {
                    continue;
                }
            } else if (*ptr == '[') {
                if (!json_type_has(refl->json_type, json_type_t::array)) {
                    return error_t::type_mismatch_structure;
                }
                ptr++;
                f.member_idx = 0;
                eat_space();
                if (ptr >= end) {
                    return error_t::unexpected_eof;
                }
                if (*ptr == ']') {
                    ptr++;
                    const auto meta = stack.describe_array(refl);
                    if (meta.is_fixed && meta.limit != 0) {
                        return error_t::array_size_mismatch;
                    }
                    if (!stack.pop()) {
                        return error_t::internal_logic_error;
                    }
                    continue;
                }
                f.state = parse_state_t::post_item;
            } else {
                // Primitive
                json_result_t result = json_parse_primitive(ptr, end);
                if (result.err() != error_t::ok) {
                    return result.err();
                }
                json_type_t actual = result.type();
                bool match = json_type_has(refl->json_type, actual);
                if (!match && actual == json_type_t::integer && json_type_has(refl->json_type, json_type_t::floating)) {
                    match = true;
                }
                if (!match) {
                    return error_t::type_mismatch_primitive;
                }
                ptr = result.end();
                if (f.base && refl->json_handler) {
                    io_value_t val{&detail::json_value_vtable, &result};
                    auto err_ptr = refl->json_handler(io_type_t::prepare_value, {
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
            eat_space();
            if (ptr >= end || *ptr != '"') {
                return error_t::parse_error_key;
            }
            json_result_t key = json_parse_string(ptr + 1, end);
            if (key.err() != error_t::ok) {
                return key.err();
            }
            size_t idx = stack.find_member(f, key.string());
            ptr = key.end();

            if (idx == size_t(-2)) {
                return error_t::duplicate_key;
            }
            if (idx == size_t(-1)) {
                if (!allow_unexpected) {
                    return error_t::unexpected_key;
                }
                eat_space();
                if (ptr >= end || *ptr != ':') {
                    return error_t::parse_error_key;
                }
                ptr++;
                MOLD_TRY(json_skip_value(ptr, end));
                continue;
            }
            eat_space();
            if (ptr >= end || *ptr != ':') {
                return error_t::parse_error_key;
            }
            ptr++;
            if (!stack.push_member(f, idx)) {
                return error_t::internal_logic_error;
            }
        } else {
            MOLD_TRY(stack.push_array_element(f));
        }
    }
    eat_space();
    if (ptr != end) {
        return error_t::trailing_data;
    }
    return error_t::ok;
}

/**
 * @brief Validate JSON against a compile-time generated schema and populate a struct directly.
 *
 * @tparam T Aggregate type to populate
 * @param instance Target struct instance
 * @param json_data JSON input; prefix is consumed on return
 * @param allow_unexpected If true, silently skip unknown keys
 * @return `error_t::ok` on success, or a specific error code
 */
template<is_aggregate T>
error_t json_to(T& instance, std::string_view& json_data, bool allow_unexpected = false)
{
    static constexpr auto node_count = json_count_nodes<T>();
    static constexpr auto type_depth = type_info_t<T>::depth();
    static constexpr auto stack_depth = (type_depth == 0 && node_count > 0) ? 1 : type_depth + 1;

    std::array<bool, node_count> found_flags = {};
    std::array<parse_frame_t, stack_depth> stack_buffer;

    return json_parse_impl(json_data, &type_info_t<T>::self(), &instance,
        found_flags, stack_buffer, allow_unexpected);
}

#endif

}

#endif
