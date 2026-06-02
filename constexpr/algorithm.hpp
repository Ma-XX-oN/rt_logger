/**
 * @file algorithm.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Constexpr-capable stand-ins for STL algorithm utilities for C++17.
 * @version 0.1
 * @date 2026-05-30
 *
 * @copyright Copyright (c) 2026
 */
#ifndef CONSTEXPR_ALGORITHM_HPP
#define CONSTEXPR_ALGORITHM_HPP

#include <iterator>

namespace Constexpr {

  /**
   * @brief Reverses elements in a range.
   *
   * Equivalent to `std::reverse`, but constexpr in C++17.
   *
   * Taken from https://en.cppreference.com/cpp/algorithm/reverse
   *
   * @tparam BidirIt - Bidirectional iterator type.
   * @param first - First element in the range to reverse.
   * @param last - One past the last element in the range to reverse.
   */
  template<class BidirIt>
  constexpr void reverse(BidirIt first, BidirIt last)
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

  /**
   * @brief Copies a range into a destination range.
   *
   * Equivalent to `std::copy`, but constexpr in C++17.
   *
   * @tparam SrcIt - Source iterator type.
   * @tparam DstIt - Destination iterator type.
   * @param src_begin - Start of the source range.
   * @param src_end - One past the end of the source range.
   * @param dst_begin - Start of the destination range.
   *
   * @return DstIt - The new location of the \p dst_begin after copy.
   */
  template <typename SrcIt, typename DstIt>
  constexpr DstIt copy(SrcIt src_begin, SrcIt src_end, DstIt dst_begin) {
    while (src_begin != src_end) {
      *dst_begin++ = *src_begin++;
    }
    return dst_begin;
  }

} // namespace Constexpr

#endif // CONSTEXPR_ALGORITHM_HPP
