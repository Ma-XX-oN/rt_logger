/**
 * @file enum_spec.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Type specifier for enums to allow for safer enum usage and clearer
 *   display.
 * @version 0.1
 * @date 2026-05-28
 *
 * @copyright Copyright (c) 2026
 *
 * TODO: Need to flesh this out more.
 */

#include <cstdint>
#include <type_traits>
#include <utility>

enum eDefineEnum : std::uint8_t {
  // MASKS
  //
  // Partitions the bit space
  OpcodeMask      = 0b1110'0000, // Mask for extracting opcode.

  PayloadMask     = 0b0001'1111, // Op-code specific payload.
  
  // INTERPRETATION OF OPCODE
  
  Invalid         = 0b0000'0000, // Catches zero/uninitialized/corrupt command bytes.
  
  Named           = 0b0010'0000, // extracted = ((active_bit_mask ? active_bit_mask : inline_bit_mask) & value);
                                 // pair_count = (PayloadMask & cmd_byte) + 1;
                                 // - Followed by pair_count pairs.
                                 // - To specify more than 32 names, just create another Named command on the same
                                 //   level.
                                 //
                                 // for (int i = 0; i < pair_count; ++i) {
                                 //   if (extracted == std::get<0>(pair)) {
                                 //     auto const& Name = std::get<1>(pair);
                                 //     if (extracted) {
                                 //       // add to non-zero value queue.  Allows display grouping as "A | Name".
                                 //     } else {
                                 //       // add to zero value queue.  Allows display grouping as "A, Name"
                                 //     }
                                 //     break;
                                 //   }
                                 // }
                                 
  Numeric         = 0b0100'0000, // extracted = (inline_bit_mask & value);
                                 // format = PayloadMask & cmd_byte;
                                 //   - Followed by a name to name the field.
                                 //   - Does NOT use active_bit_mask as that would add additional overhead as mask would
                                 //     be used only once.
                                 //   - If there is a 0 in the inline_bit_mask's least significant bit, keep shifting
                                 //     value and mask right till it's a 1.
                                 //   - Bits are packed densely into the low bits in least-significant-bit order.
                                 //   - After packing, the most significant extracted bit becomes the sign bit and is
                                 //     sign extended.
                                 //   - format can contain up to 5 independent formatting rules.
                                 //
                                 //   formatted_value = format_value(extracted, format);
                                 //   // add (name, value) pair to numeric queue.
                                 //   // Allows display grouping as "A, Name = Value"

  // CONDITIONALS
  //
  // Can specify that a bit represents how other bits are interpreted.  If a particular bit sequence requires a
  // different representation, nest the groups.  This is slightly clunky, but optimizes the stream size for the more
  // common cases.

  GroupIf         = 0b0110'0000, // select_group = (inline_bit_mask & value);
                                 // cmd_count = (PayloadMask & cmd_byte) + 1;
                                 // if ( select_group) {
                                 //   // Execute the following cmd_count commands.
                                 // } else {
                                 //   // Advance past cmd_count commands without executing them.
                                 // }
                                 
  GroupIfNot      = 0b1000'0000, // select_group = (inline_bit_mask & value);
                                 // cmd_count = (PayloadMask & cmd_byte) + 1;
                                 // if (!select_group) {
                                 //   // Execute the following cmd_count commands.
                                 // } else {
                                 //   // Advance past cmd_count commands without executing them.
                                 // }
  
  // COMBINED CONDITIONALS
  //
  // Though not explicitly required, these micro-optimize the resulting stream length.
  
  GroupIfNamed    = 0b1010'0000, // extracted = ((active_bit_mask ? active_bit_mask : inline_bit_mask) & value);
                                 // select_group = (inline_bit_mask & value);
                                 // pair_count = (PayloadMask & cmd_byte) + 1;
                                 // if ( select_group) {
                                 //   // Compare extracted against pair_count pairs as specified for Named.
                                 // } else {
                                 //   // Advance past pair_count pairs without checking them.
                                 // }

  GroupIfNotNamed = 0b1100'0000, // extracted = ((active_bit_mask ? active_bit_mask : inline_bit_mask) & value);
                                 // select_group = (inline_bit_mask & value);
                                 // pair_count = (PayloadMask & cmd_byte) + 1;
                                 // if (!select_group) {
                                 //   // Compare extracted against pair_count pairs as specified for Named.
                                 // } else {
                                 //   // Advance past pair_count pairs without checking them.
                                 // }

  SetMask         = 0b1110'0000, // Sets active_bit_mask.
                                 // - Followed by active_bit_mask value.
                                 //
                                 // active_bit_mask is initially 0.
                                 //
                                 // When entering a GroupIf/GroupIfNot body, active_bit_mask is pushed onto a stack and
                                 // restored when leaving the scope.
                                 //
                                 // This changes the number of the opcode's parameters that are expected:
                                 //
                                 //  | opcode \ abm == 0 | true  | false |
                                 //  |-------------------|-------|-------|
                                 //  | Named             |   1   |   0   |
                                 //  | Numeric           |   1   |   1   |
                                 //  | GroupIf           |   1   |   1   |
                                 //  | GroupIfNot        |   1   |   1   |
                                 //  | GroupIfNamed      |   1   |   0   |
                                 //  | GroupIfNotNamed   |   1   |   0   |
                                 //
                                 // Numeric, GroupIf and GroupIfNot always have an independent parameter.
                                 //
                                 // SetMask only takes parameters if PayloadMask is not 0.  This
                                 // means 31 very common bit masks can be specified inline in the
                                 // command byte directly.
};

#include "CStr.hpp"


template <typename E, std::size_t NumberOfPairs, E Bitmask, 
  std::pair<E, Constexpr::CStr>>
struct Named {
};


template <typename E, std::underlying_type_t<E>...Program>
struct ByteCode {
  using underlying_type = std::underlying_type_t<E>;

  using program = std::integer_sequence<underlying_type, Program...>;
};

// Enum as defined by stream
template<typename DecodeInstructions = void>
struct EnumDefinition {
  /**
   * @brief Construct a new Enum Definition object by parsing the program and
   *   generate a section cache to allow fast execution.
   *
   * @param begin_it 
   * @param end_it 
   */
  EnumDefinition(std::byte const* begin_it, std::byte const* end_it)
  {
  }
  
};

// Enum as defined type type
template<typename Enum, std::underlying_type_t<Enum>...Program>
struct EnumDefinition<ByteCode<Enum, Program...>> {

  /**
   * @brief Construct a new Enum Definition object, where the program is in the
   *   ByteCode.
   *
   */
  EnumDefinition() {}
};

