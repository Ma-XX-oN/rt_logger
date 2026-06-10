/**
 * @file machine_num.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Encodes/decodes numbers/enums into/from a std::byte buffer in machine
 *   endianness.
 * @version 0.1
 * @date 2026-05-21
 *
 * Allows a buffer to store/retrieve numbers/enums in the endianness of the
 * current machine.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef MACHINE_NUM_HPP
#define MACHINE_NUM_HPP

#include <cassert>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <stdexcept>
#include "ThrowNoThrow.hpp"
#include "byte_range.hpp"

////////////////////////////////////////////////////////////////////////////////
// Store one or more numbers or enums
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Encodes one or more numbers or enums into the byte range
 *   [\p dst_begin_it, \p dst_end_it).
 *
 * The caller must provide enough space to store all \p M values.  This
 * function asserts if the destination range is too small.
 *
 * @tparam T   - Type being stored; must be an enum or arithmetic type.
 * @tparam ItB - Writable iterator type for the destination range.
 * @tparam ItE - End-sentinel type for the destination range.
 * @param dst_begin_it - First destination byte to write.
 * @param dst_end_it - One-past-the-end of the destination range.
 * @param v_src - Pointer to value(s) to encode.
 * @param M - Number of values to encode.
 * @return Iterator one past the last byte written.
 */
template <typename T, typename ItB, typename ItE>
ItB encode_value(ItB dst_begin_it, ItE const dst_end_it, T const* v_src, std::size_t M) {
  Require::byte_like_read_write_range(dst_begin_it, dst_end_it);
  static_assert(std::is_enum_v<T> || std::is_arithmetic_v<T>, "Must be an enum or number.");

  assert(M >= 1);
  std::size_t const length{ sizeof(T) * M };
  assert(dst_begin_it + length <= dst_end_it || !"Not enough space to store in buffer");
  std::memcpy(dst_begin_it, v_src, length);
  return dst_begin_it + length;
}

/**
 * @brief Encodes a number or enum into the byte range [\p dst_begin_it,
 *   \p dst_end_it).  Delegates to the array overload.
 *
 * The caller must provide enough space to store the value.  This function
 * asserts if the destination range is too small.
 *
 * @tparam T   - Type being stored; must be an enum or arithmetic type.
 * @tparam ItB - Writable iterator type for the destination range.
 * @tparam ItE - End-sentinel type for the destination range.
 * @param dst_begin_it - First destination byte to write.
 * @param dst_end_it - One-past-the-end of the destination range.
 * @param v_src - Value to encode.
 * @return Iterator one past the last byte written.
 */
template <typename T, typename ItB, typename ItE>
ItB encode_value(ItB dst_begin_it, ItE const dst_end_it, T const& v_src) {
  return encode_value(dst_begin_it, dst_end_it, &v_src, 1);
}

////////////////////////////////////////////////////////////////////////////////
// Retrieve one or more numbers or enums
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Decodes one or more numbers or enums from the byte range
 *   [\p src_begin_it, \p src_end_it) into \p v_dst.
 *
 * On success, \p src_begin_it is advanced to the next unread byte after the
 * decoded values.
 *
 * @tparam Throws - \c Throw to throw on decode errors; \c NoThrow to restore
 *   \p src_begin_it and leave \p v_dst unchanged on failure.
 * @tparam T   - Type of number or enum to decode; must be an enum or
 *   arithmetic type.
 * @tparam ItB - Iterator type for the source range.
 * @tparam ItE - Element type that the end sentinel points to.  The end
 *   sentinel parameter has type \c ItE \c const*, so \c ItE is deduced as
 *   the element type (e.g. \c std::byte), not the pointer type.
 * @param src_begin_it - Reference to the first unread source byte.  Advanced
 *   to the next unread byte on success; restored to its initial value on
 *   \c NoThrow failure.
 * @param src_end_it - Pointer to one-past-the-end of the source range
 *   (type \c ItE \c const*).
 * @param v_dst - Destination array for decoded values.  Not updated on
 *   failure.
 * @param M - Number of values to decode.
 * @return T& - Reference to \p v_dst[0].
 * @throws std::overflow_error if the values do not fit in the source range
 *   (\c Throw mode only).
 */
template <typename Throws, typename T, typename ItB, typename ItE>
T& decode_value(ItB& src_begin_it, ItE const* const src_end_it, T* v_dst, std::size_t M) {
  Require::byte_like_readable_range(src_begin_it, src_end_it);
  static_assert(std::is_enum_v<T> || std::is_arithmetic_v<T>, "Must be an enum or number.");

  assert(M >= 1);
  std::size_t const length{ sizeof(T) * M };
  auto const init_begin_it{ src_begin_it };
  if (src_begin_it + length <= src_end_it) {
    std::memcpy(v_dst, src_begin_it, length);
    src_begin_it += length;
  } else {
    if constexpr (std::is_same_v<Throws, Throw>) {
      throw std::overflow_error("v_dst can't fit in b_src buffer at specified offset");
    } else {
      src_begin_it = init_begin_it;
    }
  }
  return *v_dst;
}

/**
 * @brief Decodes a number or enum from the byte range [\p src_begin_it,
 *   \p src_end_it) into \p v_dst.  Delegates to the array overload.
 *
 * On success, \p src_begin_it is advanced to the next unread byte.
 *
 * @tparam Throws - \c Throw to throw on decode errors; \c NoThrow to restore
 *   \p src_begin_it and leave \p v_dst unchanged on failure.
 * @tparam T   - Type of number or enum to decode; must be an enum or
 *   arithmetic type.
 * @tparam ItB - Iterator type for the source range.
 * @tparam ItE - End-sentinel type for the source range (the pointer type,
 *   e.g. \c std::byte*).
 * @param src_begin_it - Reference to the first unread source byte.  Advanced
 *   to the next unread byte on success; restored to its initial value on
 *   \c NoThrow failure.
 * @param src_end_it - One-past-the-end of the source range.
 * @param v_dst - Destination for the decoded value.  Not updated on failure.
 * @return T& - Reference to \p v_dst.
 * @throws std::overflow_error if the value does not fit in the source range
 *   (\c Throw mode only).
 */
template <typename Throws, typename T, typename ItB, typename ItE>
T& decode_value(ItB& src_begin_it, ItE const src_end_it, T& v_dst) {
  return decode_value<Throws>(src_begin_it, src_end_it, &v_dst, 1);
}

/**
 * @brief Decodes a number or enum from the byte range [\p src_begin_it,
 *   \p src_end_it) and returns it by value.  Delegates to the array overload.
 *
 * On success, \p src_begin_it is advanced to the next unread byte.
 *
 * @tparam Throws - \c Throw to throw on decode errors; \c NoThrow to restore
 *   \p src_begin_it and return a default-initialized value on failure.
 * @tparam T - Type of number or enum to decode; must be an enum or arithmetic
 *   type.  Must be specified explicitly — it cannot be deduced from the
 *   arguments.
 * @param src_begin_it - Reference to the first unread source byte.  Advanced
 *   to the next unread byte on success; restored to its initial value on
 *   \c NoThrow failure.
 * @param src_end_it - One-past-the-end of the source range.
 * @return T - The decoded value, or a default-initialized \c T on
 *   \c NoThrow failure.
 * @throws std::overflow_error if the value does not fit in the source range
 *   (\c Throw mode only).
 */
template <typename Throws, typename T>
T decode_value(std::byte const*& src_begin_it, std::byte const* const src_end_it) {
  T v_dst{};
  decode_value<Throws>(src_begin_it, src_end_it, &v_dst, 1);
  return v_dst;
}

#endif // MACHINE_NUM_HPP
