#ifndef TYPE_TRAITS_HPP
#define TYPE_TRAITS_HPP
#include <type_traits>

namespace Constexpr {
  /**
    * @brief Maps an enum or integral type to an unsigned type of the same width.
    *
    * For enums, the mapped type is the unsigned form of the enum's underlying
    * type. For integral types, the mapped type is the unsigned form of the
    * type itself.
    *
    * @tparam T - Enum or integral type to map.
    */
  template <typename T, bool IsEnum = std::is_enum<T>::value>
  struct unsigned_equivalent {
      using type = std::make_unsigned_t<T>;
  };

  template <typename T>
  struct unsigned_equivalent<T, true> {
      using type = std::make_unsigned_t<std::underlying_type_t<T>>;
  };

  /**
    * @brief Convenience alias for \c unsigned_equivalent<T>::type.
    *
    * @tparam T - Enum or integral type to map.
    */
  template <typename T>
  using unsigned_equivalent_t = typename unsigned_equivalent<T>::type;

  /**
   * @brief Makes \p value into the unsigned underlying equivalent type of it
   *   or if just an int, makes it an unsigned int.
   *
   * @tparam T - Type of value.
   * @param value - Value to convert.
   * @return unsigned_equivalent_t<T> - The value typed as the unsigned
   *   equivalent.
   */
  template <typename T>
  constexpr unsigned_equivalent_t<T> make_unsigned_equivalent(T value) {
    return static_cast<unsigned_equivalent_t<T>>(value);
  }
  


  /**
    * @brief Maps an enum or integral type to an integer type of the same width
    *   and sign type.
    *
    * @tparam T - Enum or integral type to map.
    */
  template <typename T, bool IsEnum = std::is_enum<T>::value>
  struct underlying_equivalent {
      using type = T;
  };

  template <typename T>
  struct underlying_equivalent<T, true> {
      using type = std::underlying_type_t<T>;
  };

  /**
    * @brief Convenience alias for \c underlying_equivalent<T>::type.
    *
    * @tparam T - Enum or integral type to map.
    */
  template <typename T>
  using underlying_equivalent_t = typename underlying_equivalent<T>::type;

  /**
   * @brief Makes \p value into the underlying equivalent type of it or if just
   *   an int, does nothing.
   *
   * @tparam T - Type of value.
   * @param value - Value to convert.
   * @return underlying_equivalent_t<T> - The value typed as the underlying
   *   equivalent.
   */
  template <typename T>
  constexpr underlying_equivalent_t<T> make_underlying_equivalent(T value) {
    return static_cast<underlying_equivalent_t<T>>(value);
  }
  
} // namespace Constexpr

#endif // TYPE_TRAITS_HPP
