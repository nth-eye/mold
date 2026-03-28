#ifndef MOLD_MSGPACK_DEBUG_H
#define MOLD_MSGPACK_DEBUG_H

/**
 * @file
 * @brief Debug utilities for printing MessagePack schemas and parsed data.
 */

#include "mold/refl/reflection.h"

namespace mold {

#if (MOLD_REFLECTION_MSGPACK_ENABLED)

/**
 * @brief Print static schema definition (types and structure) for MessagePack.
 *
 * @param refl Reflection info for the current node
 * @param indent Spaces per indentation level (0 = compact, no newlines)
 * @param depth Current indentation depth level
 */
inline void msgpack_print_schema(const reflection_t& refl, int indent, int depth = 0)
{
    bool printed_any = false;
    auto print_sep = [&] {
        if (printed_any) {
            MOLD_PRINT(" | ");
        }
        printed_any = true;
    };
    // Primitives
    if (msgpack_type_has(refl.msgpack_type, msgpack_type_t::null))       { print_sep(); MOLD_PRINT("null"); }
    if (msgpack_type_has(refl.msgpack_type, msgpack_type_t::boolean))    { print_sep(); MOLD_PRINT("bool"); }
    if (msgpack_type_has(refl.msgpack_type, msgpack_type_t::integer))    { print_sep(); MOLD_PRINT("int"); }
    if (msgpack_type_has(refl.msgpack_type, msgpack_type_t::floating))   { print_sep(); MOLD_PRINT("float"); }
    if (msgpack_type_has(refl.msgpack_type, msgpack_type_t::string))     { print_sep(); MOLD_PRINT("string"); }
    if (msgpack_type_has(refl.msgpack_type, msgpack_type_t::bytes))      { print_sep(); MOLD_PRINT("bytes"); }
    // Object (map)
    if (msgpack_type_has(refl.msgpack_type, msgpack_type_t::object)) {
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
            msgpack_print_schema(member_refl, indent, depth + 1);
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
    if (msgpack_type_has(refl.msgpack_type, msgpack_type_t::array)) {
        print_sep();
        MOLD_PRINT("[");
        if (indent) {
            MOLD_PRINT("\n");
        }
        if (refl.msgpack_handler) { // Homogeneous array
            print_indent(indent, depth + 1);
            if (!refl.members.empty()) {
                msgpack_print_schema(refl.members[0], indent, depth + 1);
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
                msgpack_print_schema(element_refl, indent, depth + 1);
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

#if (MOLD_REFLECTION_MSGPACK_ENABLED)

/**
 * @brief Serialize MessagePack bytes to diagnostic notation.
 *
 * @param w Sink to write to
 * @param ptr Pointer to the current position in the data
 * @param end Pointer to the end of the data
 * @param indent Indentation level
 * @param depth Depth of the current node
 * @param skip_leading_indent If true, skip leading indentation
 * @return error_t::ok on success
 */
MOLD_NOINLINE inline error_t msgpack_pretty_impl(const sink_t& w, msgpack_ptr_t& ptr, msgpack_ptr_t end, int indent, int depth, bool skip_leading_indent = false)
{
    if (ptr >= end) {
        return error_t::unexpected_eof;
    }
    if (!skip_leading_indent) {
        MOLD_TRY(w.write_repeat(' ', depth));
    }
    uint8_t b = *ptr++;

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

    auto handle_string = [&] (size_t len) {
        if (ptr + len > end) {
            return error_t::unexpected_eof;
        }
        MOLD_TRY(w.write_byte('"'));
        for (size_t i = 0; i < len; ++i) {
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
        ptr += len;
        return error_t::ok;
    };

    auto handle_bin = [&] (size_t len) {
        if (ptr + len > end) {
            return error_t::unexpected_eof;
        }
        MOLD_TRY(w.write_literal("h'"));
        for (size_t i = 0; i < len; ++i) {
            MOLD_TRY(w.write_vfmt("%02x", ptr[i]));
        }
        MOLD_TRY(w.write_byte('\''));
        ptr += len;
        return error_t::ok;
    };

    auto handle_array = [&] (size_t n) {
        MOLD_TRY(w.write_byte('['));
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) {
                MOLD_TRY(w.write_byte(','));
            }
            if (indent) {
                MOLD_TRY(w.write_byte('\n'));
            }
            MOLD_TRY(msgpack_pretty_impl(w, ptr, end, indent, depth + indent));
        }
        if (indent && n > 0) {
            MOLD_TRY(w.write_byte('\n'));
            MOLD_TRY(w.write_repeat(' ', depth));
        }
        MOLD_TRY(w.write_byte(']'));
        return error_t::ok;
    };

    auto handle_map = [&] (size_t n) {
        MOLD_TRY(w.write_byte('{'));
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) {
                MOLD_TRY(w.write_byte(','));
            }
            if (indent) {
                MOLD_TRY(w.write_byte('\n'));
            }
            MOLD_TRY(msgpack_pretty_impl(w, ptr, end, indent, depth + indent));
            MOLD_TRY(w.write_literal(indent ? ": " : ":"));
            MOLD_TRY(msgpack_pretty_impl(w, ptr, end, indent, depth + indent, true));
        }
        if (indent && n > 0) {
            MOLD_TRY(w.write_byte('\n'));
            MOLD_TRY(w.write_repeat(' ', depth));
        }
        MOLD_TRY(w.write_byte('}'));
        return error_t::ok;
    };

    auto handle_ext = [&] (size_t len) {
        if (ptr + 1 + len > end) {
            return error_t::unexpected_eof;
        }
        int8_t type = int8_t(*ptr++);
        MOLD_TRY(w.write_vfmt("ext(%d, h'", int(type)));
        for (size_t i = 0; i < len; ++i) {
            MOLD_TRY(w.write_vfmt("%02x", ptr[i]));
        }
        MOLD_TRY(w.write_literal("')"));
        ptr += len;
        return error_t::ok;
    };

    // Positive fixint: 0x00-0x7f
    if (b <= 0x7f) {
        return w.write_fmt_llu(b);
    }
    // Negative fixint: 0xe0-0xff
    if (b >= 0xe0) {
        return w.write_fmt_lld(int8_t(b));
    }
    // fixmap: 0x80-0x8f
    if ((b & 0xf0) == 0x80) {
        return handle_map(b & 0x0f);
    }
    // fixarray: 0x90-0x9f
    if ((b & 0xf0) == 0x90) {
        return handle_array(b & 0x0f);
    }
    // fixstr: 0xa0-0xbf
    if ((b & 0xe0) == 0xa0) {
        return handle_string(b & 0x1f);
    }

    switch (b) 
    {
    // nil
    case 0xc0:
        return w.write_literal("null");
    // (unused)
    case 0xc1:
        return w.write_literal("<unused-0xc1>");
    // false
    case 0xc2:
        return w.write_literal("false");
    // true
    case 0xc3:
        return w.write_literal("true");

    // bin8
    case 0xc4: {
        if (ptr >= end) { return error_t::unexpected_eof; }
        size_t len = *ptr++;
        return handle_bin(len);
    }
    // bin16
    case 0xc5: {
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        size_t len = msgpack_getbe<uint16_t>(ptr); ptr += 2;
        return handle_bin(len);
    }
    // bin32
    case 0xc6: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        size_t len = msgpack_getbe<uint32_t>(ptr); ptr += 4;
        return handle_bin(len);
    }

    // ext8
    case 0xc7: {
        if (ptr >= end) { return error_t::unexpected_eof; }
        size_t len = *ptr++;
        return handle_ext(len);
    }
    // ext16
    case 0xc8: {
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        size_t len = msgpack_getbe<uint16_t>(ptr); ptr += 2;
        return handle_ext(len);
    }
    // ext32
    case 0xc9: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        size_t len = msgpack_getbe<uint32_t>(ptr); ptr += 4;
        return handle_ext(len);
    }

    // float32
    case 0xca: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        auto val = uint_to_float(msgpack_getbe<uint32_t>(ptr)); ptr += 4;
        return handle_floating(val);
    }
    // float64
    case 0xcb: {
        if (ptr + 8 > end) { return error_t::unexpected_eof; }
        auto val = uint_to_float(msgpack_getbe<uint64_t>(ptr)); ptr += 8;
        return handle_floating(val);
    }

    // uint8
    case 0xcc: {
        if (ptr >= end) { return error_t::unexpected_eof; }
        return w.write_fmt_llu(*ptr++);
    }
    // uint16
    case 0xcd: {
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        auto val = msgpack_getbe<uint16_t>(ptr); ptr += 2;
        return w.write_fmt_llu(val);
    }
    // uint32
    case 0xce: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        auto val = msgpack_getbe<uint32_t>(ptr); ptr += 4;
        return w.write_fmt_llu(val);
    }
    // uint64
    case 0xcf: {
        if (ptr + 8 > end) { return error_t::unexpected_eof; }
        auto val = msgpack_getbe<uint64_t>(ptr); ptr += 8;
        return w.write_fmt_llu(val);
    }

    // int8
    case 0xd0: {
        if (ptr >= end) { return error_t::unexpected_eof; }
        return w.write_fmt_lld(int8_t(*ptr++));
    }
    // int16
    case 0xd1: {
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        auto val = int16_t(msgpack_getbe<uint16_t>(ptr)); ptr += 2;
        return w.write_fmt_lld(val);
    }
    // int32
    case 0xd2: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        auto val = int32_t(msgpack_getbe<uint32_t>(ptr)); ptr += 4;
        return w.write_fmt_lld(val);
    }
    // int64
    case 0xd3: {
        if (ptr + 8 > end) { return error_t::unexpected_eof; }
        auto val = int64_t(msgpack_getbe<uint64_t>(ptr)); ptr += 8;
        return w.write_fmt_lld(val);
    }

    // fixext1
    case 0xd4: return handle_ext(1);
    // fixext2
    case 0xd5: return handle_ext(2);
    // fixext4
    case 0xd6: return handle_ext(4);
    // fixext8
    case 0xd7: return handle_ext(8);
    // fixext16
    case 0xd8: return handle_ext(16);

    // str8
    case 0xd9: {
        if (ptr >= end) { return error_t::unexpected_eof; }
        size_t len = *ptr++;
        return handle_string(len);
    }
    // str16
    case 0xda: {
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        size_t len = msgpack_getbe<uint16_t>(ptr); ptr += 2;
        return handle_string(len);
    }
    // str32
    case 0xdb: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        size_t len = msgpack_getbe<uint32_t>(ptr); ptr += 4;
        return handle_string(len);
    }

    // array16
    case 0xdc: {
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        size_t n = msgpack_getbe<uint16_t>(ptr); ptr += 2;
        return handle_array(n);
    }
    // array32
    case 0xdd: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        size_t n = msgpack_getbe<uint32_t>(ptr); ptr += 4;
        return handle_array(n);
    }

    // map16
    case 0xde: {
        if (ptr + 2 > end) { return error_t::unexpected_eof; }
        size_t n = msgpack_getbe<uint16_t>(ptr); ptr += 2;
        return handle_map(n);
    }
    // map32
    case 0xdf: {
        if (ptr + 4 > end) { return error_t::unexpected_eof; }
        size_t n = msgpack_getbe<uint32_t>(ptr); ptr += 4;
        return handle_map(n);
    }

    default:
        MOLD_TRY(w.write_literal("<unknown>"));
        return error_t::internal_logic_error;
    }
}

/**
 * @brief Serialize MessagePack bytes to diagnostic notation using callback.
 *
 * @param msgpack MessagePack byte data
 * @param sink_fn Callback to output bytes
 * @param user_ctx User context for the callback
 * @param indent Spaces per indentation level (0 for compact)
 * @return error_t::ok on success
 */
inline error_t msgpack_pretty(std::span<const uint8_t> msgpack, sink_cb_t sink_fn, void* user_ctx, int indent = 0)
{
    sink_t w(sink_fn, user_ctx);
    msgpack_ptr_t ptr = msgpack.data();
    msgpack_ptr_t end = msgpack.data() + msgpack.size();
    while (ptr < end) {
        MOLD_TRY(msgpack_pretty_impl(w, ptr, end, indent, 0));
        if (ptr < end) {
            MOLD_TRY(w.write_byte('\n'));
        }
    }
    return error_t::ok;
}

/**
 * @brief Serialize MessagePack bytes to diagnostic notation to buffer.
 *
 * @param msgpack MessagePack byte data
 * @param buffer Output buffer (updated to reflect written portion)
 * @param indent Spaces per indentation level (0 for compact)
 * @return error_t::ok on success
 */
inline error_t msgpack_pretty(std::span<const uint8_t> msgpack, std::span<char>& buffer, int indent = 0)
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
    auto status = msgpack_pretty(msgpack, [] (uint8_t b, void* p) {
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
 * @brief Wrapper for `msgpack_pretty` to send output via `MOLD_PUTCHAR`.
 *
 * @param msgpack MessagePack byte data
 * @param indent Spaces per indentation level (0 for compact)
 * @return error_t::ok on success
 */
inline error_t msgpack_pretty(std::span<const uint8_t> msgpack, int indent = 0)
{
    return msgpack_pretty(msgpack, [] (uint8_t b, void*) {
        return MOLD_PUTCHAR(b) != EOF;
    }, nullptr, indent);
}

#endif

#endif

}

#endif
