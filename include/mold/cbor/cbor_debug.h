#ifndef MOLD_CBOR_DEBUG_H
#define MOLD_CBOR_DEBUG_H

/**
 * @file
 * @brief Debug utilities for printing CBOR schemas and parsed data.
 */

#include "mold/refl/reflection.h"

namespace mold {

#if (MOLD_REFLECTION_CBOR_ENABLED)

/**
 * @brief Print static schema definition (types and structure) for CBOR.
 *
 * @param refl Reflection info for the current node
 * @param indent Spaces per indentation level (0 = compact, no newlines)
 * @param depth Current indentation depth level
 */
MOLD_NOINLINE inline void cbor_print_schema(const reflection_t& refl, int indent, int depth = 0)
{
    // Print all types in mask: primitives first, then structural types
    bool printed_any = false;
    auto print_sep = [&] {
        if (printed_any) {
            MOLD_PRINT(" | ");
        }
        printed_any = true;
    };
    // Primitives
    if (cbor_type_has(refl.cbor_type, cbor_type_t::null))       { print_sep(); MOLD_PRINT("null"); }
    if (cbor_type_has(refl.cbor_type, cbor_type_t::boolean))    { print_sep(); MOLD_PRINT("bool"); }
    if (cbor_type_has(refl.cbor_type, cbor_type_t::integer))    { print_sep(); MOLD_PRINT("int"); }
    if (cbor_type_has(refl.cbor_type, cbor_type_t::floating))   { print_sep(); MOLD_PRINT("float"); }
    if (cbor_type_has(refl.cbor_type, cbor_type_t::string))     { print_sep(); MOLD_PRINT("string"); }
    if (cbor_type_has(refl.cbor_type, cbor_type_t::bytes))      { print_sep(); MOLD_PRINT("bytes"); }
    // Object (map)
    if (cbor_type_has(refl.cbor_type, cbor_type_t::object)) {
        print_sep();
        MOLD_PRINT("{");
        if (indent) {
            MOLD_PRINT("\n");
        }
        for (size_t i = 0; i < refl.members.size(); ++i) {
            const auto& member_refl = refl.members[i];
            print_indent(indent, depth + 1);
            MOLD_PRINT("\"%.*s\":", int(member_refl.name.size()), member_refl.name.data());
            if (indent) {
                MOLD_PRINT(" ");
            }
            cbor_print_schema(member_refl, indent, depth + 1);
            if (i < refl.members.size() - 1) {
                MOLD_PRINT(",");
            }
            if (indent) {
                MOLD_PRINT("\n");
            }
        }
        print_indent(indent, depth);
        MOLD_PRINT("}");
    }
    // Array
    if (cbor_type_has(refl.cbor_type, cbor_type_t::array)) {
        print_sep();
        MOLD_PRINT("[");
        if (indent) {
            MOLD_PRINT("\n");
        }
        if (refl.cbor_handler) { // Homogeneous array
            print_indent(indent, depth + 1);
            if (!refl.members.empty()) {
                cbor_print_schema(refl.members[0], indent, depth + 1);
            } else {
                MOLD_PRINT("<missing>");
            }
            if (refl.element_count != reflection_t::dynamic_count) {
                MOLD_PRINT(" * %u", refl.element_count);
            } else {
                MOLD_PRINT(" * ...");
            }
            if (indent) {
                MOLD_PRINT("\n");
            }
        } else { // Heterogeneous array (tuple-like)
            for (size_t i = 0; i < refl.members.size(); ++i) {
                const auto& element_refl = refl.members[i];
                print_indent(indent, depth + 1);
                cbor_print_schema(element_refl, indent, depth + 1);
                if (i < refl.members.size() - 1) {
                    MOLD_PRINT(",");
                }
                if (indent) {
                    MOLD_PRINT("\n");
                }
            }
        }
        print_indent(indent, depth);
        MOLD_PRINT("]");
    }
    if (!printed_any) {
        MOLD_PRINT("any");
    }
}

#endif

/**
 * @brief Serialize CBOR bytes to diagnostic notation.
 * 
 * @param w Sink to write to
 * @param ptr Pointer to the current position in the data
 * @param end Pointer to the end of the data
 * @param indent Indentation level
 * @param depth Depth of the current node
 * @return error_t::ok on success
 */
MOLD_NOINLINE inline error_t cbor_pretty_impl(const sink_t& w, cbor_ptr_t& ptr, cbor_ptr_t end, int indent, int depth, bool skip_leading_indent = false)
{
    if (ptr >= end) {
        return error_t::unexpected_eof;
    }
    // Print leading indentation (skipped for map values that follow ": ")
    if (!skip_leading_indent) {
        MOLD_TRY(w.write_repeat(' ', depth));
    }

    uint8_t header = *ptr;
    uint8_t mt = header & 0xe0;
    uint8_t ai = header & 0x1f;
    ptr++;
    // Decode additional info
    uint64_t val = ai;
    if (ai >= +cbor_ai_t::_1 && 
        ai <= +cbor_ai_t::_8) 
    {
        size_t len = 1 << (ai - +cbor_ai_t::_1);
        if (ptr + len > end) {
            return error_t::unexpected_eof;
        }
        val = 0;
        for (size_t i = 0; i < len; ++i)
            val = (val << 8) | *ptr++;
    } else if (ai > 27 && ai < 31) {
        MOLD_TRY(w.write_literal("<reserved-ai>"));
        return error_t::reserved_ai;
    }

    auto handle_floating = [&] (double val) {
        if (std::isnan(val)) {
            MOLD_TRY(w.write_literal("NaN"));
        } else if (std::isinf(val)) {
            MOLD_TRY(w.write_literal(val > 0 ? "Infinity" : "-Infinity"));
        } else if (val == 0 && std::signbit(val)) {
            MOLD_TRY(w.write_literal("-0.0"));
        } else {
            MOLD_TRY(w.write_fmt_g(val));
        }
        return error_t::ok;
    };

    switch (mt) 
    {
    case +cbor_mt_t::uint:
        MOLD_TRY(w.write_fmt_llu(val));
    break;

    case +cbor_mt_t::nint:
        MOLD_TRY(w.write_fmt_lld(~val));
    break;

    case +cbor_mt_t::data:
        if (ai == +cbor_ai_t::_indef) {
            MOLD_TRY(w.write_literal("(_ "));
            bool first = true;
            while (ptr < end && *ptr != 0xff) {
                if (!first) {
                    MOLD_TRY(w.write_literal(", "));
                }
                first = false;
                MOLD_TRY(cbor_pretty_impl(w, ptr, end, 0, 0));
            }
            if (ptr >= end) {
                return error_t::unexpected_eof;
            }
            ptr++; // skip break
            MOLD_TRY(w.write_byte(')'));
        } else {
            if (ptr + val > end) {
                return error_t::unexpected_eof;
            }
            MOLD_TRY(w.write_literal("h'"));
            for (uint64_t i = 0; i < val; ++i) {
                MOLD_TRY(w.write_vfmt("%02x", ptr[i]));
            }
            MOLD_TRY(w.write_byte('\''));
            ptr += val;
        }
        break;

    case +cbor_mt_t::text:
        if (ai == +cbor_ai_t::_indef) {
            MOLD_TRY(w.write_literal("(_ "));
            bool first = true;
            while (ptr < end && *ptr != 0xff) {
                if (!first) {
                    MOLD_TRY(w.write_literal(", "));
                }
                first = false;
                MOLD_TRY(cbor_pretty_impl(w, ptr, end, 0, 0));
            }
            if (ptr >= end) {
                return error_t::unexpected_eof;
            }
            ptr++; // skip break
            MOLD_TRY(w.write_byte(')'));
        } else {
            if (ptr + val > end) {
                return error_t::unexpected_eof;
            }
            MOLD_TRY(w.write_byte('"'));
            for (uint64_t i = 0; i < val; ++i) {
                char c = ptr[i];
                if (c == '"') {
                    MOLD_TRY(w.write_literal("\\\""));
                } else if (c == '\\') {
                    MOLD_TRY(w.write_literal("\\\\"));
                } else if (std::isprint(c)) {
                    MOLD_TRY(w.write_byte(uint8_t(c)));
                } else {
                    MOLD_TRY(w.write_vfmt("\\x%02x", (unsigned char) c));
                }
            }
            MOLD_TRY(w.write_byte('"'));
            ptr += val;
        }
        break;

    case +cbor_mt_t::arr: {
        MOLD_TRY(w.write_byte('['));
        bool is_indef = (ai == +cbor_ai_t::_indef);
        if (is_indef) {
            MOLD_TRY(w.write_literal("_ "));
            val = SIZE_MAX;
        }
        bool first = true;
        for (uint64_t i = 0; i < val; ++i) {
            if (is_indef && ptr < end && *ptr == 0xff) {
                ptr++;
                break;
            }
            if (!first) {
                MOLD_TRY(w.write_byte(','));
            }
            first = false;
            if (indent) {
                MOLD_TRY(w.write_byte('\n'));
            }
            MOLD_TRY(cbor_pretty_impl(w, ptr, end, indent, depth + indent));
        }
        if (indent && !first) {
            MOLD_TRY(w.write_byte('\n'));
            MOLD_TRY(w.write_repeat(' ', depth));
        }
        MOLD_TRY(w.write_byte(']'));
        break;
    }

    case +cbor_mt_t::map: {
        MOLD_TRY(w.write_byte('{'));
        bool is_indef = (ai == +cbor_ai_t::_indef);
        if (is_indef) {
            MOLD_TRY(w.write_literal("_ "));
            val = SIZE_MAX;
        }
        bool first = true;
        for (uint64_t i = 0; i < val; ++i) {
            if (is_indef && ptr < end && *ptr == 0xff) {
                ptr++;
                break;
            }
            if (!first) {
                MOLD_TRY(w.write_byte(','));
            }
            first = false;
            if (indent) {
                MOLD_TRY(w.write_byte('\n'));
            }
            MOLD_TRY(cbor_pretty_impl(w, ptr, end, indent, depth + indent));
            MOLD_TRY(w.write_literal(indent ? ": " : ":"));
            MOLD_TRY(cbor_pretty_impl(w, ptr, end, indent, depth + indent, true));
        }
        if (indent && !first) {
            MOLD_TRY(w.write_byte('\n'));
            MOLD_TRY(w.write_repeat(' ', depth));
        }
        MOLD_TRY(w.write_byte('}'));
        break;
    }

    case +cbor_mt_t::tag:
        MOLD_TRY(w.write_fmt_llu(val));
        MOLD_TRY(w.write_byte('('));
        MOLD_TRY(cbor_pretty_impl(w, ptr, end, 0, 0));
        MOLD_TRY(w.write_byte(')'));
    break;

    case +cbor_mt_t::simple:
        switch (ai) 
        {
        case +cbor_simple_t::bool_false:
            MOLD_TRY(w.write_literal("false"));
        break;
        case +cbor_simple_t::bool_true:
            MOLD_TRY(w.write_literal("true"));
        break;
        case +cbor_simple_t::null:
            MOLD_TRY(w.write_literal("null"));
        break;
        case +cbor_simple_t::undefined:
            MOLD_TRY(w.write_literal("undefined"));
        break;
        case +cbor_simple_t::float16:
            MOLD_TRY(handle_floating(uint_to_float(uint16_t(val))));
        break;
        case +cbor_simple_t::float32:
            MOLD_TRY(handle_floating(uint_to_float(uint32_t(val))));
        break;
        case +cbor_simple_t::float64:
            MOLD_TRY(handle_floating(uint_to_float(uint64_t(val))));
        break;
        default:
            if (ai < 24 || ai > 31) {
                MOLD_TRY(w.write_literal("simple("));
                MOLD_TRY(w.write_fmt_llu(val));
                MOLD_TRY(w.write_byte(')'));
            } else {
                MOLD_TRY(w.write_literal("<illegal-simple>"));
            }
        break;
        }
    break;

    default:
        MOLD_TRY(w.write_literal("<unknown-mt>"));
        return error_t::internal_logic_error;
    }
    return error_t::ok;
}

/**
 * @brief Serialize CBOR bytes to diagnostic notation using callback.
 *
 * @param cbor CBOR byte data
 * @param sink_fn Callback to output bytes
 * @param user_ctx User context for the callback
 * @param indent Spaces per indentation level (0 for compact)
 * @return error_t::ok on success
 */
inline error_t cbor_pretty(std::span<const uint8_t> cbor, sink_cb_t sink_fn, void* user_ctx, int indent = 0)
{
    sink_t w(sink_fn, user_ctx);
    cbor_ptr_t ptr = cbor.data();
    cbor_ptr_t end = cbor.data() + cbor.size();
    while (ptr < end) {
        MOLD_TRY(cbor_pretty_impl(w, ptr, end, indent, 0));
        if (ptr < end) {
            MOLD_TRY(w.write_byte('\n'));
        }
    }
    return error_t::ok;
}

/**
 * @brief Serialize CBOR bytes to diagnostic notation to buffer.
 *
 * @param cbor CBOR byte data
 * @param buffer Output buffer (updated to reflect written portion)
 * @param indent Spaces per indentation level (0 for compact)
 * @return error_t::ok on success
 */
inline error_t cbor_pretty(std::span<const uint8_t> cbor, std::span<char>& buffer, int indent = 0)
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
    auto status = cbor_pretty(cbor, [] (uint8_t b, void* p) {
        auto state = static_cast<buffer_sink_state_t*>(p);
        if (state->pos < state->end) {
            state->pos[0] = b;
            state->pos++;
            return true;
        }
        return false;
    }, &sink_state, indent);

    if (status == error_t::ok) {
        size_t written = sink_state.pos - buffer.data();
        buffer = buffer.subspan(0, written);
    }
    return status;
}

#if (MOLD_PRINT_ENABLED)

/**
 * @brief Wrapper for `cbor_pretty` to send output via `MOLD_PUTCHAR`.
 *
 * @param cbor CBOR byte data
 * @param indent Spaces per indentation level (0 for compact)
 * @return error_t::ok on success
 */
inline error_t cbor_pretty(std::span<const uint8_t> cbor, int indent = 0)
{
    return cbor_pretty(cbor, [] (uint8_t b, void*) {
        return MOLD_PUTCHAR(b) != EOF;
    }, nullptr, indent);
}

#endif

}

#endif
