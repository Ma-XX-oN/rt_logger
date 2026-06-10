/**
 * @file crc.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Constexpr CRC-16/CCITT (polynomial 0x1021) computation utilities.
 * @version 0.1
 * @date 2026-06-10
 *
 * Provides compile-time-capable CRC-16 functions for byte arrays, C-strings,
 * and string literals.  The default seed of 0 matches the XMODEM variant;
 * pass 0xFFFF for the standard CCITT initial value.
 *
 * @copyright Copyright (c) 2026
 */
#ifndef CONSTEXPR_CRC_HPP
#define CONSTEXPR_CRC_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Constexpr {

/**
 * @brief Fold one byte into a running CRC-16/CCITT value.
 *
 * Processes the byte MSB-first using the 0x1021 generator polynomial.
 *
 * @param crc  The current CRC accumulator.
 * @param byte The byte to fold in.
 * @returns Updated CRC value.
 */
constexpr std::uint16_t crc16_update_byte(std::uint16_t crc, std::uint8_t byte) {
  crc ^= static_cast<std::uint16_t>(byte) << 8;

  for (int i{0}; i < 8; ++i) {
    if (crc & 0x8000u) {
      crc = static_cast<std::uint16_t>((crc << 1) ^ 0x1021u);
    }
    else {
      crc = static_cast<std::uint16_t>(crc << 1);
    }
  }

  return crc;
}

/**
 * @brief Compute CRC-16/CCITT over a string.
 *
 * @param str  The string to compute the CRC over.
 * @param seed Initial CRC value (default 0 for XMODEM; use 0xFFFF for CCITT).
 * @returns CRC-16 over the string characters.
 */
constexpr std::uint16_t crc16(std::string_view str, std::uint16_t seed = 0) {
  auto crc{seed};

  for (auto ch : str) {
    crc = crc16_update_byte(crc, static_cast<std::uint8_t>(ch));
  }

  return crc;
}

/**
 * @brief Compute CRC-16/CCITT over the character range [\p begin_it, \p end_it).
 *
 * @tparam ItB - Iterator type pointing to the first character to process.
 * @tparam ItE - Sentinel type delimiting the end of the range.
 * @param begin_it - First character to process.
 * @param end_it   - One-past-the-end of the range.
 * @param seed     - Initial CRC value (default 0 for XMODEM; use 0xFFFF for CCITT).
 * @returns CRC-16 over [\p begin_it, \p end_it).
 */
template <typename ItB, typename ItE>
constexpr std::uint16_t crc16(ItB begin_it, ItE end_it, std::uint16_t seed = 0) {
  auto crc{seed};

  for (auto it{begin_it}; it != end_it; ++it) {
    crc = crc16_update_byte(
      crc,
      static_cast<std::uint8_t>(*it)
    );
  }

  return crc;
}

} // namespace Constexpr

#endif // CONSTEXPR_CRC_HPP
