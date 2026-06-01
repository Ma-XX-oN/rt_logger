/**
 * @file masked_bits.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Constexpr helpers for packing and scattering bits through sparse masks.
 * @version 0.1
 * @date 2026-05-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef CONSTEXPR_MASKED_BITS_HPP
#define CONSTEXPR_MASKED_BITS_HPP
#include <cstddef>
#include <stdexcept>
#include "type_traits.hpp"
#include <limits>

namespace Constexpr {
/**
 * @brief Finds the index of the least significant set bit.
 *
 * @tparam E - Enum or integral type of \p mask.
 * @param mask - Mask value.
 * @return std::size_t - Zero-based bit index of the least significant \c 1 bit.
 * @throws std::invalid_argument if \p mask == \c 0.
 */
template <typename E>
constexpr std::size_t least_set_bit_index(E mask) {
  auto mask_{ make_unsigned_equivalent(mask) };
  if (!mask_) {
    throw std::invalid_argument("mask cannot be 0");
  }
  std::size_t i{0};
  while (!(mask_ & 1)) {
    mask_ >>= 1;
    ++i;
  }
  return i;
}

/**
 * @brief Packs the bits selected by \p mask into a contiguous field.
 *
 * Iterates from the least significant set bit in \p mask upward, copies each
 * bit from \p value whose corresponding mask bit is \c 1, and removes the gaps
 * where the mask has \c 0 bits.
 *
 * @tparam E - Enum or integral type of \p mask and \p value.
 * @param mask - Mask value.
 * @param value - Value to condense.
 * @param align_to_lsb - If \c true, place the condensed field in the least
 *   significant bits of the result. If \c false, leave the condensed field
 *   aligned so its least significant bit stays at the original least
 *   significant set bit of \p mask.
 * @param sign_extend - Use the value at most significant set bit of the mask as
 *   a sign bit, which is to be extended in the resulting value.  Result is
 *   treated as a negative value if the underlying equivalent type is a signed
 *   type.
 * @return underlying_equivalent_t<E> - Packed result.
 * @throws std::invalid_argument if \p mask == \c 0.
 */
template <typename E>
constexpr auto condense(E mask, E value, bool align_to_lsb, bool sign_extend = false) {
  using T = unsigned_equivalent_t<E>;
  T mask_{ static_cast<T>(mask) };
  std::size_t min_bit{ least_set_bit_index(mask) };
  mask_ >>= min_bit;
  std::size_t set_bit_count{ 0 };
  T value_{ static_cast<T>(static_cast<T>(value) >> min_bit) };
  T top_bit_mask{ T{1} << (std::numeric_limits<T>::digits - 1) };
  T condensed_value{ 0 };
  do {
    if (mask_ & 1) {
      condensed_value >>= 1;
      if (value_ & 1) {
        condensed_value |= top_bit_mask;
      }
      ++set_bit_count;
    }
    value_ >>= 1;
    mask_  >>= 1;
  } while (mask_);
  std::size_t shift_back{ std::numeric_limits<T>::digits - set_bit_count
     - (align_to_lsb
      ? 0       // adjust to lsb
      : min_bit // adjust back to mask original location
    ) };

  if (sign_extend) {
    bool is_negative { (condensed_value & top_bit_mask) != 0 };
    if (is_negative) {
      T extend{ static_cast<T>(~(static_cast<T>(~0) >> shift_back)) };
      condensed_value >>= shift_back;
      return static_cast<underlying_equivalent_t<E>>(
        condensed_value | extend);
    }
  }
  return static_cast<underlying_equivalent_t<E>>(
    condensed_value >> shift_back);
}

/**
 * @brief Scatters a contiguous bit field back into the positions selected by
 *   \p mask.
 *
 * This is the inverse layout operation of \c condense: bits are read from a
 * contiguous field and written into the \c 1 positions of \p mask, leaving \c 0
 * positions in the masked span clear.
 *
 * @tparam E - Enum or integral type of \p mask.
 * @param mask - Mask value.
 * @param value - Value to expand.
 * @param read_from_lsb - If \c true, read bits from the least significant bits of
 *   \p value. If \c false, read bits from a contiguous field whose least
 *   significant bit starts at the least significant set bit of \p mask.
 * @return E - Value with bits expanded into the positions selected by \p mask.
 * @throws std::invalid_argument if \p mask == \c 0.
 */
template <typename E>
constexpr E expand(E mask, unsigned_equivalent_t<E> value, bool read_from_lsb) {
  using T = unsigned_equivalent_t<E>;
  T mask_{ static_cast<T>(mask) };
  std::size_t min_bit{ least_set_bit_index(mask) };
  mask_ >>= min_bit;
  std::size_t mask_span{ 0 };
  T value_{
    read_from_lsb
      ? static_cast<T>(value)
      : static_cast<T>(static_cast<T>(value) >> min_bit)
  };
  T top_bit_mask{ T{1} << (std::numeric_limits<T>::digits - 1) };
  T expanded_value{ 0 };
  do {
    expanded_value >>= 1;
    if (mask_ & 1) {
      if (value_ & 1) {
        expanded_value |= top_bit_mask;
      }
      value_ >>= 1;
    }
    ++mask_span;
    mask_  >>= 1;
  } while (mask_);
  std::size_t shift_back{ std::numeric_limits<T>::digits - mask_span
     - min_bit // adjust back to mask original location
  };
  return static_cast<E>(expanded_value >> shift_back);
}

} // namespace Constexpr

#endif // CONSTEXPR_MASKED_BITS_HPP
