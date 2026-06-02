/**
 * @file int_codex.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Encodes/decodes integers into/from byte-like buffers using either a
 *   fixed-width little-endian representation or a variable-width dynamic
 *   representation.
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

#ifndef INT_CODEX_HPP
#define INT_CODEX_HPP

#include <cstdint>
#include <iterator>
#include <type_traits>
#include "type_traits.hpp"
#include <cstddef>
#include <stdexcept>
#include <cassert>
#include <array>
#include <limits>
#include "ThrowNoThrow.hpp"
#include "bit.hpp"

namespace Constexpr {

/**
 * @brief Tests that type \p T is a byte in size.
 * 
 * @tparam T - Type to check.
 */
template <typename T>
inline constexpr bool is_byte_like_v =
  std::is_same_v<std::remove_cv_t<T>, char> ||
  std::is_same_v<std::remove_cv_t<T>, signed char> ||
  std::is_same_v<std::remove_cv_t<T>, unsigned char> ||
  std::is_same_v<std::remove_cv_t<T>, std::byte>;

/**
 * @brief Cast \b value to a \c std::uint8_t type.
 * 
 * @tparam B - \b value's type.
 * @param value - value to convert.
 * @return std::uint8_t - new value, truncated if needed.
 */
template <typename B>
constexpr std::uint8_t u8(B value) {
  return static_cast<std::uint8_t>(value);
}

/**
 * @brief Cast \b value to a \c std::uint8_t type.
 * 
 * @param value - value to convert.
 * @return std::uint8_t - new value.
 */
template <>
constexpr std::uint8_t u8(std::byte value) {
  return std::to_integer<std::uint8_t>(value);
}

/**
 * @brief Encodes \p value into the byte range [`dst_begin_it`, `dst_end_it`)
 *   using a fixed-width little-endian layout.
 *
 * The caller must provide enough space to store the encoded value. This
 * function asserts if the destination range is too small.
 *
 * @tparam T - Integral type to encode.
 * @tparam ItB - Iterator type pointing at writable byte-like storage.
 * @tparam ItE - Sentinel type delimiting the writable range.
 * @param dst_begin_it - First destination byte to write.
 * @param dst_end_it - One-past-the-end of the destination range.
 * @param value - Value to encode.
 * @return ItB - Iterator one past the last byte written.
 */
template <typename T, typename ItB, typename ItE>
constexpr ItB encode_int(ItB dst_begin_it, ItE const dst_end_it, T value) {
  static_assert(std::is_integral_v<T>, "T must be an integral type");
  static_assert(!std::is_same_v<T, bool>, "T must not be bool");
  static_assert(
    !std::is_signed_v<T> || std::numeric_limits<T>::min() == -std::numeric_limits<T>::max() - 1,
    "Signed int_codex types require two's-complement representation"
  );
  static_assert(
    std::numeric_limits<unsigned char>::digits == 8,
    "int_codex requires 8-bit byte storage"
  );

  static_assert(Constexpr::is_bidirectional<ItB>, "dst_begin_it must be a bidirectional iterator");
  static_assert(Constexpr::is_sentinel_for_v<ItE, ItB>, "dst_end_it must be a sentinel for dst_begin_it");

  using ItVT = typename std::iterator_traits<ItB>::value_type;
  static_assert(is_byte_like_v<ItVT>, "Iterator value_type must be byte-like");

  using Ref = decltype(*std::declval<ItB&>());
  static_assert(std::is_convertible_v<Ref, ItVT>, "Iterator must be readable");
  static_assert(std::is_assignable_v<Ref, ItVT>, "Iterator must be writable");
  //////////////////////////////////////////////////////////////////////////////////////////////////

  using UT = std::make_unsigned_t<T>;
  UT uvalue{ static_cast<UT>(value) };
  auto to_itv = [] (std::uint8_t v) { return static_cast<ItVT>(v); };

  for (std::size_t i{ 0 }; i < sizeof(T); ++i) {
    assert(dst_begin_it != dst_end_it || !"Not enough space to store in buffer");
    *dst_begin_it++ = to_itv(u8(uvalue & UT{0xff}));
    uvalue >>= 8;
  }
  return dst_begin_it;
}

/**
 * @brief Decodes a fixed-width little-endian integer from the byte range
 *   [`src_begin_it`, `src_end_it`) and stores it in \p v_dst.
 *
 * On success, \p src_begin_it is advanced past the decoded bytes.
 *
 * The caller must provide enough remaining bytes to decode \p T. This function
 * asserts if the source range is too small.
 *
 * @tparam T - Integral type to decode.
 * @tparam ItB - Iterator type pointing at readable byte-like storage.
 * @tparam ItE - Sentinel type delimiting the readable range.
 * @param src_begin_it - Start of the source bytes. Updated past the decoded
 *   value.
 * @param src_end_it - One past the end of the source bytes.
 * @param v_dst - Storage for the decoded value.
 * @return T& - \p v_dst.
 */
template <typename T, typename ItB, typename ItE>
constexpr T& decode_int(ItB& src_begin_it, ItE src_end_it, T& v_dst) {
  static_assert(std::is_integral_v<T>, "T must be an integral type");
  static_assert(!std::is_same_v<T, bool>, "T must not be bool");
  static_assert(
    !std::is_signed_v<T> || std::numeric_limits<T>::min() == -std::numeric_limits<T>::max() - 1,
    "Signed int_codex types require two's-complement representation"
  );
  static_assert(
    std::numeric_limits<unsigned char>::digits == 8,
    "int_codex requires 8-bit byte storage"
  );

  static_assert(Constexpr::is_bidirectional<ItB>, "src_begin_it must be a bidirectional iterator");
  static_assert(Constexpr::is_sentinel_for_v<ItE, ItB>, "src_end_it must be a sentinel for src_begin_it");

  using ItVT = typename std::iterator_traits<ItB>::value_type;
  static_assert(is_byte_like_v<ItVT>, "Iterator value_type must be byte-like");

  using Ref = decltype(*std::declval<ItB&>());
  static_assert(std::is_convertible_v<Ref, ItVT>, "Iterator must be readable");
  //////////////////////////////////////////////////////////////////////////////////////////////////

  using UT = std::make_unsigned_t<T>;
  UT value{};

  for (std::size_t i{ 0 }; i < sizeof(T); ++i) {
    assert(src_begin_it != src_end_it || !"Not enough bytes left to decode value");
    value |= static_cast<UT>(u8(*src_begin_it++)) << (i * 8);
  }

  v_dst = Constexpr::bit_cast<T>(value);
  return v_dst;
}

/**
 * @brief Decodes a fixed-width little-endian integer from the byte range
 *   [`src_begin_it`, `src_end_it`) and returns it by value.
 *
 * On success, \p src_begin_it is advanced past the decoded bytes.
 *
 * @tparam T - Integral type to decode.
 * @tparam ItB - Iterator type pointing at readable byte-like storage.
 * @tparam ItE - Sentinel type delimiting the readable range.
 * @param src_begin_it - Start of the source bytes. Updated past the decoded
 *   value.
 * @param src_end_it - One past the end of the source bytes.
 * @return T - Decoded value.
 */
template <typename T, typename ItB, typename ItE
  , std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
constexpr T decode_int(ItB& src_begin_it, ItE src_end_it) {
  T v_dst{};
  return decode_int(src_begin_it, src_end_it, v_dst);
}

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
 * @return Iterator one past the last byte written.
 */
template <typename T, typename ItB, typename ItE>
constexpr ItB encode_dint(ItB dst_begin_it, ItE const dst_end_it, T v_src) {
  static_assert(std::is_integral_v<T>, "T must be an integral type");
  static_assert(!std::is_same_v<T, bool>, "T must not be bool");
  static_assert(
    !std::is_signed_v<T> || std::numeric_limits<T>::min() == -std::numeric_limits<T>::max() - 1,
    "Signed dint types require two's-complement representation"
  );
  static_assert(
    std::numeric_limits<unsigned char>::digits == 8,
    "dynamic_int requires 8-bit byte-like storage"
  );

  static_assert(Constexpr::is_bidirectional<ItB>, "dst_begin_it must be a bidirectional iterator");
  static_assert(Constexpr::is_sentinel_for_v<ItE, ItB>, "dst_end_it must be a sentinel for dst_begin_it");
  
  using ItVT = typename std::iterator_traits<ItB>::value_type;
  static_assert(is_byte_like_v<ItVT>, "Iterator value_type must be byte-like");

  using Ref = decltype(*std::declval<ItB&>());
  static_assert(std::is_convertible_v<Ref, ItVT>, "Iterator must be readable");
  static_assert(std::is_assignable_v<Ref, ItVT>, "Iterator must be writable");
  //////////////////////////////////////////////////////////////////////////////////////////////////

  using UT = std::make_unsigned_t<T>;
  auto to_uint = Constexpr::overload {
    [] (std::byte v) { return std::to_integer<UT>(v); },
    [] (auto v)      { return     static_cast<UT>(v); },
  };
  auto to_itv = [] (auto v) { return static_cast<ItVT>(v); };

  UT v{ to_uint(v_src) };
  constexpr bool is_signed { std::is_signed_v<T> };
  bool is_negative { is_signed ? v_src < 0 : false };
  
  // invert negative values
  if (is_signed) {
    if (v_src < 0) {
      v = UT(~v);
    }
  }

  // write non-zero least significant bytes
  constexpr UT dint_payload{ to_uint(0x7f) };
  constexpr std::uint8_t continuation_bit{ 0x80u };
  do {
    assert(dst_begin_it != dst_end_it || !"Not enough space to store in buffer");
    std::uint8_t lsb{ u8(v & dint_payload) };
    if (v >>= 7) {
      lsb |= continuation_bit;
    }
    *dst_begin_it++ = to_itv(lsb);
  } while (v);

  // update with sign if signed
  if (is_signed) {
    constexpr std::uint8_t dst_sign_bit{ 0x40u };
    auto last_most_sig_byte_it{ dst_begin_it };
    --last_most_sig_byte_it;
    auto sign_value { is_negative ? dst_sign_bit : u8(0) };
    auto last{ u8(*last_most_sig_byte_it) };

    if (last & dst_sign_bit) {
      // Bit for sign already in use, so need another byte for sign.
      assert(dst_begin_it != dst_end_it || !"Not enough space to store in buffer");
      last |= continuation_bit;
      *last_most_sig_byte_it = to_itv(last);
      *dst_begin_it++ = to_itv(sign_value);

    } else {
      // Bit for sign not in use, so use it for sign.
      last |= sign_value;
      *last_most_sig_byte_it = to_itv(last);
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
template <typename Throws, typename T, typename ItB, typename ItE>
constexpr T& decode_dint(ItB& src_begin_it, ItE src_end_it, T& v_dst) {
  static_assert(std::is_integral_v<T>, "T must be an integral type");
  static_assert(!std::is_same_v<T, bool>, "T must not be bool");
  static_assert(
    !std::is_signed_v<T> || std::numeric_limits<T>::min() == -std::numeric_limits<T>::max() - 1,
    "Signed dint types require two's-complement representation"
  );
  static_assert(
    std::numeric_limits<unsigned char>::digits == 8,
    "dynamic_int requires 8-bit byte-like storage"
  );

  static_assert(Constexpr::is_bidirectional<ItB>, "src_begin_it must be a bidirectional iterator");
  static_assert(Constexpr::is_sentinel_for_v<ItE, ItB>, "src_end_it must be a sentinel for src_begin_it");
  
  using ItVT = typename std::iterator_traits<ItB>::value_type;
  static_assert(is_byte_like_v<ItVT>, "Iterator value_type must be byte-like");

  using Ref = decltype(*std::declval<ItB&>());
  static_assert(std::is_convertible_v<Ref, ItVT>, "Iterator must be readable");
  //////////////////////////////////////////////////////////////////////////////////////////////////

  using UT = std::make_unsigned_t<T>;
  auto to_uint = Constexpr::overload {
    [] (std::byte v) { return std::to_integer<UT>(v); },
    [] (auto v)      { return     static_cast<UT>(v); },
  };

  if (src_begin_it == src_end_it) {
    if constexpr (std::is_same_v<Throws, Throw>) {
      throw std::overflow_error("Starting offset exceeded bounds of src_begin_it buffer.");
    } else {
      return v_dst;
    }
  }

  // find last byte of dint
  constexpr std::uint8_t continuation_bit { 0x80u };
  auto init_start_it { src_begin_it };
  while (to_uint(u8(*src_begin_it) & continuation_bit)) {
    if (++src_begin_it == src_end_it) {
      break;
    }
  }

  // read in number
  if (src_begin_it == src_end_it) {
    if constexpr (std::is_same_v<Throws, Throw>) {
      throw std::overflow_error("dint value not terminated in src_begin_it buffer.");
    } else {
      src_begin_it = init_start_it;
      return v_dst;
    }
  }

  auto                   last_digit_it { src_begin_it };
  constexpr bool         is_signed     { std::is_signed_v<T> };
  constexpr std::uint8_t sign_bit      { is_signed ? 0x40u : 0x00u };
  constexpr UT           upper_7_bits  { static_cast<UT>(~(std::numeric_limits<UT>::max() >> 7)) };
  constexpr std::uint8_t payload_mask  { static_cast<std::uint8_t>(~(continuation_bit | sign_bit)) };
  bool                   is_negative   { is_signed && to_uint(u8(*src_begin_it) & sign_bit) != 0 };
  UT                     v             { to_uint(u8(*src_begin_it) & payload_mask) };
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
    v |= to_uint(u8(*--src_begin_it) & ~continuation_bit);
  }

  if (is_negative) {
    v = ~v;
  }
  ++(src_begin_it = last_digit_it);
  v_dst = Constexpr::bit_cast<T>(v);
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
constexpr T decode_dint(std::byte const*& src_begin_it, std::byte const* const src_end_it) {
  T v_dst {};
  return decode_dint<Throws>(src_begin_it, src_end_it, v_dst);
}

} // namespace Constexpr

#endif // INT_CODEX_HPP
