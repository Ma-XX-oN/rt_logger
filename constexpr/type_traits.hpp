#ifndef TYPE_TRAITS_HPP
#define TYPE_TRAITS_HPP
#include <type_traits>
#include <iterator>


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
    * @brief Maps an enum or integral type to an unsigned type of the same width.
    *
    * For enums, the mapped type is the unsigned form of the enum's underlying
    * type. For integral types, the mapped type is the unsigned form of the
    * type itself.
    *
    * Convenience alias for \c unsigned_equivalent<T>::type.
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
    * @brief Maps an enum or integral type to an integer type of the same width
    *   and sign type.
    *
    * Convenience alias for \c underlying_equivalent<T>::type.
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

  /**
   * @brief Reports whether \p Sentinel can act as an end sentinel for \p It.
   *
   * This is a pre-C++20 approximation of \c std::sentinel_for. It checks only
   * that \p Sentinel and \p It support symmetric \c == and \c != comparisons.
   * It does not attempt to model the full C++20 concept.
   *
   * @tparam Sentinel End-sentinel candidate type.
   * @tparam It Iterator type to compare against the sentinel.
   */
  template <typename Sentinel, typename It, typename = void>
  struct is_sentinel_for : std::false_type {};

  /**
   * @brief Reports whether \p Sentinel can act as an end sentinel for \p It.
   *
   * This is a pre-C++20 approximation of \c std::sentinel_for. It checks only
   * that \p Sentinel and \p It support symmetric \c == and \c != comparisons.
   * It does not attempt to model the full C++20 concept.
   *
   * @tparam Sentinel End-sentinel candidate type.
   * @tparam It Iterator type to compare against the sentinel.
   */
  template <typename Sentinel, typename It>
  struct is_sentinel_for<Sentinel, It, std::void_t<
    decltype(std::declval<It const&>() == std::declval<Sentinel const&>()),
    decltype(std::declval<It const&>() != std::declval<Sentinel const&>()),
    decltype(std::declval<Sentinel const&>() == std::declval<It const&>()),
    decltype(std::declval<Sentinel const&>() != std::declval<It const&>())
  >> : std::true_type {};

  /**
   * @brief Reports whether \p Sentinel can act as an end sentinel for \p It.
   *
   * This is a pre-C++20 approximation of \c std::sentinel_for. It checks only
   * that \p Sentinel and \p It support symmetric \c == and \c != comparisons.
   * It does not attempt to model the full C++20 concept.
   *
   * Convenience alias for \c is_sentinel_for<T>::value.
   *
   * @tparam Sentinel End-sentinel candidate type.
   * @tparam It Iterator type to compare against the sentinel.
   */
  template <typename S, typename I>
  inline constexpr bool is_sentinel_for_v = is_sentinel_for<S, I>::value;

  /**
   * @brief Reports whether \p It is at least a bidirectional iterator.
   *
   * This is a pre-C++20 approximation based on the iterator category tag.
   *
   * @tparam It Iterator type to inspect.
   */
  template <typename It>
  constexpr bool is_bidirectional =
    std::is_base_of_v<std::bidirectional_iterator_tag, typename std::iterator_traits<It>::iterator_category>;

  /**
   * @brief Combines multiple callable objects into one overload set.
   *
   * This is useful for building visitors or local conversion helpers from a set
   * of lambdas with different parameter types.
   *
   * @tparam Ts Callable base types to inherit from.
   */
  template<class... Ts> struct overload : Ts... { using Ts::operator()...; };

  /**
   * @brief Deduction guide for \c overload.
   *
   * @tparam Ts Callable types used to construct the overload set.
   */
  template<class... Ts> overload(Ts...) -> overload<Ts...>;

} // namespace Constexpr

#endif // TYPE_TRAITS_HPP
