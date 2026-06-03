/**
 * @file logger.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Logger that minimises CPU usage and data stream size.
 * @version 0.1
 * @date 2026-05-28
 *
 * @copyright Copyright (c) 2026
 *
 * This logger stores format strings as IDs and when printing an event, states
 * ID for format and the parameters as binary data, thus reducing the size of
 * the data stream and the amount of CPU to encode it.
 *
 * TODO: Flesh out specification.
 */

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <type_traits>
#include <limits>
#include <stdexcept>
#include <tuple>
#include "machine_num.hpp"
#include "int_codec.hpp"
#include "bitwise_enum.hpp"

////////////////////////////////////////////////////////////////////////////////

enum BlockType_e : uint8_t {
  // bits 0 and 1 = block type
  FmtStrings,        // contains 1 or more id/string mappings payload (string is NULL terminated)
  Event,             // contains id and data payload
  EnumStrings,       // contains (length, id, string) triples (string is NULL terminated)
  DataContinuation,  // this is a continuation block that contains more data payloads
  // bit 2 = block data continues (1) or not (0)
  DataContinues = 1 << 2, // bit flag that states that this data block is not complete and to expect a DataContinuation block
  // bit 3 = size represented by 1 byte (0) or 2 bytes (1)
  SizeTwoBytes = 1 << 3
};

inline BlockType_e operator|(BlockType_e lhs, BlockType_e rhs) {
  return static_cast<BlockType_e>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

/**
 * A block to write to the out data stream.
 *
 * bytes[4]                 - fence ("####")
 * uint16_t                 - CRC
 *                            - Starts from I_BLOCK_TYPE to end of block.
 *                            - If block is Event, then CRC is seeded from the
 *                              format string, ensuring that the event data is
 *                              tied to the format string.
 *                            - If block is a data continuation block, then
 *                              this CRC is seeded by the previous blocks.
 * BlockType_e              - block_type
 * uint8_t                  - sequence_number
 * uint8_t                  - payload_length - 1
 * bytes[2^8 + HEADER_SIZE] - block (HEADER_SIZE = 9)
 */
class Block {
  public:

  // # # # # C C B S L P P P P P P P P P P P P ...
  // # # # # R R L E E A A A A A A A A A A A A ...
  // # # # # C C K Q N Y Y Y Y Y Y Y Y Y Y Y Y ...

  // Because the length has to point at the next fence, it's better to put
  // the fence at the end every time and only put the first fence down once.
  // static constexpr int I_FENCE       = 0;
  static constexpr int I_CRC        { 0 }; // 16 bit CRC
  static constexpr int I_BLOCK_TYPE { 2 }; // Type of block
  static constexpr int I_SEQ_NUM    { 3 }; // Sequence number
  static constexpr int I_PAYLOAD_LEN{ 4 }; // Payload contains this number of bytes
  static constexpr int I_PAYLOAD    { 5 }; // Payload starts here
  static constexpr int HEADER_SIZE  { 9 }; // Header is technically still 9 bytes
                                          // it's just that it's using the fence
                                          // from the previous block or the
                                          // initial fence.
  static constexpr int MAX_PAYLOAD{ 1 << 8 };

  std::byte bytes[MAX_PAYLOAD + HEADER_SIZE];

  Block()
  {
    // memcpy(bytes, "####", 4);
  }

  /**
   * Set block type
   */
  void block_type(BlockType_e blockType) {
    encode_value(bytes + I_BLOCK_TYPE, std::end(bytes), blockType);
  }

  /**
   * Get block type
   */
  BlockType_e block_type() const {
    std::byte const* src{ bytes + I_BLOCK_TYPE };
    return decode_value<Throw, BlockType_e>(src, std::end(bytes));
  }

  /**
   * Set sequence number
   */
  void seq_num(uint8_t seqNum) {
    encode_value(bytes + I_SEQ_NUM, std::end(bytes), seqNum);
  }

  /**
   * Get sequence number
   */
  uint8_t seq_num() const {
    std::byte const* src{ bytes + I_SEQ_NUM };
    return decode_value<Throw, uint8_t>(src, std::end(bytes));
  }

  /**
   * Set payload length
   */
  void payload_len(int payloadLen) {
    assert(0 < payloadLen && payloadLen < MAX_PAYLOAD);
    encode_value(bytes + I_PAYLOAD_LEN, std::end(bytes), uint8_t(payloadLen-1));
  }

  /**
   * Get payload length
   */
  unsigned int payload_len() const {
    std::byte const* src{ bytes + I_PAYLOAD_LEN };
    return decode_value<Throw, uint8_t>(src, std::end(bytes)) + 1;
  }

  /**
   * Set CRC
   */
  void crc(uint16_t crcValue) {
    encode_value(bytes + I_CRC, std::end(bytes), crcValue);
  }

  /**
   * Get CRC
   */
  uint16_t crc() const {
    std::byte const* src{ bytes + I_CRC };
    return decode_value<Throw, uint16_t>(src, std::end(bytes));
  }

  /**
   * Check that there is space available for the value item
   *
   * @param offset - current offset to write to.
   * @param value - next item to be stored.  Only for type deduction.
   */
  template <typename T
    , std::enable_if_t<
        std::is_array_v<T> 
        && (std::is_arithmetic_v<std::remove_all_extents_t<T>>
            || std::is_enum_v<std::remove_all_extents_t<T>>)
        || std::is_arithmetic_v<T> || std::is_enum_v<T>, bool> = true>
  bool space_available(int offset, T& value) {
    return sizeof(T) <= MAX_PAYLOAD - offset;
  }
  
  /**
   * Set payload item. Asserts if space not available.
   *
   * @returns next offset
   */
  template<typename T>
  int payload_set(int offset, T const& value) {
    encode_value(bytes + offset, std::end(bytes), value);
    return HEADER_SIZE + offset + sizeof(T);
  }

  /**
   * Get payload item. Asserts if space not available.
   *
   * @returns next offset
   */
  template<typename T>
  int payload_get(int offset, T& value) {
    buff_get(bytes, offset, value);
    return HEADER_SIZE + offset + sizeof(T);
  }
};

template <std::size_t N>
class BlockManager {
  public:
  
  uint8_t next_sequence_number{ 0 };
  Block buffers[N];
  Block* pHead;
  int in_use{ 0 };
  
  BlockManager()
  : pHead(buffers)
  , in_use(0)
  {}
  
  bool is_empty() const {
    return in_use == 0;
  }
  
  bool is_full() const {
    return in_use == N;
  }
  
  Block& next() {
    assert(!is_full());
    ++in_use;
    Block* result{ pHead };
    if (++pHead >= buffers + N) {
      pHead = buffers;
    }
    result->seq_num(next_sequence_number++);
    return *result;
  }
  
  void done() {
    assert(!is_empty());
    --in_use;
  }
};

template <char...Cs>
using char_sequence = std::integer_sequence<char, Cs...>;

template <typename T>
struct is_char_seq : std::false_type {};

template <char...Cs>
struct is_char_seq<std::integer_sequence<char, Cs...>> : std::true_type {};

template <typename T>
constexpr bool is_char_seq_v{ is_char_seq<T>::value };

/**
 * @brief These are control codes stored in a fstring (format string) to specify
 *   type and formatting.
 *
 * Format strings are NUL terminated and have control codes embedded in them to
 * represent the stored type and how it should be output.  Type specification is
 * key as the data isn't translated to text at the time of writing, but at the
 * time of reading.  All values are stored in the machine's representation,
 * meaning that the endianness of the values are kept as is and has to be fixed
 * to the reader's end.  This is done to reduce the writer's latency as much as
 * possible.
 *
 * Any value in a string that is between [1, 31] or [0x01, 0x1F] inclusive are
 * considered control codes. Subtract 1 from that value results in an inclusive
 * range of [0, 30] or [0x00, 0x1E], which are the values represented in this
 * enum.
 *
 * Backslash, tab and LF are encoded in the string as R"(\\)", R"(\n)" and
 * R"(\t)" respectively.  I.e.  Those characters are stored as escaped
 * characters and are represented by 2 characters.
 *
 * These specifications prevents embedding a NUL into a string and make it
 * easier to use flags on the integer set for specific operations.
 *
 * There are 3 different control sequences.
 *
 * 1. TYPE_SPEC
 *
 * This specifies the binary representation of the type being printed.  This is
 * because the values are not translated to text, but are stored as the machine
 * loggers binary format to be converted later to reduce logging latency.
 *
 * 1a. TYPE_SPEC: Numbers and strings
 *
 * Numbers and string types are specified as one byte.
 *
 * 1b. TYPE_SPEC: \c Enum
 *
 * Enums are followed by a \c dint (dynamic integer) to specify which registered
 * enum to use.  A \c dint is a 7 bit number with a continuation bit to allow a
 * number to be as small as 1 byte for small numbers but can be theoretically
 * represent any sized number.
 *
 * 1c. TYPE_SPEC: \c Array
 *
 * All of those described previously can be prefixed with an \c Array MODIFIER.
 * That consists of \c Array, followed by a \c dint and can be stacked 3 levels
 * deep.  That limitation only exists is only because I'm not sure how to
 * represent 4D arrays on output.
 *
 * 1c. FORMATTING INFO
 *
 * The formatting info consists of 1-6 bytes (2-12 if using arrays), depending
 * on what is requested. This excludes the starting \c eType byte.  A sequence
 * always ends with a \c eFmtLetter byte.  Sizes stated are minimal values.  If
 * embedded dints specified are greater than 127, then it could mean a larger
 * size.  Embedded dints of 0 are considered an error.  If done as text, it
 * would easily be twice the size. 
 *
 *              11111             111111111122222
 *     12345678901234    123456789012345678901234 // Arrays are deduced, not
 *     {:0^+z#10.10b}    {:0^+z#10.10!a10a10a10d} // specified as shown here.
 *
 * When arrays are displayed, they are space delimited when minimum width
 * formatting is specified, otherwise is it comma delimited.  This is actually
 * dependent on the renderer side, so it could be configurable there.
 *
 * ```text
 * | byte 1    | byte 2     | byte 3   | byte 4     | byte 5     | byte 6     | byte 7     |
 * |-----------|------------|----------|------------|------------|------------|------------|
 * | eType     | eFmtLetter |          |            |            |            |            |
 * | eType     | eFmt0      | eFmt1    | eFmtLetter |            |            |            |
 * | eType     | eFmt0      | eFmt1    | MIN_DINT   | eFmtLetter |            |            |
 * | eType     | eFmt0      | eFmt1    | PREC_DINT  | eFmtLetter |            |            |
 * | eType     | eFmt0      | eFmt1    | MIN_DINT   | PREC_DINT  | eFmtLetter |            |
 * | eType     | eFmt0      | eFmt1    | FILL_CHAR  | eFmtLetter |            |            |
 * | eType     | eFmt0      | eFmt1    | MIN_DINT   | FILL_CHAR  | eFmtLetter |            |
 * | eType     | eFmt0      | eFmt1    | PREC_DINT  | FILL_CHAR  | eFmtLetter |            |
 * | eType     | eFmt0      | eFmt1    | MIN_DINT   | PREC_DINT  | FILL_CHAR  | eFmtLetter |
 * | Enum      | enum id    | ...      |            |            |            |            |
 * | Array     | DINT_SIZE  | eType ...|            |            |            |            |
 * | Array     | DINT_SIZE  | Array    | DINT_SIZE  | eType ...  |            |            |
 * | Array     | DINT_SIZE  | Array    | DINT_SIZE  | Array      | DINT_SIZE  | eType ...  |
 * ```
 *
 * 2. End of an fstring
 *
 * All fstrings are NUL terminated.
 *
 * 2a. SALT
 *
 * All fstrings have a CRC16 with them to distinguish them from each other.  If
 * an fstring has the same CRC16 as another, it is considered an error and can
 * be forced to be made different by adding a salt to the end of the string,
 * which is the \c Salt value followed by nothing or a dint THAT IS NOT ZERO.
 *
 * This adds one or more bytes to the string. A DINT_SALT value of 0 is an
 * error, but it's equivalent is to have no DINT_SALT at all.  A dint is used to
 * ensure that the salt is always part of the fstring and doesn't prematurely
 * terminate it with an inadvertent NUL. The salt shouldn't be printed on the
 * displaying end.
 *
 * ```text
 * | byte 1    | byte 2        | byte 3   |
 * |-----------|---------------|----------|
 * | NUL       |               |          |
 * | Salt      | NUL           |          |
 * | Salt      | DINT_SALT     | NUL      |
 * ```
 */
enum eType : uint8_t {
  // TYPE SPEC:

  // Integer types (Reserved is probably for future 128 bit)
  UInt16 = 0x00, UInt32 = 0x01, UInt64 = 0x02, Reserved0 = 0x03,
   Int16 = 0x04,  Int32 = 0x05,  Int64 = 0x06, Reserved1 = 0x07,

  Dint   = 0x08, // bit flag to state if Int*/UInt* are compressed as dints.
  NonInt = 0x10, // bit flag to state if not an Int*/UInt* with size > 1.
  
  // C-String
  String = 0x11,

  // Not integer types
  Char  = 0x12, Bool   = 0x13,
  Float = 0x14, Double = 0x15,

   Int8 = 0x16,
  UInt8 = 0x17,

  // 0x18, 0x19, 0x1A, 0x1B - Unused
  // Maybe long double?
  
  // TYPE MODIFIES:

  // Enum has dint id following it
  Enum  = 0x1C,
  
  // Array has a dint value and a eType after it to state the number of elements
  // and the type of the array.
  //
  //   E.g. Array 30 Int16        = an array of 30 int16_t elements.
  //   E.g. Array 16 Array 4 Int8 = a 16 x 4 array of int8_t elements.
  Array = 0x1D,

  // SALT:

  // Next bytes are for CRC calc only.
  // - Used only at the end of the format string.  Everything from here to the
  //   NUL is not printed but is used in the calculation of the CRC.
  Salt = 0x1E,
};

template <>
struct BitwiseOps<eType> : std::true_type {
};

/**
 * @brief These are optionally placed after eType.
 * 
 * Recognised by it's value being less than 32.
 *
 * If specified, NegativeMask bits will never be 0 and is always followed by
 * eFmt1.
 */
enum eFmt0 : uint8_t {
  // 000000ba make  -  mandatory (01) Not starting at 0 so this doesn't terminate string.
  // 000000ba make +/- mandatory (10)
  // 000000ba make  /- mandatory (11)
  NegativeMask   = 0b00000011,
  Negative       = 0b00000001, // Shows -, but no character for +
  NegativePos    = 0b00000010, // Shows - and +
  NegativeSpace  = 0b00000011, // Shows - and a space for +
  
  // 00000c00 0 padding for most significant digit (0 is ' ', 1 is '0')
  PadZero        = 0b00000100,

  // 0000d000 Uses alt form
  AltForm        = 0b00001000,
};

template <>
struct BitwiseOps<eFmt0> : std::true_type {
};

/**
 * @brief These are placed after eFmt0.
 *
 * If FillSpecified is set, then followed by a FILL_CHAR and then an
 * eFmtLetter.
 * 
 * Otherwise, it's followed by a eFmtLetter.
 */
enum eFmt1 : uint8_t {
  // 000000fe min field width (embedded/parameter)
  //          (f = 0 no min field supplied, f = 1 min field supplied next)
  //          (e = 0 embedded, e = 1 parameter)
  MinField       = 0b00000001, // has a minimum field
  MinFieldParam  = 0b00000010, // minimum field is not embedded in string, but as a parameter.

  // 0000hg00   left/right justified (0 is left justified, 1 is right justified)
  JustifyMask    = 0b00001100,
  JustifyLeft    = 0b00000000, // Left justify with pad chars after it if field larger than number.
  JustifyFill    = 0b00000100, // Right justify with pad chars after sign, before number.
  JustifyRight   = 0b00001000, // Right justify with pad chars before sign, sign is next to number.
  JustifyCentre  = 0b00001100, // Centred.  Any extra pad char is on left.

  // 00ji0000 precision (embedded/parameter)
  //          (g = 0 no precision supplied, g = 1 precision supplied)
  //          (h = 0 embedded, h = 1 parameter)
  Precision      = 0b00010000, // has a precision field
  PrecisionParam = 0b00100000, // precision field is not embedded in string, but as a parameter.

  // 0k000000 Fill character specified after eFmt1 and before eFmtLetter
  FillSpecified  = 0b01000000,

  // l0000000 Coerces negative zero floating-point values to positive zero after
  // rounding to the format precision. 
  PositiveZero   = 0b10000000,
};

template <>
struct BitwiseOps<eFmt1> : std::true_type {
};

enum eFmtLetter : char {
  FormatMask    = 0x0F,
  NoCaseMask    = 0x08, // Has no case associated with format.
  UpperCaseMask = 0x04, // Is uppercase (value 0x04 itself isn't actually used since it's lowercase value is 0, which isn't valid in an fstring)

  SciLower      = 0x01, // 'e' float / double - (lowercase) Scientific
  FixLower      = 0x02, // 'f' float / double - (lowercase) Fixed precision
  GenLower      = 0x03, // 'g' float / double - (lowercase) General
  SciUpper      = 0x05, // 'E' float / double - (UPPERCASE) Scientific
  FixUpper      = 0x06, // 'F' float / double - (UPPERCASE) Fixed precision
  GenUpper      = 0x07, // 'G' float / double - (UPPERCASE) General

  Binary        = 0x08, // 'b' int - binary format
  DecInt        = 0x09, // 'd' int (default) - decimal, if no valid letter, this is used.
  OctInt        = 0x0A, // 'o' int - octal
  HexIntLower   = 0x0B, // 'x' int - hex (lowercase)
  HexIntUpper   = 0x0C, // 'X' int - Hex (uppercase)
  Character     = 0x0D, // 'c' char - character
  String        = 0x0E, // 's' char const* / char const[] / std::string
  Percent       = 0x0F, // '%' float / double - Show as percentage (mult by 100 add % sign)

  GroupFracMask    = 0b11000000,
  GroupFracNone    = 0b00000000, // No grouping chars used in fraction part.
  GroupFracComma   = 0b01000000, // in fraction part, put a ',' every 3 digits (dec)
  GroupFracUnder   = 0b10000000, // in fraction part, put a '_' every 3 digits (dec)
  GroupFracLocale  = 0b11000000, // in fraction part, put a locale separator character
                                 //   every 3 digits (dec)

  GroupWholeMask   = 0b00110000,
  GroupWholeNone   = 0b00000000, // No grouping chars used in whole part.
  GroupWholeComma  = 0b00010000, // in whole part, put a ',' every 3 digits (dec) or 4 digits (bin, oct, hex)
  GroupWholeUnder  = 0b00100000, // in whole part, put a '_' every 3 digits (dec) or 4 digits (bin, oct, hex)
  GroupWholeLocale = 0b00110000, // in whole part, put a locale separator character every 3 digits (dec) or 4
                                 //   digits (bin, oct, hex)
};

template<>
struct BitwiseOps<eFmtLetter> : std::true_type {
};

// template<
//   std::size_t N,
//   const char (&Str)[N],
//   std::size_t I   = 0,
//   auto Type       = static_cast<eType>(0),
//   auto Additional = static_cast<eFmt0>(0),
//   auto Format     = static_cast<eFmt1>(0),
//   auto Letter     = static_cast<eFmtLetter>(0),
//   char... Cs, typename... Ts
// >
// auto parse_fstr(std::index_sequence<Cs...> result, Ts&&...params);

// template<
//   std::size_t N,
//   const char (&Str)[N],
//   std::size_t I   = 0,
//   auto Type       = static_cast<eType>(0),
//   auto Additional = static_cast<eFmt0>(0),
//   auto Format     = static_cast<eFmt1>(0),
//   auto Letter     = static_cast<eFmtLetter>(0),
//   char... Cs
// >
// auto parse_fstr_param(char_sequence<Cs...> param) {
//   if constexpr (I == N) {
//     return param;
//   } else {
//     if constexpr (Str[I] < 32) {
      
//     }
//   }
// }

// template<typename T, char...Cs>
// auto fstr_param_type_array(T&& value, char_sequence<Cs...> result) {
//   static_assert(std::is_array_v<std::remove_reference_t<T>>);
// }

/**
 * @brief Append \p C1s to \p C0s in the returned char_sequence type.
 * 
 * @tparam C0s - Characters to append to.
 * @tparam C1s - Characters to append.
 * @return char_sequence<C0s..., C1s...> 
 */
template<char...C0s, char...C1s>
auto append_to_seq(char_sequence<C0s...>, char_sequence<C1s...>) {
  return char_sequence<C0s..., C1s...>;
}

template<auto Buff, char...C0s, std::size_t...Is>
auto append_to_seq_impl(char_sequence<C0s...>, std::index_sequence<Is...>) {
  return char_sequence<C0s..., static_cast<char>(Buff[Is])...>{};
}

/**
 * @brief Appends characters in \p Buff to \p CS.
 * 
 * @tparam Buff - An \c std::array or a C-array containing elements to append.
 * @tparam CS - A \c char_sequence type to append to.
 * @param char_seq - Used to pass the CS type.
 * 
 * @return auto - A new char_sequence with the values appended.
 */
template<auto Buff, typename CS>
auto append_to_seq(CS char_seq) {
  return append_to_seq_impl<Buff>(char_seq, std::make_index_sequence<std::size(Buff)>{});  
}

// All logged enums need to be registered
template<typename E>
struct enum_id {};

/**
 * @brief Generate a \p char_sequence that contains the appropriate eType info.
 *
 * It's a \c char_sequence because it could be an array or enum which has
 * additional data. Size and Type for array and Enum and EnumId for enum. 
 *
 * @tparam T - Type being encoded.
 * @tparam Cs - Current character sequence.
 * @param result - Used to update return type.
 * @return char_sequence<...> - Character sequence containing eType info.
 */
template<typename T, char...Cs>
constexpr auto fstr_param_type(char_sequence<Cs...> result = char_sequence<>{}) {
  if constexpr (std::is_array_v<std::remove_reference_t<T>>) {
    constexpr auto array_size_dint { Constexpr::encode_dint(std::size(T{})) };
    using DT = std::remove_extent_t<T>;
    return fstr_param_type<DT>(append_to_seq(result
      , append_to_seq<array_size_dint>(char_sequence<eType::Array>{})));
  }
  else if constexpr (std::is_enum_v<T>) {
    constexpr auto enum_id_dint{ Constexpr::encode_dint(enum_id<T>::value) };
    return append_to_seq(result
      , append_to_seq<enum_id_dint>(char_sequence<eType::Enum>{}));
  }
  // NOTE that if std::int8_t is to be treated as a char, it needs check the
  // format letter to fix it.
  else if constexpr (std::is_same_v<T, std::int8_t>) {
    return append_to_seq(result, char_sequence<eType::Int8>{});
  }
  else if constexpr (std::is_same_v<T, std::int16_t>) {
    return append_to_seq(result, char_sequence<eType::Int16>{});
  }
  else if constexpr (std::is_same_v<T, std::int32_t>) {
    return append_to_seq(result, char_sequence<eType::Int32>{});
  }
  else if constexpr (std::is_same_v<T, std::int64_t>) {
    return append_to_seq(result, char_sequence<eType::Int64>{});
  }
  else if constexpr (std::is_same_v<T, std::uint8_t>) {
    return append_to_seq(result, char_sequence<eType::UInt8>{});
  }
  else if constexpr (std::is_same_v<T, std::uint16_t>) {
    return append_to_seq(result, char_sequence<eType::UInt16>{});
  }
  else if constexpr (std::is_same_v<T, std::uint32_t>) {
    return append_to_seq(result, char_sequence<eType::UInt32>{});
  }
  else if constexpr (std::is_same_v<T, std::uint64_t>) {
    return append_to_seq(result, char_sequence<eType::UInt64>{});
  }
  else if constexpr (std::is_same_v<T, float>) {
    return append_to_seq(result, char_sequence<eType::Float>{});
  }
  else if constexpr (std::is_same_v<T, double>) {
    return append_to_seq(result, char_sequence<eType::Double>{});
  }
  else if constexpr (std::is_same_v<T, bool>) {
    return append_to_seq(result, char_sequence<eType::Bool>{});
  } else {
    throw "Can only format numeric/boolean/enum types.";
  }
}

template<
  std::size_t N,
  const char (&Str)[N],
  std::size_t StrI   = 0,
  std::size_t ParamI = 0,
  auto Type       = static_cast<eType>(0),
  auto Additional = static_cast<eFmt0>(0),
  auto Format     = static_cast<eFmt1>(0),
  auto Letter     = static_cast<eFmtLetter>(0),
  std::size_t... Is, typename... Ts
>
auto parse_fstr(char_sequence<Is...> result, Ts&&...params) {
  if constexpr (StrI == N) {
    return param;
  } else {
    if constexpr (Str[StrI] == '{') {
      if constexpr (Str[StrI] == '}') {

      }
      return parse_fstr_param<N, Str, I, 
    }
  }
}

// Using C++20 string formatting
//
// {} // default output
// Field positions are currently not allowed.
// {1:} // Error
//
// 
//
// format_spec:             [options][width_and_precision][compressed][array][type]
// options:                 [[fill]align][sign]["z"]["#"]["0"]
// fill:                    <any character>
// align:                   "<" | ">" | "=" | "^"
// sign:                    "+" | "-" | " "
// width_and_precision:     [width_with_grouping][precision_with_grouping]
// width_with_grouping:     [width][grouping]
// precision_with_grouping: "." [precision][grouping] | "." grouping
// width:                   digit+
// precision:               digit+
// grouping:                "," | "_"
// compressed:              ["!"]
// array:                   ["a"]
// type:                    "b" | "c" | "d" | "e" | "E" | "f" | "F" | "g"
//                          | "G" | "o" | "s" | "x" | "X" | "%"
//                          NOTE: "n" not supported.


template <std::size_t I = 0, std::size_t N, std::size_t...Is>
constexpr auto build_fstr_type(char const (&fstr)[N], char_sequence<Is...>{}) {
  return build_fstr_type<I+1>(fstr, char_sequence<Is..., fstr[I]);
}  


// fstr
//
// R'(\)'  = '\\'
// R'(\n)' = '\n'
// R'(\t)' = '\t'

// R'(%8i)'  = eType::Int8  + 1
// R'(%16i)' = eType::Int16 + 1
// R'(%32i)' = eType::Int32 + 1
// R'(%64i)' = eType::Int64 + 1

// R'(%8u)'  = eType::Uint8  + 1
// R'(%16u)' = eType::Uint16 + 1
// R'(%32u)' = eType::Uint32 + 1
// R'(%64u)' = eType::Uint64 + 1

// R'(%8I)'  = eType::Int8   | (eType::Compressed + 1)
// R'(%16I)' = eType::Int16  | (eType::Compressed + 1)
// R'(%32I)' = eType::Int32  | (eType::Compressed + 1)
// R'(%64I)' = eType::Int64  | (eType::Compressed + 1)

// R'(%8U)'  = eType::Uint8  | (eType::Compressed + 1)
// R'(%16U)' = eType::Uint16 | (eType::Compressed + 1)
// R'(%32U)' = eType::Uint32 | (eType::Compressed + 1)
// R'(%64U)' = eType::Uint64 | (eType::Compressed + 1)

// R'(%b)'   = eType::Bool   + 1
// R'(%d)'   = eType::Double + 1

// R'(%c)'   = eType::Char  + 1
// R'(%f)'   = eType::Float + 1
// R'(%e)'   = eType::Enum  + 1
// R'(%a)'   = eType::Array + 1


/** get a block and initialise its header */
Block& get_block(BlockType_e block_type, int sequence_number) {
};

enum eSeverity : char {
  Info, Warn, Err
};

class IdFstring {
  eSeverity severity;  // severity level
  char const *fstring; // format string
  int fstring_len;    // length of format string
  uint16_t recurrence; // number of times this appeared in the output stream.
  uint16_t id;         // id that represents this string

  IdFstring(char const* fstring)
  : fstring(fstring)
  , fstring_len(strlen(fstring))
  , recurrence(0)
  , id(0)
  {}

  /**
   * Inject this string into a block.
   *
   * @param block: block to inject into
   * @param offset: what offset to inject this into
   *
   * @returns offset for next item to insert at.
   */
  int inject(Block& block, int offset){
  }


  /**
   * Inject swap into block.
   *
   * This shouldn't happen often since this only swaps if id
   * goes from higher number of bytes to lower number of bytes.
   *
   * @param block: block to inject into
   * @param other_IdFstring: the other IdFstring to swap with.
   *
   * @returns offset for next item to insert at.
   */
  int inject_swap(Block& block, IdFstring& other_IdFstring) {

  }

/**
 * Converts id to 7 bit number plus continuation bit format,
 * writing to the block at offset.
 *
 * @returns offset for next item to insert at.
 */
int id_to_bytes(int id, Block& block, int offset) {
}

class StringInfo {
  severity: SeverityEnum
  format: str
}
