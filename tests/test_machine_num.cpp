#include "../include/machine_num.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>

namespace {

enum class SampleEnum : std::uint16_t {
  Alpha = 0x1234,
  Beta = 0xabcd
};

/**
 * @brief Capture the in-memory machine-endian byte representation of one value.
 *
 * @tparam T - Type of value to serialize into bytes.
 * @param value - Value whose raw bytes to capture.
 * @return std::array<std::byte, sizeof(T)> - Raw bytes for \p value.
 */
template <typename T>
std::array<std::byte, sizeof(T)> object_bytes(T const& value)
{
  std::array<std::byte, sizeof(T)> bytes{};
  std::memcpy(bytes.data(), &value, sizeof(T));
  return bytes;
}

/**
 * @brief Capture the concatenated in-memory machine-endian byte representation
 *   of an array.
 *
 * @tparam T - Element type of the source array.
 * @tparam N - Number of elements in the source array.
 * @param values - Array whose raw bytes to capture.
 * @return std::array<std::byte, sizeof(T) * N> - Raw bytes for \p values.
 */
template <typename T, std::size_t N>
std::array<std::byte, sizeof(T) * N> object_bytes(T const (&values)[N])
{
  std::array<std::byte, sizeof(T) * N> bytes{};
  std::memcpy(bytes.data(), values, sizeof(values));
  return bytes;
}

/**
 * @brief Compare a slice of a byte buffer against expected raw bytes.
 *
 * @tparam N - Number of expected bytes.
 * @param buffer - Buffer to inspect.
 * @param offset - Starting offset into \p buffer.
 * @param expected - Expected bytes at \p offset.
 */
template <std::size_t N>
void expect_buffer_bytes(
  std::byte const* buffer,
  std::size_t offset,
  std::array<std::byte, N> const& expected)
{
  for (std::size_t i{ 0 }; i < N; ++i) {
    EXPECT_EQ(expected[i], buffer[offset + i]) << "byte index " << (offset + i);
  }
}

/**
 * @brief Compare decoded arrays element-by-element for clearer test failures.
 *
 * @tparam T - Element type being compared.
 * @tparam N - Number of elements in each array.
 * @param expected - Expected decoded values.
 * @param actual - Actual decoded values.
 */
template <typename T, std::size_t N>
void expect_values_equal(T const (&expected)[N], T const (&actual)[N])
{
  for (std::size_t i{ 0 }; i < N; ++i) {
    EXPECT_EQ(expected[i], actual[i]) << "element index " << i;
  }
}

TEST(MachineNumSpaceAvailable, ReportsRemainingCapacityInElements)
{
  EXPECT_EQ(4u, space_available<std::uint16_t>(10, 1));
  EXPECT_EQ(4u, space_available<std::uint16_t>(10, 2));
  EXPECT_EQ(3u, space_available<std::uint16_t>(10, 3));
  EXPECT_EQ(0u, space_available<std::uint32_t>( 4, 3));
  EXPECT_EQ(0u, space_available<std::uint32_t>( 4, 4));
  EXPECT_EQ(0u, space_available<std::uint16_t>( 4, 5));
}

TEST(MachineNumEncode, SupportsSingleValueOverloads)
{
  constexpr std::uint32_t value{ 0x12345678u };
  auto const expected{ object_bytes(value) };

  {
    std::byte buffer[16]{};
    std::size_t offset{ 1 };
    std::size_t const end{ encode_value(buffer, std::size(buffer), offset, value) };
    ASSERT_EQ(1u + sizeof(value), end);
    EXPECT_EQ(end, offset);
    expect_buffer_bytes(buffer, 1, expected);
  }

  {
    std::byte buffer[16]{};
    std::size_t const end{ encode_value(buffer, std::size(buffer), std::size_t{2}, value) };
    ASSERT_EQ(2u + sizeof(value), end);
    expect_buffer_bytes(buffer, 2, expected);
  }

  {
    std::byte buffer[16]{};
    std::size_t offset{ 3 };
    std::size_t const end{ encode_value(buffer, offset, value) };
    ASSERT_EQ(3u + sizeof(value), end);
    EXPECT_EQ(end, offset);
    expect_buffer_bytes(buffer, 3, expected);
  }

  {
    std::byte buffer[16]{};
    std::size_t const end{ encode_value(buffer, 4, value) };
    ASSERT_EQ(4u + sizeof(value), end);
    expect_buffer_bytes(buffer, 4, expected);
  }
}

TEST(MachineNumEncode, SupportsValueArrayOverloads)
{
  std::uint16_t const values[]{0x1122u, 0x3344u};
  auto const expected{ object_bytes(values) };

  {
    std::byte buffer[16]{};
    std::size_t offset{ 1 };
    std::size_t const end{ encode_value(buffer, std::size(buffer), offset, values, std::size(values)) };
    ASSERT_EQ(1u + sizeof(values), end);
    EXPECT_EQ(end, offset);
    expect_buffer_bytes(buffer, 1, expected);
  }

  {
    std::byte buffer[16]{};
    std::size_t const end{ encode_value(buffer, std::size(buffer), std::size_t{2}, values, std::size(values)) };
    ASSERT_EQ(2u + sizeof(values), end);
    expect_buffer_bytes(buffer, 2, expected);
  }
}

TEST(MachineNumDecode, SupportsSingleValueOverloads)
{
  constexpr std::int32_t value{ -123456789 };
  auto const encoded{ object_bytes(value) };
  std::byte buffer[16]{};
  std::memcpy(buffer + 1, encoded.data(), encoded.size());

  {
    std::size_t offset{ 1 };
    std::int32_t decoded{};
    std::int32_t& result{ decode_value<Throw>(buffer, std::size(buffer), offset, decoded) };
    EXPECT_EQ(&decoded, &result);
    EXPECT_EQ(1u + sizeof(value), offset);
    EXPECT_EQ(value, decoded);
  }

  {
    std::int32_t decoded{};
    std::int32_t& result{ decode_value<Throw>(buffer, std::size(buffer), std::size_t{1}, decoded) };
    EXPECT_EQ(&decoded, &result);
    EXPECT_EQ(value, decoded);
  }

  {
    std::size_t offset{ 1 };
    std::int32_t decoded{};
    std::int32_t& result{ decode_value<Throw>(buffer, offset, decoded) };
    EXPECT_EQ(&decoded, &result);
    EXPECT_EQ(1u + sizeof(value), offset);
    EXPECT_EQ(value, decoded);
  }

  {
    std::int32_t decoded{};
    std::int32_t& result{ decode_value<Throw>(buffer, std::size_t{1}, decoded) };
    EXPECT_EQ(&decoded, &result);
    EXPECT_EQ(value, decoded);
  }
}

TEST(MachineNumDecode, SupportsValueArrayOverloads)
{
  std::uint16_t const expected_values[]{0x1122u, 0x3344u};
  auto const encoded{ object_bytes(expected_values) };
  std::byte buffer[16]{};
  std::memcpy(buffer + 2, encoded.data(), encoded.size());

  {
    std::size_t offset{ 2 };
    std::uint16_t decoded_values[std::size(expected_values)]{};
    std::uint16_t& result{ decode_value<Throw>(
      buffer,
      std::size(buffer),
      offset,
      decoded_values,
      std::size(decoded_values)) };
    EXPECT_EQ(&decoded_values[0], &result);
    EXPECT_EQ(2u + sizeof(expected_values), offset);
    expect_values_equal(expected_values, decoded_values);
  }

  {
    std::uint16_t decoded_values[std::size(expected_values)]{};
    std::uint16_t& result{ decode_value<Throw>(
      buffer,
      std::size(buffer),
      std::size_t{2},
      decoded_values,
      std::size(decoded_values)) };
    EXPECT_EQ(&decoded_values[0], &result);
    expect_values_equal(expected_values, decoded_values);
  }
}

TEST(MachineNumDecode, SupportsEnumAndValueReturningOverloads)
{
  constexpr SampleEnum enum_value{ SampleEnum::Beta };
  auto const enum_bytes{ object_bytes(enum_value) };
  std::byte enum_buffer[sizeof(SampleEnum)]{};
  std::memcpy(enum_buffer, enum_bytes.data(), enum_bytes.size());

  std::size_t enum_offset{ 0 };
  SampleEnum decoded_enum{ SampleEnum::Alpha };
  SampleEnum& enum_result{ decode_value<Throw>(enum_buffer, enum_offset, decoded_enum) };
  EXPECT_EQ(&decoded_enum, &enum_result);
  EXPECT_EQ(enum_value, decoded_enum);
  EXPECT_EQ(sizeof(SampleEnum), enum_offset);

  constexpr std::uint64_t value{ 0x0123456789abcdefULL };
  auto const value_bytes{ object_bytes(value) };
  std::byte value_buffer[sizeof(value)]{};
  std::memcpy(value_buffer, value_bytes.data(), value_bytes.size());

  std::size_t offset{ 0 };
  std::uint64_t const lvalue_result{ decode_value<Throw, std::uint64_t>(value_buffer, offset) };
  EXPECT_EQ(value, lvalue_result);
  EXPECT_EQ(sizeof(value), offset);

  std::uint64_t const rvalue_result{ decode_value<Throw, std::uint64_t>(value_buffer, std::size_t{0}) };
  EXPECT_EQ(value, rvalue_result);
}

TEST(MachineNumFailure, EncodeFailsWhenValueDoesNotFitRemainingBuffer)
{
  std::byte buffer[4]{};
  std::size_t offset{ 3 };
  std::uint16_t const value{ 0x1122u };
  EXPECT_EQ(0u, encode_value(buffer, std::size(buffer), offset, value));
  EXPECT_EQ(3u, offset);
}

TEST(MachineNumFailure, DecodeThrowsWhenValueDoesNotFitRemainingBuffer)
{
  std::byte const buffer[3]{};

  std::size_t offset{ 0 };
  std::uint32_t value{};
  EXPECT_THROW(
    decode_value<Throw>(buffer, offset, value),
    std::overflow_error
  );
}

TEST(MachineNumNoThrow, DecodeResetsOffsetAndPreservesDestinationWhenValueDoesNotFitRemainingBuffer)
{
  std::byte const buffer[3]{};

  std::size_t offset{ 0 };
  std::uint32_t value{ 0xdeadbeefu };
  std::uint32_t& result{ decode_value<NoThrow>(buffer, offset, value) };
  EXPECT_EQ(&value, &result);
  EXPECT_EQ(0u, offset);
  EXPECT_EQ(0xdeadbeefu, value);
}

} // namespace
