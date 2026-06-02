#include "constexpr/dynamic_int.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>

namespace {

// Compile-time coverage
constexpr bool kConstexprEncodePointerOverloadWorks{ []() constexpr {
  // Encode a known signed payload through the pointer overload.
  std::byte buffer[2]{};
  std::byte* const end_it{ encode_dint(buffer, buffer + 2, int16_t(-600)) };
  return end_it == buffer + 2
    && buffer[0] == std::byte{0xd7}
    && buffer[1] == std::byte{0x44};
}() };
static_assert(kConstexprEncodePointerOverloadWorks);

constexpr bool kConstexprEncodedTemplateWorks{ []() constexpr {
  // The value-template overload should produce the same two-byte payload.
  constexpr auto encoded{ encode_dint<int16_t, int16_t(-600)>() };
  return encoded.size() == 2u
    && encoded[0] == std::byte{0xd7}
    && encoded[1] == std::byte{0x44};
}() };
static_assert(kConstexprEncodedTemplateWorks);

// Runtime helper assertions
/**
 * @brief Verify that a value encodes to the expected byte sequence and
 *   decodes back to the original value.
 *
 * @tparam T Integral type being encoded and decoded.
 * @param value Value to encode.
 * @param expected Expected encoded byte sequence.
 */
template <typename T>
void expect_encoding(T value, std::initializer_list<std::byte> expected)
{
  std::byte buffer[16]{};

  // Encode once and confirm the byte count matches the golden payload.
  std::byte* const end_it{ encode_dint(buffer, buffer + 16, value) };
  std::size_t const encoded_size{ static_cast<std::size_t>(end_it - buffer) };
  ASSERT_EQ(expected.size(), encoded_size);

  // Then compare each emitted byte for readable failure output.
  std::size_t i{ 0 };
  for (std::byte expected_byte : expected) {
    EXPECT_EQ(expected_byte, buffer[i]) << "byte index " << i;
    ++i;
  }

  // Finally decode the buffer and prove the iterator is fully consumed.
  std::byte const* read_it{ buffer };
  T decoded{};
  decode_dint<Throw>(read_it, end_it, decoded);
  EXPECT_EQ(end_it, read_it);
  EXPECT_EQ(value, decoded);
}

/**
 * @brief Verify that a value round-trips through dynamic-int
 *   encoding/decoding.
 *
 * @tparam T Integral type being encoded and decoded.
 * @param value Value to round-trip.
 */
template <typename T>
void expect_round_trip(T value)
{
  std::byte buffer[16]{};

  // Encode the source value into a scratch buffer first.
  std::byte* const end_it{ encode_dint(buffer, buffer + 16, value) };

  // Then decode it back and make sure no trailing bytes remain unread.
  std::byte const* read_it{ buffer };
  T decoded{};
  decode_dint<Throw>(read_it, end_it, decoded);
  EXPECT_EQ(end_it, read_it);
  EXPECT_EQ(value, decoded);
}

/**
 * @brief Verify round-trip behavior for the minimum, zero, and maximum values
 *   of an integral type.
 *
 * @tparam T Integral type being checked.
 */
template <typename T>
void expect_min_max_round_trip()
{
  // Probe the full range anchor points for the target integer type.
  expect_round_trip<T>(std::numeric_limits<T>::lowest());
  expect_round_trip<T>(T(0));
  expect_round_trip<T>(std::numeric_limits<T>::max());
}

// Runtime coverage
TEST(DynamicIntConstexpr, SupportsCompileTimeEncoding)
{
  // Both constexpr entry points should agree on the known signed payload.
  EXPECT_TRUE(kConstexprEncodePointerOverloadWorks);
  EXPECT_TRUE(kConstexprEncodedTemplateWorks);
}

TEST(DynamicIntEncode, EncodesKnownUnsignedValues)
{
  // Check one representative positive value at each unsigned width.
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

TEST(DynamicIntEncode, EncodesKnownSignedValues)
{
  // Check both positive and negative representatives across signed widths.
  expect_encoding<int8_t>(
    int8_t(2),
    {std::byte{0x02}}
  );

  expect_encoding<int8_t>(
    int8_t(-3),
    {std::byte{0x42}}
  );

  expect_encoding<int16_t>(
    int16_t(500),
    {std::byte{0xf4}, std::byte{0x03}}
  );

  expect_encoding<int16_t>(
    int16_t(-600),
    {std::byte{0xd7}, std::byte{0x44}}
  );

  expect_encoding<int32_t>(
    int32_t(800),
    {std::byte{0xa0}, std::byte{0x06}}
  );

  expect_encoding<int32_t>(
    int32_t(-900),
    {std::byte{0x83}, std::byte{0x47}}
  );

  expect_encoding<int64_t>(
    int64_t(1100),
    {std::byte{0xcc}, std::byte{0x08}}
  );

  expect_encoding<int64_t>(
    int64_t(-1200),
    {std::byte{0xaf}, std::byte{0x49}}
  );
}

TEST(DynamicIntEncode, UsesExtraSignByteOnlyWhenNeeded)
{
  // Positive values at the sign boundary need an extra byte to stay positive.
  expect_encoding<int8_t>(
    int8_t(64),
    {std::byte{0xc0}, std::byte{0x00}}
  );

  // The matching negative boundary still fits in one encoded byte.
  expect_encoding<int8_t>(
    int8_t(-64),
    {std::byte{0x7f}}
  );

  // The signed extremes should still round-trip through the chosen encoding.
  expect_encoding<int8_t>(
    std::numeric_limits<int8_t>::max(),
    {std::byte{0xff}, std::byte{0x00}}
  );

  expect_encoding<int8_t>(
    std::numeric_limits<int8_t>::lowest(),
    {std::byte{0xff}, std::byte{0x40}}
  );
}

TEST(DynamicIntRoundTrip, HandlesMinZeroMaxForIntegerTypes)
{
  // Reuse the same lowest/zero/max contract at every supported width.
  expect_min_max_round_trip<uint8_t>();
  expect_min_max_round_trip<int8_t>();

  expect_min_max_round_trip<uint16_t>();
  expect_min_max_round_trip<int16_t>();

  expect_min_max_round_trip<uint32_t>();
  expect_min_max_round_trip<int32_t>();

  expect_min_max_round_trip<uint64_t>();
  expect_min_max_round_trip<int64_t>();
}

TEST(DynamicIntStream, EncodesAndDecodesSequentialValues)
{
  // Build one mixed stream that exercises every supported integer family.
  std::byte buffer[21]{};
  std::byte* write_it{ buffer };

  // Encode the one-byte values first.
  write_it = encode_dint(write_it, buffer + 21, uint8_t(1));
  write_it = encode_dint(write_it, buffer + 21, int8_t(2));
  write_it = encode_dint(write_it, buffer + 21, int8_t(-3));

  // Then append the 16-bit values.
  write_it = encode_dint(write_it, buffer + 21, uint16_t(400));
  write_it = encode_dint(write_it, buffer + 21, int16_t(500));
  write_it = encode_dint(write_it, buffer + 21, int16_t(-600));

  // Then the 32-bit values.
  write_it = encode_dint(write_it, buffer + 21, uint32_t(700));
  write_it = encode_dint(write_it, buffer + 21, int32_t(800));
  write_it = encode_dint(write_it, buffer + 21, int32_t(-900));

  // Finish the stream with the 64-bit values.
  write_it = encode_dint(write_it, buffer + 21, uint64_t(1000));
  write_it = encode_dint(write_it, buffer + 21, int64_t(1100));
  write_it = encode_dint(write_it, buffer + 21, int64_t(-1200));

  ASSERT_EQ(buffer + 21, write_it);

  // Decode back in the same order so iterator state is exercised too.
  uint8_t uint8_v{};
  int8_t int8_v{};
  uint16_t uint16_v{};
  int16_t int16_v{};
  uint32_t uint32_v{};
  int32_t int32_v{};
  uint64_t uint64_v{};
  int64_t int64_v{};

  std::byte const* read_it{ buffer };

  // The first three values cover the one-byte cases.
  decode_dint<Throw>(read_it, write_it, uint8_v);
  EXPECT_EQ(1, uint8_v);

  decode_dint<Throw>(read_it, write_it, int8_v);
  EXPECT_EQ(2, int8_v);

  decode_dint<Throw>(read_it, write_it, int8_v);
  EXPECT_EQ(-3, int8_v);

  // Then step through the 16-bit encodings.
  decode_dint<Throw>(read_it, write_it, uint16_v);
  EXPECT_EQ(400, uint16_v);

  decode_dint<Throw>(read_it, write_it, int16_v);
  EXPECT_EQ(500, int16_v);

  decode_dint<Throw>(read_it, write_it, int16_v);
  EXPECT_EQ(-600, int16_v);

  // Then the 32-bit encodings.
  decode_dint<Throw>(read_it, write_it, uint32_v);
  EXPECT_EQ(700u, uint32_v);

  decode_dint<Throw>(read_it, write_it, int32_v);
  EXPECT_EQ(800, int32_v);

  decode_dint<Throw>(read_it, write_it, int32_v);
  EXPECT_EQ(-900, int32_v);

  // Finish with the 64-bit encodings and confirm the stream is exhausted.
  decode_dint<Throw>(read_it, write_it, uint64_v);
  EXPECT_EQ(1000u, uint64_v);

  decode_dint<Throw>(read_it, write_it, int64_v);
  EXPECT_EQ(1100, int64_v);

  decode_dint<Throw>(read_it, write_it, int64_v);
  EXPECT_EQ(-1200, int64_v);
  EXPECT_EQ(write_it, read_it);
}

TEST(DynamicIntCharStorage, RoundTripsPlainCharBytes)
{
  // Plain char storage should preserve the encoded byte sequence on this target.
  char buffer[2]{};
  char* const write_it{ encode_dint(buffer, buffer + 2, int16_t(-600)) };
  ASSERT_EQ(buffer + 2, write_it);
  EXPECT_EQ(0xd7u, static_cast<unsigned char>(buffer[0]));
  EXPECT_EQ(0x44u, static_cast<unsigned char>(buffer[1]));

  char const* read_it{ buffer };
  char const* const end_it{ write_it };
  int16_t value{};
  decode_dint<Throw>(read_it, end_it, value);
  EXPECT_EQ(int16_t(-600), value);
  EXPECT_EQ(end_it, read_it);
}

TEST(DynamicIntCharStorage, RoundTripsSignedCharBytes)
{
  // Signed-char storage exercises the negative-byte read path explicitly.
  signed char buffer[2]{};
  signed char* const write_it{ encode_dint(buffer, buffer + 2, int16_t(-600)) };
  ASSERT_EQ(buffer + 2, write_it);
  EXPECT_EQ(0xd7u, static_cast<unsigned char>(buffer[0]));
  EXPECT_EQ(0x44u, static_cast<unsigned char>(buffer[1]));

  signed char const* read_it{ buffer };
  signed char const* const end_it{ write_it };
  int16_t value{};
  decode_dint<Throw>(read_it, end_it, value);
  EXPECT_EQ(int16_t(-600), value);
  EXPECT_EQ(end_it, read_it);
}

TEST(DynamicIntDecode, ReturnsValueFromValueReturningOverload)
{
  // The value-returning overload should decode and advance the iterator.
  std::byte const buffer[2]{
    std::byte{0xd7},
    std::byte{0x44}
  };

  std::byte const* read_it{ buffer };
  int16_t const value{ decode_dint<Throw, int16_t>(read_it, buffer + 2) };
  EXPECT_EQ(-600, value);
  EXPECT_EQ(buffer + 2, read_it);
}

TEST(DynamicIntFailure, DecodeThrowsWhenSourceRangeIsEmpty)
{
  // An empty source range cannot even provide the first encoded byte.
  std::byte const buffer[1]{};
  std::byte const* read_it{ buffer };
  uint8_t value{};
  EXPECT_THROW(
    decode_dint<Throw>(read_it, buffer, value),
    std::overflow_error
  );
}

TEST(DynamicIntNoThrow, DecodeLeavesIteratorWhenSourceRangeIsEmpty)
{
  // The no-throw variant should preserve both iterator and destination state.
  std::byte const buffer[1]{};
  std::byte const* read_it{ buffer };
  uint8_t value{ 42 };
  uint8_t& result{ decode_dint<NoThrow>(read_it, buffer, value) };
  EXPECT_EQ(&value, &result);
  EXPECT_EQ(buffer, read_it);
  EXPECT_EQ(uint8_t(42), value);
}

TEST(DynamicIntFailure, DecodeThrowsForUnterminatedValue)
{
  // A continuation byte with no terminator should be rejected.
  std::byte const buffer[1]{std::byte{0x80}};

  std::byte const* read_it{ buffer };
  uint8_t value{};
  EXPECT_THROW(
    decode_dint<Throw>(read_it, buffer + 1, value),
    std::overflow_error
  );
}

TEST(DynamicIntNoThrow, DecodeResetsIteratorForUnterminatedValue)
{
  // The no-throw variant should rewind the iterator on partial input.
  std::byte const buffer[1]{std::byte{0x80}};

  std::byte const* read_it{ buffer };
  uint8_t value{ 42 };
  uint8_t& result{ decode_dint<NoThrow>(read_it, buffer + 1, value) };
  EXPECT_EQ(&value, &result);
  EXPECT_EQ(buffer, read_it);
  EXPECT_EQ(uint8_t(42), value);
}

TEST(DynamicIntFailure, DecodeThrowsWhenValueDoesNotFitTargetType)
{
  // This byte sequence decodes to a value larger than uint8_t can hold.
  std::byte const buffer[2]{
    std::byte{0x80},
    std::byte{0x02}
  };

  std::byte const* read_it{ buffer };
  uint8_t value{};
  EXPECT_THROW(
    decode_dint<Throw>(read_it, buffer + 2, value),
    std::overflow_error
  );
}

TEST(DynamicIntNoThrow, DecodeResetsIteratorWhenValueDoesNotFitTargetType)
{
  // The no-throw path should leave the caller's destination untouched.
  std::byte const buffer[2]{
    std::byte{0x80},
    std::byte{0x02}
  };

  std::byte const* read_it{ buffer };
  uint8_t value{ 42 };
  uint8_t& result{ decode_dint<NoThrow>(read_it, buffer + 2, value) };
  EXPECT_EQ(&value, &result);
  EXPECT_EQ(buffer, read_it);
  EXPECT_EQ(uint8_t(42), value);
}

} // namespace
