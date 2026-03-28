#ifndef MOLD_JSON_WRITE_H
#define MOLD_JSON_WRITE_H

/**
 * @file
 * @brief Reflection-driven JSON serialization.
 */

#include "mold/refl/reflection.h"

namespace mold {

#if (MOLD_REFLECTION_JSON_ENABLED)

/**
 * @brief Internal implementation of JSON encoding using reflection.
 * 
 * @param w JSON sink for output
 * @param refl Reflection info for current value
 * @param base Pointer to the value
 * @return Error code
 */
MOLD_NOINLINE inline error_t json_write_impl(const json_sink_t& w, const reflection_t* refl, const void* base, 
    size_t indent, size_t depth)
{
    if (!refl) {
        return error_t::internal_logic_error;
    }
    auto newline_and_indent = [&] (size_t level) {
        if (indent) {
            MOLD_TRY(w.write_byte('\n'));
            MOLD_TRY(w.write_repeat(' ', indent * level));
        }
        return error_t::ok;
    };
    io_handler_t handler = refl->json_handler;

    // Optional unwrap when null is permitted
    if (json_type_has(refl->json_type, json_type_t::null)) {
        if (!handler ||
            !handler(io_type_t::visit_nullable, {.cptr = base, .slot_idx = 0}))
        {
            MOLD_TRY(w.write_literal("null"));
            return error_t::ok;
        }
    }
    // Object aggregates
    if (json_type_has(refl->json_type, json_type_t::object)) {

        MOLD_TRY(w.write_byte('{'));

        if (!refl->members.empty()) {

            bool first = true;
            for (size_t i = 0; i < refl->members.size(); ++i) {
                const reflection_t& m = refl->members[i];
                if (should_skip(m, base, &reflection_t::json_handler))
                    continue;
                if (!first) {
                    MOLD_TRY(w.write_byte(','));
                }
                MOLD_TRY(newline_and_indent(depth + 1));
                first = false;
                MOLD_TRY(w.write_escaped_string(m.name));
                MOLD_TRY(w.write_literal(indent ? ": " : ":"));
                MOLD_TRY(json_write_impl(w, &m, static_cast<const char*>(base) + m.offset, indent, depth + 1));
            }
            if (!first && indent) {
                MOLD_TRY(newline_and_indent(depth));
            }
        }
        MOLD_TRY(w.write_byte('}'));
        return error_t::ok;
    }
    // Array
    if (json_type_has(refl->json_type, json_type_t::array)) {

        MOLD_TRY(w.write_byte('['));
        size_t cnt = 0;

        if (handler) { // Homogeneous array
            if (refl->members.empty()) {
                MOLD_DEBUG_LOG("FAIL: Homogeneous array missing element reflection during encode.");
                return error_t::internal_logic_error;
            }
            io_data_t ed {.cptr = base, .previous_element = nullptr};
            for (;; ++cnt) {
                ed.previous_element = handler(io_type_t::emit_or_next, ed);
                if (!ed.previous_element) {
                    break;
                }
                if (cnt) {
                    MOLD_TRY(w.write_byte(','));
                }
                MOLD_TRY(newline_and_indent(depth + 1));
                MOLD_TRY(json_write_impl(w, &refl->members[0], ed.previous_element, indent, depth + 1));
            }
            if (refl->element_count != reflection_t::dynamic_count &&
                refl->element_count != cnt)
            {
                MOLD_DEBUG_LOG("FAIL: Encoded homogeneous array '%s' count mismatch. Expected %u, wrote %zu.",
                    refl->name.data(),
                    refl->element_count, cnt);
                return error_t::array_size_mismatch;
            }
        } else { // Heterogeneous tuple
            for (cnt = 0; cnt < refl->members.size(); ++cnt) {
                if (cnt) {
                    MOLD_TRY(w.write_byte(','));
                }
                const reflection_t& m = refl->members[cnt];
                MOLD_TRY(newline_and_indent(depth + 1));
                MOLD_TRY(json_write_impl(w, &m, static_cast<const char*>(base) + m.offset, indent, depth + 1));
            }
        }
        if (indent && cnt) {
            MOLD_TRY(newline_and_indent(depth));
        }
        MOLD_TRY(w.write_byte(']'));
        return error_t::ok;
    }
    MOLD_ASSERT(handler);
    // Primitives (numbers, strings, booleans)
    io_sink_t sink{&detail::json_sink_vtable, &w};
    handler(io_type_t::emit_or_next, {.cptr = base, .io_sink = &sink});
    return error_t::ok;
}

/**
 * @brief Basic encoding function to send output using a callback with context.
 * 
 * @tparam T Aggregate type
 * @param instance Instance of the aggregate
 * @param sink_fn Callback to output byte
 * @param user_ctx User context for the callback
 * @param indent Number of spaces per indent level (0 = compact)
 * @return Status of the encoding operation
 */
template<is_aggregate T>
error_t json_from(const T& instance, sink_cb_t sink_fn, void* user_ctx, size_t indent = 0)
{
    json_sink_t sink(sink_fn, user_ctx);
    error_t status = json_write_impl(sink, &type_info_t<T>::self(), &instance, indent, 0);
    return status;
}

/**
 * @brief Wrappper for `json_from` to write output to buffer.
 * 
 * @tparam T Aggregate type
 * @param instance Instance of the aggregate
 * @param buffer Buffer to write output to
 * @param indent Number of spaces per indent level (0 = compact)
 * @return Status of the encoding operation
 */
template<is_aggregate T>
error_t json_from(const T& instance, std::span<char>& buffer, size_t indent = 0)
{
    if (buffer.empty()) {
        return error_t::invalid_argument;
    }
    struct buffer_sink_state_t {
        char* pos;
        char* end;
    } sink_state {
        .pos = buffer.data(),
        .end = buffer.data() + buffer.size()
    };
    auto status = json_from(instance, [] (uint8_t b, void* p) {
        auto state = static_cast<buffer_sink_state_t*>(p);
        if (state->pos < state->end) {
            state->pos[0] = char(b);
            state->pos++;
            return true;
        }
        return false;
    }, &sink_state, indent);

    buffer = {buffer.data(), sink_state.pos};

    return status;
}

#if (MOLD_PRINT_ENABLED)

/**
 * @brief Wrappper for `json_from` to send output via `MOLD_PUTCHAR`.
 *
 * @tparam T Aggregate type
 * @param instance Instance of the aggregate
 * @param indent Number of spaces per indent level (0 = compact)
 * @return Status of the encoding operation
 */
template<is_aggregate T>
error_t json_from(const T& instance, size_t indent = 0)
{
    return json_from(instance, [] (uint8_t b, void*) {
        return MOLD_PUTCHAR(b) != EOF;
    }, nullptr, indent);
}

#endif

#endif

}

#endif
