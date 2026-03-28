#ifndef MOLD_UTIL_CONTAINER_H
#define MOLD_UTIL_CONTAINER_H

/**
 * @file
 * @brief Compile-time container traits for arrays, vectors, and tuples.
 */

#include <array>
#include <limits>
#include <vector>
#include "mold/util/concepts.h"

namespace mold {

/**
 * @brief Provides compile-time shape information for containers and tuples.
 * 
 * - Default: not a container (`element_count = 0`)
 * - `std::vector` / `std::array`: expose `element_type` and `element_count`
 * - Tuple-like: expose only `element_count`
 * 
 */
template<class T, class = void>
struct container_traits_t {
    static constexpr uint32_t element_count = 0;
};

/**
 * @brief Specialization for `std::vector` (homogenous).
 * 
 * @tparam U Element type
 */
template<class U>
struct container_traits_t<std::vector<U>, void> {
    using element_type = U;
    static constexpr uint32_t element_count = std::numeric_limits<uint32_t>::max();
};

/**
 * @brief Specialization for `std::array` (homogenous).
 * 
 * @tparam U Element type
 * @tparam N Element count
 */
template<class U, size_t N>
struct container_traits_t<std::array<U, N>, void> {
    using element_type = U;
    static constexpr uint32_t element_count = N;
};

/**
 * @brief Specialization for tuple-like types (heterogeneous).
 * 
 * Exposes only `element_count`. No `element_type` is defined.
 * 
 * @tparam T Type
 */
template<class T>
struct container_traits_t<T, std::enable_if_t<is_tuple<T>>> {
    static constexpr uint32_t element_count = std::tuple_size_v<T>;
};

/**
 * @brief Check if the type is a homogenous container in accordance with `container_traits_t`.
 * 
 * @note Detected by presence of `element_type` in `container_traits_t<T>`.
 * 
 * @tparam T Type
 */
template<class T>
concept is_homogenous_container = requires { 
    typename container_traits_t<std::remove_cvref_t<T>>::element_type; 
};

}

#endif
