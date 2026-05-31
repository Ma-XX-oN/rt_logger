/**
 * @file CStr.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Allows a c-string to be passed around more easily as a constexpr
 *   object.
 * @version 0.1
 * @date 2026-05-24
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef CONSTEXPR_CSTR_HPP
#define CONSTEXPR_CSTR_HPP

#include <cassert>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <limits>

namespace Constexpr
{
  /**
   * @brief An immutable contiguous container that stores a null-terminated
   *   string view with constexpr-friendly construction.
   *
   * Got rid of my implementation as the STL has one that works.
   */
  using CStr = std::string_view;

  /**
   * @brief Exchanges the contents of two string containers.
   *
   * Needed in c++17 because swap<std::string_view>() isn't a constexpr
   * function.
   *
   * @param lhs - Left-hand operand.
   * @param rhs - Right-hand operand.
   */
  inline constexpr void swap(CStr& lhs, CStr& rhs) noexcept {
    lhs.swap(rhs);
  }
} // namespace Constexpr

#endif // CONSTEXPR_CSTR_HPP
