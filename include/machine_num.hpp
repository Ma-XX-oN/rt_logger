/**
 * @file machine_num.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Encodes/decodes integers/enums into/from a std::byte buffer in machine
 *   endianness.
 * @version 0.1
 * @date 2026-05-21
 *
 * Allows a buffer to store/retrieve integers/enums in the endianness of the
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

////////////////////////////////////////////////////////////////////////////////
//
//  # | Buffer     | Value or array of values
//    | dst        | src
//  --|------------|-------------------------
//  0 | ptr & size | ptr & size
//  1 | ptr & size | val ref
//  2 | array ref  | val ref
//  3 | array ref  | array ref // If dst array is spread over multiple blks, this
//    |            |           // is cumbersome, so not doing.
//
//  # | Buffer     | Value or array of values
//    | src        | dst
//  --|------------|-------------------------
//  4 | ptr & size | ptr & size
//  5 | ptr & size | val ref
//  6a| array ref  | val ref
//  7 | array ref  | array ref // If dst array is spread over multiple blks, this
//    |            |           // is cumbersome, so not doing.
//  6b| array ref  | type specified, val storage not given
//
// Each overload exists in two forms: an lvalue (std::size_t&) base
// implementation and an rvalue (std::size_t&&) convenience wrapper that
// creates a local offset copy and delegates to the lvalue base.  NoThrow is
// not permitted with rvalue offsets on decode overloads (enforced via
// static_assert).
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Get the number of elements of a type that can be stored.
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Calculate the number of elements of type \p T that can be stored in an
 *   array of length \p N starting at offset \p offset.
 *
 * @tparam T - Type of value to store
 * @param N - Number of bytes in buffer.
 * @param offset - Offset where to put value if encoded.
 * @return std::size_t - Number of elements that can be fully stored in buffer.
 */
template <typename T
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
std::size_t space_available(std::size_t N, std::size_t offset) {
  assert(N >= 1);
  if (N >= offset) {
    return (N-offset) / sizeof(T);
  } else {
    return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Store one or more integers or enums
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief 0. Store one or more integers or enums to \p b_dst buffer at \p
 *   offset.
 *
 * @tparam T - Type being stored.
 * @param b_dst - destination buffer.
 * @param N - Size of \p b_dst buffer.
 * @param offset - Offset in \p b_dst buffer to write to.  Updated to next
 *   offset to write to if fits; unchanged if it doesn't.
 * @param v_src - Value(s) to write.
 * @param M - Number of values to put in \p b_dst buffer.
 *
 * @return std::size_t - Next offset or 0 if \p v_src doesn't fit.
 */
template <typename T
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
std::size_t encode_value(std::byte* b_dst, std::size_t N, std::size_t& offset, T const* v_src, std::size_t M) {
  assert(M >= 1);
  std::size_t length{ sizeof(T) * M };
  if (length <= N && offset <= N - length) {
    std::memcpy(b_dst + offset, v_src, length);
    return offset += length;
  } else {
    return 0;
  }
}

/**
 * @brief 0. Store one or more integers or enums to \p b_dst buffer at \p offset
 *   (rvalue-offset overload).  Delegates to the lvalue overload.
 *
 * @tparam T - Type being stored.
 * @param b_dst - destination buffer.
 * @param N - Size of \p b_dst buffer.
 * @param offset - Offset in \p b_dst buffer to write to.  Not updated (rvalue);
 *   use the return value.
 * @param v_src - Value(s) to write.
 * @param M - Number of values to put in \p b_dst buffer.
 *
 * @return std::size_t - Next offset or 0 if \p v_src doesn't fit.
 */
template <typename T
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
std::size_t encode_value(std::byte* b_dst, std::size_t N, std::size_t&& offset, T const* v_src, std::size_t M) {
  std::size_t local{ offset };
  return encode_value(b_dst, N, local, v_src, M);
}

/**
 * @brief 1. Store one or more integers or enums to \p b_dst buffer at \p
 * offset.
 *
 * @tparam T - Type being stored.
 * @param b_dst - destination buffer.
 * @param N - Size of \p b_dst buffer.
 * @param offset - Offset in \p b_dst buffer to write to.  Updated to next
 *   offset to write to if fits; unchanged if it doesn't.
 * @param v_src - Value to write.
 *
 * @return std::size_t - Next offset or 0 if \p v_src doesn't fit.
 */
template <typename T
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
std::size_t encode_value(std::byte* b_dst, std::size_t N, std::size_t& offset, T const& v_src) {
  return encode_value(b_dst, N, offset, &v_src, 1);
}

/**
 * @brief 1. Store one or more integers or enums to \p b_dst buffer at \p offset
 *   (rvalue-offset overload).  Delegates to the lvalue overload.
 *
 * @tparam T - Type being stored.
 * @param b_dst - destination buffer.
 * @param N - Size of \p b_dst buffer.
 * @param offset - Offset in \p b_dst buffer to write to.  Not updated (rvalue);
 *   use the return value.
 * @param v_src - Value to write.
 *
 * @return std::size_t - Next offset or 0 if \p v_src doesn't fit.
 */
template <typename T
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
std::size_t encode_value(std::byte* b_dst, std::size_t N, std::size_t&& offset, T const& v_src) {
  std::size_t local{ offset };
  return encode_value(b_dst, N, local, v_src);
}

/**
 * @brief 2. Store an integer or enum to \p b_dst buffer at \p offset.
 *
 * @tparam T - Type being stored.
 * @tparam N - Size of \p b_dst buffer.
 * @param b_dst - destination buffer.
 * @param offset - Offset in \p b_dst buffer to write to.  Updated to next
 *   offset to write to if fits; unchanged if it doesn't.
 * @param v_src - Value to write.
 *
 * @return std::size_t - Next offset or 0 if \p v_src doesn't fit.
 */
template <typename T, std::size_t N
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
std::size_t encode_value(std::byte (&b_dst)[N], std::size_t& offset, T const& v_src) {
  return encode_value(b_dst, N, offset, v_src);
}

/**
 * @brief 2. Store an integer or enum to \p b_dst buffer at \p offset
 *   (rvalue-offset overload).  Delegates to the lvalue overload.
 *
 * @tparam T - Type being stored.
 * @tparam N - Size of \p b_dst buffer.
 * @param b_dst - destination buffer.
 * @param offset - Offset in \p b_dst buffer to write to.  Not updated (rvalue);
 *   use the return value.
 * @param v_src - Value to write.
 *
 * @return std::size_t - Next offset or 0 if \p v_src doesn't fit.
 */
template <typename T, std::size_t N
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
std::size_t encode_value(std::byte (&b_dst)[N], std::size_t&& offset, T const& v_src) {
  std::size_t local{ offset };
  return encode_value(b_dst, N, local, v_src);
}

////////////////////////////////////////////////////////////////////////////////
// Retrieve one or more integers or enums
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief 4. Gets an integer or enum from \p b_src buffer at \p offset.
 *
 * @tparam Throws - \c Throw if throws on error. \c NoThrow if sets offset to 0
 *   and return value is not defined on error.
 * @tparam T - Type of integer or enum to get.
 * @param b_src - Source buffer.
 * @param N - Size of source buffer.
 * @param offset - Offset in \p b_src buffer to read from.  Updated to next
 *   offset to read from if fits; set to 0 if it doesn't.
 * @param v_dst - destination for value.  Not updated if value doesn't fit in \p
 *   b_src buffer at \p offset.  Check if \p offset is 0 for this error
 *   condition.
 * @param M - Number of values to retrieve.
 * @return T& - \p v_dst.
 */
template <typename Throws, typename T
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
T& decode_value(std::byte const* b_src, std::size_t N, std::size_t& offset, T* v_dst, std::size_t M) {
  assert(M >= 1);
  std::size_t length{ sizeof(T) * M };
  if (length <= N && offset <= N - length) {
    std::memcpy(v_dst, b_src + offset, length);
    offset += length;
  } else {
    if constexpr (std::is_same_v<Throws, Throw>) {
      throw std::overflow_error("v_dst can't fit in b_src buffer at specified offset");
    } else {
      offset = 0;
    }
  }
  return *v_dst;
}

/**
 * @brief 4. Gets an integer or enum from \p b_src buffer at \p offset
 *   (rvalue-offset overload).  Delegates to the lvalue overload. \c NoThrow is
 *   not permitted with rvalue offsets.
 *
 * @tparam Throws - Must be \c Throw.  \c NoThrow is not permitted since the
 *   caller cannot observe an updated offset.
 * @tparam T - Type of integer or enum to get.
 * @param b_src - Source buffer.
 * @param N - Size of source buffer.
 * @param offset - Offset in \p b_src buffer to read from.  Not updated
 *   (rvalue); errors are indicated by exception.
 * @param v_dst - destination for value.
 * @param M - Number of values to retrieve.
 * @return T& - \p v_dst.
 */
template <typename Throws, typename T
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
T& decode_value(std::byte const* b_src, std::size_t N, std::size_t&& offset, T* v_dst, std::size_t M) {
  static_assert(std::is_same_v<Throws, Throw>
    , "NoThrow requires an lvalue offset to observe errors.");
  std::size_t local{ offset };
  return decode_value<Throws>(b_src, N, local, v_dst, M);
}

/**
 * @brief 5. Gets an integer or enum from \p b_src buffer at \p offset.
 *
 * @tparam Throws - \c Throw if throws on error. \c NoThrow if sets offset to 0
 *   and return value is not defined on error.
 * @tparam T - Type of integer or enum to get.
 * @param b_src - Source buffer.
 * @param N - Size of source buffer.
 * @param offset - Offset in \p b_src buffer to read from.  Updated to next
 *   offset to read from if fits; set to 0 if it doesn't.
 * @param v_dst - destination for value.  Not updated if value doesn't fit in \p
 *   b_src buffer at \p offset.  Check if \p offset is 0 for this error
 *   condition.
 * @return T& - \p v_dst.
 */
template <typename Throws, typename T
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
T& decode_value(std::byte const* b_src, std::size_t N, std::size_t& offset, T& v_dst) {
  return decode_value<Throws>(b_src, N, offset, &v_dst, 1);
}

/**
 * @brief 5. Gets an integer or enum from \p b_src buffer at \p offset
 *   (rvalue-offset overload).  Delegates to the lvalue overload. \c NoThrow is
 *   not permitted with rvalue offsets.
 *
 * @tparam Throws - Must be \c Throw.  \c NoThrow is not permitted since the
 *   caller cannot observe an updated offset.
 * @tparam T - Type of integer or enum to get.
 * @param b_src - Source buffer.
 * @param N - Size of source buffer.
 * @param offset - Offset in \p b_src buffer to read from.  Not updated
 *   (rvalue); errors are indicated by exception.
 * @param v_dst - destination for value.
 * @return T& - \p v_dst.
 */
template <typename Throws, typename T
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
T& decode_value(std::byte const* b_src, std::size_t N, std::size_t&& offset, T& v_dst) {
  static_assert(std::is_same_v<Throws, Throw>
    , "NoThrow requires an lvalue offset to observe errors.");
  std::size_t local{ offset };
  return decode_value<Throws>(b_src, N, local, v_dst);
}

/**
 * @brief 6a. Gets an integer or enum from \p b_src buffer at \p offset.
 *
 * @tparam Throws - \c Throw if throws on error. \c NoThrow if sets offset to 0
 *   and return value is not defined on error.
 * @tparam T - Type of integer or enum to get.
 * @tparam N - Size of source buffer.
 * @param b_src - Source buffer.
 * @param offset - Offset in \p b_src buffer to read from.  Updated to next
 *   offset to read from if fits; set to 0 if it doesn't.
 * @param v_dst - destination for value.  Not updated if value doesn't fit in \p
 *   b_src buffer at \p offset.  Check if \p offset is 0 for this error
 *   condition.
 * @return T& - \p v_dst.
 */
template <typename Throws, typename T, std::size_t N
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
T& decode_value(std::byte const (&b_src)[N], std::size_t& offset, T& v_dst) {
  return decode_value<Throws>(b_src, N, offset, &v_dst, 1);
}

/**
 * @brief 6a. Gets an integer or enum from \p b_src buffer at \p offset
 *   (rvalue-offset overload).  Delegates to the lvalue overload. \c NoThrow is
 *   not permitted with rvalue offsets.
 *
 * @tparam Throws - Must be \c Throw.  \c NoThrow is not permitted since the
 *   caller cannot observe an updated offset.
 * @tparam T - Type of integer or enum to get.
 * @tparam N - Size of source buffer.
 * @param b_src - Source buffer.
 * @param offset - Offset in \p b_src buffer to read from.  Not updated
 *   (rvalue); errors are indicated by exception.
 * @param v_dst - destination for value.
 * @return T& - \p v_dst.
 */
template <typename Throws, typename T, std::size_t N
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
T& decode_value(std::byte const (&b_src)[N], std::size_t&& offset, T& v_dst) {
  static_assert(std::is_same_v<Throws, Throw>
    , "NoThrow requires an lvalue offset to observe errors.");
  std::size_t local{ offset };
  return decode_value<Throws>(b_src, N, local, v_dst);
}

/**
 * @brief 6b. Gets an integer or enum from \p b_src buffer at \p offset.
 *
 * @tparam Throws - \c Throw if throws on error. \c NoThrow if sets offset to 0
 *   and return value is not defined on error.
 * @tparam T - Type of integer or enum to get.
 * @tparam N - Size of source buffer.
 * @param b_src - Source buffer.
 * @param offset - Offset in \p b_src buffer to read from.  Updated to next
 *   offset to read from if fits; set to 0 if it doesn't.
 * @return T - Decoded value.
 */
template <typename Throws, typename T, std::size_t N
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
T decode_value(std::byte const (&b_src)[N], std::size_t& offset) {
  T v_dst{};
  return decode_value<Throws>(b_src, N, offset, &v_dst, 1);
}

/**
 * @brief 6b. Gets an integer or enum from \p b_src buffer at \p offset
 *   (rvalue-offset overload).  Delegates to the lvalue overload. \c NoThrow is
 *   not permitted with rvalue offsets.
 *
 * @tparam Throws - Must be \c Throw.  \c NoThrow is not permitted since the
 *   caller cannot observe an updated offset.
 * @tparam T - Type of integer or enum to get.
 * @tparam N - Size of source buffer.
 * @param b_src - Source buffer.
 * @param offset - Offset in \p b_src buffer to read from.  Not updated
 *   (rvalue); errors are indicated by exception.
 * @return T - Decoded value.
 */
template <typename Throws, typename T, std::size_t N
  , std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
T decode_value(std::byte const (&b_src)[N], std::size_t&& offset) {
  static_assert(std::is_same_v<Throws, Throw>
    , "NoThrow requires an lvalue offset to observe errors.");
  std::size_t local{ offset };
  return decode_value<Throws, T>(b_src, local);
}

#endif // MACHINE_NUM_HPP
