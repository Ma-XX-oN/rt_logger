#include "constexpr/bit.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace {

struct WordPair {
  std::uint16_t low;
  std::uint16_t high;
};

static_assert(std::is_trivially_copyable_v<WordPair>);
static_assert(Constexpr::has_constexpr_bit_cast_v<std::int16_t, std::uint16_t>);
static_assert(Constexpr::has_constexpr_bit_cast_v<std::uint16_t, std::int16_t>);
static_assert(
  Constexpr::has_constexpr_bit_cast_v<float, std::uint32_t>
  == Constexpr::has_native_constexpr_bit_cast_v
);

constexpr bool kIntegerFallbackSupportsCompileTimeRoundTrip{ []() constexpr {
  // Exercise the integer-only fallback directly even when a native backend exists.
  constexpr std::uint16_t source{ 0xff9c };
  constexpr std::int16_t signed_value{
    Constexpr::detail::integer_bit_cast<std::int16_t>(source)
  };
  constexpr std::uint16_t round_trip{
    Constexpr::detail::integer_bit_cast<std::uint16_t>(signed_value)
  };
  return round_trip == source;
}() };
static_assert(kIntegerFallbackSupportsCompileTimeRoundTrip);

constexpr bool kBitCastSupportsCompileTimeIntegerRoundTrip{ []() constexpr {
  // Verify that a signed/unsigned reinterpretation preserves all bits.
  constexpr std::uint16_t source{ 0xff9c };
  constexpr std::int16_t signed_value{ Constexpr::bit_cast<std::int16_t>(source) };
  constexpr std::uint16_t round_trip{ Constexpr::bit_cast<std::uint16_t>(signed_value) };
  return round_trip == source;
}() };
static_assert(kBitCastSupportsCompileTimeIntegerRoundTrip);

constexpr bool kBitCastSupportsCompileTimeByteRoundTrip{ []() constexpr {
  // Round-trip a trivially copyable struct through raw bytes.
  constexpr WordPair source{ std::uint16_t{0x1234}, std::uint16_t{0xabcd} };
  constexpr auto bytes{
    Constexpr::bit_cast<std::array<std::byte, sizeof(WordPair)>>(source)
  };
  constexpr WordPair round_trip{ Constexpr::bit_cast<WordPair>(bytes) };
  return round_trip.low == source.low
    && round_trip.high == source.high;
}() };
static_assert(kBitCastSupportsCompileTimeByteRoundTrip);

constexpr bool kBitCastSupportsCompileTimeFloatRoundTrip{ []() constexpr {
  if constexpr (Constexpr::has_constexpr_bit_cast_v<float, std::uint32_t>) {
    // Float and integer payloads should round-trip through the same bit pattern.
    constexpr std::uint32_t source{ 0x3f800000u };
    constexpr float value{ Constexpr::bit_cast<float>(source) };
    constexpr std::uint32_t round_trip{ Constexpr::bit_cast<std::uint32_t>(value) };
    return value == 1.0F
      && round_trip == source;
  } else {
    return true;
  }
}() };
static_assert(kBitCastSupportsCompileTimeFloatRoundTrip);

TEST(BitConstexpr, SupportsCompileTimeEvaluation)
{
  EXPECT_TRUE((Constexpr::has_constexpr_bit_cast_v<std::int16_t, std::uint16_t>));
  EXPECT_TRUE(kIntegerFallbackSupportsCompileTimeRoundTrip);
  EXPECT_TRUE(kBitCastSupportsCompileTimeIntegerRoundTrip);
  EXPECT_TRUE(kBitCastSupportsCompileTimeByteRoundTrip);
  EXPECT_TRUE(kBitCastSupportsCompileTimeFloatRoundTrip);
}

TEST(BitRuntime, RoundTripsSignedAndUnsignedRepresentations)
{
  // The integer round-trip should preserve every source bit.
  std::uint16_t const unsigned_value{ 0xff9c };
  std::int16_t const signed_value{ Constexpr::bit_cast<std::int16_t>(unsigned_value) };
  std::uint16_t const round_trip{ Constexpr::bit_cast<std::uint16_t>(signed_value) };

  EXPECT_EQ(unsigned_value, round_trip);
}

TEST(BitRuntime, RoundTripsTrivialStructThroughBytes)
{
  // Byte-wise transport should preserve both struct members.
  WordPair const source{ std::uint16_t{0x55aa}, std::uint16_t{0xaa55} };
  auto const bytes{
    Constexpr::bit_cast<std::array<std::byte, sizeof(WordPair)>>(source)
  };
  WordPair const round_trip{ Constexpr::bit_cast<WordPair>(bytes) };

  EXPECT_EQ(source.low, round_trip.low);
  EXPECT_EQ(source.high, round_trip.high);
}

TEST(BitRuntime, RoundTripsFloatAndUint32Representations)
{
  if constexpr (!Constexpr::has_constexpr_bit_cast_v<float, std::uint32_t>) {
    GTEST_SKIP() << "Native constexpr bit_cast backend unavailable in this build";
  }

  // The canonical IEEE-754 payload for 1.0f should survive both directions.
  std::uint32_t const source{ 0x3f800000u };
  float const value{ Constexpr::bit_cast<float>(source) };
  std::uint32_t const round_trip{ Constexpr::bit_cast<std::uint32_t>(value) };

  EXPECT_FLOAT_EQ(1.0F, value);
  EXPECT_EQ(source, round_trip);
}

} // namespace
