#ifndef MOLD_JSON_SINK_H
#define MOLD_JSON_SINK_H

/**
 * @file
 * @brief JSON output sink with escaped string support.
 */

#include "mold/util/common.h"
#include "mold/util/sink.h"

namespace mold {

/**
 * @brief Helper for writing JSON data to a sink callback.
 * 
 * Extends with JSON-specific functionality like escaped strings.
 */
struct json_sink_t : sink_t {

    using sink_t::sink_t;

    /**
     * @brief Write a JSON-escaped string with quotes.
     * 
     * @param sv String view to escape and emit
     * @return `error_t::ok` on success
     */
    error_t write_escaped_string(std::string_view sv) const
    {
        MOLD_TRY(write_byte('"'));
        for (char c : sv) {
            switch (c) {
                case '\\': MOLD_TRY(write_literal("\\\\")); break;
                case '"':  MOLD_TRY(write_literal("\\\"")); break;
                case '\b': MOLD_TRY(write_literal("\\b")); break;
                case '\f': MOLD_TRY(write_literal("\\f")); break;
                case '\n': MOLD_TRY(write_literal("\\n")); break;
                case '\r': MOLD_TRY(write_literal("\\r")); break;
                case '\t': MOLD_TRY(write_literal("\\t")); break;
                default: 
                    if ((unsigned char) c < 0x20) {
                        // control char -> \u00XX
                        MOLD_TRY(write_literal("\\u00"));
                        MOLD_TRY(write_byte(detail::hex_digits[(c >> 4) & 0xf]));
                        MOLD_TRY(write_byte(detail::hex_digits[c & 0xf]));
                    } else {
                        MOLD_TRY(write_byte(c));
                    }
                break;
            }
        }
        return write_byte('"');
    }

};

}

#endif
