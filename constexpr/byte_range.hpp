/**
 * @file byte_range.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Compile-time range requirement enforcement for byte-like iterator
 *   ranges.
 * @version 0.1
 * @date 2026-06-09
 *
 * Provides a family of void helper functions whose sole purpose is to fire
 * \c static_assert diagnostics when an iterator range does not meet the
 * byte-like, readable, writable, or read-write requirements.  Call the
 * appropriate function at the top of any template that operates on a byte
 * buffer to get clear, centralised error messages at the call site.
 *
 * @copyright Copyright (c) 2026
 */

#ifndef BYTE_RANGE_HPP
#define BYTE_RANGE_HPP

#include <iterator>
#include <limits>
#include <type_traits>
#include "type_traits.hpp"

static_assert(
  std::numeric_limits<unsigned char>::digits == 8,
  "byte_range requires 8-bit byte storage"
);

namespace Require {

/**
 * @brief Enforces that [\p begin, \p end) is a bidirectional byte-like range.
 *
 * Fires \c static_assert if:
 * - \p ItB is not a bidirectional iterator,
 * - \p ItE is not a valid sentinel for \p ItB, or
 * - the iterator's \c value_type is not byte-like (\c char, \c signed char,
 *   \c unsigned char, or \c std::byte).
 *
 * @tparam ItB Bidirectional iterator type.
 * @tparam ItE Sentinel type for \p ItB.
 */
template <typename ItB, typename ItE>
constexpr void byte_like_range(ItB, ItE const)
{
  static_assert(Constexpr::is_bidirectional<ItB>, "ItB must be a bidirectional iterator");
  static_assert(Constexpr::is_sentinel_for_v<ItE, ItB>, "ItE must be a sentinel for ItB");

  using ItVT = typename std::iterator_traits<ItB>::value_type;
  static_assert(Constexpr::is_byte_like_v<ItVT>, "Iterator value_type must be byte-like");
}

/**
 * @brief Enforces that [\p begin, \p end) is a writable byte-like range.
 *
 * Extends \c byte_like_range with an additional check that the iterator
 * supports assignment through its dereference operator.
 *
 * @tparam ItB Bidirectional iterator type.
 * @tparam ItE Sentinel type for \p ItB.
 */
template <typename ItB, typename ItE>
constexpr void byte_like_writable_range(ItB begin, ItE const end)
{
  byte_like_range(begin, end);
  using ItVT = typename std::iterator_traits<ItB>::value_type;
  using Ref = decltype(*std::declval<ItB&>());
  static_assert(std::is_assignable_v<Ref, ItVT>, "Iterator must be writable");
}

/**
 * @brief Enforces that [\p begin, \p end) is a readable byte-like range.
 *
 * Extends \c byte_like_range with an additional check that the iterator's
 * dereference result is convertible to its \c value_type.
 *
 * @tparam ItB Bidirectional iterator type.
 * @tparam ItE Sentinel type for \p ItB.
 */
template <typename ItB, typename ItE>
constexpr void byte_like_readable_range(ItB begin, ItE const end)
{
  byte_like_range(begin, end);
  using ItVT = typename std::iterator_traits<ItB>::value_type;
  using Ref = decltype(*std::declval<ItB&>());
  static_assert(std::is_convertible_v<Ref, ItVT>, "Iterator must be readable");
}

/**
 * @brief Enforces that [\p begin, \p end) is a readable and writable
 *   byte-like range.
 *
 * Combines \c byte_like_readable_range and \c byte_like_writable_range.
 *
 * @tparam ItB Bidirectional iterator type.
 * @tparam ItE Sentinel type for \p ItB.
 */
template <typename ItB, typename ItE>
constexpr void byte_like_read_write_range(ItB begin, ItE const end)
{
  byte_like_readable_range(begin, end);
  byte_like_writable_range(begin, end);
}

} // namespace Require

#endif // BYTE_RANGE_HPP
