#ifndef MOLD_REFL_PARSE_COMMON_H
#define MOLD_REFL_PARSE_COMMON_H

/**
 * @file
 * @brief Shared types and helpers for iterative JSON and CBOR parsers.
 */

#include <algorithm>
#include "mold/refl/reflection.h"

namespace mold {

/**
 * @brief Validator state for iterative parsers.
 *
 */
enum class parse_state_t : uint8_t {
    initial,    ///< Starting point for any value.
    post_item,  ///< Just finished a child item.
};

/**
 * @brief Stack frame for iterative parsing.
 *
 */
struct parse_frame_t {

    parse_frame_t() = default;
    parse_frame_t(const reflection_t* refl_ptr, void* mptr, size_t flags_off = 0) :
        refl(refl_ptr),
        base(mptr),
        flags_offset(flags_off)
    {}

    const reflection_t* refl = nullptr;             ///< Reflection info for the C++ type this frame represents.
    void* base = nullptr;                           ///< Base pointer of the struct instance.
    size_t member_idx = 0;                          ///< Current index being processed.
    size_t container_size = 0;                      ///< Expected number of elements (CBOR only, ignored by JSON).
    size_t saved_flags_offset = 0;                  ///< Saved offset for flag buffer management.
    size_t flags_offset = 0;                        ///< Offset into global flags buffer.
    parse_state_t state = parse_state_t::initial;   ///< Current state.
    bool is_indefinite = false;                     ///< True if indefinite-length container (CBOR only).
};

/**
 * @brief Metadata describing the current array frame.
 *
 */
struct array_meta_t {
    size_t limit = 0;               ///< Expected number of elements.
    const char* label = nullptr;    ///< Label for the array type.
    bool is_fixed = false;          ///< True if the array is fixed size.
};

/**
 * @brief Shared stack management for iterative parsers.
 *
 * @tparam FormatPolicy Must provide:
 *   - `type_t` - type mask enum
 *   - `type_has(type_t, type_t) -> bool` - bitmask test
 *   - `get_type(reflection_t) -> type_t` - extract type from reflection
 *   - `get_handler(reflection_t) -> io_handler_t` - extract handler
 *   - `object_type` - the object type mask value
 *   - `array_type` - the array type mask value
 */
template<class FormatPolicy>
struct parse_stack_t {

    using type_t = typename FormatPolicy::type_t;

    /**
     * @brief Pop the current frame from the stack.
     *
     * @return true on success, false on stack underflow
     */
    bool pop()
    {
        if (top) {
            top--;
            if (FormatPolicy::type_has(FormatPolicy::get_type(*frames[top].refl), FormatPolicy::object_type)) {
                next_flags_offset = frames[top].saved_flags_offset;
            }
            return true;
        }
        MOLD_DEBUG_LOG("FAIL: Stack underflow.");
        return false;
    }

    /**
     * @brief Push a new frame onto the stack for the given reflection node.
     *
     * @param refl Reflection info for the type to parse
     * @param base Base pointer of the struct instance
     * @return true on success, false on overflow or invalid input
     */
    bool push(const reflection_t* refl, void* base)
    {
        if (top >= frames.size()) {
            MOLD_DEBUG_LOG("FAIL: Stack overflow. Depth > %zu", frames.size());
            return false;
        }
        if (!refl) {
            MOLD_DEBUG_LOG("FAIL: Pushing frame with null reflection_t ptr.");
            return false;
        }
        size_t local_flags_off = 0;
        size_t saved_offset = 0;

        if (FormatPolicy::type_has(FormatPolicy::get_type(*refl), FormatPolicy::object_type)) {
            saved_offset = next_flags_offset;
            size_t n = refl->members.size();
            if (next_flags_offset + n > flags.size() && n > 0) {
                MOLD_DEBUG_LOG("FAIL: Flags overflow.");
                return false;
            }
            local_flags_off = next_flags_offset;
            if (n > 0) {
                std::fill(flags.begin() + local_flags_off,
                          flags.begin() + local_flags_off + n, false);
                next_flags_offset += n;
            }
        }
        frames[top] = parse_frame_t(refl, base, local_flags_off);
        frames[top].saved_flags_offset = saved_offset;

        if (FormatPolicy::type_has(FormatPolicy::get_type(*refl), FormatPolicy::array_type)) {
            if (refl->members.empty() && FormatPolicy::get_handler(*refl)) {
                MOLD_DEBUG_LOG("FAIL: Array frame pushed but element reflection is empty.");
                return false;
            }
        }
        top++;
        return true;
    }

    /**
     * @brief Build metadata for the current array frame.
     *
     * @param refl Reflection info for the array type
     * @return Array metadata with element limit and fixed-size flag
     */
    static array_meta_t describe_array(const reflection_t* refl)
    {
        if (FormatPolicy::get_handler(*refl)) {
            return {
                .limit = refl->element_count,
                .label = "Homogeneous array",
                .is_fixed = (refl->element_count != reflection_t::dynamic_count),
            };
        } else {
            return {
                .limit = refl->members.size(),
                .label = "Tuple array",
                .is_fixed = true,
            };
        }
    }

    /**
     * @brief Get the current (topmost) stack frame.
     *
     * @return Reference to the current frame
     */
    parse_frame_t& current()
    { 
        return frames[top - 1];
    }

    /**
     * @brief Check that all required fields were found for the current object frame.
     *
     * @param frame Frame to validate
     * @return error_t::ok if all required fields present, error_t::missing_key otherwise
     */
    error_t check_required(const parse_frame_t& frame)
    {
        const auto* refl = frame.refl;
        for (size_t i = 0; i < refl->members.size(); ++i) {
            if (!flags[frame.flags_offset + i] &&
                !FormatPolicy::type_has(FormatPolicy::get_type(refl->members[i]), FormatPolicy::null_type))
            {
                MOLD_DEBUG_LOG("FAIL: Missing required key: '%s'", refl->members[i].name.data());
                return error_t::missing_key;
            }
        }
        return error_t::ok;
    }

    /**
     * @brief Push a child frame for an array element (homogeneous or tuple).
     *
     * @param frame Current array frame
     * @return error_t::ok on success, or error code on failure
     */
    error_t push_array_element(parse_frame_t& frame)
    {
        const auto* refl = frame.refl;
        auto handler = FormatPolicy::get_handler(*refl);
        if (handler) {
            if (!push(&refl->members[0], nullptr)) {
                return error_t::internal_logic_error;
            }
            io_data_t evt { .mptr = frame.base, .slot_idx = frame.member_idx };
            auto slot = handler(io_type_t::prepare_value, evt);
            if (!slot) {
                MOLD_DEBUG_LOG("FAIL: Container prepare_value returned null for '%s' element %zu.",
                    refl->name.data(), frame.member_idx);
                pop();
                return error_t::handler_failure;
            }
            current().base = slot;
        } else {
            if (frame.member_idx >= refl->members.size()) {
                MOLD_DEBUG_LOG("FAIL: Tuple '%s' exceeded expected size (%zu).",
                    refl->name.data(), refl->members.size());
                return error_t::array_size_mismatch;
            }
            const auto& child = refl->members[frame.member_idx];
            if (!push(&child, static_cast<char*>(frame.base) + child.offset)) {
                return error_t::internal_logic_error;
            }
        }
        return error_t::ok;
    }

    /**
     * @brief Handle nullable unwrap for container opening.
     *
     * @param frame Current frame whose base pointer may be updated
     * @return true if parsing should proceed, false on handler failure
     */
    bool visit_nullable(parse_frame_t& frame)
    {
        auto handler = FormatPolicy::get_handler(*frame.refl);
        if (!FormatPolicy::type_has(FormatPolicy::get_type(*frame.refl), FormatPolicy::null_type) || !handler) {
            return true;
        }
        io_data_t evt { .mptr = frame.base, .slot_idx = 1 };
        auto prepared = handler(io_type_t::visit_nullable, evt);
        if (!prepared) {
            MOLD_DEBUG_LOG("FAIL: visit_nullable returned null for '%s'.", frame.refl->name.data());
            return false;
        }
        frame.base = prepared;
        return true;
    }

#if (MOLD_REFLECTION_NAME_NEEDED)

    /**
     * @brief Find schema member by key name and mark as found.
     *
     * @param frame Current object frame
     * @param key Key name to look up
     * @return Member index, size_t(-1) if not found, size_t(-2) if duplicate
     */
    size_t find_member(const parse_frame_t& frame, std::string_view key)
    {
        const auto* refl = frame.refl;
        for (size_t i = 0; i < refl->members.size(); ++i) {
            if (refl->members[i].name == key) {
                if (flags[frame.flags_offset + i]) {
                    MOLD_DEBUG_LOG("FAIL: Duplicate key '%.*s'.", int(key.size()), key.data());
                    return size_t(-2); // duplicate
                }
                flags[frame.flags_offset + i] = true;
                return i;
            }
        }
        return size_t(-1); // not found
    }

#endif

#if (MOLD_REFLECTION_CBOR_FIELD_KEYS)

    /**
     * @brief Find schema member by raw CBOR key bytes and mark as found.
     *
     * Compares the raw wire bytes of the incoming key against the pre-encoded
     * CBOR key stored in each member's reflection data.
     *
     * @param frame Current object frame
     * @param key_bytes Raw CBOR bytes of the incoming key
     * @return Member index, size_t(-1) if not found, size_t(-2) if duplicate
     */
    size_t find_member_by_cbor_key(const parse_frame_t& frame, std::span<const uint8_t> key_bytes)
    {
        const auto* refl = frame.refl;
        for (size_t i = 0; i < refl->members.size(); ++i) {
            const auto& mk = refl->members[i].cbor_key;
            if (mk.empty()) {
                continue;
            }
            if (mk.size() == key_bytes.size() &&
                std::equal(mk.begin(), mk.end(), key_bytes.begin()))
            {
                if (flags[frame.flags_offset + i]) {
                    MOLD_DEBUG_LOG("FAIL: Duplicate CBOR key.");
                    return size_t(-2);
                }
                flags[frame.flags_offset + i] = true;
                return i;
            }
        }
        return size_t(-1);
    }

#endif

    /**
     * @brief Push a child frame for an object member by schema index.
     *
     * @param frame Current object frame
     * @param idx Member index within the reflection schema
     * @return true on success, false on stack overflow
     */
    bool push_member(parse_frame_t& frame, size_t idx)
    {
        const auto& child = frame.refl->members[idx];
        return push(&child, static_cast<char*>(frame.base) + child.offset);
    }

    std::span<parse_frame_t> frames;    ///< Pre-allocated stack frame buffer.
    std::span<bool> flags;              ///< Pre-allocated found-flags buffer for required field tracking.
    size_t top = 0;                     ///< Current stack depth (index of next free slot).
    size_t next_flags_offset = 0;       ///< Next available offset into the flags buffer.
};

}

#endif
