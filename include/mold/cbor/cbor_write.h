#ifndef MOLD_CBOR_WRITE_H
#define MOLD_CBOR_WRITE_H

/**
 * @file
 * @brief Reflection-driven CBOR serialization.
 */

#include "mold/refl/reflection.h"
#include "mold/types/vector.h"

namespace mold {

#if (MOLD_REFLECTION_CBOR_ENABLED)

MOLD_NOINLINE inline error_t cbor_write_fields_impl(const cbor_sink_t& w, const reflection_t* refl, const void* base);

/**
 * @brief Internal implementation of CBOR encoding using reflection.
 *
 * @param w CBOR sink for output
 * @param refl Reflection info for current value
 * @param base Pointer to the value
 * @return Error code
 */
MOLD_NOINLINE inline error_t cbor_write_impl(const cbor_sink_t& w, const reflection_t* refl, const void* base)
{
    if (!refl) {
        return error_t::internal_logic_error;
    }
    io_handler_t handler = refl->cbor_handler;

    // Optional unwrap when null is permitted
    if (cbor_type_has(refl->cbor_type, cbor_type_t::null)) {
        if (!handler ||
            !handler(io_type_t::visit_nullable, {.cptr = base, .slot_idx = 0}))
        {
            MOLD_TRY(w.write_null());
            return error_t::ok;
        }
    }
    // Object aggregates (maps)
    if (cbor_type_has(refl->cbor_type, cbor_type_t::object)) {
        size_t member_count = count_non_skipped(refl, base, &reflection_t::cbor_handler);
        MOLD_TRY(w.write_map(member_count));
        MOLD_TRY(cbor_write_fields_impl(w, refl, base));
        return error_t::ok;
    }
    // Array
    if (cbor_type_has(refl->cbor_type, cbor_type_t::array)) {

        if (handler) { // Homogeneous array
            if (refl->members.empty()) {
                MOLD_DEBUG_LOG("FAIL: Homogeneous array missing element reflection during encode.");
                return error_t::internal_logic_error;
            }
            // Count elements first
            size_t cnt = 0;
            io_data_t count_data {.cptr = base, .previous_element = nullptr};
            while (true) {
                count_data.previous_element = handler(io_type_t::emit_or_next, count_data);
                if (!count_data.previous_element) {
                    break;
                }
                ++cnt;
            }
            // Write array header
            MOLD_TRY(w.write_array(cnt));
            // Write elements
            io_data_t ed {.cptr = base, .previous_element = nullptr};
            for (size_t i = 0; i < cnt; ++i) {
                ed.previous_element = handler(io_type_t::emit_or_next, ed);
                if (!ed.previous_element) {
                    break;
                }
                MOLD_TRY(cbor_write_impl(w, &refl->members[0], ed.previous_element));
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
            MOLD_TRY(w.write_array(refl->members.size()));
            
            for (size_t cnt = 0; cnt < refl->members.size(); ++cnt) {
                const reflection_t& m = refl->members[cnt];
                MOLD_TRY(cbor_write_impl(w, &m, static_cast<const char*>(base) + m.offset));
            }
        }
        return error_t::ok;
    }
    MOLD_ASSERT(handler);
    // Primitives (numbers, strings, booleans, bytes)
    io_sink_t sink{&detail::cbor_sink_vtable, &w};
    handler(io_type_t::emit_or_next, {.cptr = base, .io_sink = &sink});
    return error_t::ok;
}

/**
 * @brief Write only the key-value pairs of an object (no map header).
 *
 * Useful with indefinite-length maps for incremental encoding.
 *
 * @param w CBOR sink for output
 * @param refl Reflection info (must be an object type)
 * @param base Pointer to the struct
 * @return Error code
 */
MOLD_NOINLINE inline error_t cbor_write_fields_impl(const cbor_sink_t& w, const reflection_t* refl, const void* base)
{
    for (size_t i = 0; i < refl->members.size(); ++i) {
        const reflection_t& m = refl->members[i];
        if (should_skip(m, base, &reflection_t::cbor_handler))
            continue;
#if (MOLD_REFLECTION_CBOR_FIELD_KEYS)
        if (!m.cbor_key.empty()) {
            MOLD_TRY(w.write_bytes(m.cbor_key));
        } else
#endif
        {
#if (MOLD_REFLECTION_NAME_NEEDED)
            MOLD_TRY(w.write_text(m.name));
#endif
        }
        MOLD_TRY(cbor_write_impl(w, &m, static_cast<const char*>(base) + m.offset));
    }
    return error_t::ok;
}

/**
 * @brief Encode only the key-value pairs of a struct (no map header).
 *
 * Use with `cbor_sink_t::write_indef_map()` / `write_break()` for
 * incremental map composition from multiple structs.
 *
 * @tparam T Aggregate type
 * @param instance Instance of the aggregate
 * @param sink_fn Callback to output byte
 * @param user_ctx User context for the callback
 * @return Status of the encoding operation
 */
template<is_aggregate T>
error_t cbor_from_fields(const T& instance, sink_cb_t sink_fn, void* user_ctx)
{
    cbor_sink_t sink(sink_fn, user_ctx);
    return cbor_write_fields_impl(sink, &type_info_t<T>::self(), &instance);
}

/**
 * @brief Basic encoding function to send output using a callback with context.
 * 
 * @tparam T Aggregate type
 * @param instance Instance of the aggregate
 * @param sink_fn Callback to output byte
 * @param user_ctx User context for the callback
 * @return Status of the encoding operation
 */
template<is_aggregate T>
error_t cbor_from(const T& instance, sink_cb_t sink_fn, void* user_ctx)
{
    cbor_sink_t sink(sink_fn, user_ctx);
    error_t status = cbor_write_impl(sink, &type_info_t<T>::self(), &instance);
    return status;
}

/**
 * @brief Wrapper for `cbor_from` to write output to buffer.
 * 
 * @tparam T Aggregate type
 * @param instance Instance of the aggregate
 * @param buffer Buffer to write output to (updated to reflect written portion)
 * @return Status of the encoding operation
 */
template<is_aggregate T>
error_t cbor_from(const T& instance, std::span<uint8_t>& buffer)
{
    if (buffer.empty()) {
        return error_t::invalid_argument;
    }
    struct buffer_sink_state_t {
        uint8_t* pos;
        uint8_t* end;
    } sink_state {
        .pos = buffer.data(),
        .end = buffer.data() + buffer.size()
    };
    auto status = cbor_from(instance, [] (uint8_t b, void* p) {
        auto state = static_cast<buffer_sink_state_t*>(p);
        if (state->pos < state->end) {
            state->pos[0] = b;
            state->pos++;
            return true;
        }
        return false;
    }, &sink_state);

    buffer = {buffer.data(), sink_state.pos};

    return status;
}

/**
 * @brief Wrapper for `cbor_from` to write to a fixed-size array.
 * 
 * @tparam T Aggregate type
 * @tparam N Array size
 * @param instance Instance of the aggregate
 * @param buffer Array buffer to write to
 * @param written_size Output parameter for number of bytes written
 * @return Status of the encoding operation
 */
template<is_aggregate T, size_t N>
error_t cbor_from(const T& instance, std::array<uint8_t, N>& buffer, size_t& written_size)
{
    std::span<uint8_t> span = buffer;
    auto status = cbor_from(instance, span);
    written_size = span.size();
    return status;
}

/**
 * @brief Fixed-capacity CBOR encoder with built-in buffer storage.
 *
 * Combines `cbor_sink_t` primitives (`write_uint`, `write_text`,
 * `write_indef_map`, etc.) with `vector_t` buffer management,
 * concept-dispatched `write()` for primitives and enums, and
 * reflection-driven `write()` / `write_fields()` for aggregates.
 *
 * @tparam N Maximum buffer capacity in bytes
 */
template<size_t N>
struct cbor_writer_t : cbor_sink_t, vector_t<uint8_t, N> {

    cbor_writer_t() : cbor_sink_t(put_cb, this) {}

    /**
     * @brief View the encoded buffer as a byte span.
     *
     */
    operator std::span<const uint8_t>() const
    {
        return {this->data(), this->size()};
    }

    /**
     * @brief Write any value, dispatching by type.
     *
     * Accepts primitives (integers, floats, booleans, strings),
     * enumerations, and aggregate structs.
     *
     * @param val Value to encode
     * @return `error_t::ok` on success
     */
    error_t write(auto&& val)
    {
        using T = std::remove_cvref_t<decltype(val)>;
        if constexpr (is_boolean<T>) {
            return write_bool(val);
        } else if constexpr (is_unsigned<T>) {
            return write_uint(val);
        } else if constexpr (is_signed<T>) {
            return write_sint(val);
        } else if constexpr (is_floating<T>) {
            return write_float64(val);
        } else if constexpr (is_string<T>) {
            return write_text(val);
        } else if constexpr (is_enumeration<T>) {
            if constexpr (std::is_signed_v<decltype(+val)>) {
                return write_sint(+val);
            } else {
                return write_uint(+val);
            }
        } else if constexpr (is_aggregate<T>) {
            return cbor_write_impl(*this, &type_info_t<T>::self(), &val);
        } else {
            static_assert(!sizeof(T), "Unsupported type for write()");
        }
    }

    /**
     * @brief Encode only the key-value pairs of a struct (no map header).
     *
     * Use with `write_indef_map()` / `write_break()` for incremental
     * map composition from multiple structs.
     *
     * @tparam T Aggregate type
     * @param instance Instance to encode
     * @return `error_t::ok` on success
     */
    template<class T>
        requires (is_aggregate<T> && !is_string<T>)
    error_t write_fields(const T& instance)
    {
        return cbor_write_fields_impl(*this, &type_info_t<T>::self(), &instance);
    }

private:
    static bool put_cb(uint8_t b, void* ctx)
    {
        auto self = static_cast<cbor_writer_t*>(ctx);
        if (self->full())
            return false;
        self->push_back(b);
        return true;
    }
};

#endif

}

#endif
