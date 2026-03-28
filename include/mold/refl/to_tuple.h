#ifndef MOLD_REFL_TO_TUPLE_H
#define MOLD_REFL_TO_TUPLE_H

/**
 * @file
 * @brief Compile-time decomposition of aggregate types into tuples without user annotation.
 */

#include "mold/util/concepts.h"

namespace mold {

/**
 * @brief A placeholder type implicitly convertible to any other type.
 *
 * Used in compile-time reflection techniques to determine the number of members
 * an aggregate type can be initialized with. Handles std::optional separately.
 *
 */
struct any_type_t {
    template<class T>
    constexpr operator T() const noexcept { return {}; }
    template<is_optional T>
    constexpr operator T() const noexcept { return std::nullopt; }
};

/**
 * @brief Counts the number of data members in an aggregate at compile time.
 *
 * Repeatedly tries to aggregate-initialize T with one more placeholder argument
 * (`any_type_t`). The recursion stops when the `requires` clause fails (because
 * T cannot be initialized with that many arguments), returning the previous
 * successful count.
 *
 * @tparam T Aggregate type
 * @param members Internal pack used for recursion, starts empty
 * @return The number of data members in T
 */
template<class T>
constexpr size_t count_members(auto ...members) noexcept
{
    if constexpr (is_aggregate<T> || is_tuple<T>) {
        if constexpr (!requires { T{members...}; }) {
            return sizeof...(members) - 1;
        } else {
            return count_members<T>(members..., any_type_t());
        }
    } else {
        return 0u;
    }
}

#if (__cpp_structured_bindings >= 202411L)

/**
 * @brief Creates a tuple of references to the members of an aggregate type.
 *
 * Uses C++26 structured binding packs for unlimited member count.
 *
 * @tparam T Aggregate type
 * @param data Aggregate value
 * @return Tuple of references to the members of the aggregate
 */
template<class T>
constexpr auto tie_as_tuple(const T& data) noexcept
{
    auto& [...members] = data;
    return std::tie(members...);
}

#else

/**
 * @brief Creates a tuple of references to the members of an aggregate type.
 *
 * Manual enumeration fallback for compilers without P1061R10 support.
 *
 * @tparam T Aggregate type
 * @param data Aggregate value
 * @return Tuple of references to the members of the aggregate
 */
template<class T>
constexpr auto tie_as_tuple(const T& data) noexcept
{
    constexpr size_t count = count_members<T>();

    if constexpr (count == 0) {
        return std::tie();
    } else if constexpr (count == 1) {
        auto& [m1] = data;
        return std::tie(m1);
    } else if constexpr (count == 2) {
        auto& [m1, m2] = data;
        return std::tie(m1, m2);
    } else if constexpr (count == 3) {
        auto& [m1, m2, m3] = data;
        return std::tie(m1, m2, m3);
    } else if constexpr (count == 4) {
        auto& [m1, m2, m3, m4] = data;
        return std::tie(m1, m2, m3, m4);
    } else if constexpr (count == 5) {
        auto& [m1, m2, m3, m4, m5] = data;
        return std::tie(m1, m2, m3, m4, m5);
    } else if constexpr (count == 6) {
        auto& [m1, m2, m3, m4, m5, m6] = data;
        return std::tie(m1, m2, m3, m4, m5, m6);
    } else if constexpr (count == 7) {
        auto& [m1, m2, m3, m4, m5, m6, m7] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7);
    } else if constexpr (count == 8) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8);
    } else if constexpr (count == 9) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9);
    } else if constexpr (count == 10) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10);
    } else if constexpr (count == 11) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11);
    } else if constexpr (count == 12) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12);
    } else if constexpr (count == 13) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13);
    } else if constexpr (count == 14) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14);
    } else if constexpr (count == 15) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15);
    } else if constexpr (count == 16) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16);
    } else if constexpr (count == 17) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17);
    } else if constexpr (count == 18) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18);
    } else if constexpr (count == 19) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19);
    } else if constexpr (count == 20) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20);
    } else if constexpr (count == 21) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21);
    } else if constexpr (count == 22) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22);
    } else if constexpr (count == 23) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23);
    } else if constexpr (count == 24) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24);
    } else if constexpr (count == 25) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25);
    } else if constexpr (count == 26) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26);
    } else if constexpr (count == 27) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27);
    } else if constexpr (count == 28) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28);
    } else if constexpr (count == 29) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29);
    } else if constexpr (count == 30) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30);
    } else if constexpr (count == 31) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31);
    } else if constexpr (count == 32) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32] = data;
        return std::tie(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32);
    } else {
        static_assert(count != count, "Too many fields, add more if statements!");
    }
}

#endif

/**
 * @brief Creates a tuple of the members of an aggregate type.
 *
 * @tparam T Aggregate type
 * @param data Instance of the aggregate
 * @return Tuple of the members of the aggregate
 */
template<class T>
constexpr auto to_tuple(T&& data) noexcept
{
    return std::apply([] (auto&&... args) {
        return std::make_tuple(args...);
    }, tie_as_tuple(data));
}

}

#endif
