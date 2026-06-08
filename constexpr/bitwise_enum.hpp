#ifndef BITWISE_ENUM_HPP
#define BITWISE_ENUM_HPP

#include <type_traits>
#include "type_traits.hpp"
#include "bit.hpp"

/**
 * @brief Enable bitwise operations for specific enums.
 * 
 * @tparam E - Enum to enable
 */
template <typename E, typename = void>
struct enable_bitwise_ops : std::false_type {};

/**
 * @brief Check if type is an enum and if bitwise operations have been enabled for it.
 * 
 * @tparam E - Type to check.
 */
template <typename E, bool match = (std::is_enum_v<E> && enable_bitwise_ops<E>::value)>
struct is_bitwise_enum : std::false_type {};

template <typename E>
struct is_bitwise_enum<E, true> : std::true_type {};

/**
 * @brief Check if type is an enum and if bitwise operations have been enabled for it.
 * 
 * @tparam E - Type to check.
 *
 * Convenience alias for \c is_bitwise_enum<T>::type.
 */
template <typename E>
constexpr bool is_bitwise_enum_v = is_bitwise_enum<E>::value;

/**
 * @brief bitwise or
 * 
 * @tparam E - Enum type
 * @param lhs - Left hand side of | operator.
 * @param rhs - Right hand side of | operator.
 * @return E - New ORed value.
 */
template <typename E
  , std::enable_if_t<is_bitwise_enum<E>::value, bool> = true>
constexpr E operator|(E lhs, E rhs) {
  using namespace Constexpr;
  using U = unsigned_equivalent_t<E>;
  return bit_cast<E>(static_cast<U>(make_unsigned_equivalent(lhs) | make_unsigned_equivalent(rhs)));
}

/**
 * @brief Inplace bitwise or assignment
 * 
 * @tparam E - Enum type
 * @param lhs - Left hand side of | operator.
 * @param rhs - Right hand side of | operator.
 * @return E - New ORed value.
 */
template <typename E
  , std::enable_if_t<is_bitwise_enum<E>::value, bool> = true>
constexpr E& operator|=(E& lhs, E rhs) {
  return lhs = lhs | rhs;
}

/**
 * @brief bitwise and
 * 
 * @tparam E - Enum type
 * @param lhs - Left hand side of & operator.
 * @param rhs - Right hand side of & operator.
 * @return E - New ANDed value.
 */
template <typename E
  , std::enable_if_t<is_bitwise_enum<E>::value, bool> = true>
constexpr E operator&(E lhs, E rhs) {
  using namespace Constexpr;
  using U = unsigned_equivalent_t<E>;
  return bit_cast<E>(static_cast<U>(make_unsigned_equivalent(lhs) & make_unsigned_equivalent(rhs)));
}

/**
 * @brief Inplace bitwise and assignment.
 * 
 * @tparam E - Enum type
 * @param lhs - Left hand side of & operator.
 * @param rhs - Right hand side of & operator.
 * @return E - New ANDed value.
 */
template <typename E
  , std::enable_if_t<is_bitwise_enum<E>::value, bool> = true>
constexpr E& operator&=(E& lhs, E rhs) {
  return lhs = lhs & rhs;
}

/**
 * @brief Bitwise xor
 * 
 * @tparam E - Enum type
 * @param lhs - Left hand side of ^ operator.
 * @param rhs - Right hand side of ^ operator.
 * @return E - New XORed value.
 */
template <typename E
  , std::enable_if_t<is_bitwise_enum<E>::value, bool> = true>
constexpr E operator^(E lhs, E rhs) {
  using namespace Constexpr;
  using U = unsigned_equivalent_t<E>;
  return bit_cast<E>(static_cast<U>(make_unsigned_equivalent(lhs) ^ make_unsigned_equivalent(rhs)));
}

/**
 * @brief Inplace bitwise xor assignment
 * 
 * @tparam E - Enum type
 * @param lhs - Left hand side of ^ operator.
 * @param rhs - Right hand side of ^ operator.
 * @return E - New XORed value.
 */
template <typename E
  , std::enable_if_t<is_bitwise_enum<E>::value, bool> = true>
constexpr E& operator^=(E& lhs, E rhs) {
  return lhs = lhs ^ rhs;
}

/**
 * @brief Bitwise not
 * 
 * @tparam E - Enum type
 * @param rhs - Right hand side of ~ operator.
 * @return E - New NOTted value.
 */
template <typename E
  , std::enable_if_t<is_bitwise_enum<E>::value, bool> = true>
constexpr E operator~(E rhs) {
  using namespace Constexpr;
  using U = unsigned_equivalent_t<E>;
  return bit_cast<E>(static_cast<U>(~make_unsigned_equivalent(rhs)));
}

#endif // BITWISE_ENUM_HPP
