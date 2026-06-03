#ifndef BITWISE_ENUM_HPP
#define BITWISE_ENUM_HPP

#include <type_traits>

/**
 * @brief Enable bitwise operations for specific enums.
 * 
 * @tparam E - Enum to enable
 */
template <typename E, typename = void>
struct BitwiseOps : std::false_type {
};

/**
 * @brief bitwise or
 * 
 * @tparam E - Enum type
 * @param lhs - Left hand side of | operator.
 * @param rhs - Right hand side of | operator.
 * @return E - New ORed value.
 */
template <typename E
  , std::enable_if_t<std::is_enum_v<E> && BitwiseOps<E>::value, bool> = true>
constexpr E operator|(E lhs, E rhs) {
  using I = std::underlying_type_t<E>;
  return static_cast<E>(static_cast<I>(lhs) | static_cast<I>(rhs));
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
  , std::enable_if_t<std::is_enum_v<E> && BitwiseOps<E>::value, bool> = true>
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
  , std::enable_if_t<std::is_enum_v<E> && BitwiseOps<E>::value, bool> = true>
constexpr E operator&(E lhs, E rhs) {
  using I = std::underlying_type_t<E>;
  return static_cast<E>(static_cast<I>(lhs) & static_cast<I>(rhs));
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
  , std::enable_if_t<std::is_enum_v<E> && BitwiseOps<E>::value, bool> = true>
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
  , std::enable_if_t<std::is_enum_v<E> && BitwiseOps<E>::value, bool> = true>
constexpr E operator^(E lhs, E rhs) {
  using I = std::underlying_type_t<E>;
  return static_cast<E>(static_cast<I>(lhs) ^ static_cast<I>(rhs));
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
  , std::enable_if_t<std::is_enum_v<E> && BitwiseOps<E>::value, bool> = true>
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
  , std::enable_if_t<std::is_enum_v<E> && BitwiseOps<E>::value, bool> = true>
constexpr E operator~(E rhs) {
  using I = std::underlying_type_t<E>;
  return static_cast<E>(~static_cast<I>(rhs));
}

#endif // BITWISE_ENUM_HPP
