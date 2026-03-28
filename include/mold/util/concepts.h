#ifndef MOLD_UTIL_CONCEPTS_H
#define MOLD_UTIL_CONCEPTS_H

/**
 * @file
 * @brief C++20 concepts for type classification used throughout the library.
 */

#include <tuple>
#include <concepts>
#include <optional>
#include <string_view>
#include <type_traits>

namespace mold {

/**
 * @brief Check if a type is a specialization of a template.
 * 
 * @tparam T Type
 * @tparam Template Template
 */
template<class T, template <typename...> class Template> 
concept is_specialization_of = requires ( std::remove_cvref_t<T> t ) { 
    [] <typename... Args> ( Template<Args...>& ) {} ( t ); 
};

/**
 * @brief Concept for strong boolean.
 * 
 * @tparam T Type to check
 */
template<class T>
concept is_boolean = std::is_same<T, bool>::value;

/**
 * @brief Concept for strong integral (fails for boolean).
 * 
 * @tparam T Type to check
 */
template<class T>
concept is_integer = std::integral<T> && !is_boolean<T>;

/**
 * @brief Concept for unsigned integral type.
 * 
 * @tparam T Type to check
 */
template<class T>
concept is_unsigned = is_integer<T> && std::is_unsigned_v<T>;

/**
 * @brief Concept for signed integral type.
 * 
 * @tparam T Type to check
 */
template<class T>
concept is_signed = is_integer<T> && std::is_signed_v<T>;

/**
 * @brief Concept for floaring point number.
 * 
 * @tparam T Type to check
 */
template<class T>
concept is_floating = std::floating_point<T>;

/**
 * @brief Concept for convertible to `std::string_view`.
 * 
 * @tparam T Type to check
 */
template<class T> 
concept is_string = std::convertible_to<T, std::string_view>;

/**
 * @brief Check if a type is a specialization of std::tuple.
 * 
 * @tparam T Type
 */
template<class T>
concept is_tuple = is_specialization_of<T, std::tuple>;

/**
 * @brief Check if a type is a specialization of std::optional.
 * 
 * @tparam T Type
 */
template<class T>
concept is_optional = is_specialization_of<T, std::optional>;

/**
 * @brief Check if a type is an aggregate.
 * 
 * @tparam T Type
 */
template<class T>
concept is_aggregate = std::is_aggregate_v<T> && !is_tuple<T>;

/**
 * @brief Concept for enumeration types.
 * 
 * @tparam T Type to check
 */
template<class T>
concept is_enumeration = std::is_enum_v<T>;

}

#endif
