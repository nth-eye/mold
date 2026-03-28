#ifndef MOLD_UTIL_ENUM_H
#define MOLD_UTIL_ENUM_H

/**
 * @file
 * @brief Utilities for enabling bitwise operations on C++ enum classes.
 * 
 * To enable bitwise operations for an enum 'T', add the following after its definition
 * (in the same namespace as the enum): `MOLD_ENABLE_ENUM_BITWISE_OPERATORS(T)`.
 */

#include "mold/util/concepts.h"

namespace mold {

/**
 * @brief Shortcut for converting strong enum to underlying integer type. 
 * 
 * @tparam T Enum class type
 * @param e Enumeration value
 * @return Value of underlying integer type 
 */
template<is_enumeration T>
constexpr auto operator+(T e)
{
    return std::underlying_type_t<T>(e);
}

/**
 * @brief Proxy type for the result of bitwise operations on enums.
 * 
 * This allows the result to be contextually converted to bool (for if statements)
 * and also implicitly converted back to the original enum type.
 * 
 * @tparam E The enum type, constrained by the 'is_enumeration' concept.
 */
template<is_enumeration E>
struct enum_t {

    /**
     * @brief Constructs the result from an enum value.
     * 
     * @param val The enum value.
     */
    constexpr enum_t(E val) : value(val) {}

    /**
     * @brief Implicitly converts the result back to the enum type E.
     * @return The stored enum value.
     */
    constexpr operator E() const 
    { 
        return value; 
    }

    /**
     * @brief Implicitly converts the result to bool.
     * 
     * @return True if the underlying value of the enum is non-zero, false otherwise.
     */
    constexpr operator bool() const 
    { 
        return +value != 0; 
    }

    /**
     * @brief Converts the result to the underlying integer type.
     * 
     * @return The underlying integer value.
     */
    constexpr auto operator+() const noexcept
    {
        return +value;
    }

    E value; ///< The resulting enum value from the bitwise operation.
};

/**
 * @brief Trait to enable bitmask operations for an enum.
 * 
 * By default, bitmask operations are disabled.
 * 
 * @tparam E The enum type.
 */
template<class E>
struct enum_traits_t {
    static constexpr bool enable_operators = false;
};

/**
 * @brief Helper macro to specialize `enum_traits_t` for a given enum.
 * 
 * Call this macro in the same namespace as the enum definition.
 * 
 * @tparam E The enum type to enable bitmask operations for.
 */
#define MOLD_ENABLE_ENUM_BITWISE_OPERATORS(E) \
template<> \
struct enum_traits_t<E> { \
    using type = E; \
    static constexpr bool enable_operators = true; \
};

/**
 * @brief Specialization that maps the proxy back to its original enum type.
 *
 */
template<is_enumeration E>
struct enum_traits_t<enum_t<E>> {
    using type = E;
    static constexpr bool enable_operators = enum_traits_t<E>::enable_operators;
};

/**
 * @brief Concept that verifies bitwise helpers are enabled for a type.
 *
 */
template<class E>
concept is_bitwise_enum = enum_traits_t<E>::enable_operators;

/**
 * @brief Extracts the base enum type from any bitwise-enabled operand.
 *
 */
template<is_bitwise_enum T>
using enum_operand_t = typename enum_traits_t<std::decay_t<T>>::type;

/**
 * @brief Bitwise OR operator for enums.
 * 
 * @tparam L Left-hand side operand type.
 * @tparam R Right-hand side operand type.
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 * @return Result of bitwise OR, as enhanced enum type.
 */
template<is_bitwise_enum L, is_bitwise_enum R> requires std::same_as<enum_operand_t<L>, enum_operand_t<R>>
constexpr auto operator|(L lhs, R rhs) noexcept 
{
    return enum_t(enum_operand_t<L>(+lhs | +rhs));
}

/**
 * @brief Bitwise AND operator for enums.
 * 
 * @tparam L Left-hand side operand type.
 * @tparam R Right-hand side operand type.
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 * @return Result of bitwise AND, as enhanced enum type.
 */
template<is_bitwise_enum L, is_bitwise_enum R> requires std::same_as<enum_operand_t<L>, enum_operand_t<R>>
constexpr auto operator&(L lhs, R rhs) noexcept 
{
    return enum_t(enum_operand_t<L>(+lhs & +rhs));
}

/**
 * @brief Bitwise XOR operator for enums.
 * 
 * @tparam L Left-hand side operand type.
 * @tparam R Right-hand side operand type.
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 * @return Result of bitwise XOR, as enhanced enum type.
 */
template<is_bitwise_enum L, is_bitwise_enum R> requires std::same_as<enum_operand_t<L>, enum_operand_t<R>>
constexpr auto operator^(L lhs, R rhs) noexcept 
{
    return enum_t(enum_operand_t<L>(+lhs ^ +rhs));
}

/**
 * @brief Bitwise NOT operator for enums.
 * 
 * @tparam T The operand type.
 * @param val The enum value.
 * @return Result of bitwise NOT, as enhanced enum type.
 */
template<is_bitwise_enum T>
constexpr auto operator~(T val) noexcept 
{
    return enum_t(enum_operand_t<T>(~+val));
}

/**
 * @brief Bitwise OR assignment operator for enums.
 * 
 * @tparam L Left-hand side operand type.
 * @tparam R Right-hand side operand type.
 * @param lhs Left-hand side operand (will be modified).
 * @param rhs Right-hand side operand.
 * @return Reference to the modified left-hand side operand.
 */
template<is_bitwise_enum L, is_bitwise_enum R> requires std::same_as<enum_operand_t<L>, enum_operand_t<R>>
constexpr L& operator|=(L& lhs, R rhs) noexcept
{
    lhs = enum_operand_t<L>(+lhs | +rhs);
    return lhs;
}

/**
 * @brief Bitwise AND assignment operator for enums.
 * 
 * @tparam L Left-hand side operand type.
 * @tparam R Right-hand side operand type.
 * @param lhs Left-hand side operand (will be modified).
 * @param rhs Right-hand side operand.
 * @return Reference to the modified left-hand side operand.
 */
template<is_bitwise_enum L, is_bitwise_enum R> requires std::same_as<enum_operand_t<L>, enum_operand_t<R>>
constexpr L& operator&=(L& lhs, R rhs) noexcept
{
    lhs = enum_operand_t<L>(+lhs & +rhs);
    return lhs;
}

/**
 * @brief Bitwise XOR assignment operator for enums.
 * 
 * @tparam L Left-hand side operand type.
 * @tparam R Right-hand side operand type.
 * @param lhs Left-hand side operand (will be modified).
 * @param rhs Right-hand side operand.
 * @return Reference to the modified left-hand side operand.
 */
template<is_bitwise_enum L, is_bitwise_enum R> requires std::same_as<enum_operand_t<L>, enum_operand_t<R>>
constexpr L& operator^=(L& lhs, R rhs) noexcept
{
    lhs = enum_operand_t<L>(+lhs ^ +rhs);
    return lhs;
}

}

#endif
