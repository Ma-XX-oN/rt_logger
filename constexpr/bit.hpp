#ifndef BIT_HPP
#define BIT_HPP

#include <limits>
#include <type_traits>

#if defined(__has_include)
#  if __has_include(<bit>)
#    include <bit>
#  endif
#endif

namespace Constexpr {
  namespace detail {
#if !defined(CONSTEXPR_DISABLE_STD_BIT_CAST) && defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L
    inline constexpr bool has_std_bit_cast_v{ true };
#else
    inline constexpr bool has_std_bit_cast_v{ false };
#endif

#if !defined(CONSTEXPR_DISABLE_BUILTIN_BIT_CAST) && defined(__has_builtin)
#  if __has_builtin(__builtin_bit_cast)
    inline constexpr bool has_builtin_bit_cast_v{ true };
#  else
    inline constexpr bool has_builtin_bit_cast_v{ false };
#  endif
#else
    inline constexpr bool has_builtin_bit_cast_v{ false };
#endif

    template <typename...>
    inline constexpr bool always_false_v{ false };

    inline constexpr bool has_native_constexpr_bit_cast_v{
      has_std_bit_cast_v || has_builtin_bit_cast_v
    };

    template <typename T>
    inline constexpr bool is_supported_fallback_integer_v{
      std::is_integral_v<T>
      && !std::is_same_v<std::remove_cv_t<T>, bool>
    };

    template <typename T>
    inline constexpr bool is_twos_complement_v{
      !std::is_signed_v<T>
      || std::numeric_limits<T>::min() == -std::numeric_limits<T>::max() - 1
    };

    template <typename To, typename From>
    inline constexpr bool has_integer_bit_cast_fallback_v{
      is_supported_fallback_integer_v<To>
      && is_supported_fallback_integer_v<From>
      && sizeof(To) == sizeof(From)
      && is_twos_complement_v<To>
      && is_twos_complement_v<From>
    };

    /**
     * @brief Reinterprets unsigned integer bits as the corresponding signed
     *   integer value of the same width.
     *
     * @tparam Signed - Signed destination integer type.
     * @param value - Source bit pattern in the matching unsigned type.
     * @return Signed - Signed value with the same bit pattern.
     */
    template <typename Signed>
    constexpr Signed unsigned_bits_to_signed(std::make_unsigned_t<Signed> value) noexcept
    {
      static_assert(std::is_signed_v<Signed>, "Signed must be signed");

      using Unsigned = std::make_unsigned_t<Signed>;

      static_assert(sizeof(Signed) == sizeof(Unsigned), "Sizes must match");
      static_assert(
        std::numeric_limits<Unsigned>::digits == std::numeric_limits<Signed>::digits + 1,
        "Bit widths must match"
      );
      static_assert(
        std::numeric_limits<Signed>::min() == -std::numeric_limits<Signed>::max() - 1,
        "Requires two's-complement"
      );

      constexpr auto smax{ static_cast<Unsigned>(std::numeric_limits<Signed>::max()) };

      return value <= smax
        ? static_cast<Signed>(value)
        : static_cast<Signed>(
          Signed{-1} - static_cast<Signed>(static_cast<Unsigned>(~value))
        );
    }

    /**
     * @brief Fallback constexpr bit-cast for same-size non-bool integral
     *   types.
     *
     * @tparam To - Destination integral type.
     * @tparam From - Source integral type.
     * @param from - Source value whose bits are copied.
     * @return To - Value with the same bit pattern as \p from.
     */
    template <typename To, typename From>
    constexpr To integer_bit_cast(From from) noexcept
    {
      static_assert(
        has_integer_bit_cast_fallback_v<To, From>,
        "Constexpr::bit_cast fallback supports only same-size non-bool integral types."
      );

      using FromUnsigned = std::make_unsigned_t<From>;
      using ToUnsigned = std::make_unsigned_t<To>;

      auto const source_bits{ static_cast<FromUnsigned>(from) };
      auto const target_bits{ static_cast<ToUnsigned>(source_bits) };

      if constexpr (std::is_unsigned_v<To>) {
        return static_cast<To>(target_bits);
      } else {
        return unsigned_bits_to_signed<To>(target_bits);
      }
    }
  } // namespace detail

  /**
   * @brief Reports whether \c Constexpr::bit_cast has a native constexpr
   *   backend on the current compiler.
   */
  inline constexpr bool has_native_constexpr_bit_cast_v{
    detail::has_native_constexpr_bit_cast_v
  };

  /**
   * @brief Reports whether \c Constexpr::bit_cast is available in a constexpr
   *   context for the given source and destination types.
   *
   * @tparam To - Destination type.
   * @tparam From - Source type.
   */
  template <typename To, typename From>
  inline constexpr bool has_constexpr_bit_cast_v{
    has_native_constexpr_bit_cast_v || detail::has_integer_bit_cast_fallback_v<To, From>
  };

  /**
   * @brief Reinterprets the object representation of \p from as a value of
   *   type \c To.
   *
   * This is a C++17 backport of \c std::bit_cast. It participates in overload
   * resolution only when \c To and \c From have the same size and are both
   * trivially copyable. Without a native constexpr backend, only same-size
   * non-bool integral casts remain available in C++17; other type pairs fail
   * with a diagnostic and \c has_constexpr_bit_cast_v<To, From> reports that
   * the facility is unavailable.
   *
   * @tparam To - Destination type.
   * @tparam From - Source type.
   * @param from - Source object whose bits are copied.
   * @return To - Value whose bits match the object representation of \p from.
   */
  template <class To, class From
    , std::enable_if_t<
        sizeof(To) == sizeof(From)
        && std::is_trivially_copyable_v<To>
        && std::is_trivially_copyable_v<From>, bool> = true>
  constexpr To bit_cast(From const& from) noexcept
  {
#if defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L
    return std::bit_cast<To>(from);
#elif defined(__has_builtin) && __has_builtin(__builtin_bit_cast)
    return __builtin_bit_cast(To, from);
#else
    static_assert(
      has_constexpr_bit_cast_v<To, From>,
      "Constexpr::bit_cast is not available for these types on this compiler in C++17."
    );
    return detail::integer_bit_cast<To>(from);
#endif
  }
} // namespace Constexpr

#endif // BIT_HPP
