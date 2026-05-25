#include "../include/dynamic_num.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {

constexpr std::size_t kFailure = 0;
constexpr bool kConstexprEncodePointerOverloadWorks = []() constexpr {
  std::byte buffer[2]{};
  std::size_t offset = 0;
  std::size_t const end = encode_dnum(buffer, 2u, offset, int16_t(-600));
  return end == 2u
    && offset == 2u
    && buffer[0] == std::byte{0xaf}
    && buffer[1] == std::byte{0x09};
}();
static_assert(kConstexprEncodePointerOverloadWorks);

constexpr bool kConstexprEncodeArrayOverloadWorks = []() constexpr {
  std::byte buffer[2]{};
  std::size_t offset = 0;
  std::size_t const end = encode_dnum(buffer, offset, uint16_t(400));
  return end == 2u
    && offset == 2u
    && buffer[0] == std::byte{0x90}
    && buffer[1] == std::byte{0x03};
}();
static_assert(kConstexprEncodeArrayOverloadWorks);

constexpr bool kConstexprEncodeFailureWorks = []() constexpr {
  std::byte buffer[1]{};
  std::size_t offset = 0;
  std::size_t const end = encode_dnum(buffer, offset, uint16_t(400));
  return end == kFailure && offset == 0u;
}();
static_assert(kConstexprEncodeFailureWorks);

constexpr auto kConstexprEncodedTemplate = encode_dnum<int16_t, int16_t(-600)>();
static_assert(kConstexprEncodedTemplate.size() == 2u);
static_assert(kConstexprEncodedTemplate[0] == std::byte{0xaf});
static_assert(kConstexprEncodedTemplate[1] == std::byte{0x09});

constexpr bool kConstexprDecodePointerOverloadWorks = []() constexpr {
  std::byte const buffer[2]{
    std::byte{0xaf},
    std::byte{0x09}
  };
  std::size_t offset = 0;
  int16_t value{};
  decode_dnum<Throw>(buffer, 2u, offset, value);
  return offset == 2u && value == int16_t(-600);
}();
static_assert(kConstexprDecodePointerOverloadWorks);

constexpr bool kConstexprDecodeArrayOverloadWorks = []() constexpr {
  std::byte const buffer[2]{
    std::byte{0xaf},
    std::byte{0x09}
  };
  std::size_t offset = 0;
  int16_t value{};
  decode_dnum<Throw>(buffer, offset, value);
  return offset == 2u && value == int16_t(-600);
}();
static_assert(kConstexprDecodeArrayOverloadWorks);

constexpr bool kConstexprDecodeValueOverloadWorks = []() constexpr {
  std::byte const buffer[2]{
    std::byte{0xaf},
    std::byte{0x09}
  };
  std::size_t offset = 0;
  int16_t const value = decode_dnum<Throw, int16_t>(buffer, offset);
  return offset == 2u && value == int16_t(-600);
}();
static_assert(kConstexprDecodeValueOverloadWorks);

constexpr bool kConstexprDecodeNoThrowFailureWorks = []() constexpr {
  std::byte const buffer[1]{std::byte{0x80}};
  std::size_t offset = 0;
  uint8_t value = 42;
  decode_dnum<NoThrow>(buffer, offset, value);
  return offset == 0u && value == uint8_t(42);
}();
static_assert(kConstexprDecodeNoThrowFailureWorks);

/**
 * @brief Verify that a value encodes to the expected byte sequence and
 *   decodes back to the original value.
 *
 * @tparam T - Integral type being encoded and decoded.
 * @param value - Value to encode.
 * @param expected - Expected encoded byte sequence.
 */
template <typename T>
void expect_encoding(T value, std::initializer_list<std::byte> expected)
{
  std::byte buffer[16]{};

  std::size_t offset = 0;
  std::size_t const end = encode_dnum(buffer, offset, value);
  ASSERT_NE(kFailure, end);
  ASSERT_EQ(expected.size(), end);

  std::size_t i = 0;
  for (std::byte expected_byte : expected) {
    EXPECT_EQ(expected_byte, buffer[i]) << "byte index " << i;
    ++i;
  }

  std::size_t decoded_offset = 0;
  T decoded{};
  decode_dnum<Throw>(buffer, decoded_offset, decoded);
  EXPECT_EQ(expected.size(), decoded_offset);
  EXPECT_EQ(value, decoded);
}

/**
 * @brief Verify that a value round-trips through dynamic-number
 *   encoding/decoding.
 *
 * @tparam T - Integral type being encoded and decoded.
 * @param value - Value to round-trip.
 */
template <typename T>
void expect_round_trip(T value)
{
  std::byte buffer[16]{};

  std::size_t offset = 0;
  std::size_t const end = encode_dnum(buffer, offset, value);
  ASSERT_NE(kFailure, end);

  std::size_t decoded_offset = 0;
  T decoded{};
  decode_dnum<Throw>(buffer, decoded_offset, decoded);
  EXPECT_EQ(end, decoded_offset);
  EXPECT_EQ(value, decoded);
}

/**
 * @brief Verify round-trip behavior for the minimum, zero, and maximum values
 *   of an integral type.
 *
 * @tparam T - Integral type being checked.
 */
template <typename T>
void expect_min_max_round_trip()
{
  expect_round_trip<T>(std::numeric_limits<T>::lowest());
  expect_round_trip<T>(T(0));
  expect_round_trip<T>(std::numeric_limits<T>::max());
}

TEST(DynamicNumConstexpr, SupportsCompileTimeEvaluation)
{
  EXPECT_TRUE(kConstexprEncodePointerOverloadWorks);
  EXPECT_TRUE(kConstexprEncodeArrayOverloadWorks);
  EXPECT_TRUE(kConstexprEncodeFailureWorks);
  EXPECT_EQ(2u, kConstexprEncodedTemplate.size());
  EXPECT_EQ(std::byte{0xaf}, kConstexprEncodedTemplate[0]);
  EXPECT_EQ(std::byte{0x09}, kConstexprEncodedTemplate[1]);
  EXPECT_TRUE(kConstexprDecodePointerOverloadWorks);
  EXPECT_TRUE(kConstexprDecodeArrayOverloadWorks);
  EXPECT_TRUE(kConstexprDecodeValueOverloadWorks);
  EXPECT_TRUE(kConstexprDecodeNoThrowFailureWorks);
}

TEST(DynamicNumEncode, EncodesKnownUnsignedValues)
{
  expect_encoding<uint8_t>(
    uint8_t(1),
    {std::byte{0x01}}
  );

  expect_encoding<uint16_t>(
    uint16_t(400),
    {std::byte{0x90}, std::byte{0x03}}
  );

  expect_encoding<uint32_t>(
    uint32_t(700),
    {std::byte{0xbc}, std::byte{0x05}}
  );

  expect_encoding<uint64_t>(
    uint64_t(1000),
    {std::byte{0xe8}, std::byte{0x07}}
  );
}

TEST(DynamicNumEncode, EncodesKnownSignedValues)
{
  expect_encoding<int8_t>(
    int8_t(2),
    {std::byte{0x04}}
  );

  expect_encoding<int8_t>(
    int8_t(-3),
    {std::byte{0x05}}
  );

  expect_encoding<int16_t>(
    int16_t(500),
    {std::byte{0xe8}, std::byte{0x07}}
  );

  expect_encoding<int16_t>(
    int16_t(-600),
    {std::byte{0xaf}, std::byte{0x09}}
  );

  expect_encoding<int32_t>(
    int32_t(800),
    {std::byte{0xc0}, std::byte{0x0c}}
  );

  expect_encoding<int32_t>(
    int32_t(-900),
    {std::byte{0x87}, std::byte{0x0e}}
  );

  expect_encoding<int64_t>(
    int64_t(1100),
    {std::byte{0x98}, std::byte{0x11}}
  );

  expect_encoding<int64_t>(
    int64_t(-1200),
    {std::byte{0xdf}, std::byte{0x12}}
  );
}

TEST(DynamicNumRoundTrip, HandlesMinZeroMaxForIntegerTypes)
{
  expect_min_max_round_trip<uint8_t>();
  expect_min_max_round_trip<int8_t>();

  expect_min_max_round_trip<uint16_t>();
  expect_min_max_round_trip<int16_t>();

  expect_min_max_round_trip<uint32_t>();
  expect_min_max_round_trip<int32_t>();

  expect_min_max_round_trip<uint64_t>();
  expect_min_max_round_trip<int64_t>();
}

TEST(DynamicNumStream, EncodesAndDecodesSequentialValues)
{
  std::byte buffer[21]{};
  std::size_t offset = 0;

  offset = encode_dnum(buffer, offset, uint8_t(1));
  ASSERT_NE(kFailure, offset);
  offset = encode_dnum(buffer, offset, int8_t(2));
  ASSERT_NE(kFailure, offset);
  offset = encode_dnum(buffer, offset, int8_t(-3));
  ASSERT_NE(kFailure, offset);

  offset = encode_dnum(buffer, offset, uint16_t(400));
  ASSERT_NE(kFailure, offset);
  offset = encode_dnum(buffer, offset, int16_t(500));
  ASSERT_NE(kFailure, offset);
  offset = encode_dnum(buffer, offset, int16_t(-600));
  ASSERT_NE(kFailure, offset);

  offset = encode_dnum(buffer, offset, uint32_t(700));
  ASSERT_NE(kFailure, offset);
  offset = encode_dnum(buffer, offset, int32_t(800));
  ASSERT_NE(kFailure, offset);
  offset = encode_dnum(buffer, offset, int32_t(-900));
  ASSERT_NE(kFailure, offset);

  offset = encode_dnum(buffer, offset, uint64_t(1000));
  ASSERT_NE(kFailure, offset);
  offset = encode_dnum(buffer, offset, int64_t(1100));
  ASSERT_NE(kFailure, offset);
  offset = encode_dnum(buffer, offset, int64_t(-1200));
  ASSERT_NE(kFailure, offset);

  ASSERT_EQ(21u, offset);

  uint8_t uint8_v{};
  int8_t int8_v{};
  uint16_t uint16_v{};
  int16_t int16_v{};
  uint32_t uint32_v{};
  int32_t int32_v{};
  uint64_t uint64_v{};
  int64_t int64_v{};

  offset = 0;
  decode_dnum<Throw>(buffer, offset, uint8_v);
  EXPECT_EQ(1, uint8_v);

  decode_dnum<Throw>(buffer, offset, int8_v);
  EXPECT_EQ(2, int8_v);

  decode_dnum<Throw>(buffer, offset, int8_v);
  EXPECT_EQ(-3, int8_v);

  decode_dnum<Throw>(buffer, offset, uint16_v);
  EXPECT_EQ(400, uint16_v);

  decode_dnum<Throw>(buffer, offset, int16_v);
  EXPECT_EQ(500, int16_v);

  decode_dnum<Throw>(buffer, offset, int16_v);
  EXPECT_EQ(-600, int16_v);

  decode_dnum<Throw>(buffer, offset, uint32_v);
  EXPECT_EQ(700u, uint32_v);

  decode_dnum<Throw>(buffer, offset, int32_v);
  EXPECT_EQ(800, int32_v);

  decode_dnum<Throw>(buffer, offset, int32_v);
  EXPECT_EQ(-900, int32_v);

  decode_dnum<Throw>(buffer, offset, uint64_v);
  EXPECT_EQ(1000u, uint64_v);

  decode_dnum<Throw>(buffer, offset, int64_v);
  EXPECT_EQ(1100, int64_v);

  decode_dnum<Throw>(buffer, offset, int64_v);
  EXPECT_EQ(-1200, int64_v);
  EXPECT_EQ(21u, offset);
}

TEST(DynamicNumDecode, ReturnsValueFromValueReturningOverload)
{
  std::byte const buffer[2]{
    std::byte{0xaf},
    std::byte{0x09}
  };

  std::size_t offset = 0;
  int16_t const value = decode_dnum<Throw, int16_t>(buffer, offset);
  EXPECT_EQ(-600, value);
  EXPECT_EQ(2u, offset);
}

TEST(DynamicNumFailure, EncodeFailsWhenStartOffsetIsAtEnd)
{
  std::byte buffer[1]{};
  std::size_t offset = 1;
  EXPECT_EQ(kFailure, encode_dnum(buffer, offset, uint8_t(1)));
  EXPECT_EQ(1u, offset);
}

TEST(DynamicNumFailure, EncodeFailsWhenValueOutgrowsRemainingBuffer)
{
  std::byte buffer[1]{};
  std::size_t offset = 0;
  EXPECT_EQ(kFailure, encode_dnum(buffer, offset, uint16_t(400)));
  EXPECT_EQ(0u, offset);
}

TEST(DynamicNumFailure, DecodeThrowsWhenStartOffsetIsOutOfBounds)
{
  std::byte const buffer[1]{};

  std::size_t offset = 1;
  uint8_t value{};
  EXPECT_THROW(
    decode_dnum<Throw>(buffer, offset, value),
    std::overflow_error
  );
}

TEST(DynamicNumNoThrow, DecodeResetsOffsetWhenStartOffsetIsOutOfBounds)
{
  std::byte const buffer[1]{};

  std::size_t offset = 1;
  uint8_t value = 42;
  uint8_t& result = decode_dnum<NoThrow>(buffer, offset, value);
  EXPECT_EQ(&value, &result);
  EXPECT_EQ(0u, offset);
  EXPECT_EQ(uint8_t(42), value);
}

TEST(DynamicNumFailure, DecodeThrowsForUnterminatedValue)
{
  std::byte const buffer[1]{std::byte{0x80}};

  std::size_t offset = 0;
  uint8_t value{};
  EXPECT_THROW(
    decode_dnum<Throw>(buffer, offset, value),
    std::overflow_error
  );
}

TEST(DynamicNumNoThrow, DecodeResetsOffsetForUnterminatedValue)
{
  std::byte const buffer[1]{std::byte{0x80}};

  std::size_t offset = 0;
  uint8_t value = 42;
  uint8_t& result = decode_dnum<NoThrow>(buffer, offset, value);
  EXPECT_EQ(&value, &result);
  EXPECT_EQ(0u, offset);
  EXPECT_EQ(uint8_t(42), value);
}

TEST(DynamicNumFailure, DecodeThrowsWhenValueDoesNotFitTargetType)
{
  std::byte const buffer[2]{
    std::byte{0x80},
    std::byte{0x02}
  };

  std::size_t offset = 0;
  uint8_t value{};
  EXPECT_THROW(
    decode_dnum<Throw>(buffer, offset, value),
    std::overflow_error
  );
}

TEST(DynamicNumNoThrow, DecodeResetsOffsetWhenValueDoesNotFitTargetType)
{
  std::byte const buffer[2]{
    std::byte{0x80},
    std::byte{0x02}
  };

  std::size_t offset = 0;
  uint8_t value = 42;
  uint8_t& result = decode_dnum<NoThrow>(buffer, offset, value);
  EXPECT_EQ(&value, &result);
  EXPECT_EQ(0u, offset);
  EXPECT_EQ(uint8_t(42), value);
}

} // namespace
