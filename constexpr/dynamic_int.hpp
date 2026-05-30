/**
 * @file dynamic_int.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Encodes/decodes integers into/from a std::byte buffer in/from a 7 bit
 *   dynamic representation from/to a machine readable 8 bit representation.
 * @version 0.1
 * @date 2026-05-21
 *
 * This is a quick compression mechanism for dynamic length signed and unsigned
 * integers, making small numbers take the less than or the same amount of bytes
 * as it would for 8 bit values and larger numbers to take anywhere between 1-2
 * bytes more.
 *
 * The most significant bit on each byte is a continuation marker:
 *
 *   1 = number continues on next byte 0 = last byte for number
 *
 * If the number is signed
 *   - If the number is negative
 *     - The number is negated (all bits are flipped)
 *     - The most significant bit (0x40) of the most significant byte is set.
 *
 * Example (using 0z as a proxy for dint as binary.  Most significant bit for
 * each byte is the continuation bit):
 *
 *   ```cpp
 *   // Unsigned.
 *   0b01111111 == 0z01111111
 *
 *   // Unsigned overflowing to next byte
 *   0b11111111 == 0z00000001'11111111
 *
 *   // Signed bit (s) moved to most significant bit of 7 bits. Number bits inverted.
 *   0bs1111111 == 0z0s000000
 *
 *   // Signed bit (s) moved to most significant bit of 7 bits. Number bits inverted.
 *   0bs1111111 == 0z0s000000'10000000
 *
 *   0b00111111'11111111 ==
 *   0z01111111'11111111
 *
 *   0b00011111'11111111'11111111 ==
 *   0z01111111'11111111'11111111
 *
 *   0b00001111'11111111'11111111'11111111 ==
 *   0z01111111'11111111'11111111'11111111
 *
 *   ...
 *
 *                     0b11111111'11111111'11111111'11111111'11111111'11111111'11111111'11111111 ==
 *   0z00000001'11111111'11111111'11111111'11111111'11111111'11111111'11111111'11111111'11111111
 *   ```
 *
 * @copyright Copyright (c) 2026
 */

#ifndef DYNAMIC_INT_HPP
#define DYNAMIC_INT_HPP

#include <type_traits>
#include <cstddef>
#include <stdexcept>
#include <cassert>
#include <array>
#include <cstring>
#include <limits>
#include "ThrowNoThrow.hpp"

/**
 * @brief Encodes \p v_src into the byte range [`dst_begin_it`, `dst_end_it`).
 *
 * Does not throw because this is part of the RT code.
 *
 * Unsigned values are emitted in 7-bit payload chunks. Signed values are first
 * bitwise-negated when negative, then the sign is stored in the most
 * significant 7-bit chunk. If that chunk already uses bit `0x40` as payload, an
 * extra terminal byte is appended to store only the sign.
 *
 * The caller must provide enough space to store the encoded value. This
 * function asserts if the destination range is too small.
 *
 * @tparam T Type of \p v_src.
 * @param dst_begin_it First destination byte to write.
 * @param dst_end_it One-past-the-end of the destination range.
 * @param v_src Value to encode.
 *
 * @return Pointer one past the last byte written.
 */
template <typename T
  , std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
constexpr std::byte* encode_dint(std::byte* dst_begin_it, std::byte* const dst_end_it, T v_src)
{
  // Must always provide enough space to store value.
  using UT = std::make_unsigned_t<T>;
  UT v{ UT(v_src) };
  constexpr bool is_signed { std::is_signed_v<T> };
  bool is_negative { is_signed ? v_src < 0 : false };
  if (is_signed) {
    if (v_src < 0) {
      v = UT(~v);
    }
  }

  constexpr UT dint_payload { UT(0x7f) };
  constexpr auto continuation_bit { std::byte{0x80} };
  do {
    assert (dst_begin_it != dst_end_it || !"Not enough space to store in buffer");
    std::byte lsb{ std::byte(v & dint_payload) };
    if (v >>= 7) {
      lsb |= continuation_bit;
    }
    *dst_begin_it++ = lsb;
  } while (v);

  std::byte dst_sign_bit { std::byte{0x40} };
  if (is_signed) {
    auto last_most_sig_byte_it { dst_begin_it - 1 };
    auto sign_value { is_negative ? dst_sign_bit : std::byte{0} };
    if (std::byte{0} != (*last_most_sig_byte_it & dst_sign_bit)) {
      // Bit for sign already in use, so need another byte for sign.
      assert (dst_begin_it != dst_end_it || !"Not enough space to store in buffer");
      *last_most_sig_byte_it |= continuation_bit;
      *dst_begin_it++ = sign_value;
    } else {
      // Bit for sign not in use, so use it for sign.
      *last_most_sig_byte_it |= sign_value;
    }
  }
  return dst_begin_it;
}

/**
 * @brief Generates the dint and size of the dint in one go.
 *
 * Keeps from having to recompute the dint after getting the size.  Just have to
 * copy the dint afterwards to the new location, which is less work.
 *
 * @tparam N - Size of initial buffer to store dint into.
 */
template <std::size_t N>
struct DIntAndSize {
  std::array<std::byte, N> b_dst{};
  std::size_t final_offset{0};

  template <typename T>
  constexpr explicit DIntAndSize(T v_src) {
    auto end{ encode_dint(b_dst.begin(), b_dst.end(), v_src) };
    final_offset = end - b_dst.begin();
  }

  constexpr std::byte operator[](std::size_t i) const noexcept {
    return b_dst[i];
  }
  
  constexpr std::size_t size() const noexcept {
    return final_offset;
  }
};

template <typename T>
inline constexpr std::size_t max_dint_size_v{ [] {
  // Number of bits per T use to store the value sans sign bit.
  constexpr std::size_t value_bits{ std::numeric_limits<T>::digits };

  constexpr std::size_t payload_bytes{ (value_bits + 6) / 7 };

  constexpr bool needs_extra_sign_byte {
    std::is_signed_v<T> && (value_bits % 7 == 0) };

  return payload_bytes + needs_extra_sign_byte;
}() };

template <typename T>
DIntAndSize(T) -> DIntAndSize<max_dint_size_v<T>>;

/**
 * @brief Returns a \c std::array<std::byte,N> with the encoded data in it.
 * 
 * @tparam T - Type of v_src to encode.
 * @tparam v_src - Value to encode.
 * 
 * @return auto - Encoded value in a \c std::array<std::byte,N>.
 */
template <typename T, T v_src
  , std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
constexpr auto encode_dint() {
  constexpr DIntAndSize encoded{v_src};
  std::array<std::byte, encoded.size()> b_dst{};
  for (std::size_t i{ 0 }; i < encoded.size(); ++i) {
    b_dst[i] = encoded[i];
  }
  return b_dst;
}

/**
 * @brief Decodes a dint from the byte range [`src_begin_it`, `src_end_it`) and
 *   stores it in \p v_dst.
 *
 * On success, \p src_begin_it is advanced to the next unread byte after the
 * decoded dint.
 *
 * @tparam Throws \c Throw to throw on decode errors, or \c NoThrow to leave
 *   \p v_dst unchanged and restore \p src_begin_it on failure.
 * @tparam T Decoded integral type.
 * @param src_begin_it Reference to the first unread source byte. Updated to the
 *   next unread byte on success.
 * @param src_end_it One-past-the-end of the source range.
 * @param v_dst Storage for the decoded integer.
 *
 * @return \p v_dst.
 *
 * @throws std::overflow_error
 *   - The source range is empty at entry.
 *   - The dint is not terminated before \p src_end_it.
 *   - The decoded value requires more bits than fit in \p T.
 */
template <typename Throws, typename T
  , std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
constexpr T& decode_dint(std::byte const*& src_begin_it, std::byte const* const src_end_it, T& v_dst)
{
  using UT = std::make_unsigned_t<T>;
  auto to_int{ [] (std::byte v) {
    return std::to_integer<UT>(v);
  } };

  if (src_begin_it == src_end_it) {
    if constexpr (std::is_same_v<Throws, Throw>) {
      throw std::overflow_error("Starting offset exceeded bounds of src_begin_it buffer.");
    } else {
      return v_dst;
    }
  }

  // find last byte of dint
  auto continuation_bit { std::byte{0x80} };
  auto init_start_it { src_begin_it };
  while (to_int(*src_begin_it & continuation_bit)) {
    if (++src_begin_it == src_end_it) {
      break;
    }
  }

  if (src_begin_it == src_end_it) {
    if constexpr (std::is_same_v<Throws, Throw>) {
      throw std::overflow_error("dint value not terminated in src_begin_it buffer.");
    } else {
      src_begin_it = init_start_it;
      return v_dst;
    }
  }

  auto last_digit_it { src_begin_it };

  constexpr bool      is_signed{ std::is_signed_v<T> };
  constexpr std::byte  sign_bit{ is_signed ? std::byte{0x40} : std::byte{0} };
  constexpr UT     upper_7_bits{ static_cast<UT>(~(std::numeric_limits<UT>::max() >> 7)) };
  bool              is_negative{ is_signed && to_int(*src_begin_it & sign_bit) != 0 };
  UT v { to_int(*src_begin_it & ~(continuation_bit | sign_bit)) };
  while (src_begin_it != init_start_it) {
    if (v & upper_7_bits) {
      if (std::is_same_v<Throws, Throw>) {
        throw std::overflow_error("dint cannot fit in value type T.");
      } else {
        src_begin_it = init_start_it;
        return v_dst;
      }
    }
    v <<= 7;
    v |= to_int(*--src_begin_it & ~continuation_bit);
  }

  if (is_negative) {
    v = ~v;
  }
  ++(src_begin_it = last_digit_it);
  memcpy(&v_dst, &v, sizeof(T));
  return v_dst;
}

/**
 * @brief Decodes a dint from the byte range [`src_begin_it`, `src_end_it`) and
 *   returns it by value.
 *
 * This overload uses a temporary destination internally and returns the decoded
 * value directly.
 *
 * @tparam Throws \c Throw to throw on decode errors, or \c NoThrow to return a
 *   default-initialized value when decoding fails.
 * @tparam T Decoded integral type.
 * @param src_begin_it Reference to the first unread source byte. Updated to the
 *   next unread byte on success.
 * @param src_end_it One-past-the-end of the source range.
 *
 * @return The decoded dint as type \p T.
 *
 * @throws std::overflow_error
 *   - The source range is empty at entry.
 *   - The dint is not terminated before \p src_end_it.
 *   - The decoded value requires more bits than fit in \p T.
 */
template <typename Throws, typename T
  , std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
constexpr T decode_dint(std::byte const*& src_begin_it, std::byte const* const src_end_it)
{
  T v_dst {};
  return decode_dint<Throws>(src_begin_it, src_end_it, v_dst);
}

#endif // DYNAMIC_INT_HPP
