#ifndef CONSTEXPR_NUM_TO_STR_ARRAY_HPP
#define CONSTEXPR_NUM_TO_STR_ARRAY_HPP

/**
 * @file constexpr_num_to_StrArray.hpp
 * @author your name (you@domain.com)
 * @brief Formats a number into a constexpr StrArray object.
 * @version 0.1
 * @date 2026-05-24
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <cstdint>
#include <array>
#include "bitwise_enum.hpp"

/**
 * @brief Shows how an enum is to be viewed when viewed as a number.
 */
enum eNumFmt : std::uint8_t {
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

template<>
struct BitwiseOps<eNumFmt> : std::true_type {};

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
    std::size_t digits{ value == 0 || std::is_signed_v<T> && value < 0 && base == 10 };
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
   * @tparam Num - Number to display as number.
   * @tparam NumFmt - Info stating how to format \p Num.
   * @return std::size_t - Size of stringized number, including NUL.
   */
  template <typename Num, eNumFmt NumFmt>
  constexpr std::size_t max_digits_required() {
    constexpr int bases[] { 10, 2, 8, 16 };
    using UT = std::underlying_type_t<Num>;
    constexpr int base{ bases[NumFmt & BaseMask] };
    if (base == 10) {
      return max_digits_required<UT>(base) + 1;
    } else {
      return max_digits_required<UT>(base) + 1
        // add 2 chars for the leading base prefix
        + (NumFmt & eNumFmt::ShowLeadingBase ? 2 : 0);
    }
  }

  /**
   * @brief Reverses elements in an array.
   *
   * Using this because it's constexpr and runs in C++17.
   *  
   * Taken from https://en.cppreference.com/cpp/algorithm/reverse
   *
   * @tparam BidirIt - Bidirectional iterator type.
   * @param first - First element in container to reverse.
   * @param last - One past last element in container to reverse.
   */
  template<class BidirIt>
  constexpr // since C++20
  void reverse(BidirIt first, BidirIt last)
  {
    using iter_cat = typename std::iterator_traits<BidirIt>::iterator_category;
    
    // Tag dispatch, e.g. calling reverse_impl(first, last, iter_cat()),
    // can be used in C++14 and earlier modes.
    if constexpr (std::is_base_of_v<std::random_access_iterator_tag, iter_cat>)
    {
      if (first == last)
        return;
      
      for (--last; first < last; (void)++first, --last)
        std::iter_swap(first, last);
    }
    else
      while (first != last && first != --last)
        std::iter_swap(first++, last);
  }

  template <eNumFmt NumFmt>
  struct enum_fmt_traits {
    static constexpr int base_index{
      static_cast<int>(NumFmt & eNumFmt::BaseMask)
    };
    static constexpr int base{ [] {
      constexpr int bases[] { 10, 2, 8, 16 };
      return bases[base_index];
    }() };
    static constexpr char base_symbol{ " box"[base_index] };
    static constexpr char const* digits{
      NumFmt & eNumFmt::ShowUppercase
      ? "0123456789ABCDEF"
      : "0123456789abcdef"
    };
    static constexpr bool align_right{ NumFmt & eNumFmt::AlignRight };
    static constexpr bool pad_with_space{ NumFmt & eNumFmt::PadWithSpace };
    static constexpr bool sign_next_to_number{
      NumFmt & eNumFmt::SignNextToNumber
    };
    static constexpr bool show_leading_base{
      NumFmt & eNumFmt::ShowLeadingBase
    };
    static constexpr bool show_plus_sign{
      NumFmt & eNumFmt::ShowPlusSign
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
   * @tparam NumFmt - How to format the number.
   * @tparam T - Integer type of the value.
   * @tparam It - Iterator type for the output buffer.
   * @param it - Current insertion point. Advanced after writing.
   * @param end - Position reserved for the terminating NUL.
   * @param value - Value to format.
   */
  template <eNumFmt NumFmt, typename T, typename It
    , std::enable_if_t<std::is_integral_v<T>, bool> = true>
  constexpr void append_reversed_digits(It& it, It end, T value) {
    using fmt = enum_fmt_traits<NumFmt>;

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
   * @tparam NumFmt - How to format the number.
   * @tparam IsSigned - States if the type to stringize a signed type.
   * @tparam It - Iterator type for the output buffer.
   * @param it - Current insertion point. Advanced after writing.
   * @param pad_end - Position reserved for the terminating NUL.
   * @param end - One past the end of the output buffer.
   * @param is_negative - Whether the original value is negative.
   */
  template <eNumFmt NumFmt, bool IsSigned, typename It>
  constexpr void append_reversed_affixes(It& it, It pad_end, It end,
    bool is_negative) {
    using fmt = enum_fmt_traits<NumFmt>;

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
 * @brief Create text representation of number and return in std::array.
 *
 * @tparam Fmt - How to format the number.
 * @tparam N - Number of elements to use for std::array.
 *
 *   NOTE: It is UB if too small for the number.  Use \c max_digits_required()
 *         with \c <NumType,eNumFmt> template parameters to help prevent this
 *         issue.
 *
 * @tparam T - Integer type of value to make into a string. 
 * @param value - value to make into a string. 
 * @return std::array<char, N> - Storage of string representation.
 */
template <eNumFmt NumFmt, std::size_t N, typename T
  , std::enable_if_t<std::is_integral_v<T>, bool> = true>
constexpr std::array<char, N> to_str_as_arr(T value)
{
  std::array<char, N> str{ };
  auto it { str.begin() };
  auto end { str.end() - 1 };
  const bool is_negative = std::is_signed_v<T> && value < 0;

  if (value) {
    append_reversed_digits<NumFmt>(it, end, value);
  } else {
    static_assert(N >= 2);
    str[it++] = '0';
  }

  append_reversed_affixes<NumFmt, std::is_signed_v<T>>(
    it, end, str.end(), is_negative);
  reverse(str.begin(), it);
  assert(it != str.end()); // make sure haven't overwritten the last char
  return str;
}

#endif // CONSTEXPR_NUM_TO_STR_ARRAY_HPP
