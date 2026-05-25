/**
 * @file dynamic_num.hpp
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
 *     - the number is negated (all bits are flipped)
 *   - The bits are left shifted by 1 and the sign bit is placed there 1 =
 *       negative 0 = positive
 *
 * Example (using 0z as a proxy for dnum as binary.  Most significant bit for
 * each byte is the continuation bit):
 *
 *   ```cpp
 *   // Unsigned.
 *   0b01111111 == 0z01111111
 *
 *   // Unsigned overflowing to next byte
 *   0b11111111 == 0z00000001'11111111
 *
 *   // Signed bit (s) moved to least significant bit. Number bits inverted.
 *   0bs0111111 == 0z0000000s
 *
 *   // Value is negated when stored and sign is made lsb.
 *   0bs1111111 == 0z00000000'1000000s
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

#ifndef DYNAMIC_NUM_HPP
#define DYNAMIC_NUM_HPP

#include <type_traits>
#include <cstddef>
#include <stdexcept>
#include <cassert>
#include <array>
#include "ThrowNoThrow.hpp"

/**
 * @brief Takes \p v_src and places it at \p offset in \p b_dst as a dynamic
 * int.
 *
 * Does not throw because this is part of the RT code.
 *
 * @tparam T - Type of v_src.
 * @param b_dst - Destination buffer.
 * @param N - Size of buffer.
 * @param offset - Offset in \p b_dst to write dynamic int.  Updated to next
 *   offset to write to or unchanged and may write partial value if it doesn't
 *   fit.
 * @param v_src - Value of int to write.
 *
 * @return
 *   Next location to write to or 0 if can't fit \p v_src in \p b_dst at \p
 *   offset.  May write partial value at \p offset even if it doesn't fit.
 */
template <typename T
  , std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
constexpr std::size_t encode_dnum(std::byte* b_dst, std::size_t N, std::size_t& offset, T v_src)
{
  using UT = std::make_unsigned_t<T>;
  UT v = UT(v_src);
  if constexpr(std::is_signed_v<T>) {
    if (v_src < 0) {
      v = (UT(~v) << 1) | UT(1);
    } else {
      v <<= 1;
    }
  }

  std::size_t old_offset(offset);
  do {
    if (offset >= N) {
      // Not enough space to encode
      offset = old_offset;
      return 0;
    }
    std::byte lsb = std::byte(v & UT(0x7f));
    if (v >>= 7) {
      lsb |= std::byte(0x80);
    }
    b_dst[offset++] = lsb;
  } while (v != 0);
  return offset;
}

/**
 * @brief Takes \p v_src and places it at \p offset in \p b_dst as a dynamic
 * int.
 *
 * Does not throw because this is part of the RT code.
 *
 * @tparam T - Type of v_src.
 * @tparam N - Size of buffer.
 * @param b_dst - Destination buffer.
 * @param offset - Offset in \p b_dst to write dynamic int.  Updated to next
 *   offset to write to or unchanged and may write partial value if it doesn't
 *   fit.
 * @param v_src - Value of int to write.
 *
 * @return
 *   Next location to write to or 0 if can't fit \p v_src in \p b_dst at \p
 *   offset.  May write partial value at \p offset even if it doesn't fit.
 */
template <typename T, std::size_t N
  , std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
constexpr std::size_t encode_dnum(std::byte (&b_dst)[N], std::size_t& offset, T v_src)
{
  return encode_dnum(b_dst, N, offset, v_src);
}

/**
 * @brief Generates the dnum and size of the dnum in one go.
 *
 * Keeps from having to recompute the dnum after getting the size.  Just have to
 * copy the dnum afterwards to the new location, which is less work.
 *
 * @tparam N - Size of initial buffer to store dnum into.
 */
template <std::size_t N>
struct DNumAndSize {
  std::array<std::byte, N> b_dst{};
  std::size_t final_offset{0};

  template <typename T>
  constexpr explicit DNumAndSize(T v_src) {
    encode_dnum(b_dst.data(), N, final_offset, v_src);
  }

  constexpr std::byte operator[](std::size_t i) const noexcept {
    return b_dst.data()[i];
  }
  
  constexpr std::size_t size() const noexcept {
    return final_offset;
  }
};

template <typename T>
DNumAndSize(T) -> DNumAndSize<sizeof(T)*2>;

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
constexpr auto encode_dnum() {
  constexpr DNumAndSize encoded{v_src};
  std::array<std::byte, encoded.size()> b_dst{};
  for (std::size_t i = 0; i < encoded.size(); ++i) {
    b_dst[i] = encoded[i];
  }
  return b_dst;
}

/**
 * @brief Reads dynamic num from \p b_src at \p offset and stores it in \p v_dst.
 *
 * @tparam Throws - \c Throw if throws on error. \c NoThrow if sets offset to 0
 *   and return v_dst is not updated on error.
 * @tparam T - Type of v_dst.
 * @param b_src - Source buffer.
 * @param N - Size of buffer.
 * @param offset - Offset in \p b_src to read dynamic int.  Updated to next offset
 *   to write to or to 0 if it doesn't fit.
 * @param v_dst - Storage for read integer.
 *
 * @return
 *   \p v_dst.
 *
 * @throws std::overflow_error
 *   - \p offset >= \p N.
 *   - dynamic number not terminated within \p b_src.
 *   - value retrieved can't fit in \p T type.
 */
template <typename Throws, typename T
  , std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
constexpr T& decode_dnum(std::byte const* b_src, std::size_t N, std::size_t& offset, T& v_dst)
{
  if (offset >= N) {
    if constexpr (std::is_same_v<Throws, Throw>) {
      throw std::overflow_error("Starting offset exceeded bounds of b_src buffer.");
    } else {
      offset = 0;
      return v_dst;
    }
  }

  // find last byte of dnum
  std::size_t last_offset = offset;
  while (bool(b_src[last_offset] & std::byte(0x80))) {
    if (++last_offset >= N) {
      if constexpr (std::is_same_v<Throws, Throw>) {
        throw std::overflow_error("dnum value not terminated in b_src buffer.");
      } else {
        offset = 0;
        return v_dst;
      }
    }
  }

  const std::size_t last_offset_ = last_offset;
  using UT = std::make_unsigned_t<T>;
  UT v = UT(b_src[last_offset]);
  while (last_offset-- != offset) {
    UT old_value = v;
    v <<= 7;
    if (static_cast<UT>(v) >> 7 != old_value) {
      if constexpr (std::is_same_v<Throws, Throw>) {
        throw std::overflow_error("dnum cannot fit inside of value type.");
      } else {
        offset = 0;
        return v_dst;
      }
    }
    v |= UT(b_src[last_offset] & std::byte(0x7f));
  }

  // correct for sign
  if constexpr (std::is_signed_v<T>) {
    bool const is_neg = v & T(1);
    v >>= 1;
    if (is_neg) {
      v = ~v;
    }
  }
  offset = last_offset_ + 1;
  return v_dst = T(v);
}

/**
 * @brief Reads dynamic num from \p b_src at \p offset and stores it in \p v_dst.
 * 
 * @tparam Throws - \c Throw if throws on error. \c NoThrow if sets offset to 0
 *   and return value is not updated on error.
 * @tparam T - Type of v_dst.
 * @tparam N - Size of buffer.
 * @param b_src - Source buffer.
 * @param offset - Offset in \p b_src to read dynamic int.  Updated to next offset
 *   to write to or to 0 if it doesn't fit.
 * @param v_dst - Storage for read int.
 * 
 * @return
 *   Value read.
 * 
 * @throws std::overflow_error
 *   - \p offset >= \p N.
 *   - dynamic number not terminated within \p b_src.
 *   - value retrieved can't fit in \p T type.
 */
template <typename Throws, typename T, std::size_t N
  , std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
constexpr T& decode_dnum(std::byte const (&b_src)[N], std::size_t& offset, T& v_dst)
{
  return decode_dnum<Throws>(b_src, N, offset, v_dst);
}

/**
 * @brief Reads dynamic num from \p b_src at \p offset and returns it.
 * 
 * @tparam Throws - \c Throw if throws on error. \c NoThrow if sets offset to 0
 *   and return value is not defined on error.
 * @tparam T - Type of value.
 * @tparam N - Size of buffer.
 * @param b_src - Source buffer.
 * @param offset - Offset in \p b_src to read dynamic int.  Updated to next offset
 *   to write to or to 0 if it doesn't fit.
 * 
 * @return
 *   Value read.
 * 
 * @throws std::overflow_error
 *   - \p offset >= \p N.
 *   - dynamic number not terminated within \p b_src.
 *   - value retrieved can't fit in \p T type.
 */
template <typename Throws, typename T, std::size_t N
  , std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
constexpr T decode_dnum(std::byte const (&b_src)[N], std::size_t& offset)
{
  T value{};
  return decode_dnum<Throws>(b_src, N, offset, value);
}

#endif // DYNAMIC_NUM_HPP
