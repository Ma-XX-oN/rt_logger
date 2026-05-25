#ifndef ENUM_REGISTRY_HPP
#define ENUM_REGISTRY_HPP

#include <iterator>
#include <utility>
#include <tuple>
#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <cassert>
#include "bitwise_enum.hpp"
#include "constexpr_cstr.hpp"
#include "constexpr_num_to_StrArray.hpp"

// All logged enums need to be registered
template<typename E>
struct enum_info {};

enum enum_test {
  val0, val1, val2
};

template<std::size_t ID, char const *Str>
struct enum_base : std::integral_constant<int, ID>
{
  // template<typename E>
  // static constexpr to_string()
};

namespace impl {

  /**
   * @brief Gets the string length.
   * 
   * @param str - String to get the length from.
   * @return std::size_t - The length of the string.
   */
  constexpr std::size_t strlen(char const* str) {
    std::size_t i = 0;
    for (; str[i]; ++i) {}
    return i;
  }
  
  /**
   * @brief Gets the maximum string length from a list of enum, c-string tuples.
   * 
   * @tparam N - The tuple list length.
   * @tparam E - Enum in the tuple.
   * @return std::size_t - Max string length in the list.
   */
  template<std::size_t N, typename E>
  constexpr std::size_t max_len(std::tuple<E, Constexpr::CStr> const (&list)[N]) {
    constexpr std::size_t N = std::size(list);
    std::size_t max_value{0};
    for (auto const& v : list) {
      max_value = std::max(max_value, strlen(std::get<1>(v)));
    }
    return max_value;
  }

  /**
   * @brief Places the characters in \p Str into an array and return it.
   *
   * @tparam Str - A pointer to a constexpr string.
   * @return auto - An std::array that stores the \p Str text.  Includes the
   *   final NUL.
   */
  template<std::size_t N>
  constexpr auto str_to_array(Constexpr::CStr const& Str) {
    std::array<char, N> arr{};
    for (std::size_t i = 0; i < N; ++i) {
      arr[i] = Str[i];
    }
    return arr;
  }

  /**
   * @brief Copies the items from \p src to \p dst in a forward manner.
   *
   *   NOTE: it is UB to write beyond the end of \p dst.
   *
   * @tparam T - The type of elements being copied.
   * @param dst - Destination container.
   * @param dst_i - Offset in destination container to write to.
   * @param src - Source container.
   * @param src_begin - Offset in source container to read from.
   * @param src_end - One past last offset in source container to read from.
   * @return T& - Reference to the \p dst container.
   */
  template<typename T>
  constexpr T& copy(T& dst, std::size_t dst_i,
    T const& src, std::size_t src_begin, std::size_t src_end)
  {
    while (src_end != src_begin) {
      dst[dst_i++] = src[src_begin++];
    }
    return dst;
  }

} // namespace impl

constexpr char const enum_test_name[] = "enum_test";

constexpr auto EnumName = enum_test_name;
constexpr auto NumFmt = eNumFmt::Dec;
using Enum = enum_test;

template<>
struct enum_info<Enum> : enum_base<1, EnumName>
{
  using MapType = std::tuple<Enum, Constexpr::CStr>;

  static constexpr int find(Enum e) {
    for (int i = 0; i < std::size(values); ++i) {
      if (std::get<0>(values[i]) == e) {
        return i;
      }
    }
    return -1;
  }

  static constexpr MapType values[] {
    { val0, "val0" },
    { val1, "val1" },
    { val2, "val2" },
  };

  using UT = std::underlying_type_t<Enum>;
  static constexpr auto MAX_POSSIBLE_DIGITS{
    (NumFmt & eNumFmt::BaseMask) == eNumFmt::Dec ? 1 + impl::max_digits_required<UT>(10) :
    (NumFmt & eNumFmt::BaseMask) == eNumFmt::Bin ? 1 + impl::max_digits_required<UT>(2)  :
    (NumFmt & eNumFmt::BaseMask) == eNumFmt::Oct ? 1 + impl::max_digits_required<UT>(8)  :
    (NumFmt & eNumFmt::BaseMask) == eNumFmt::Hex ? 1 + impl::max_digits_required<UT>(16) :
    throw "Impossible. There are only 4 options"
  };
  static constexpr auto NUM_DECORATOR_SIZE{ sizeof(Enum) / 4 + 3 };

  template<Enum E>
  static constexpr auto to_string() {

    constexpr auto UNKNOWN { "*UNKNOWN*" };
    constexpr std::size_t N {
      std::max({
        impl::max_len(values),
        impl::strlen(UNKNOWN) + NUM_DECORATOR_SIZE,
        impl::strlen(enum_test_name)
      })
    };

    // append number
    return impl::str_to_array<UNKNOWN>();
  }
};

#endif // ENUM_REGISTRY_HPP
