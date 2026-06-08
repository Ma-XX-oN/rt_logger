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
#include <cstdint>
#include "bit.hpp"

namespace Constexpr {
/**
 * @brief Finds the index of the least significant set bit.
 *
 * @tparam ValueT - Enum or integral type of \p value.
 * @param value - Value to search.
 * @return std::size_t - Zero-based bit index of the least significant \c 1 bit.
 * @throws std::invalid_argument if \p value == \c 0.
 */
template <typename ValueT>
constexpr std::size_t least_set_bit_index(ValueT value) {
  auto v{ make_unsigned_equivalent(value) };
  if (!v) {
    throw std::invalid_argument("mask cannot be 0");
  }
  std::size_t i{0};
  while (!(v & 1)) {
    v >>= 1;
    ++i;
  }
  return i;
}

/**
 * @brief Gets the number of bits set in a value.
 *
 * @tparam ValueT - Enum or integral type of \p value.
 * @param value - Value whose set bits are counted.
 * @return std::uint8_t - Number of bits set.
 */
template <typename ValueT>
constexpr std::uint8_t count_bits_set(ValueT value) {
  auto v{ make_unsigned_equivalent(value) };
  std::uint8_t set_bit_count{0};

  static_assert(
    std::numeric_limits<decltype(v)>::digits <= std::numeric_limits<decltype(set_bit_count)>::max()
    , "Cannot represent number of bits in mask.");

  while (v) {
    v &= v - 1;
    ++set_bit_count;
  }
  return set_bit_count;
}

/**
 * @brief Indicates whether exactly one bit is set.
 *
 * @tparam ValueT - Enum or integral type of \p value.
 * @param value - Value being checked.
 * @return \c true if exactly one bit is set.
 * @return \c false if no bits are set or more than one bit is set.
 */
template <typename ValueT>
constexpr bool has_only_one_bit_set(ValueT value) {
  auto const bits{ make_unsigned_equivalent(value) };
  return !(!bits || (bits & (bits - 1u)) != 0u);
}

/**
 * @brief Packs the bits selected by \p mask into a contiguous field.
 *
 * Iterates from the least significant set bit in \p mask upward, copies each
 * bit from \p value whose corresponding mask bit is \c 1, and removes the gaps
 * where the mask has \c 0 bits.
 *
 * @tparam ValueT - Enum or integral type of \p value.
 * @tparam MaskT - Enum or integral type of \p mask.
 * @tparam CondensedT - Type of the packed result; defaults to
 *   \c underlying_equivalent_t<ValueT>.
 * @param mask - Mask value.
 * @param value - Value to condense.
 * @param align_to_lsb - If \c true, place the condensed field in the least
 *   significant bits of the result. If \c false, leave the condensed field
 *   aligned so its least significant bit stays at the original least
 *   significant set bit of \p mask.
 * @param sign_extend - Use the value at the most significant set bit of the mask as
 *   a sign bit, which is to be extended in the resulting value.  Result is
 *   treated as a negative value if the underlying equivalent type is a signed
 *   type.
 * @return CondensedT - Packed result.
 * @throws std::invalid_argument if \p mask == \c 0.
 */
template <typename ValueT, typename MaskT, typename CondensedT = underlying_equivalent_t<ValueT>>
constexpr CondensedT condense(MaskT mask, ValueT value, bool align_to_lsb, bool sign_extend = false) {
  using uv_t = unsigned_equivalent_t<ValueT>;
  uv_t mask_{ static_cast<uv_t>(mask) };
  std::size_t min_bit{ least_set_bit_index(mask) };
  mask_ >>= min_bit;
  std::size_t set_bit_count{ 0 };
  uv_t value_{ static_cast<uv_t>(static_cast<uv_t>(value) >> min_bit) };
  uv_t top_bit_mask{ uv_t{1} << (std::numeric_limits<uv_t>::digits - 1) };
  uv_t condensed_value{ 0 };
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
  std::size_t shift_back{ std::numeric_limits<uv_t>::digits - set_bit_count
     - (align_to_lsb
      ? 0       // adjust to lsb
      : min_bit // adjust back to mask original location
    ) };

  if (sign_extend) {
    bool is_negative { (condensed_value & top_bit_mask) != 0 };
    if (is_negative) {
      uv_t extend{ static_cast<uv_t>(~(static_cast<uv_t>(~0) >> shift_back)) };
      condensed_value >>= shift_back;
      return bit_cast<CondensedT>(static_cast<uv_t>(condensed_value | extend));
    }
  }
  return bit_cast<CondensedT>(static_cast<uv_t>(condensed_value >> shift_back));
}

/**
 * @brief Scatters a contiguous bit field back into the positions selected by
 *   \p mask.
 *
 * This is the inverse layout operation of \c condense: bits are read from a
 * contiguous field and written into the \c 1 positions of \p mask, leaving \c 0
 * positions in the masked span clear.
 *
 * @tparam ValueT - Enum or integral type of the expanded result.
 * @tparam MaskT - Enum or integral type of \p mask.
 * @param mask - Mask value.
 * @param value - Value to expand.
 * @param read_from_lsb - If \c true, read bits from the least significant bits of
 *   \p value. If \c false, read bits from a contiguous field whose least
 *   significant bit starts at the least significant set bit of \p mask.
 * @return ValueT - Value with bits expanded into the positions selected by \p mask.
 * @throws std::invalid_argument if \p mask == \c 0.
 */
template <typename ValueT, typename MaskT>
constexpr ValueT expand(MaskT mask, unsigned_equivalent_t<ValueT> value, bool read_from_lsb) {
  using uv_t = unsigned_equivalent_t<ValueT>;
  uv_t mask_{ static_cast<uv_t>(mask) };
  std::size_t min_bit{ least_set_bit_index(mask) };
  mask_ >>= min_bit;
  std::size_t mask_span{ 0 };
  uv_t value_{
    read_from_lsb
      ? static_cast<uv_t>(value)
      : static_cast<uv_t>(static_cast<uv_t>(value) >> min_bit)
  };
  uv_t top_bit_mask{ uv_t{1} << (std::numeric_limits<uv_t>::digits - 1) };
  uv_t expanded_value{ 0 };
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
  std::size_t shift_back{ std::numeric_limits<uv_t>::digits - mask_span
     - min_bit // adjust back to mask original location
  };
  return bit_cast<ValueT>(static_cast<uv_t>(expanded_value >> shift_back));
}

} // namespace Constexpr

#endif // CONSTEXPR_MASKED_BITS_HPP
