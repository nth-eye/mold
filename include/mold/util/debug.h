#ifndef MOLD_UTIL_DEBUG_H
#define MOLD_UTIL_DEBUG_H

/**
 * @file
 * @brief Debugging and profiling utilities (benchmarking, reflection printing).
 */

#include <ctime>
#include "mold/refl/reflection.h"

namespace mold {

/**
 * @brief Measure execution time of a function.
 * 
 * @tparam N Number of calls
 * @tparam Fn Function pointer
 * @tparam Args Arguments of the function
 * @param fn Function pointer
 * @param args Arguments of the function
 * @return Total clock ticks for N executions
 */
template<size_t N = 1, class Fn, class ...Args>
clock_t exec_time(Fn&& fn, Args&& ...args)
{
    clock_t begin = clock();
    for (size_t i = 0; i < N; ++i) {
        fn(std::forward<Args>(args)...);
    }
    clock_t end = clock();
    return (end - begin);
}

/**
 * @brief Measure average execution time of a function for N calls.
 * 
 * @tparam N Number of calls
 * @tparam Args Function pointer followed by arguments
 * @param args Function pointer followed by arguments
 * @return Average clock ticks for each call
 */
template<size_t N = 1, class ...Args>
clock_t exec_time_avg(Args&& ...args)
{
    return exec_time<N>(std::forward<Args>(args)...) / N;
}

/**
 * @brief Recursively print a struct's reflection tree with offset and type info.
 *
 * @param members Span of reflection entries for the current level.
 * @param num_pad Hex digit width for offset printing.
 * @param col_pad_1 Padding between absolute address and relative offset columns.
 * @param col_pad_2 Padding between relative offset and definition columns.
 * @param base_offset Absolute byte offset of the current struct within the root.
 * @param indent Number of spaces per indentation level.
 * @param depth Current nesting depth (0 = top level).
 */
inline void print_reflection_recursive(reflection_span_t members, int num_pad, int col_pad_1, int col_pad_2, size_t base_offset, int indent, int depth = 0)
{
    auto print_end_marker = [&] {
        char end_marker_buf[32];
        snprintf(end_marker_buf, sizeof(end_marker_buf), ".level_end_%d", depth);
        MOLD_PRINT("%-*s", col_pad_1 + col_pad_2 + 11 + num_pad * 2, end_marker_buf);
        print_indent(indent, depth - 1);
    };
    for (const auto& member_info : members) {
        // Calculate absolute offset
        size_t absolute_offset = base_offset + member_info.offset;
        // Print absolute and relative offsets
        MOLD_PRINT("<0x%0*zx>%-*s+0x%0*x%-*s",
            num_pad, absolute_offset, col_pad_1, "",
            num_pad, member_info.offset, col_pad_2, "");
        print_indent(indent, depth);

        if (member_info.element_count) {
            if (member_info.element_count == reflection_t::dynamic_count) {
#if (MOLD_REFLECTION_SIZE_ENABLED)
                MOLD_PRINT("vector [ // %u-byte\n", member_info.size);
#else
                MOLD_PRINT("vector [\n");
#endif
            } else {
#if (MOLD_REFLECTION_SIZE_ENABLED)
                MOLD_PRINT("array<%u> [ // %u-byte\n", member_info.element_count, member_info.size);
#else
                MOLD_PRINT("array<%u> [\n", member_info.element_count);
#endif
            }
            print_reflection_recursive(member_info.members, num_pad, col_pad_1, col_pad_2, absolute_offset, indent, depth + 1);
            print_end_marker();
#if (MOLD_REFLECTION_NAME_NEEDED)
            if (member_info.name.empty()) {
                MOLD_PRINT("];\n");
            } else {
                MOLD_PRINT("] %.*s;\n", int(member_info.name.size()), member_info.name.data());
            }
#else
            MOLD_PRINT("];\n");
#endif
        } else {
            if (member_info.members.empty()) {
#if (MOLD_REFLECTION_TYPE_NAME_ENABLED)
                MOLD_PRINT("%.*s ", int(member_info.type.size()), member_info.type.data()); 
#endif
#if (MOLD_REFLECTION_NAME_NEEDED)
                if (!member_info.name.empty()) {
#if (MOLD_REFLECTION_SIZE_ENABLED)
                    MOLD_PRINT("%.*s; // %u-byte\n", int(member_info.name.size()),
                        member_info.name.data(),
                        member_info.size);
#else
                    MOLD_PRINT("%.*s;\n", int(member_info.name.size()), member_info.name.data());
#endif
                } else
#endif
                {
#if (MOLD_REFLECTION_SIZE_ENABLED)
                    MOLD_PRINT("; // %u-byte\n", member_info.size);
#else
                    MOLD_PRINT(";\n");
#endif
                }
            } else {
#if (MOLD_REFLECTION_TYPE_NAME_ENABLED)
                MOLD_PRINT("struct %.*s {", int(member_info.type.size()), member_info.type.data());
#else
                MOLD_PRINT("struct {");
#endif
#if (MOLD_REFLECTION_SIZE_ENABLED)
                MOLD_PRINT("// %u-byte\n", member_info.size);
#else
                MOLD_PRINT("\n");
#endif
                print_reflection_recursive(member_info.members, num_pad, col_pad_1, col_pad_2, absolute_offset, indent, depth + 1);
                print_end_marker();
#if (MOLD_REFLECTION_NAME_NEEDED)
                if (!member_info.name.empty()) {
                    MOLD_PRINT("} %.*s;\n", int(member_info.name.size()), member_info.name.data());
                } else
#endif
                {
                    MOLD_PRINT("};\n");
                }
            }
        }
    }
}

/**
 * @brief Print a formatted reflection dump of type T to stdout via `MOLD_PRINT`.
 *
 * Displays a table with absolute address, relative offset, and definition
 * columns for every member, recursing into nested structs and arrays.
 *
 * @tparam T The reflected type to print.
 */
template<class T>
void print_reflection()
{
    constexpr int top_level_padding = [] {
        if (type_info_t<T>::size() > 0xffffffff) {
            return 16;
        }
        if (type_info_t<T>::size() > 0xffff) {
            return 8;
        }
        if (type_info_t<T>::size() > 0xff) {
            return 4;
        }
        return 2;
    }();
    constexpr int min_col_width_1 = top_level_padding + 6;
    constexpr int min_col_width_2 = top_level_padding + 5;
    constexpr int header_width_1 = sizeof("ADDRESS") + 1;
    constexpr int header_width_2 = sizeof("OFFSET") + 1;
    constexpr int col_width_1 = std::max(min_col_width_1, header_width_1);
    constexpr int col_width_2 = std::max(min_col_width_2, header_width_2);
    constexpr int total_offset_width = col_width_1 + col_width_2;

    MOLD_PRINT("%-*s%-*s%s\n",
        col_width_1, "ADDRESS",
        col_width_2, "OFFSET", "DEFINITION");
    MOLD_PRINT("%-*sstruct %.*s { // %u-byte\n", 
        total_offset_width, ".begin", int(type_info_t<T>::name().size()), 
        type_info_t<T>::name().data(),
        type_info_t<T>::size());
    print_reflection_recursive(type_info_t<T>::members(), top_level_padding,
        col_width_1 - 6 - top_level_padding + 2,
        col_width_2 - 5 - top_level_padding + 2, 0, 4, 1);
    MOLD_PRINT("%-*s};\n", total_offset_width, ".level_end_0");
}

}

#endif
