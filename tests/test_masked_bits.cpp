#include "constexpr/masked_bits.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

namespace {

// Scoped enum version of the sparse anchor case for enum-overload coverage.
enum class SparseEnumBits : std::uint8_t {
  Mask = 0b10110u,
  Value = 0b10010u,
};

// Canonical sparse-mask example used throughout the file.
constexpr std::uint8_t kSparseMask{ 0b10110u };
constexpr std::uint8_t kSparseValue{ 0b10010u };
constexpr std::uint8_t kSparsePackedLsb{ 0b00101u };
constexpr std::uint8_t kSparsePackedAligned{ 0b01010u };

// Same logical payload under a contiguous three-bit mask.
constexpr std::uint8_t kContiguousMask{ 0b01110u };
constexpr std::uint8_t kContiguousValue{ 0b01010u };
constexpr std::uint8_t kContiguousPackedLsb{ 0b00101u };
constexpr std::uint8_t kContiguousPackedAligned{ 0b01010u };

// A bottom-bit-only mask exercises the no-offset path with the smallest field.
// Both packed forms are identical because the selected bit already lives at bit 0.
constexpr std::uint32_t kLeastBitMask32{ 0x00000001u };
constexpr std::uint32_t kLeastBitValue32{ 0x00000001u };
constexpr std::uint32_t kLeastBitPackedLsb32{ 0x00000001u };
constexpr std::uint32_t kLeastBitPackedAligned32{ 0x00000001u };

// A top-bit-only mask exercises the opposite edge of the word.
// LSB-packed form becomes `1`, while aligned form preserves the original top bit.
constexpr std::uint32_t kMostBitMask32{ 0x80000000u };
constexpr std::uint32_t kMostBitValue32{ 0x80000000u };
constexpr std::uint32_t kMostBitPackedLsb32{ 0x00000001u };
constexpr std::uint32_t kMostBitPackedAligned32{ 0x80000000u };

// A top-and-bottom mask spans the full word while still starting at bit 0.
// Condensing pulls those two extreme bits together into the packed field `0b11`.
constexpr std::uint32_t kEdgeBitsMask32{ 0x80000001u };
constexpr std::uint32_t kEdgeBitsValue32{ 0x80000001u };
constexpr std::uint32_t kEdgeBitsPackedLsb32{ 0x00000003u };
constexpr std::uint32_t kEdgeBitsPackedAligned32{ 0x00000003u };

// A 32-bit sparse mask reaches into the high bit to exercise word-width logic.
constexpr std::uint32_t kWideSparseMask32{ 0x80010012u };
constexpr std::uint32_t kWideSparseValue32{ 0x80010002u };
constexpr std::uint32_t kWideSparsePackedLsb32{ 0x0000000du };
constexpr std::uint32_t kWideSparsePackedAligned32{ 0x0000001au };

// A 16-bit contiguous mask behaves like a shifted nibble field.
constexpr std::uint16_t kWideContiguousMask16{ 0x0f00u };
constexpr std::uint16_t kWideContiguousValue16{ 0x0b00u };
constexpr std::uint16_t kWideContiguousPackedLsb16{ 0x000bu };
constexpr std::uint16_t kWideContiguousPackedAligned16{ 0x0b00u };

constexpr bool kConstexprLeastSetBitIndexWorks{ []() constexpr {
  // The sparse anchor mask starts at bit index 1.
  return Constexpr::least_set_bit_index(kSparseMask) == 1u;
}() };
static_assert(kConstexprLeastSetBitIndexWorks);

constexpr bool kConstexprCondenseWorks{ []() constexpr {
  // Condensing to the LSB removes the holes in the sparse mask entirely.
  bool const lsb_ok{
    Constexpr::condense(kSparseMask, kSparseValue, true) == kSparsePackedLsb
  };

  // Offset-preserving condense keeps the packed field anchored at bit 1.
  bool const aligned_ok{
    Constexpr::condense(kSparseMask, kSparseValue, false) == kSparsePackedAligned
  };

  return lsb_ok && aligned_ok;
}() };
static_assert(kConstexprCondenseWorks);

constexpr bool kConstexprExpandWorks{ []() constexpr {
  // LSB-packed bits should scatter back into the sparse-mask positions.
  bool const lsb_ok{
    Constexpr::expand(kSparseMask, kSparsePackedLsb, true) == kSparseValue
  };

  // Offset-preserving packed bits should land in the same logical positions.
  bool const aligned_ok{
    Constexpr::expand(kSparseMask, kSparsePackedAligned, false) == kSparseValue
  };

  return lsb_ok && aligned_ok;
}() };
static_assert(kConstexprExpandWorks);

constexpr bool kConstexprEnumSupportWorks{ []() constexpr {
  // Enum masks should participate in the same sparse-mask computations.
  bool const least_bit_ok{
    Constexpr::least_set_bit_index(SparseEnumBits::Mask) == 1u
  };

  bool const condense_ok{
    Constexpr::condense(SparseEnumBits::Mask, SparseEnumBits::Value, true)
      == kSparsePackedLsb
  };

  bool const expand_ok{
    Constexpr::expand(SparseEnumBits::Mask, kSparsePackedLsb, true)
      == SparseEnumBits::Value
  };

  return least_bit_ok && condense_ok && expand_ok;
}() };
static_assert(kConstexprEnumSupportWorks);

constexpr bool kConstexprIntegralReturnTypesMatchContract{ []() constexpr {
  // Integral masks should condense to the unsigned working type.
  bool const condense_type_ok{ std::is_same_v<
    decltype(Constexpr::condense(kSparseMask, kSparseValue, true)),
    Constexpr::impl::unsigned_equivalent_t<std::uint8_t>
  > };

  // Expanding should restore the original integral type.
  bool const expand_type_ok{ std::is_same_v<
    decltype(Constexpr::expand(kSparseMask, kSparsePackedLsb, true)),
    std::uint8_t
  > };

  return condense_type_ok && expand_type_ok;
}() };
static_assert(kConstexprIntegralReturnTypesMatchContract);

constexpr bool kConstexprEnumReturnTypesMatchContract{ []() constexpr {
  // Enum masks should still condense through their unsigned storage type.
  bool const condense_type_ok{ std::is_same_v<
    decltype(Constexpr::condense(SparseEnumBits::Mask, SparseEnumBits::Value, true)),
    Constexpr::impl::unsigned_equivalent_t<SparseEnumBits>
  > };

  // Expanding should recover the enum type itself, not the storage type.
  bool const expand_type_ok{ std::is_same_v<
    decltype(Constexpr::expand(SparseEnumBits::Mask, kSparsePackedLsb, true)),
    SparseEnumBits
  > };

  return condense_type_ok && expand_type_ok;
}() };
static_assert(kConstexprEnumReturnTypesMatchContract);

constexpr bool kConstexprEdgeBitScenariosWork{ []() constexpr {
  // Bottom-bit-only masks should make the aligned and LSB-packed forms coincide.
  bool const least_bit_ok{
    Constexpr::least_set_bit_index(kLeastBitMask32) == 0u
    && Constexpr::condense(kLeastBitMask32, kLeastBitValue32, true)
      == kLeastBitPackedLsb32
    && Constexpr::condense(kLeastBitMask32, kLeastBitValue32, false)
      == kLeastBitPackedAligned32
    && Constexpr::expand(kLeastBitMask32, kLeastBitPackedLsb32, true)
      == kLeastBitValue32
    && Constexpr::expand(kLeastBitMask32, kLeastBitPackedAligned32, false)
      == kLeastBitValue32
  };

  // A top-bit mask should still condense to bit 0 and expand back to bit 31.
  bool const most_bit_ok{
    Constexpr::least_set_bit_index(kMostBitMask32) == 31u
    && Constexpr::condense(kMostBitMask32, kMostBitValue32, true)
      == kMostBitPackedLsb32
    && Constexpr::condense(kMostBitMask32, kMostBitValue32, false)
      == kMostBitPackedAligned32
    && Constexpr::expand(kMostBitMask32, kMostBitPackedLsb32, true)
      == kMostBitValue32
    && Constexpr::expand(kMostBitMask32, kMostBitPackedAligned32, false)
      == kMostBitValue32
  };

  // A top-and-bottom mask should condense to a two-bit field spanning both edges.
  // Because the mask already starts at bit 0, both packed forms are the same.
  bool const edge_bits_ok{
    Constexpr::least_set_bit_index(kEdgeBitsMask32) == 0u
    && Constexpr::condense(kEdgeBitsMask32, kEdgeBitsValue32, true)
      == kEdgeBitsPackedLsb32
    && Constexpr::condense(kEdgeBitsMask32, kEdgeBitsValue32, false)
      == kEdgeBitsPackedAligned32
    && Constexpr::expand(kEdgeBitsMask32, kEdgeBitsPackedLsb32, true)
      == kEdgeBitsValue32
    && Constexpr::expand(kEdgeBitsMask32, kEdgeBitsPackedAligned32, false)
      == kEdgeBitsValue32
  };

  return least_bit_ok && most_bit_ok && edge_bits_ok;
}() };
static_assert(kConstexprEdgeBitScenariosWork);

constexpr bool kConstexprWiderIntegralWidthsWork{ []() constexpr {
  // A sparse 32-bit mask should still condense and expand through the high bit.
  bool const sparse_32_ok{
    Constexpr::least_set_bit_index(kWideSparseMask32) == 1u
    && Constexpr::condense(kWideSparseMask32, kWideSparseValue32, true)
      == kWideSparsePackedLsb32
    && Constexpr::condense(kWideSparseMask32, kWideSparseValue32, false)
      == kWideSparsePackedAligned32
    && Constexpr::expand(kWideSparseMask32, kWideSparsePackedLsb32, true)
      == kWideSparseValue32
    && Constexpr::expand(kWideSparseMask32, kWideSparsePackedAligned32, false)
      == kWideSparseValue32
  };

  // A 16-bit contiguous field should still behave like a shifted nibble.
  bool const contiguous_16_ok{
    Constexpr::least_set_bit_index(kWideContiguousMask16) == 8u
    && Constexpr::condense(kWideContiguousMask16, kWideContiguousValue16, true)
      == kWideContiguousPackedLsb16
    && Constexpr::condense(kWideContiguousMask16, kWideContiguousValue16, false)
      == kWideContiguousPackedAligned16
    && Constexpr::expand(kWideContiguousMask16, kWideContiguousPackedLsb16, true)
      == kWideContiguousValue16
    && Constexpr::expand(kWideContiguousMask16, kWideContiguousPackedAligned16, false)
      == kWideContiguousValue16
  };

  return sparse_32_ok && contiguous_16_ok;
}() };
static_assert(kConstexprWiderIntegralWidthsWork);

/**
 * @brief Convert an enum or integral value to the unsigned type used by
 *   \c masked_bits.hpp.
 *
 * @tparam T Enum or integral type to convert.
 * @param value Value to convert.
 * @return Constexpr::impl::unsigned_equivalent_t<T> Unsigned representation of
 *   \p value.
 */
template <typename T>
constexpr Constexpr::impl::unsigned_equivalent_t<T> as_unsigned(T value)
{
  using U = Constexpr::impl::unsigned_equivalent_t<T>;
  return static_cast<U>(value);
}

/**
 * @brief Verify that condensing and then expanding with matching alignment
 *   semantics reproduces the masked portion of a value.
 *
 * @tparam T Enum or integral type under test.
 * @param mask Sparse or contiguous mask to apply.
 * @param value Value to round-trip through \c condense and \c expand.
 */
template <typename T>
void expect_expand_after_condense_round_trip(T mask, T value)
{
  using U = Constexpr::impl::unsigned_equivalent_t<T>;

  // The round-trip should never recreate bits outside the original mask.
  U const expected{ static_cast<U>(as_unsigned(value) & as_unsigned(mask)) };

  // First verify the path that packs the field down to bit 0.
  U const expanded_from_lsb{
    as_unsigned(Constexpr::expand(mask, Constexpr::condense(mask, value, true), true))
  };

  // Then verify the path that preserves the field's original offset.
  U const expanded_from_aligned{
    as_unsigned(Constexpr::expand(mask, Constexpr::condense(mask, value, false), false))
  };

  EXPECT_EQ(expected, expanded_from_lsb);
  EXPECT_EQ(expected, expanded_from_aligned);
}

/**
 * @brief Verify that expanding a packed field and then condensing it again
 *   reproduces the original packed bits.
 *
 * @tparam T Enum or integral mask type under test.
 * @param mask Sparse or contiguous mask to apply.
 * @param packed Packed value to expand.
 * @param read_from_lsb Whether \p packed is read from the least significant
 *   bits of the packed field.
 */
template <typename T>
void expect_condense_after_expand_round_trip(
  T mask,
  Constexpr::impl::unsigned_equivalent_t<T> packed,
  bool read_from_lsb
)
{
  // Expand the packed field into the positions selected by the mask first.
  T const expanded{ Constexpr::expand(mask, packed, read_from_lsb) };

  // Re-condensing should recover the original packed representation exactly.
  auto const recondensed{ Constexpr::condense(mask, expanded, read_from_lsb) };
  EXPECT_EQ(packed, recondensed);
}

// Confirm the core API stays usable in constexpr contexts, including enum inputs.
TEST(MaskedBitsConstexpr, SupportsCompileTimeEvaluation)
{
  // Confirm the basic sparse-mask behavior and enum participation first.
  EXPECT_TRUE(kConstexprLeastSetBitIndexWorks);
  EXPECT_TRUE(kConstexprCondenseWorks);
  EXPECT_TRUE(kConstexprExpandWorks);
  EXPECT_TRUE(kConstexprEnumSupportWorks);

  // Then confirm the API contracts for return types and edge-position behavior.
  EXPECT_TRUE(kConstexprIntegralReturnTypesMatchContract);
  EXPECT_TRUE(kConstexprEnumReturnTypesMatchContract);
  EXPECT_TRUE(kConstexprEdgeBitScenariosWork);
  EXPECT_TRUE(kConstexprWiderIntegralWidthsWork);
}

// Sparse masks should pack selected bits tightly down to bit 0 when requested.
TEST(MaskedBitsRuntime, CondensesSparseMaskToLsb)
{
  // Packing to the LSB removes the gaps between mask bits 1, 2, and 4.
  EXPECT_EQ(kSparsePackedLsb, Constexpr::condense(kSparseMask, kSparseValue, true));
}

// Sparse masks should also preserve the original least-significant-bit offset.
TEST(MaskedBitsRuntime, CondensesSparseMaskAtOriginalOffset)
{
  // Offset-preserving condense keeps the packed field anchored at bit 1.
  EXPECT_EQ(kSparsePackedAligned, Constexpr::condense(kSparseMask, kSparseValue, false));
}

// Packed bits at bit 0 should scatter back into the sparse mask positions.
TEST(MaskedBitsRuntime, ExpandsSparseMaskFromLsb)
{
  // The packed field `0b101` should scatter back into mask bits 1, 2, and 4.
  EXPECT_EQ(kSparseValue, Constexpr::expand(kSparseMask, kSparsePackedLsb, true));
}

// Offset-preserving packed bits should also scatter back to the masked layout.
TEST(MaskedBitsRuntime, ExpandsSparseMaskFromOriginalOffset)
{
  // The offset-preserving packed field should expand back to the same sparse layout.
  EXPECT_EQ(kSparseValue, Constexpr::expand(kSparseMask, kSparsePackedAligned, false));
}

// Contiguous masks are the simple case: condense/expand should behave like shifts.
TEST(MaskedBitsRuntime, HandlesContiguousMasksAsBitShifts)
{
  // With no interior holes, condense behaves like removing a shared bit offset.
  EXPECT_EQ(kContiguousPackedLsb, Constexpr::condense(kContiguousMask, kContiguousValue, true));
  EXPECT_EQ(
    kContiguousPackedAligned,
    Constexpr::condense(kContiguousMask, kContiguousValue, false)
  );

  // Expanding those packed fields should put the same contiguous bits back.
  EXPECT_EQ(kContiguousValue, Constexpr::expand(kContiguousMask, kContiguousPackedLsb, true));
  EXPECT_EQ(
    kContiguousValue,
    Constexpr::expand(kContiguousMask, kContiguousPackedAligned, false)
  );
}

// Bottom-bit-only masks should take the no-offset path with a one-bit field.
TEST(MaskedBitsRuntime, HandlesLeastSignificantBitOnlyScenarios)
{
  EXPECT_EQ(0u, Constexpr::least_set_bit_index(kLeastBitMask32));

  // Both condense modes collapse to the same one-bit field when the mask starts at bit 0.
  EXPECT_EQ(
    kLeastBitPackedLsb32,
    Constexpr::condense(kLeastBitMask32, kLeastBitValue32, true)
  );
  EXPECT_EQ(
    kLeastBitPackedAligned32,
    Constexpr::condense(kLeastBitMask32, kLeastBitValue32, false)
  );

  // Both expand modes recover the same original value for this one-bit field.
  EXPECT_EQ(
    kLeastBitValue32,
    Constexpr::expand(kLeastBitMask32, kLeastBitPackedLsb32, true)
  );
  EXPECT_EQ(
    kLeastBitValue32,
    Constexpr::expand(kLeastBitMask32, kLeastBitPackedAligned32, false)
  );
}

// Top-bit masks should still condense to bit 0 and expand back to bit 31.
TEST(MaskedBitsRuntime, HandlesMostSignificantBitScenarios)
{
  EXPECT_EQ(31u, Constexpr::least_set_bit_index(kMostBitMask32));

  // Condensing to the LSB produces a one-bit packed field, while aligned condense preserves bit 31.
  EXPECT_EQ(
    kMostBitPackedLsb32,
    Constexpr::condense(kMostBitMask32, kMostBitValue32, true)
  );
  EXPECT_EQ(
    kMostBitPackedAligned32,
    Constexpr::condense(kMostBitMask32, kMostBitValue32, false)
  );

  // Expanding from either packed form should recover the original top-bit value.
  EXPECT_EQ(
    kMostBitValue32,
    Constexpr::expand(kMostBitMask32, kMostBitPackedLsb32, true)
  );
  EXPECT_EQ(
    kMostBitValue32,
    Constexpr::expand(kMostBitMask32, kMostBitPackedAligned32, false)
  );
}

// Top-and-bottom masks should condense to two adjacent bits and expand back out.
TEST(MaskedBitsRuntime, HandlesTopAndBottomBitScenarios)
{
  EXPECT_EQ(0u, Constexpr::least_set_bit_index(kEdgeBitsMask32));

  // The two extreme bits condense into adjacent packed bits because the field starts at bit 0.
  EXPECT_EQ(
    kEdgeBitsPackedLsb32,
    Constexpr::condense(kEdgeBitsMask32, kEdgeBitsValue32, true)
  );
  EXPECT_EQ(
    kEdgeBitsPackedAligned32,
    Constexpr::condense(kEdgeBitsMask32, kEdgeBitsValue32, false)
  );

  // Expanding the two-bit packed field should restore both edges of the word.
  EXPECT_EQ(
    kEdgeBitsValue32,
    Constexpr::expand(kEdgeBitsMask32, kEdgeBitsPackedLsb32, true)
  );
  EXPECT_EQ(
    kEdgeBitsValue32,
    Constexpr::expand(kEdgeBitsMask32, kEdgeBitsPackedAligned32, false)
  );
}

// Wider sparse masks should still preserve far-apart bit positions correctly.
TEST(MaskedBitsRuntime, HandlesWideSparseMasks)
{
  // The LSB-packed form should collapse far-apart selected bits into `0b1101`.
  EXPECT_EQ(
    kWideSparsePackedLsb32,
    Constexpr::condense(kWideSparseMask32, kWideSparseValue32, true)
  );

  // The aligned form keeps that packed field rooted at the original least set bit.
  EXPECT_EQ(
    kWideSparsePackedAligned32,
    Constexpr::condense(kWideSparseMask32, kWideSparseValue32, false)
  );

  // Expanding either packed representation should restore the same sparse value.
  EXPECT_EQ(
    kWideSparseValue32,
    Constexpr::expand(kWideSparseMask32, kWideSparsePackedLsb32, true)
  );
  EXPECT_EQ(
    kWideSparseValue32,
    Constexpr::expand(kWideSparseMask32, kWideSparsePackedAligned32, false)
  );
}

// Wider contiguous masks should still behave like shifted fields rather than bytes.
TEST(MaskedBitsRuntime, HandlesWideContiguousMasks)
{
  // A nibble stored at bits 8-11 should condense to the low nibble `0x000b`.
  EXPECT_EQ(
    kWideContiguousPackedLsb16,
    Constexpr::condense(kWideContiguousMask16, kWideContiguousValue16, true)
  );

  // The aligned form should keep that same nibble at bits 8-11.
  EXPECT_EQ(
    kWideContiguousPackedAligned16,
    Constexpr::condense(kWideContiguousMask16, kWideContiguousValue16, false)
  );

  // Expanding either form should restore the original shifted nibble.
  EXPECT_EQ(
    kWideContiguousValue16,
    Constexpr::expand(kWideContiguousMask16, kWideContiguousPackedLsb16, true)
  );
  EXPECT_EQ(
    kWideContiguousValue16,
    Constexpr::expand(kWideContiguousMask16, kWideContiguousPackedAligned16, false)
  );
}

// Small underlying types and scoped enums should instantiate and round-trip cleanly.
TEST(MaskedBitsRuntime, SupportsSmallWidthEnums)
{
  // Enum masks should still report the correct least-significant set bit.
  EXPECT_EQ(1u, Constexpr::least_set_bit_index(SparseEnumBits::Mask));

  // Condense should accept enum values and produce the same packed field as integers.
  EXPECT_EQ(
    kSparsePackedLsb,
    Constexpr::condense(SparseEnumBits::Mask, SparseEnumBits::Value, true)
  );

  // Expand should return the enum type and preserve the original enumerator value.
  SparseEnumBits const expanded{ Constexpr::expand(SparseEnumBits::Mask, kSparsePackedLsb, true) };
  EXPECT_EQ(as_unsigned(SparseEnumBits::Value), as_unsigned(expanded));
}

// A zero mask is nonsensical for all three APIs and should fail consistently.
TEST(MaskedBitsFailure, RejectsZeroMasks)
{
  // There is no least set bit in the zero mask.
  EXPECT_THROW((void)Constexpr::least_set_bit_index(std::uint8_t{ 0u }), std::invalid_argument);

  // Condense cannot decide which bits to keep when the mask is zero.
  EXPECT_THROW(
    (void)Constexpr::condense(std::uint8_t{ 0u }, std::uint8_t{ 1u }, true),
    std::invalid_argument
  );

  // Expand cannot decide where to scatter bits when the mask is zero.
  EXPECT_THROW(
    (void)Constexpr::expand(std::uint8_t{ 0u }, std::uint8_t{ 1u }, true),
    std::invalid_argument
  );
}

// Condense followed by expand should recover exactly the masked bits of each value.
TEST(MaskedBitsRoundTrip, ExpandAfterCondenseMatchesMaskedValue)
{
  // Exercise the holey-mask path with a spread of partially and fully populated values.
  std::array<std::uint8_t, 6u> const sparse_values{
    std::uint8_t{ 0b00000u },
    std::uint8_t{ 0b00010u },
    std::uint8_t{ 0b00100u },
    std::uint8_t{ 0b10010u },
    std::uint8_t{ 0b10110u },
    std::uint8_t{ 0b11111u },
  };

  for (std::uint8_t value : sparse_values) {
    expect_expand_after_condense_round_trip(kSparseMask, value);
  }

  // Also verify the dense-mask case where the same operations reduce to shifts.
  std::array<std::uint8_t, 4u> const contiguous_values{
    std::uint8_t{ 0b00000u },
    std::uint8_t{ 0b00010u },
    std::uint8_t{ 0b01010u },
    std::uint8_t{ 0b01110u },
  };

  for (std::uint8_t value : contiguous_values) {
    expect_expand_after_condense_round_trip(kContiguousMask, value);
  }

  // Single bottom-bit masks should keep only bit 0 through the round-trip.
  std::array<std::uint32_t, 3u> const least_bit_values{
    std::uint32_t{ 0x00000000u },
    std::uint32_t{ 0x00000001u },
    std::uint32_t{ 0xffffffffu },
  };

  for (std::uint32_t value : least_bit_values) {
    expect_expand_after_condense_round_trip(kLeastBitMask32, value);
  }

  // Single top-bit masks should keep only bit 31 through the round-trip.
  std::array<std::uint32_t, 3u> const most_bit_values{
    std::uint32_t{ 0x00000000u },
    std::uint32_t{ 0x80000000u },
    std::uint32_t{ 0xffffffffu },
  };

  for (std::uint32_t value : most_bit_values) {
    expect_expand_after_condense_round_trip(kMostBitMask32, value);
  }

  // Edge-spanning masks should preserve both extreme bits together.
  std::array<std::uint32_t, 3u> const edge_bit_values{
    std::uint32_t{ 0x00000000u },
    std::uint32_t{ 0x80000001u },
    std::uint32_t{ 0xffffffffu },
  };

  for (std::uint32_t value : edge_bit_values) {
    expect_expand_after_condense_round_trip(kEdgeBitsMask32, value);
  }

  // Higher-width sparse masks should ignore unmasked noise and preserve high bits.
  std::array<std::uint32_t, 3u> const wide_sparse_values{
    std::uint32_t{ 0x00000000u },
    std::uint32_t{ 0x80010002u },
    std::uint32_t{ 0xf1f3b5d7u },
  };

  for (std::uint32_t value : wide_sparse_values) {
    expect_expand_after_condense_round_trip(kWideSparseMask32, value);
  }

  // Wider contiguous fields should also round-trip through shifted positions.
  std::array<std::uint16_t, 3u> const wide_contiguous_values{
    std::uint16_t{ 0x0000u },
    std::uint16_t{ 0x0b00u },
    std::uint16_t{ 0xabcdu },
  };

  for (std::uint16_t value : wide_contiguous_values) {
    expect_expand_after_condense_round_trip(kWideContiguousMask16, value);
  }
}

// Expand followed by condense should preserve the meaningful packed field exactly.
TEST(MaskedBitsRoundTrip, CondenseAfterExpandReturnsPackedField)
{
  // Baseline sparse and contiguous cases should preserve both packed conventions.
  expect_condense_after_expand_round_trip(kSparseMask, kSparsePackedLsb, true);
  expect_condense_after_expand_round_trip(kSparseMask, kSparsePackedAligned, false);

  expect_condense_after_expand_round_trip(kContiguousMask, kContiguousPackedLsb, true);
  expect_condense_after_expand_round_trip(kContiguousMask, kContiguousPackedAligned, false);

  // The single-bit edge masks exercise the two extremes of the word independently.
  expect_condense_after_expand_round_trip(kLeastBitMask32, kLeastBitPackedLsb32, true);
  expect_condense_after_expand_round_trip(kLeastBitMask32, kLeastBitPackedAligned32, false);

  expect_condense_after_expand_round_trip(kMostBitMask32, kMostBitPackedLsb32, true);
  expect_condense_after_expand_round_trip(kMostBitMask32, kMostBitPackedAligned32, false);

  // The combined edge mask proves that both extremes survive the packed round-trip together.
  expect_condense_after_expand_round_trip(kEdgeBitsMask32, kEdgeBitsPackedLsb32, true);
  expect_condense_after_expand_round_trip(kEdgeBitsMask32, kEdgeBitsPackedAligned32, false);

  // Wider representative masks keep the generic helpers honest across word widths.
  expect_condense_after_expand_round_trip(kWideSparseMask32, kWideSparsePackedLsb32, true);
  expect_condense_after_expand_round_trip(kWideSparseMask32, kWideSparsePackedAligned32, false);

  expect_condense_after_expand_round_trip(kWideContiguousMask16, kWideContiguousPackedLsb16, true);
  expect_condense_after_expand_round_trip(kWideContiguousMask16, kWideContiguousPackedAligned16, false);
}

} // namespace
