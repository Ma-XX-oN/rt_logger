/**
 * @file int_to_string.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Formats an integer into a constexpr string object.
 * @version 0.1
 * @date 2026-05-24
 *
 * @copyright Copyright (c) 2026
 *
 * NODE: Incomplete
 */
#ifndef CONSTEXPR_INT_TO_STRING_HPP
#define CONSTEXPR_INT_TO_STRING_HPP

#include <cstdint>
#include "algorithm.hpp"
#include "bitwise_enum.hpp"
#include "string.hpp"

namespace Constexpr {
  /**
  * @brief Shows how an enum is to be viewed when viewed as a number.
  */
  enum eIntFmt : std::uint8_t {
    BaseMask                  = 0b00000011, // If any bits in BaseMask set, then not decimal
    Dec                       = 0b00000000, // Make decimal value string
    Bin                       = 0b00000001, // Make binary value string
    Oct                       = 0b00000010, // Make octal value string
    Hex                       = 0b00000011, // Make hexadecimal value string
    ShowLeadingBase           = 0b00000100, // 0x - hex, 0o - octal, 0b - binary. Only when BaseMask not 0.
    ShowPlusSign              = 0b00000100, // Use '+' for non negative values when signed number and BaseMask is 0.
    ShowUppercase             = 0b00001000, // Only meaningful for hex

    AlignRight                = 0b00010000, // 0 = left, 1 = right

    // Base 10 - ShowLeadingBase has no effect
    PadWithSpace              = 0b00100000, // 0 = '0', 1 = ' '. Only when AlignRight is set.
    SignNextToNumber          = 0b01000000, // 0 = Aligned left, 1 = next to number.  Only when AlignRight is set.
    Unused                    = 0b10000000,

    // Not base 10
    //  AlignRight && ShowLeadingBase &&  PadWithSpace => 0x next to min width number
    //  AlignRight && ShowLeadingBase && !PadWithSpace => 0x left most of field
    // !AlignRight && ShowLeadingBase                  => 0x next to min width number
  };
} // namespace Constexpr

template<>
struct BitwiseOps<Constexpr::eIntFmt> : std::true_type {};

namespace Constexpr {
namespace impl {
  /**
   * @brief Calculates the number of characters required to display value.
   *
   * @tparam T - Type of number to be displayed.
   * @param value - Value to get info for.
   * @param base - Base to display in.  Can be any positive number, but 2, 8,
   *   10, and 16 are the ones most likely used for binary, octal, decimal and
   *   hex respectively.
   * @return std::size_t - Number of chars to represent number assuming base 10
   *   uses a sign and 2, 8 and 16 do not.
   */
  template <typename T
    , std::enable_if_t<std::is_integral_v<T>, bool> = true>
  constexpr std::size_t digits_required(T value, T base) {
    std::size_t digits{ value == 0 || (std::is_signed_v<T> && value < 0 && base == 10) };
    if (base == 10) {
      while (value) {
        value /= base;
        ++digits;
      }
    } else {
      using UT = std::make_unsigned_t<T>;
      UT v { static_cast<UT>(value) };
      while (v) {
        v /= base;
        ++digits;
      }
    }
    return digits;
  }

  /**
   * @brief Calculates the maximum number of characters required to display the
   *   largest / smallest number.
   *
   * @tparam T - Type of number to be displayed.
   * @param base - Base to display in.  Can be any positive number, but 2, 8,
   *   10, and 16 are the ones most likely used for binary, octal, decimal and
   *   hex respectively.
   * @return std::size_t - Retuns the number of characters required to represent
   *   the value.
   */
  template <typename T
    , std::enable_if_t<std::is_integral_v<T>, bool> = true>
  constexpr std::size_t max_digits_required(T base) {
    if (std::is_signed_v<T>) {
      return digits_required(std::numeric_limits<T>::min(), base);
    } else {
      return digits_required(std::numeric_limits<T>::max(), base);
    }
  }

  /**
   * @brief Gets the max number of characters to store string rep of enum value.
   *
   * @tparam Int - Integer to display.
   * @tparam IntFmt - Info stating how to format \p Int.
   * @return std::size_t - Size of stringized number, including NUL.
   */
  template <typename Int, eIntFmt IntFmt>
  constexpr std::size_t max_digits_required() {
    constexpr int bases[] { 10, 2, 8, 16 };
    using UT = std::underlying_type_t<Int>;
    constexpr int base{ bases[IntFmt & BaseMask] };
    if (base == 10) {
      return max_digits_required<UT>(base) + 1;
    } else {
      return max_digits_required<UT>(base) + 1
        // add 2 chars for the leading base prefix
        + (IntFmt & eIntFmt::ShowLeadingBase ? 2 : 0);
    }
  }

  template <eIntFmt IntFmt>
  struct enum_fmt_traits {
    static constexpr int base_index{
      static_cast<int>(IntFmt & eIntFmt::BaseMask)
    };
    static constexpr int base{ [] {
      constexpr int bases[] { 10, 2, 8, 16 };
      return bases[base_index];
    }() };
    static constexpr char base_symbol{ " box"[base_index] };
    static constexpr char const* digits{
      IntFmt & eIntFmt::ShowUppercase
      ? "0123456789ABCDEF"
      : "0123456789abcdef"
    };
    static constexpr bool align_right{ IntFmt & eIntFmt::AlignRight };
    static constexpr bool pad_with_space{ IntFmt & eIntFmt::PadWithSpace };
    static constexpr bool sign_next_to_number{
      IntFmt & eIntFmt::SignNextToNumber
    };
    static constexpr bool show_leading_base{
      IntFmt & eIntFmt::ShowLeadingBase
    };
    static constexpr bool show_plus_sign{
      IntFmt & eIntFmt::ShowPlusSign
    };
  };

  /**
   * @brief Append one character to a reversed string buffer.
   *
   * @tparam It - Iterator type for the output buffer.
   * @param it - Current insertion point. Advanced after writing.
   * @param end - One past the end of the output buffer.
   * @param ch - Character to append.
   */
  template <typename It>
  constexpr void append_char(It& it, It end, char ch) {
    assert(it != end);
    *it++ = ch;
  }

  /**
   * @brief Append a reversed numeric base prefix to a string buffer.
   *
   * Appends the characters for `0b`, `0o`, or `0x` in reverse order because
   * the number formatter builds the full string backwards before one final
   * `reverse()` call.
   *
   * @tparam It - Iterator type for the output buffer.
   * @param it - Current insertion point. Advanced after writing.
   * @param end - One past the end of the output buffer.
   * @param base_symbol - One of `b`, `o`, or `x`.
   */
  template <typename It>
  constexpr void append_reversed_base_prefix(It& it, It end, char base_symbol) {
    append_char(it, end, base_symbol);
    append_char(it, end, '0');
  }

  /**
   * @brief Append the digits of an integer in reverse order.
   *
   * @tparam IntFmt - How to format the integer.
   * @tparam T - Integer type of the value.
   * @tparam It - Iterator type for the output buffer.
   * @param it - Current insertion point. Advanced after writing.
   * @param end - Position reserved for the terminating NUL.
   * @param value - Value to format.
   */
  template <eIntFmt IntFmt, typename T, typename It
    , std::enable_if_t<std::is_integral_v<T>, bool> = true>
  constexpr void append_reversed_digits(It& it, It end, T value) {
    using fmt = enum_fmt_traits<IntFmt>;

    if (fmt::base == 10) {
      for (; it != end && value; ++it) {
        if constexpr (std::is_signed_v<T>) {
          if (value < 0) {
            *it = '0' - (value % fmt::base);
          } else {
            *it = '0' + (value % fmt::base);
          }
        } else {
          *it = '0' + (value % fmt::base);
        }
        value /= fmt::base;
      }
    } else {
      using UT = std::make_unsigned_t<T>;
      constexpr UT masks[] { 0, 1, 7, 15 };
      constexpr auto mask { masks[fmt::base_index] };
      constexpr int shifts[] { 0, 1, 3, 4 };
      constexpr auto shift { shifts[fmt::base_index] };
      UT v { static_cast<UT>(value) };
      for (; it != end && v; ++it) {
        *it = fmt::digits[v & mask];
        v >>= shift;
      }
    }
  }

  /**
   * @brief Append sign, base prefix, and padding in reverse order.
   *
   * @tparam IntFmt - How to format the integer.
   * @tparam IsSigned - States if the type to stringize a signed type.
   * @tparam It - Iterator type for the output buffer.
   * @param it - Current insertion point. Advanced after writing.
   * @param end - One past the end of the output buffer.
   * @param is_negative - Whether the original value is negative.
   */
  template <eIntFmt IntFmt, bool IsSigned, typename It>
  constexpr void append_reversed_affixes(It& it, It end, bool is_negative) {
    using fmt = enum_fmt_traits<IntFmt>;
    It pad_end { end };

    if (fmt::align_right) {
      if (fmt::base == 10) {
        if (is_negative) {
          if (fmt::sign_next_to_number) {
            append_char(it, end, '-'); // sign next to number
          } else {
            --pad_end; // (1) reserve space for sign
          }
        } else if (IsSigned && fmt::show_plus_sign) {
          if (fmt::sign_next_to_number) {
            append_char(it, end, is_negative ? '-' : '+');
          } else {
            --pad_end; // (1) reserve space for sign
          }
        }
      } else if (fmt::show_leading_base) {
        if (fmt::pad_with_space) {
          append_reversed_base_prefix(it, end, fmt::base_symbol);
        } else {
          pad_end -= 2;
        }
      }

      for (; it != pad_end; ++it) {
        *it = fmt::pad_with_space ? ' ' : '0';
      }

      if (fmt::base == 10) {
        if (is_negative) {
          if (!fmt::sign_next_to_number) {
            append_char(it, end, '-'); // (1) use reserved space for sign
          }
        } else if (IsSigned && fmt::show_plus_sign) {
            // (1) use reserved space for sign
            append_char(it, end, is_negative ? '-' : '+');
        }
      } else if (fmt::show_leading_base && !fmt::pad_with_space) {
        append_reversed_base_prefix(it, end, fmt::base_symbol);
      }
    } else { // align left
      if (fmt::base == 10) {
        // sign always next to number when left aligned
        if (is_negative) {
          append_char(it, end, '-');
        } else if (IsSigned && fmt::show_plus_sign) {
          append_char(it, end, is_negative ? '-' : '+');
        }
      } else if (fmt::show_leading_base) {
        append_reversed_base_prefix(it, end, fmt::base_symbol);
      }
    }
  }
} // namespace impl

  /**
  * @brief Create text representation of an integer and return as a string.
  *
  * @tparam IntFmt - How to format the integer.
  * @tparam N - Storage capacity of the returned string, including the
  *   trailing null terminator.
  *
  *   NOTE: It is UB if too small for the number.  Use \c max_digits_required()+1
  *         with \c <IntType,eIntFmt> template parameters to help prevent this
  *         issue.
  *
  * @tparam T - Integer type of value to make into a string.
  * @param value - value to make into a string.
  * @return Constexpr::string<N> - String representation of the integer.
  */
  template <eIntFmt IntFmt, std::size_t N, typename T
    , std::enable_if_t<std::is_integral_v<T>, bool> = true>
  constexpr Constexpr::string<N> int_to_string(T value)
  {
    Constexpr::string<N> buf{ };
    buf.resize(buf.capacity());

    auto it { buf.begin() };
    auto end { buf.end() };
    bool const is_negative{ std::is_signed_v<T> && value < 0 };

    if (value) {
      impl::append_reversed_digits<IntFmt>(it, end, value);
    } else {
      static_assert(N >= 2);
      *it++ = '0';
    }

    impl::append_reversed_affixes<IntFmt, std::is_signed_v<T>>(
      it, end, is_negative);

    auto const count{ static_cast<std::size_t>(it - buf.begin()) };
    buf.resize(count);
    Constexpr::reverse(buf.begin(), buf.end());
    return buf;
  }
} // namespace Constexpr

#endif // CONSTEXPR_INT_TO_STRING_HPP
