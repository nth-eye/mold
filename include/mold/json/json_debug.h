#ifndef MOLD_JSON_DEBUG_H
#define MOLD_JSON_DEBUG_H

/**
 * @file
 * @brief Debug utilities for printing JSON schemas and parsed data.
 */

#include "mold/refl/reflection.h"

namespace mold {

#if (MOLD_REFLECTION_JSON_ENABLED)

/**
 * @brief Print static schema definition (types and structure) recursively.
 *
 * @param refl Reflection info for the current node
 * @param indent Spaces per indentation level (0 = compact, no newlines)
 * @param depth Current indentation depth level
 */
inline void json_print_schema(const reflection_t& refl, int indent, int depth = 0)
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
    if (json_type_has(refl.json_type, json_type_t::null))       { print_sep(); MOLD_PRINT("null"); }
    if (json_type_has(refl.json_type, json_type_t::boolean))    { print_sep(); MOLD_PRINT("boolean"); }
    if (json_type_has(refl.json_type, json_type_t::integer))    { print_sep(); MOLD_PRINT("integer"); }
    if (json_type_has(refl.json_type, json_type_t::floating))   { print_sep(); MOLD_PRINT("floating"); }
    if (json_type_has(refl.json_type, json_type_t::string))     { print_sep(); MOLD_PRINT("string"); }
    // Object
    if (json_type_has(refl.json_type, json_type_t::object)) {
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
            json_print_schema(member_refl, indent, depth + 1);
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
    if (json_type_has(refl.json_type, json_type_t::array)) {
        print_sep();
        MOLD_PRINT("[");
        if (indent) {
            MOLD_PRINT("\n");
        }
        if (refl.json_handler) { // Homogeneous array (std::vector, std::array)
            print_indent(indent, depth + 1);
            if (!refl.members.empty()) {
                json_print_schema(refl.members[0], indent, depth + 1);
            } else {
                MOLD_PRINT("<missing_in_reflection>");
            }
            if (refl.element_count != reflection_t::dynamic_count) {
                MOLD_PRINT("...%u", refl.element_count);
            } else {
                MOLD_PRINT("...");
            }
            if (indent) {
                MOLD_PRINT("\n");
            }
        } else { // Heterogeneous array (tuple-like)
            for (size_t i = 0; i < refl.members.size(); ++i) {
                const auto& element_refl = refl.members[i];
                print_indent(indent, depth + 1);
                json_print_schema(element_refl, indent, depth + 1);
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
        MOLD_PRINT("undefined");
    }
}

#endif

}

#endif
