#include "constexpr/machine_num.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>

namespace {

// Small enum used to prove the enum overloads preserve raw storage bytes.
enum class SampleEnum : std::uint16_t {
  Alpha = 0x1234,
  Beta = 0xabcd
};

// Shared byte helpers
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
  // Copy the raw object representation exactly as machine_num will encode it.
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
  // Arrays should flatten into one contiguous machine-endian byte sequence.
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
  // Compare byte-by-byte so failures identify the exact mismatched position.
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
  // Compare element-by-element so array decoding failures stay readable.
  for (std::size_t i{ 0 }; i < N; ++i) {
    EXPECT_EQ(expected[i], actual[i]) << "element index " << i;
  }
}

// Runtime coverage
TEST(MachineNumEncode, SupportsSingleValueEncoding)
{
  // Use one known value so the returned pointer and written bytes can be verified.
  constexpr std::uint32_t value{ 0x12345678u };
  auto const expected{ object_bytes(value) };

  {
    // Encode at offset 1 and verify the returned pointer advanced by sizeof(T).
    std::byte buffer[16]{};
    std::byte* const end{ encode_value(buffer + 1, std::end(buffer), value) };
    ASSERT_EQ(buffer + 1 + sizeof(value), end);
    expect_buffer_bytes(buffer, 1, expected);
  }

  {
    // Encode at offset 3 to prove arbitrary starting positions work.
    std::byte buffer[16]{};
    std::byte* const end{ encode_value(buffer + 3, std::end(buffer), value) };
    ASSERT_EQ(buffer + 3 + sizeof(value), end);
    expect_buffer_bytes(buffer, 3, expected);
  }
}

TEST(MachineNumEncode, SupportsValueArrayEncoding)
{
  // The array overload should emit the two raw object payloads back-to-back.
  std::uint16_t const values[]{ 0x1122u, 0x3344u };
  auto const expected{ object_bytes(values) };

  std::byte buffer[16]{};
  std::byte* const end{ encode_value(buffer + 1, std::end(buffer), values, std::size(values)) };
  ASSERT_EQ(buffer + 1 + sizeof(values), end);
  expect_buffer_bytes(buffer, 1, expected);
}

TEST(MachineNumDecode, SupportsSingleValueDecoding)
{
  // Seed the buffer with one machine-endian payload at an offset of one byte.
  constexpr std::int32_t value{ -123456789 };
  auto const encoded{ object_bytes(value) };
  std::byte buffer[16]{};
  std::memcpy(buffer + 1, encoded.data(), encoded.size());

  {
    // By-reference overload: iterator advances, decoded value matches.
    std::byte const* src{ buffer + 1 };
    std::int32_t decoded{};
    std::int32_t& result{ decode_value<Throw>(src, std::end(buffer), decoded) };
    EXPECT_EQ(&decoded, &result);
    EXPECT_EQ(buffer + 1 + sizeof(value), src);
    EXPECT_EQ(value, decoded);
  }

  {
    // By-value overload: iterator advances, returned value matches.
    std::byte const* src{ buffer + 1 };
    std::int32_t const result{ decode_value<Throw, std::int32_t>(src, std::end(buffer)) };
    EXPECT_EQ(buffer + 1 + sizeof(value), src);
    EXPECT_EQ(value, result);
  }
}

TEST(MachineNumDecode, SupportsValueArrayDecoding)
{
  // Seed the buffer with two adjacent values and decode them as an array.
  std::uint16_t const expected_values[]{ 0x1122u, 0x3344u };
  auto const encoded{ object_bytes(expected_values) };
  std::byte buffer[16]{};
  std::memcpy(buffer + 2, encoded.data(), encoded.size());

  std::byte const* src{ buffer + 2 };
  std::uint16_t decoded_values[std::size(expected_values)]{};
  std::uint16_t& result{ decode_value<Throw>(
    src,
    std::end(buffer),
    decoded_values,
    std::size(decoded_values)) };
  EXPECT_EQ(&decoded_values[0], &result);
  EXPECT_EQ(buffer + 2 + sizeof(expected_values), src);
  expect_values_equal(expected_values, decoded_values);
}

TEST(MachineNumDecode, SupportsEnumDecoding)
{
  // Prove enum decoding uses the same raw-byte contract as arithmetic types.
  constexpr SampleEnum enum_value{ SampleEnum::Beta };
  auto const enum_bytes{ object_bytes(enum_value) };
  std::byte buffer[sizeof(SampleEnum)]{};
  std::memcpy(buffer, enum_bytes.data(), enum_bytes.size());

  {
    // By-reference overload.
    std::byte const* src{ buffer };
    SampleEnum decoded{ SampleEnum::Alpha };
    SampleEnum& result{ decode_value<Throw>(src, std::end(buffer), decoded) };
    EXPECT_EQ(&decoded, &result);
    EXPECT_EQ(enum_value, decoded);
    EXPECT_EQ(buffer + sizeof(SampleEnum), src);
  }

  {
    // By-value overload.
    std::byte const* src{ buffer };
    SampleEnum const result{ decode_value<Throw, SampleEnum>(src, std::end(buffer)) };
    EXPECT_EQ(enum_value, result);
    EXPECT_EQ(buffer + sizeof(SampleEnum), src);
  }
}

TEST(MachineNumFailure, DecodeThrowsWhenValueDoesNotFitRemainingBuffer)
{
  // Throwing decode should reject a truncated raw object representation.
  std::byte const buffer[3]{};
  std::byte const* src{ buffer };
  std::uint32_t value{};
  EXPECT_THROW(
    decode_value<Throw>(src, std::end(buffer), value),
    std::overflow_error
  );
}

TEST(MachineNumNoThrow, DecodeResetsIteratorAndPreservesDestinationWhenValueDoesNotFitRemainingBuffer)
{
  // The no-throw variant should restore the iterator and leave v_dst untouched.
  std::byte const buffer[3]{};
  std::byte const* src{ buffer };
  std::uint32_t value{ 0xdeadbeefu };
  std::uint32_t& result{ decode_value<NoThrow>(src, std::end(buffer), value) };
  EXPECT_EQ(&value, &result);
  EXPECT_EQ(buffer, src);
  EXPECT_EQ(0xdeadbeefu, value);
}

} // namespace
