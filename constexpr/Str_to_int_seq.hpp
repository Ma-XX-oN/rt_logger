/**
 * @file Str_to_int_seq.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Convert a string to an integer_sequence.
 * @version 0.1
 * @date 2026-05-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <cstring>
#include <cstdint>
#include "CStr.hpp"
#include <utility>

template <char...Cs>
using char_seq = std::integer_sequence<char, Cs...>;

template <typename T>
struct str_to_seq {
  using type = T;
};

template <char C, char...Cs>
struct str_to_seq<char_seq<C, Cs...>> {
  Constexpr::CStr const& str;

  constexpr str_to_seq(Constexpr::CStr const& str)
  : str(str)
  {
  }
  
  constexpr auto seq() const {
    if constexpr (sizeof(Cs) == 0) {
      return 
    }
  }
};

template <char C, char...Cs>
constexpr auto append(char_seq<Cs...>) -> char_seq<Cs..., C>;

void fn() {
  constexpr Constexpr::CStr s{ "Hello" };
  constexpr auto x1 { append<s[0]>(char_seq<>{}) };
}