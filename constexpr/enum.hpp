/**
 * @file enum.hpp
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
#ifndef CONSTEXPR_ENUM_HPP
#define CONSTEXPR_ENUM_HPP

#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <variant>
#include <string_view>
#include <type_traits>
#include <utility>

#include "masked_bits.hpp"
#include "bitwise_enum.hpp"
#include "int_codec.hpp"
#include "string.hpp"
#include "bitwise_enum.hpp"
#include "type_traits.hpp"
#include "heap.hpp"
#include "int_codec.hpp"

namespace Constexpr {

 // Using a Namespace-Enclosed Enum and then aliasing the enclosed enum with the wanted name makes
 // accessing enum values require full qualification while still allowing implicit number
 // conversion.

namespace eEnumCommand_ns {
  enum eEnumCommand : std::uint8_t {
    mOpCode            = 0b1110'0000, // Mask for opcode
    mCountLarge        = 0b0001'1111, // Mask for count
    mCountMedium       = 0b0000'1111, // Mask for count
    mCountSmall        = 0b0000'0111, // Mask for count

    //                    Count size --v-- Holds number of items that follow and may be 0 or 1 based.
    Terminate          = 0 << 5,  // | - | End of stream if stream length not known.

    Named              = 1 << 5,  // | M | Specifies pairs; compares (value & current or stated bitmask) to enum_value.
     fHasBitmask       =  1 << 4, // |   |   States if bitmask is specified.

    Numeric            = 2 << 5,  // | - | Specifies name for bits for stated bitmask.
     fRightShiftBits   =  1 << 0, // |   |   Shift bits so least significant bits coinciding with bitmask are at the 0th bit.
     fPackedBits       =  1 << 1, // |   |   Condense bits coinciding with bitmask.
     fIsSigned         =  1 << 2, // |   |   Sign extend bit coinciding with most significant bit of bitmask.

    GroupIf            = 3 << 5,  // | M | If value's (1 << group_shift) bit is set, use bitmask on following commands.
    GroupIfNamed       = 4 << 5,  // | M | If value's (1 << group_shift) bit is set, use bitmask on following pairs.
    Else               = 5 << 5,  // | S | Continue the current conditional scope as else group.
     fHasGroupName     =  1 << 4, // |   | States if group name is specified for GroupIf* and Else.
     fElseCmds         =  1 << 3, // |   | Used only by Else.  If set, Else is for commands otherwise pairs.
    ContinueScope      = 6 << 5,  // | L | Continue the current named or command branch.

    GroupIfNumeric     = 7 << 5,  // | - | If value's (1 << group_shift) bit is set, specify numeric output for stated bitmask.
     fNegate           =  1 << 3, // |   |   Inverts the inline numeric condition, so the numeric item belongs to the else case.
    // GroupIfNumeric also can take fRightShiftBits, fPackedBits, fIsSigned and fHasGroupName flags.
  };
}
/**
 * @brief Enum command opcodes
 */
using eEnumCommand = eEnumCommand_ns::eEnumCommand;

namespace eEnumStorageType_ns {
  enum eEnumStorageType : std::uint8_t {
    fIsSigned= 0x04, // Bit states if type is signed
    UInt8    = 0x00, UInt16   = 0x01, UInt32   = 0x02, UInt64   = 0x03,
     Int8    = 0x04,  Int16   = 0x05,  Int32   = 0x06,  Int64   = 0x07,

    // States how constrained group_bitmask, bitmask and enum_value values are stored
    Compress = 0x08, // Encode constrained values as condensed dints under the parent scope
  };
}
/**
 * @brief Describes the stored underlying type and constrained-value encoding
 *   mode for one definition stream.
 */
using eEnumStorageType = eEnumStorageType_ns::eEnumStorageType;

} // namespace Constexpr

template <>
struct BitwiseOps<Constexpr::eEnumCommand> : std::true_type {};

template <>
struct BitwiseOps<Constexpr::eEnumStorageType> : std::true_type {};

// What we need:
//
// 1. Enum definition container instance that will have the specification of the enum.
// 2. Item 1 implies that we need a class to create the container instance.
// 3. Will need a function to start creating the byte stream, which will return a type instance that
//    will chain and produce the byte stream.  This means that the builder and the definition holder
//    needs to be the same object, unless I have a terminator for the chain.  Separating out the
//    concerns is probably for the best and will prevent users from seeing builder functions when
//    they just want access functions.
// 4. There is a value() member that weill return an EnumInstance container, which is just a class
//    wrapping around the integer.
// 5. To be able to set a value, the EnumInstance object will overload the operator=() to take a
//    some `set` and `force_set` object to allow it to work as I've designed.
// 6. When building with EnumBuilder, it either has to know the max allocation size and reuse the
//    EnumBuilder or it'll have to create a new EnumBuilder at every step, copying the previous
//    data.  Although the latter is a bit abhorrent to me, it is the best way through and the
//    intermediate object will vaporise soon enough anyway.
//

// Types needed:
//
// 1. EnumBuilder - Builds the Enum
// 2. Enum - defines the enum, the names it uses, the underlying type, and the dependencies.
// 3. EnumInstance - Wrapper around the underlying type.
// 4. EnumSet - Used to set EnumInstance's underlying value.
// 5. EnumForceSet - Used to set EnumInstance's underlying value, but reduces restrictions.
// 6. Abstraction
//
//    ## Graph Classes
//
//    Each class below is basically a set of integers. The items below each is the number of aggregate integers in each
//
//        Pairs {
//          E value
//          Int name_string_id
//          Int next_id
//
//          build_stream(string current) : string
//        } 3 items
//
//        Cmd {
//          get_string(E value) : string
//        } Conceptual base class
//
//        Named : Cmd {
//          bool has_mask
//          E mask
//          Pairs first_pair_id
//          Pairs last_pair_id  // optimisation
//
//          build_stream(string current) : string
//        } 5 items + 1
//
//        Numeric : Cmd {
//          E mask
//          Format format
//          Str name_string_id
//
//          build_stream(string current) : string
//        } 3 items
//
//        Cmds {
//          Cmd command_id
//          Int next_id
//
//          build_stream(string current) : string
//        } 2 items
//
//        Group {
//          Str name_string_id
//          Cmds first_cmd_id
//          Cmds last_cmd_id    // optimisation
//
//          build_stream(string current) : string
//        } 3 items + 1
//
//        Conditional : Cmd {
//          E group_bitmask
//          Group true_commands_id
//          Group false_commands_id
//
//          build_stream(string current) : string
//        } 7 items
//
//    ## Storage classes
//
//    These are storage classes. The indices in the previous classes refer to an object or string here.   This reduces
//    the class template count parameters to 2 values. The total length of strings bytes stored and the number of
//    objects.  This makes it simple to manage and since nothing is ever deleted, we don't even have to track that
//    anywhere.
//
//        Strings {
//          String memory
//
//          add_string(String str_to_register) : index
//        }
//
//        Items {
//          array<variant<Pairs, Named, Numeric, Group, Conditional>> memory
//
//          add_item(T item) : index
//        }
//

// using E = std::int8_t;

namespace Constexpr {
  /**
  * @brief Base parse error for malformed enum definition streams.
  */
  class EnumParseError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
  };

  /**
  * @brief Reports that no header byte was available to start decoding.
  */
  class EnumParseEmptyInput : public EnumParseError {
  public:
    using EnumParseError::EnumParseError;
  };

  /**
  * @brief Reports that the decoded storage-type header does not match the
  *   requested enum value type.
  */
  class EnumParseHeaderMismatch : public EnumParseError {
  public:
    using EnumParseError::EnumParseError;
  };

  /**
  * @brief Reports that the stream ended before the current item could be fully
  *   decoded.
  */
  class EnumParseUnexpectedEof : public EnumParseError {
  public:
    using EnumParseError::EnumParseError;
  };

  /**
  * @brief Reports that an opcode byte or inline flag combination is invalid.
  */
  class EnumParseInvalidOpcode : public EnumParseError {
  public:
    using EnumParseError::EnumParseError;
  };

  /**
  * @brief Reports that the stream violates one of the enum grammar or graph
  *   invariants.
  */
  class EnumParseInvalidStructure : public EnumParseError {
  public:
    using EnumParseError::EnumParseError;
  };

  /**
  * @brief Reports that the decoded payload exceeds the destination enum's fixed
  *   string or item capacity.
  */
  class EnumParseCapacityExceeded : public EnumParseError {
  public:
    using EnumParseError::EnumParseError;
  };

  template <typename EnumT>
  class EnumValueView;

  template <std::uint32_t StringAndItemCapacity>
  class AnyEnumDescription;

  template <typename AnyEnumT>
  class AnyEnumValueView;

  /**
   * @brief Used to pack string_space and item_space into one number.
   *
   * @param string_space - Space to allocate for strings.
   * @param item_space - Space to allocate for items.
   * @return std::uint32_t - Packed values.
   */
  constexpr std::uint32_t pack_space(std::uint16_t string_space, std::uint16_t item_space) {
    return static_cast<std::uint32_t>(string_space) | (static_cast<std::uint32_t>(item_space) << 16);
  }

  namespace impl {

    template <typename Parent>
    class IfScope;

    template <typename Parent>
    class ElseScope;

    template <typename Derived>
    class CommandScopeFacade;

    template <typename Settings>
    class EnumDecoder;

    using size_t = std::uint16_t;

    /**
     * @brief Extract the string space value out of the packed 32 bit number.
     *
     * @param packed_value - Packed value to extract from.
     * @return std::uint16_t - Unpacked string space value.
     */
    constexpr std::uint16_t string_space(std::uint32_t packed_value) {
      return static_cast<std::uint16_t>(packed_value);
    }

    /**
     * @brief Extract the item space value out of the packed 32 bit number.
     *
     * @param packed_value - Packed value to extract from.
     * @return std::uint16_t - Unpacked item space value.
     */
    constexpr std::uint16_t item_space(std::uint32_t packed_value) {
      return static_cast<std::uint16_t>(packed_value >> 16);
    }

    using program_cursor_t = char*;

    /**
     * @brief Non-owning byte sink for building a definition stream.
     */
    class ProgramWriter {
      program_cursor_t m_begin{};
      program_cursor_t m_end{};
      program_cursor_t m_cursor{};

    public:
      using cursor_type = program_cursor_t;

      /**
       * @brief Constructs an empty writer.
       */
      constexpr ProgramWriter() noexcept = default;

      /**
       * @brief Constructs a writer over an explicit writable byte range.
       *
       * @param begin - First writable byte.
       * @param end - One-past-the-end of the writable range.
       */
      constexpr ProgramWriter(program_cursor_t begin, program_cursor_t end) noexcept
      : m_begin{ begin }
      , m_end{ end }
      , m_cursor{ begin }
      {
      }

      /**
       * @brief Constructs a writer over a fixed-capacity constexpr string.
       *
       * The target string is expanded to its logical character capacity first
       * so the writer can use the full raw storage without a later resize
       * clobbering already-written bytes.
       *
       * @tparam N - Storage size of the destination string.
       * @param program - Destination buffer.
       */
      template <std::size_t N>
      explicit constexpr ProgramWriter(Constexpr::string<N>& program) noexcept
      : ProgramWriter(program.data(), program.data() + program.capacity())
      {
        program.resize(program.capacity());
      }

      /**
       * @brief Returns a pointer to the next byte that will be written.
       *
       * @return program_cursor_t - Current write cursor.
       */
      constexpr program_cursor_t program_cursor() const noexcept {
        return m_cursor;
      }

      /**
       * @brief Returns the number of bytes already written.
       *
       * @return std::size_t - Written byte count.
       */
      constexpr std::size_t size() const noexcept {
        return static_cast<std::size_t>(m_cursor - m_begin);
      }

      /**
       * @brief Returns a writable reference to a byte already inside the buffer.
       *
       * @param cursor - Pointer to the byte to patch.
       * @return char& - Writable byte reference.
       */
      constexpr char& byte_at(program_cursor_t cursor) {
        assert(m_begin <= cursor && cursor < m_end);
        return *cursor;
      }

      /**
       * @brief Returns a const reference to a byte already inside the buffer.
       *
       * @param cursor - Pointer to the byte to inspect.
       * @return char const& - Const byte reference.
       */
      constexpr char const& byte_at(program_cursor_t cursor) const {
        assert(m_begin <= cursor && cursor < m_end);
        return *cursor;
      }

      /**
       * @brief Appends one opcode byte.
       *
       * @param cmd - Opcode to append.
       */
      constexpr void write_opcode(eEnumCommand cmd) {
        write_byte(static_cast<std::uint8_t>(cmd));
      }

      /**
       * @brief Appends one fixed-width integer.
       *
       * @tparam T - Integral value type.
       * @param value - Value to append.
       */
      template <typename T,
        typename std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value, int> = 0>
      constexpr void write_int(T value) {
        if constexpr (sizeof(T) == 1) {
          write_byte(static_cast<std::uint8_t>(value));
        } else {
          m_cursor = Constexpr::encode_int(m_cursor, m_end, value);
        }
      }

      /**
       * @brief Appends one dynamic-length integer.
       *
       * @tparam T - Integral value type.
       * @param value - Value to append.
       */
      template <typename T,
        typename std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value, int> = 0>
      constexpr void write_dint(T value) {
        m_cursor = Constexpr::encode_dint(m_cursor, m_end, value);
      }

      /**
       * @brief Appends a NUL-terminated string.
       *
       * @param value - String to append without its terminator.
       */
      constexpr void write_c_string(std::string_view value) {
        for (char const ch : value) {
          write_byte(static_cast<std::uint8_t>(static_cast<unsigned char>(ch)));
        }
        write_byte(0u);
      }

      /**
       * @brief Updates a constexpr string's logical size to match the bytes written.
       *
       * @tparam N - Storage size of the destination string.
       * @param program - Destination string that backs this writer.
       */
      template <std::size_t N>
      constexpr void finish(Constexpr::string<N>& program) const {
        assert(program.data() == m_begin);
        assert(program.data() + program.capacity() == m_end);
        program.resize(size());
      }

      /**
       * @brief Appends one raw byte.
       *
       * @param byte - Byte to append.
       */
      constexpr void write_byte(std::uint8_t byte) {
        assert(m_cursor != m_end || !"Not enough space to store in buffer");
        *m_cursor++ = static_cast<char>(byte);
      }
    };

    /**
     * @brief Non-owning byte sink that counts exact encoded output size.
     *
     * This writer mirrors ProgramWriter's surface closely enough for the
     * encoder to reuse the same walk during constexpr sizing passes.
     */
    class CountingProgramWriter {
      std::size_t m_size{};
      char m_dummy{};

    public:
      using cursor_type = std::size_t;

      /**
       * @brief Constructs an empty counting writer.
       */
      constexpr CountingProgramWriter() noexcept = default;

      /**
       * @brief Returns the current logical byte position.
       *
       * @return cursor_type - Current logical byte offset.
       */
      constexpr cursor_type program_cursor() const noexcept {
        return m_size;
      }

      /**
       * @brief Returns the total number of bytes that would have been written.
       *
       * @return std::size_t - Exact encoded size in bytes.
       */
      constexpr std::size_t size() const noexcept {
        return m_size;
      }

      /**
       * @brief Returns a writable dummy byte for opcode patching.
       *
       * @param cursor - Previously reserved logical byte offset.
       * @return char& - Stable dummy byte reference.
       */
      constexpr char& byte_at(cursor_type cursor) noexcept {
        assert(cursor < m_size);
        (void)cursor;
        return m_dummy;
      }

      /**
       * @brief Returns a const dummy byte for opcode inspection.
       *
       * @param cursor - Previously reserved logical byte offset.
       * @return char const& - Stable dummy byte reference.
       */
      constexpr char const& byte_at(cursor_type cursor) const noexcept {
        assert(cursor < m_size);
        (void)cursor;
        return m_dummy;
      }

      /**
       * @brief Accounts for one opcode byte.
       *
       * @param cmd - Ignored opcode value.
       */
      constexpr void write_opcode([[maybe_unused]] eEnumCommand cmd) noexcept {
        ++m_size;
      }

      /**
       * @brief Accounts for one opcode byte.
       *
       * @param byte - Ignored opcode value.
       */
      constexpr void write_byte([[maybe_unused]] std::uint8_t byte) noexcept {
        ++m_size;
      }

      /**
       * @brief Accounts for one fixed-width integer payload.
       *
       * @tparam T - Integral value type.
       * @param value - Ignored fixed-width value.
       */
      template <typename T,
        typename std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value, int> = 0>
      constexpr void write_int([[maybe_unused]] T value) noexcept {
        m_size += sizeof(T);
      }

      /**
       * @brief Accounts for one condensed dint payload.
       *
       * @tparam T - Integral value type.
       * @param value - Value whose condensed byte length should be counted.
       */
      template <typename T,
        typename std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value, int> = 0>
      constexpr void write_dint(T value) noexcept {
        m_size += DIntAndSize{ value }.size();
      }

      /**
       * @brief Accounts for one NUL-terminated string payload.
       *
       * @param value - String to count without its trailing terminator.
       */
      constexpr void write_c_string(std::string_view value) noexcept {
        m_size += value.size() + 1u;
      }
    };

    // This might not be needed.  Maybe useful for symbolic access to maximum number of distinct
    // items.  The rest...?  Meh.
    enum class eBlockType {
                                     // Range   Max Number of
                                     //         distinct items

      Pair           = 0b0'0'000'10, /// 1-16   16
      IfPair         = 0b0'0'001'00, /// 0-15   15
      ElsePair       = 0b0'0'010'01, /// 1-8    8
      ContPair       = 0b0'0'011'11, /// 1-32   32
      IfCmd          = 0b0'1'001'00, /// 0-15   15
      ElseCmd        = 0b0'1'010'01, /// 1-8    8
      ContCmd        = 0b0'1'011'11, /// 1-32   32

      mBlockType     = 0b0'0'111'00,
      Named          = 0b0'0'000'00, // Laying down pairs          for Named block
      If             = 0b0'0'001'00, // Laying down pairs/commands for If block
      Else           = 0b0'0'010'00, // Laying down pairs/commands for Else block
      Continue       = 0b0'0'011'00, // Laying down pairs/commands for Cont block

      mCmd           = 0b0'1'000'00, // Set means list of commands, otherwise list of pairs

      mMaxCount      = 0b0'0'000'11,
      Max15          = 0b0'0'000'00, // Minimum value 0. Maximum elements: 15
      Max8           = 0b0'0'000'01, // Minimum value 1. Maximum elements: 8
      Max16          = 0b0'0'000'10, // Minimum value 1. Maximum elements: 16
      Max32          = 0b0'0'000'11, // Minimum value 1. Maximum elements: 32
    };

  } // namespace impl
} // namespace Constexpr

template<>
struct BitwiseOps<Constexpr::impl::eBlockType> : std::true_type {};

namespace Constexpr {
  namespace impl {

    constexpr inline auto to_underlying(Constexpr::impl::eBlockType value) {
      using E = Constexpr::impl::eBlockType;
      return static_cast<std::underlying_type_t<E>>(value);
    }

    /**
     * @brief Returns the underlying byte of one stream storage-type flag value.
     *
     * @param value - Storage-type flag to inspect.
     * @return std::underlying_type_t<eEnumStorageType> - Underlying byte value.
     */
    constexpr inline auto to_underlying(Constexpr::eEnumStorageType value) {
      using E = Constexpr::eEnumStorageType;
      return static_cast<std::underlying_type_t<E>>(value);
    }

    /**
     * @brief Returns the non-flag storage-type discriminator for a value type.
     *
     * @tparam T - Enum or integral value type.
     * @return eEnumStorageType - Width/sign storage discriminator.
     */
    template <typename T>
    constexpr eEnumStorageType storage_type_for_value_type() noexcept {
      using U = underlying_equivalent_t<T>;

      if constexpr (std::is_signed_v<U>) {
        if constexpr (sizeof(U) == 1u) {
          return eEnumStorageType::Int8;
        } else if constexpr (sizeof(U) == 2u) {
          return eEnumStorageType::Int16;
        } else if constexpr (sizeof(U) == 4u) {
          return eEnumStorageType::Int32;
        } else {
          static_assert(sizeof(U) == 8u, "Unsupported signed enum storage width.");
          return eEnumStorageType::Int64;
        }
      } else {
        if constexpr (sizeof(U) == 1u) {
          return eEnumStorageType::UInt8;
        } else if constexpr (sizeof(U) == 2u) {
          return eEnumStorageType::UInt16;
        } else if constexpr (sizeof(U) == 4u) {
          return eEnumStorageType::UInt32;
        } else {
          static_assert(sizeof(U) == 8u, "Unsupported unsigned enum storage width.");
          return eEnumStorageType::UInt64;
        }
      }
    }

    /**
     * @brief Returns whether one storage-type header enables condensed integer
     *   encoding.
     *
     * @param header - Decoded storage-type header.
     * @return bool - \c true when constrained values are dint-condensed.
     */
    constexpr bool storage_type_is_compressed(eEnumStorageType header) noexcept {
      return (to_underlying(header) & to_underlying(eEnumStorageType::Compress)) != 0u;
    }

    /**
     * @brief Removes the compression flag from one storage-type header.
     *
     * @param header - Decoded storage-type header.
     * @return eEnumStorageType - Base width/sign discriminator.
     */
    constexpr eEnumStorageType storage_type_base(eEnumStorageType header) noexcept {
      return static_cast<eEnumStorageType>(to_underlying(header) & 0x07u);
    }

    /**
     * @brief Reads and validates the one-byte storage-type header prefix of one
     *   enum definition stream.
     *
     * @param program - Definition stream including the header byte.
     * @return eEnumStorageType - Decoded storage-type header including the
     *   optional Compress bit.
     *
     * @throws EnumParseEmptyInput when \p program is empty.
     * @throws EnumParseHeaderMismatch when the header uses unsupported bits.
     */
    constexpr eEnumStorageType read_storage_header(std::string_view program) {
      if (program.empty()) {
        throw EnumParseEmptyInput("Enum program must contain at least the storage-type header byte.");
      }

      std::uint8_t const raw_header{ static_cast<std::uint8_t>(program.front()) };
      if ((raw_header & static_cast<std::uint8_t>(~0x0fu)) != 0u) {
        throw EnumParseHeaderMismatch("Enum program header uses unsupported storage-type bits.");
      }

      return static_cast<eEnumStorageType>(raw_header);
    }

    /**
     * @brief Returns the encoded one-byte storage header for a value type.
     *
     * @tparam T - Enum or integral value type.
     * @param compress - Whether constrained values are dint-condensed.
     * @return std::uint8_t - Encoded storage-type header byte.
     */
    template <typename T>
    constexpr std::uint8_t storage_header_for_value_type(bool compress = false) noexcept {
      auto header{ static_cast<std::uint8_t>(storage_type_for_value_type<T>()) };
      if (compress) {
        header = static_cast<std::uint8_t>(header | to_underlying(eEnumStorageType::Compress));
      }
      return header;
    }

    constexpr inline size_t max_items_for_block(eBlockType block_type) {
      constexpr size_t counts [] { 15, 8, 16, 32 };
      return counts[to_underlying(block_type & eBlockType::mMaxCount)];
    }

    constexpr inline bool does_count_start_from_one(eBlockType block_type) {
      return to_underlying(block_type & eBlockType::mMaxCount);
    }

    template <typename E>
    struct ScopeData {
      E scope_bitmask{ static_cast<E>(-1) };
      int remaining_items_allowed_in_block{ -1 }; // Negative means the current scope has no block-local limit.

      /**
       * @brief Narrows the active scope bitmask.
       *
       * @param new_bitmask - Replacement bitmask that must remain inside the current scope.
       */
      constexpr void verify_scope_bitmask([[maybe_unused]] E new_bitmask) {
        assert((scope_bitmask & new_bitmask) == new_bitmask || !"new_bitmask must be a subset of the scope_bitmask");
      }

      /**
       * @brief Narrows the active scope bitmask.
       *
       * @param new_bitmask - Replacement bitmask that must remain inside the current scope.
       */
      constexpr void set_scope_bitmask(E new_bitmask) {
        verify_scope_bitmask(new_bitmask);
        scope_bitmask = new_bitmask;
      }
    };

    template <typename EnumT, typename WriterT = ProgramWriter>
    class EnumEncoder {
      EnumT const* m_enum{};
      WriterT* m_writer{};
      ScopeData<typename EnumT::value_type> m_scope{};
      bool m_compress{};

    public:
      using enum_type = EnumT;
      using value_type = typename EnumT::value_type;
      using writer_type = WriterT;
      using cursor_type = typename writer_type::cursor_type;

      /**
       * @brief Constructs an encoder for one definition-stream emission pass.
       *
       * @param enum_def - Immutable enum representation to read from.
       * @param writer - Writable stream sink.
       * @param compress - Whether scoped integer values should be condensed as dints.
       */
      constexpr EnumEncoder(EnumT const& enum_def, WriterT& writer, bool compress = false) noexcept
      : m_enum{ &enum_def }
      , m_writer{ &writer }
      , m_scope{}
      , m_compress{ compress }
      {
      }

      /**
       * @brief Constructs an encoder with an explicit starting scope snapshot.
       *
       * @param enum_def - Immutable enum representation to read from.
       * @param writer - Writable stream sink.
       * @param scope - Initial scope state.
       * @param compress - Whether scoped integer values should be condensed as dints.
       */
      constexpr EnumEncoder(
        EnumT const& enum_def,
        WriterT& writer,
        ScopeData<value_type> scope,
        bool compress = false) noexcept
      : m_enum{ &enum_def }
      , m_writer{ &writer }
      , m_scope{ scope }
      , m_compress{ compress }
      {
      }

      /**
       * @brief Returns the immutable enum representation.
       *
       * @return EnumT const& - Referenced enum definition.
       */
      constexpr EnumT const& enum_def() const noexcept {
        return *m_enum;
      }

      /**
       * @brief Returns the writable stream sink.
       *
       * @return WriterT& - Referenced output sink.
       */
      constexpr WriterT& writer() const noexcept {
        return *m_writer;
      }

      /**
       * @brief Returns the active scope snapshot.
       *
       * @return ScopeData<value_type>& - Mutable scope state.
       */
      constexpr ScopeData<value_type>& scope_data() noexcept {
        return m_scope;
      }

      /**
       * @brief Returns the active scope snapshot as const.
       *
       * @return ScopeData<value_type> const& - Const scope state.
       */
      constexpr ScopeData<value_type> const& scope_data() const noexcept {
        return m_scope;
      }

      /**
       * @brief Returns the current scope bitmask.
       *
       * @return value_type - Current scope bitmask.
       */
      constexpr value_type scope_bitmask() const noexcept {
        return m_scope.scope_bitmask;
      }

      /**
       * @brief Narrows the current scope bitmask.
       *
       * @param new_bitmask - Replacement bitmask that must remain inside the current scope.
       */
      constexpr void verify_scope_bitmask(value_type new_bitmask) {
        m_scope.verify_scope_bitmask(new_bitmask);
      }

      /**
       * @brief Narrows the current scope bitmask.
       *
       * @param new_bitmask - Replacement bitmask that must remain inside the current scope.
       */
      constexpr void set_scope_bitmask(value_type new_bitmask) {
        m_scope.set_scope_bitmask(new_bitmask);
      }

      /**
       * @brief Returns the mutable remaining item count for the current limited block.
       *
       * @return int& - Remaining item count or `-1` for an unlimited scope.
       */
      constexpr int& remaining_items_allowed_in_block() noexcept {
        return m_scope.remaining_items_allowed_in_block;
      }

      /**
       * @brief Returns the remaining item count for the current limited block.
       *
       * @return int - Remaining item count or `-1` for an unlimited scope.
       */
      constexpr int remaining_items_allowed_in_block() const noexcept {
        return m_scope.remaining_items_allowed_in_block;
      }

      /**
       * @brief Returns whether scoped integers are emitted in compressed form.
       *
       * @return bool - `true` when scoped values are condensed as dints.
       */
      constexpr bool compress() const noexcept {
        return m_compress;
      }

      /**
       * @brief Returns a pointer to the next byte to be written in the program buffer.
       *
       * @return cursor_type - Current write position token.
       */
      constexpr cursor_type program_cursor() const noexcept {
        return m_writer->program_cursor();
      }

      /**
       * @brief Returns a pointer to the next byte to be written in the program buffer.
       *
       * @return cursor_type - Reserved byte position token.
       */
      constexpr cursor_type reserve_byte() noexcept {
        auto cursor{ m_writer->program_cursor() };
        encode_int(eEnumCommand::Terminate);
        return cursor;
      }

      /**
       * @brief Returns a writable reference to a previously written byte.
       *
       * @param cursor - Writer-specific token naming the byte to patch.
       * @return char& - Writable byte reference.
       */
      constexpr char& byte_at(cursor_type cursor) {
        return m_writer->byte_at(cursor);
      }

      /**
       * @brief Returns a const reference to a previously written byte.
       *
       * @param cursor - Writer-specific token naming the byte to inspect.
       * @return char const& - Const byte reference.
       */
      constexpr char const& byte_at(cursor_type cursor) const {
        return m_writer->byte_at(cursor);
      }

      /**
       * @brief Bitwise-ORs selected bits into a previously written byte.
       *
       * @tparam T - Integral or enum byte-sized source of bits.
       * @param cursor - Writer-specific token naming the byte to update.
       * @param bits - Bits to OR into the byte.
       */
      template <typename T>
      constexpr void or_byte_at(cursor_type cursor, T bits) {
        auto const updated_byte {
          static_cast<unsigned char>(byte_at(cursor)) | static_cast<unsigned char>(bits)
        };
        byte_at(cursor) = static_cast<char>(updated_byte);
      }

      /**
       * @brief Encodes the referenced stored string into the stream as a NUL-terminated sequence.
       *
       * @param id - String id.
       */
      constexpr void encode_string(string_id_t id) {
        m_writer->write_c_string(m_enum->get_string(id));
      }

      /**
       * @brief Encodes a scoped value, compressing it when this encoder is configured to do so.
       *
       * @tparam T - Value type.
       * @param value - Value to encode.
       * @param scope_bitmask - Active scope bitmask for validation and condensation.
       */
      template <typename T>
      constexpr void encode_int(T value, T scope_bitmask) {
        assert((value & scope_bitmask) == value || !"value must be a subset of scope_bitmask");
        if (m_compress) {
          assert(sizeof(T) > 1 || !"Can't compress a type that has a byte length of 1.");
          auto const mask { make_unsigned_equivalent(scope_bitmask) };
          auto const cvalue { condense(mask, make_unsigned_equivalent(value), true) };
          m_writer->write_dint(cvalue);
        } else if constexpr (std::is_enum<T>::value) {
          using UT = typename std::underlying_type<T>::type;
          m_writer->write_int(static_cast<UT>(value));
        } else {
          m_writer->write_int(value);
        }
      }

      /**
       * @brief Encodes one enum-stream opcode byte.
       *
       * @param cmd - Opcode to encode.
       */
      constexpr void encode_int(eEnumCommand cmd) {
        m_writer->write_opcode(cmd);
      }

      /**
       * @brief Encodes one byte.
       *
       * @param byte - Byte to encode.
       */
      constexpr void encode_byte(std::uint8_t byte) {
        m_writer->write_byte(byte);
      }

      /**
       * @brief Retrieves an unresolved stored item variant by id.
       *
       * @param item_id - Item id to look up.
       * @return typename EnumT::command_t const& - Stored item variant.
       */
      constexpr typename EnumT::command_t const& item(item_id_t item_id) const {
        return m_enum->item(item_id);
      }

      /**
       * @brief Retrieves a stored item as a specific type.
       *
       * @tparam Item - Requested stored item type.
       * @param item_id - Item id to look up.
       * @return Item const& - Stored item.
       */
      template <typename Item>
      constexpr Item const& item(item_id_t item_id) const {
        return m_enum->template item<Item>(item_id);
      }

      /**
       * @brief Retrieves a stored item as a specific type.
       *
       * @tparam Item - Requested stored item type.
       * @param item_id - Item id to look up.
       * @return Item const& - Stored item.
       */
      template <typename Item>
      constexpr Item const* item_if(item_id_t item_id) const {
        return m_enum->template item_if<Item>(item_id);
      }
    };

    /**
     * @brief Creates a new scope that can limit the number of items to be
     *   generated in a first block and returns how many were.
     *
     * @tparam EnumT - Referenced enum representation type.
     * @tparam FnEncodeBlock - Callback type used to encode the child block.
     * @param ec - Active encoder for the parent scope.
     * @param block_type - Shape of the block being emitted.
     * @param fn_encode_block - Callback that emits the child block using a
     *   copied encoder.
     * @return size_t - Number of elements stored in first block.
     */
    template <typename EnumT, typename WriterT, typename FnEncodeBlock>
    constexpr size_t encode_block(
      EnumEncoder<EnumT, WriterT>& ec, eBlockType block_type, FnEncodeBlock fn_encode_block)
    {
      auto new_ec { ec };
      size_t const max_items { max_items_for_block(block_type) };
      new_ec.remaining_items_allowed_in_block() = static_cast<int>(max_items);

      fn_encode_block(new_ec);

      // Update the opcode's count parameter.
      size_t number_in_1st_block {
        static_cast<size_t>(max_items - static_cast<size_t>(new_ec.remaining_items_allowed_in_block())) };
      if (does_count_start_from_one(block_type)) {
        number_in_1st_block -= 1;
      }
      return number_in_1st_block;
    }

    template <typename E>
    struct Pairs {
      E value{};
      string_id_t name_id{};
      item_id_t next_pairs_id{};

      /**
       * @brief Encodes this pair node into the active named branch.
       *
       * @tparam EnumT - Referenced enum representation type.
       * @param ec - Active encoder for the current scope.
       */
      template <typename EnumT, typename WriterT>
      constexpr void encode(EnumEncoder<EnumT, WriterT>& ec) const {
        if (ec.remaining_items_allowed_in_block() > 0) {
          ec.remaining_items_allowed_in_block() -= 1;
          ec.encode_int(value, ec.scope_bitmask());
          ec.encode_string(name_id);
          if (next_pairs_id) {
            ec.template item<Pairs<E>>(next_pairs_id).encode(ec);
          }
        } else {
          auto const pc{ ec.reserve_byte() };
          size_t stored {
            encode_block(ec, eBlockType::ContPair,
              [&](auto& new_ec) { encode(new_ec); }) };
          ec.or_byte_at(pc, eEnumCommand::ContinueScope);
          ec.or_byte_at(pc, stored);
        }
      }
    };

    // Cmd {
    //   get_string(E value) : string
    // } Conceptual base class

    template <typename E>
    struct Named {
      bool has_mask{};
      E mask{};
      item_id_t pairs_id{}; // Type:  Pairs<E>
      // item_id_t last_pairs_id{}; // optimisation

      /**
       * @brief Encodes this named block and its child pairs.
       *
       * @tparam EnumT - Referenced enum representation type.
       * @param ec - Active encoder for the current scope.
       */
      template <typename EnumT, typename WriterT>
      constexpr void encode(EnumEncoder<EnumT, WriterT>& ec) const {
        assert(pairs_id || !"Can't define a Named block with no pairs.");

        auto const pc{ ec.reserve_byte() };
        auto new_ec { ec };
        size_t stored { encode_body(new_ec) };

        // Update the opcode.
        eEnumCommand opCode {};
        if (has_mask) {
          opCode = (eEnumCommand::Named | eEnumCommand::fHasBitmask);
        } else {
          opCode = (eEnumCommand::Named);
        }
        ec.or_byte_at(pc, opCode);
        ec.or_byte_at(pc, stored);
      }

      /**
       * @brief Encodes this named block body using the requested pair-block shape.
       *
       * @tparam EnumT - Referenced enum representation type.
       * @param ec - Active encoder for the current scope.
       * @param block_type - Pair-block shape used for the first emitted chunk.
       * @return size_t - Number of items stored in first block.
       */
      template <typename EnumT, typename WriterT>
      constexpr size_t encode_body(EnumEncoder<EnumT, WriterT>& ec, eBlockType block_type = eBlockType::Pair) const {
        assert(pairs_id || !"Can't define a Named block with no pairs.");

        if (has_mask) {
          assert(block_type == eBlockType::Pair || !"Can't contain a mask if emitting for an GroupIfNamed/Else");
          ec.encode_int(mask, ec.scope_bitmask());
          ec.set_scope_bitmask(mask);
        }
        return encode_block(ec, block_type,
          [&](auto& new_ec) {
            new_ec.template item<Pairs<E>>(pairs_id).encode(new_ec);
          }) ;
      }
    };

    template <typename E>
    struct Numeric {
      E mask{};
      eEnumCommand format{};
      string_id_t name_id{};

      /**
       * @brief Encodes this numeric payload body.
       *
       * @tparam EnumT - Referenced enum representation type.
       * @param ec - Active encoder for the current scope.
       */
      template <typename EnumT, typename WriterT>
      constexpr void encode(EnumEncoder<EnumT, WriterT>& ec) const {
        ec.encode_int(eEnumCommand::Numeric | format);
        ec.verify_scope_bitmask(mask);
        ec.encode_int(mask, ec.scope_bitmask());
        ec.encode_string(name_id);
      }

      /**
       * @brief Encodes only the inline name payload for a GroupIfNumeric branch.
       *
       * @tparam EnumT - Referenced enum representation type.
       * @param ec - Active encoder for the current scope.
       */
      template <typename EnumT, typename WriterT>
      constexpr eEnumCommand encode_inline_body(EnumEncoder<EnumT, WriterT>& ec) const {
        assert(mask == ec.scope_bitmask() || !"Inline numeric branch must use the conditional scope bitmask.");
        ec.encode_string(name_id);
        return format;
      }
    };

    template <typename E>
    struct Cmds;

    template <typename E>
    struct Group {
      string_id_t name_id{};
      item_id_t cmds_id{};   // Type: Cmds<E> MUST BE SET OR NO Group SHALL EXISTS!

      template <typename EnumT>
      constexpr bool has_only_one_Numeric (EnumT const& enum_def) const {
        assert(cmds_id || !"Invalid Group!");
        auto& commands { enum_def.template item<Cmds<E>>(cmds_id) };
        if (!commands.next_id) {
          if (auto* pNumeric = enum_def.template item_if<Numeric<E>>(commands.command_id)) {
            return true;
          }
        }
        return false;
      }

      template <typename EnumT>
      constexpr bool has_only_one_Named_with_no_bitmask (EnumT const& enum_def) const {
        assert(cmds_id || !"Invalid Group!");
        auto& commands { enum_def.template item<Cmds<E>>(cmds_id) };
        if (!commands.next_id) {
          if (auto* pNamed = enum_def.template item_if<Named<E>>(commands.command_id)) {
            return !pNamed->has_mask;
          }
        }
        return false;
      }
    };

    template <typename E>
    struct Conditional {
      E group_bitmask{};
      E bitmask{};
      item_id_t true_group_id{};  // Type: Group<E> AT LEAST ONE MUST BE SPECIFIED, OR NO CONDITIONAL SHALL EXIST!
      item_id_t false_group_id{}; // Type: Group<E> AT LEAST ONE MUST BE SPECIFIED, OR NO CONDITIONAL SHALL EXIST!

      /**
       * @brief Encodes this conditional block using the selected if/else wire forms.
       *
       * @tparam EnumT - Referenced enum representation type.
       * @param ec - Active encoder for the current scope.
       */
      template <typename EnumT, typename WriterT>
      constexpr void encode(EnumEncoder<EnumT, WriterT>& ec) const {
        assert(true_group_id || false_group_id || !"Invalid Conditional!");

        auto const& enum_def{ ec.enum_def() };

        auto const* true_group{
          true_group_id ? &enum_def.template item<Group<E>>(true_group_id) : nullptr
        };
        auto const* false_group{
          false_group_id ? &enum_def.template item<Group<E>>(false_group_id) : nullptr
        };
        auto const* if_group{ true_group };
        auto const* else_group{ false_group };

        auto if_opcode{ eEnumCommand::GroupIf };
        if (true_group && true_group->has_only_one_Numeric(enum_def)) {
          if_group = true_group;
          else_group = false_group;
          if_opcode = eEnumCommand::GroupIfNumeric;
        } else if (false_group && false_group->has_only_one_Numeric(enum_def)) {
          if_group = false_group;
          else_group = true_group;
          if_opcode = (eEnumCommand::GroupIfNumeric | eEnumCommand::fNegate);
        } else if (true_group && true_group->has_only_one_Named_with_no_bitmask(enum_def)) {
          if_group = true_group;
          else_group = false_group;
          if_opcode = eEnumCommand::GroupIfNamed;
        }

        assert(if_group || if_opcode == eEnumCommand::GroupIf || !"Conditional requires a true group unless the false group can inline as negated numeric.");

        // A GroupIfNamed branch still stores its payload as Group -> Cmds -> Named.
        // This helper unwraps that single command after the branch-shape checks above
        // have already proven the group is exactly one unmasked Named command.
        auto const inline_named_command = [&](Group<E> const& branch_group) -> Named<E> const& {
          auto const& cmds{ enum_def.template item<Cmds<E>>(branch_group.cmds_id) };
          auto const* named{ enum_def.template item_if<Named<E>>(cmds.command_id) };
          assert(named || !"Inline named group must reference a Named command.");
          assert(!named->has_mask || !"Inline named group must be unmasked.");
          return *named;
        };

        // A GroupIfNumeric branch is stored the same way: Group -> Cmds -> Numeric.
        // This helper unwraps that one Numeric command so encode() can emit it inline
        // instead of treating the branch as a command block.
        auto const inline_numeric_command = [&](Group<E> const& branch_group) -> Numeric<E> const& {
          auto const& cmds{ enum_def.template item<Cmds<E>>(branch_group.cmds_id) };
          auto const* numeric{ enum_def.template item_if<Numeric<E>>(cmds.command_id) };
          assert(numeric || !"Inline numeric group must reference a Numeric command.");
          return *numeric;
        };

        auto if_opcode_with_flags {
          if_group && if_group->name_id
          ? if_opcode | eEnumCommand::fHasGroupName
          : if_opcode
        };


        // EMITTING IF AND MASKS
        auto const if_pc{ ec.reserve_byte() };
        ec.encode_byte(least_set_bit_index(group_bitmask));
        assert(count_bits_set(group_bitmask) == 1 || !"group_bitmask must only set one bit.");
        ec.encode_int(bitmask, ec.scope_bitmask());
        if (if_group && if_group->name_id) {
          ec.encode_string(if_group->name_id);
        }


        // EMITTING IF BLOCK
        auto if_ec { ec };
        if_ec.set_scope_bitmask(bitmask);

        if (if_opcode == eEnumCommand::GroupIf) {
          size_t const stored {
            encode_block(if_ec, eBlockType::IfCmd,
              [&](auto& child_ec) {
                if (if_group) {
                  child_ec.template item<Cmds<E>>(if_group->cmds_id).encode(child_ec);
                }
              })
            };
          ec.or_byte_at(if_pc, stored);
        } else if (if_opcode == eEnumCommand::GroupIfNamed) {
          size_t const stored { inline_named_command(*if_group).encode_body(if_ec, eBlockType::IfPair) };
          ec.or_byte_at(if_pc, stored);
        } else {
          eEnumCommand const format { inline_numeric_command(*if_group).encode_inline_body(if_ec) };
          if_opcode_with_flags |= format;
        }
        ec.or_byte_at(if_pc, if_opcode_with_flags);


        // EMITTING ELSE BLOCK
        if (!else_group) {
          return;
        }

        auto else_opcode{ eEnumCommand::Else };
        auto const else_pc{ ec.reserve_byte() };

        if (else_group->name_id) {
          ec.encode_string(else_group->name_id);
          else_opcode |= eEnumCommand::fHasGroupName;
        }

        auto else_ec { ec };
        else_ec.set_scope_bitmask(bitmask);

        if (else_group->has_only_one_Named_with_no_bitmask(enum_def)) {
          size_t const stored {
            inline_named_command(*else_group).encode_body(else_ec, eBlockType::ElsePair)
          };
          ec.or_byte_at(else_pc, stored);
        } else {
          else_opcode |= eEnumCommand::fElseCmds;
          size_t const stored {
            encode_block(else_ec, eBlockType::ElseCmd,
              [&](auto& child_ec) {
                child_ec.template item<Cmds<E>>(else_group->cmds_id).encode(child_ec);
              })
          };
          ec.or_byte_at(else_pc, stored);
        }
        ec.or_byte_at(else_pc, else_opcode);
      }
    };

    template <typename E>
    struct Cmds {
      item_id_t command_id{};
      item_id_t next_id{};

      private:
      /**
       * @brief Encodes the current stored command node.
       *
       * @tparam EnumT - Referenced enum representation type.
       * @param ec - Active encoder for the current scope.
       */
      template <typename EnumT, typename WriterT>
      constexpr void encode_current(EnumEncoder<EnumT, WriterT>& ec) const {
        // TODO: Make this into ec.template item_if call
        auto const dispatch_current = overload{
          [&ec](Named<E> const&       cmd) { cmd.encode(ec); },
          [&ec](Numeric<E> const&     cmd) { cmd.encode(ec); },
          [&ec](Conditional<E> const& cmd) { cmd.encode(ec); },
          [](auto const&) { assert(false && !"Command list must reference a command item."); },
        };
        std::visit(dispatch_current, ec.item(command_id));
      }

    public:
      /**
       * @brief Encodes this command-list node and any following sibling commands.
       *
       * @tparam EnumT - Referenced enum representation type.
       * @param ec - Active encoder for the current scope.
       */
      template <typename EnumT, typename WriterT>
      constexpr void encode(EnumEncoder<EnumT, WriterT>& ec) const {
        if (ec.remaining_items_allowed_in_block() < 0) {
          encode_current(ec);
        } else if (ec.remaining_items_allowed_in_block() > 0) {
          ec.remaining_items_allowed_in_block() -= 1;
          encode_current(ec);
        } else {
          auto const pc{ ec.reserve_byte() };
          size_t stored {
            encode_block(ec, eBlockType::ContCmd, [&](auto& new_ec) { encode(new_ec); }) };
          ec.or_byte_at(pc, eEnumCommand::ContinueScope);
          ec.or_byte_at(pc, stored);
          return;
        }

        if (next_id) {
          ec.template item<Cmds<E>>(next_id).encode(ec);
        }
      }
    };

    /**
     * @brief Finds the index of the most significant set bit.
     *
     * @tparam E - Enum or integral type of \p mask.
     * @param mask - Mask value with at least one set bit.
     * @return std::size_t - Zero-based bit index of the highest set bit.
     */
    template <typename E>
    constexpr std::size_t most_set_bit_index(E mask) {
      auto mask_value{ make_unsigned_equivalent(mask) };
      assert(mask_value || !"mask must not be 0.");
      std::size_t index{};
      while (mask_value >>= 1u) {
        ++index;
      }
      return index;
    }

    /**
     * @brief Counts how many bits are set in a mask.
     *
     * @tparam E - Enum or integral type of \p mask.
     * @param mask - Mask value.
     * @return std::size_t - Number of set bits in \p mask.
     */
    template <typename E>
    constexpr std::size_t set_bit_count(E mask) {
      auto mask_value{ make_unsigned_equivalent(mask) };
      std::size_t count{};
      while (mask_value) {
        count += static_cast<std::size_t>(mask_value & 1u);
        mask_value >>= 1u;
      }
      return count;
    }

    /**
     * @brief Sign-extends a bit field whose sign bit index is already known.
     *
     * @tparam UInt - Unsigned integer type holding the bit field.
     * @param value - Unsigned value to extend.
     * @param sign_bit_index - Zero-based position of the sign bit.
     * @return std::make_signed_t<UInt> - Sign-extended signed result.
     */
    template <typename UInt>
    constexpr std::make_signed_t<UInt> sign_extend(UInt value, std::size_t sign_bit_index) {
      static_assert(std::is_unsigned_v<UInt>, "sign_extend requires an unsigned integer type.");

      std::size_t const digits{ std::numeric_limits<UInt>::digits };
      std::size_t const width{ sign_bit_index + 1u };
      UInt const sign_bit{ static_cast<UInt>(UInt{ 1u } << sign_bit_index) };
      if ((value & sign_bit) == 0u) {
        return static_cast<std::make_signed_t<UInt>>(value);
      }

      UInt const width_mask{
        width >= digits
          ? static_cast<UInt>(~UInt{})
          : static_cast<UInt>((UInt{ 1u } << width) - 1u)
      };
      return static_cast<std::make_signed_t<UInt>>(value | static_cast<UInt>(~width_mask));
    }

    /**
     * @brief Renders one enum description plus runtime value into output text.
     *
     * @tparam EnumT - Immutable enum description type.
     */
    template <typename EnumT>
    class EnumTextRenderer {
      using value_type = typename EnumT::value_type;
      using unsigned_value_type = unsigned_equivalent_t<value_type>;

      EnumT const* m_enum{};
      value_type m_value{};
      std::string m_nonzero_items{};
      std::string m_zero_items{};
      std::string m_numeric_items{};
      bool m_has_nonzero_items{};
      bool m_has_zero_items{};
      bool m_has_numeric_items{};

      /**
       * @brief Appends one delimited item into a text bucket.
       *
       * @param bucket - Destination bucket text.
       * @param has_items - Whether the bucket already has earlier items.
       * @param separator - Separator for every item after the first.
       * @param item - Item text to append.
       */
      static void append_bucket_item(
        std::string& bucket,
        bool& has_items,
        std::string_view separator,
        std::string_view item)
      {
        if (has_items) {
          bucket.append(separator.data(), separator.size());
        }
        bucket.append(item.data(), item.size());
        has_items = true;
      }

      /**
       * @brief Returns the root-scope bitmask with every bit enabled.
       *
       * @return value_type - All-bits-set scope bitmask for \c value_type.
       */
      static constexpr value_type full_scope_bitmask() noexcept {
        return static_cast<value_type>(static_cast<unsigned_value_type>(~unsigned_value_type{}));
      }

      /**
       * @brief Appends one named output to the zero or non-zero bucket.
       *
       * @param name - Display name to append.
       * @param is_zero_value - Whether the matching pair stored enum value 0.
       */
      void append_named_item(std::string_view name, bool is_zero_value) {
        if (is_zero_value) {
          append_bucket_item(m_zero_items, m_has_zero_items, ", ", name);
          return;
        }
        append_bucket_item(m_nonzero_items, m_has_nonzero_items, " | ", name);
      }

      /**
       * @brief Appends one numeric output item.
       *
       * @param name - Numeric field label.
       * @param rendered_value - Already formatted numeric value text.
       */
      void append_numeric_item(std::string_view name, std::string const& rendered_value) {
        std::string item{};
        item.reserve(name.size() + 1u + rendered_value.size());
        item.append(name.data(), name.size());
        item.push_back('=');
        item.append(rendered_value);
        append_bucket_item(m_numeric_items, m_has_numeric_items, ", ", item);
      }

      /**
       * @brief Formats one numeric command using the documented extraction flags.
       *
       * @param numeric - Numeric command to evaluate.
       * @return std::string - Decimal text for the extracted numeric value.
       */
      std::string format_numeric_value(Numeric<value_type> const& numeric) const {
        unsigned_value_type const raw_value{ make_unsigned_equivalent(m_value) };
        unsigned_value_type const mask{ make_unsigned_equivalent(numeric.mask) };
        unsigned_value_type const masked_value{ static_cast<unsigned_value_type>(raw_value & mask) };
        bool const packed_bits{ (numeric.format & eEnumCommand::fPackedBits) != 0 };
        bool const shifted_bits{ (numeric.format & eEnumCommand::fRightShiftBits) != 0 };
        bool const is_signed{ (numeric.format & eEnumCommand::fIsSigned) != 0 };

        unsigned_value_type rendered_value{ masked_value };
        std::size_t sign_bit_index{ most_set_bit_index(numeric.mask) };

        if (packed_bits) {
          rendered_value = make_unsigned_equivalent(condense(numeric.mask, m_value, true));
          sign_bit_index = set_bit_count(numeric.mask) - 1u;
        } else if (shifted_bits) {
          std::size_t const least_bit{ least_set_bit_index(numeric.mask) };
          rendered_value = static_cast<unsigned_value_type>(masked_value >> least_bit);
          sign_bit_index -= least_bit;
        }

        if (is_signed) {
          auto const signed_value{ sign_extend(rendered_value, sign_bit_index) };
          return std::to_string(static_cast<long long>(signed_value));
        }

        return std::to_string(static_cast<unsigned long long>(rendered_value));
      }

      /**
       * @brief Renders one named command against the active scope bitmask.
       *
       * @param named - Named command to evaluate.
       * @param scope_bitmask - Active scope bitmask for the current command list.
       */
      void render_named(Named<value_type> const& named, value_type scope_bitmask) {
        auto const effective_bitmask{
          named.has_mask
            ? make_unsigned_equivalent(named.mask)
            : make_unsigned_equivalent(scope_bitmask)
        };
        auto const current_value{ make_unsigned_equivalent(m_value) };

        for (item_id_t pair_id{ named.pairs_id }; pair_id != 0u;) {
          auto const& pair{ m_enum->template item<Pairs<value_type>>(pair_id) };
          if ((current_value & effective_bitmask) == make_unsigned_equivalent(pair.value)) {
            append_named_item(m_enum->get_string(pair.name_id), pair.value == static_cast<value_type>(0));
          }
          pair_id = pair.next_pairs_id;
        }
      }

      /**
       * @brief Renders one numeric command.
       *
       * @param numeric - Numeric command to evaluate.
       */
      void render_numeric(Numeric<value_type> const& numeric) {
        append_numeric_item(m_enum->get_string(numeric.name_id), format_numeric_value(numeric));
      }

      /**
       * @brief Renders one conditional command.
       *
       * @param conditional - Conditional command to evaluate.
       * @param scope_bitmask - Parent scope bitmask. Present for signature symmetry.
       */
      void render_conditional(Conditional<value_type> const& conditional, value_type scope_bitmask) {
        (void)scope_bitmask;

        bool const condition_met{
          (make_unsigned_equivalent(m_value) & make_unsigned_equivalent(conditional.group_bitmask)) != 0u
        };
        if (condition_met) {
          if (conditional.true_group_id) {
            auto const& group{ m_enum->template item<Group<value_type>>(conditional.true_group_id) };
            render_cmds(group.cmds_id, conditional.bitmask);
          }
          return;
        }

        if (conditional.false_group_id) {
          auto const& group{ m_enum->template item<Group<value_type>>(conditional.false_group_id) };
          render_cmds(group.cmds_id, conditional.bitmask);
        }
      }

      /**
       * @brief Renders one linked list of stored command nodes.
       *
       * @param cmds_id - First command-list node id.
       * @param scope_bitmask - Active scope bitmask for that command list.
       */
      void render_cmds(item_id_t cmds_id, value_type scope_bitmask) {
        for (item_id_t current_cmds_id{ cmds_id }; current_cmds_id != 0u;) {
          auto const& cmds{ m_enum->template item<Cmds<value_type>>(current_cmds_id) };
          if (auto const* named{ m_enum->template item_if<Named<value_type>>(cmds.command_id) }) {
            render_named(*named, scope_bitmask);
          } else if (auto const* numeric{ m_enum->template item_if<Numeric<value_type>>(cmds.command_id) }) {
            render_numeric(*numeric);
          } else {
            auto const* conditional{ m_enum->template item_if<Conditional<value_type>>(cmds.command_id) };
            assert(conditional || !"Cmds must reference Named, Numeric or Conditional.");
            render_conditional(*conditional, scope_bitmask);
          }
          current_cmds_id = cmds.next_id;
        }
      }

      /**
       * @brief Combines the named and numeric buckets into final output text.
       *
       * @return std::string - Final rendered text.
       */
      std::string combine_output() const {
        std::string result{ m_nonzero_items };
        bool has_output{ !result.empty() };

        if (m_has_zero_items) {
          if (has_output) {
            result.append(", ");
          }
          result.append(m_zero_items);
          has_output = true;
        }

        if (m_has_numeric_items) {
          if (has_output) {
            result.append(", ");
          }
          result.append(m_numeric_items);
        }

        return result;
      }

    public:
      /**
       * @brief Constructs a renderer for one enum description and runtime value.
       *
       * @param enum_def - Immutable enum description.
       * @param value - Runtime value to interpret.
       */
      constexpr EnumTextRenderer(EnumT const& enum_def, value_type value) noexcept
      : m_enum(&enum_def)
      , m_value(value)
      {
      }

      /**
       * @brief Renders the full enum description into display text.
       *
       * @return std::string - Final rendered text.
       */
      std::string render() {
        if (m_enum->cmds_id()) {
          render_cmds(m_enum->cmds_id(), full_scope_bitmask());
        }
        return combine_output();
      }
    };

    /*
     * The builder was designed by me but made by Codex.  It's been a while
     * since I've done CRTP coding.
     *
     * This is how the class hierarchy works:
     *
     *   Constexpr::EnumBuilder<Settings>
     *     owns:
     *       Enum<Settings>
     *       CommandScopeState<Value>
     *     inherits:
     *       CommandScopeFacade<EnumBuilder<Settings>>
     *
     *   CommandScopeFacade<Derived>
     *     provides fluent ops:
     *       Named
     *       Numeric
     *       If / IfNot
     *     calls back into Derived for:
     *       enum_ref()
     *       command_state()
     *       on_first_command()
     *       begin_if_impl()
     *
     *   IfScope<Parent>
     *     owns:
     *       Parent
     *       conditional_id
     *       group_id
     *       CommandScopeState<Value>
     *     inherits:
     *       CommandScopeFacade<IfScope<Parent>>
     *     extra ops:
     *       Else(...)
     *       End() -> Parent
     *
     *   ElseScope<Parent>
     *     owns:
     *       Parent
     *       group_id
     *       CommandScopeState<Value>
     *     inherits:
     *       CommandScopeFacade<ElseScope<Parent>>
     *     extra ops:
     *       End() -> Parent
     *
     * Code flow:
     *
     *   Named(value, "x")
     *     -> ensure/create impl::Named
     *     -> append impl::Pairs
     *     -> ensure root/branch impl::Cmds exists
     *
     *   Number(mask, "bits")
     *     -> create impl::Numeric
     *     -> append impl::Cmds
     *
     *   If / IfNot
     *     -> create impl::Conditional
     *     -> create first impl::Group
     *     -> return IfScope<Parent>
     *
     *   Else
     *     -> create second impl::Group
     *     -> return ElseScope<Parent>
     *
     *   End
     *     -> return exact Parent type
     *
     *   Build
     *     -> return Enum<Settings>
     *     -> set root cmds_id
     */

    /**
     * @brief Tracks the currently open implicit named command inside one
     *   command scope while building an enum graph.
     *
     * @tparam E - Enum or integer value type.
     */
    template <typename E>
    struct ImplicitNamedState {
      item_id_t named_id{};
      item_id_t last_pair_id{};
      bool has_mask{};
      E mask{};
    };

    /**
     * @brief Tracks the mutable list-building state for one command scope.
     *
     * @tparam E - Enum or integer value type.
     */
    template <typename E>
    struct CommandScopeState {
      item_id_t first_cmd_id{};
      item_id_t last_cmd_id{};
      ImplicitNamedState<E> implicit_named{};
    };

    /**
     * @brief Shared typed-chaining operations for builder command scopes.
     *
     * @tparam Derived - Concrete command-scope type.
     */
    template <typename Derived>
    class CommandScopeFacade {
    public:
      /**
       * @brief Add one named value that uses the current scope bitmask.
       *
       * @param value - Enum value matched by the pair.
       * @param name - Display name for the pair.
       * @return Derived& - Updated command scope.
       */
      template <typename D = Derived>
      constexpr Derived& Named(typename D::value_type value, std::string_view name) {
        derived().append_named_pair_impl(false, typename D::value_type{}, value, name);
        return derived();
      }

      /**
       * @brief Add one named value under an explicit command-local bitmask.
       *
       * @param value - Masked enum value matched by the pair.
       * @param name - Display name for the pair.
       * @param bitmask - Command-local bitmask for the named block.
       * @return Derived& - This scope, mutated in-place.
       */
      template <typename D = Derived>
      constexpr Derived& Named(
        typename D::value_type value,
        std::string_view name,
        typename D::value_type bitmask)
      {
        derived().append_named_pair_impl(true, bitmask, value, name);
        return derived();
      }

      /**
       * @brief Add one numeric command to the current command scope.
       *
       * @param bitmask - Bitmask selecting the numeric field.
       * @param name - Display label for the numeric field.
       * @param format - Numeric formatting flags.
       * @return Derived& - This scope, mutated in-place.
       */
      template <typename D = Derived>
      constexpr Derived& Numeric(
        typename D::value_type bitmask,
        std::string_view name,
        eEnumCommand format = eEnumCommand{})
      {
        derived().append_numeric_impl(bitmask, name, format);
        return derived();
      }

      /**
       * @brief Start a conditional scope for the selected group bit.
       *
       * @param group_bitmask - Single-bit selector for the branch.
       * @param scope_bitmask - Scope bitmask to apply inside the branch.
       * @return IfScope<Derived> - Builder state for the if branch.
       */
      template <typename D = Derived>
      constexpr auto If(
        typename D::value_type group_bitmask,
        typename D::value_type scope_bitmask)
      {
        return derived().begin_if_impl(false, group_bitmask, scope_bitmask, {}, false);
      }

      /**
       * @brief Start a named conditional scope for the selected group bit.
       *
       * @param group_bitmask - Single-bit selector for the branch.
       * @param scope_bitmask - Scope bitmask to apply inside the branch.
       * @param group_name - Group label for the if branch.
       * @return IfScope<Derived> - Builder state for the if branch.
       */
      template <typename D = Derived>
      constexpr auto If(
        typename D::value_type group_bitmask,
        typename D::value_type scope_bitmask,
        std::string_view group_name)
      {
        return derived().begin_if_impl(false, group_bitmask, scope_bitmask, group_name, true);
      }

      /**
       * @brief Start a conditional scope whose first user-authored branch is the
       *   false case.
       *
       * @param group_bitmask - Single-bit selector for the branch.
       * @param scope_bitmask - Scope bitmask to apply inside the branch.
       * @return IfScope<Derived> - Builder state for the first user-authored branch.
       */
      template <typename D = Derived>
      constexpr auto IfNot(
        typename D::value_type group_bitmask,
        typename D::value_type scope_bitmask)
      {
        return derived().begin_if_impl(true, group_bitmask, scope_bitmask, {}, false);
      }

      /**
       * @brief Start a named conditional scope whose first user-authored branch
       *   is the false case.
       *
       * @param group_bitmask - Single-bit selector for the branch.
       * @param scope_bitmask - Scope bitmask to apply inside the branch.
       * @param group_name - Group label for the first user-authored branch.
       * @return IfScope<Derived> - Builder state for the first user-authored branch.
       */
      template <typename D = Derived>
      constexpr auto IfNot(
        typename D::value_type group_bitmask,
        typename D::value_type scope_bitmask,
        std::string_view group_name)
      {
        return derived().begin_if_impl(true, group_bitmask, scope_bitmask, group_name, true);
      }

    protected:
      /**
       * @brief Returns the concrete command-scope object.
       *
       * @return Derived& - Concrete scope reference.
       */
      constexpr Derived& derived() noexcept {
        return static_cast<Derived&>(*this);
      }

      /**
       * @brief Returns the concrete command-scope object (const overload).
       *
       * @return Derived const& - Concrete scope reference.
       */
      constexpr Derived const& derived() const noexcept {
        return static_cast<Derived const&>(*this);
      }

      /**
       * @brief Clears any open implicit named target in the command scope.
       *
       * @param scope - Command scope being updated.
       */
      template <typename D = Derived>
      static constexpr void clear_implicit_named(D& scope) {
        scope.command_state().implicit_named = ImplicitNamedState<typename D::value_type>{};
      }

      /**
       * @brief Verifies that one named block does not reuse the same masked value.
       *
       * @param scope - Command scope being updated.
       * @param named_id - Stored named-command id being extended.
       * @param value - Candidate masked enum value for the new pair.
       */
      template <typename D = Derived>
      static constexpr void verify_unique_named_value(
        D& scope,
        item_id_t named_id,
        [[maybe_unused]] typename D::value_type value)
      {
        #ifndef NDEBUG
        auto& enum_def{ scope.enum_ref() };
        auto const& named{ enum_def.template item<Constexpr::impl::Named<typename D::value_type>>(named_id) };

        for (item_id_t pair_id{ named.pairs_id }; pair_id != 0u;) {
          auto const& pair{ enum_def.template item<Pairs<typename D::value_type>>(pair_id) };
          assert(pair.value != value || !"Named command cannot reuse the same masked enum value.");
          pair_id = pair.next_pairs_id;
        }
        #endif // NDEBUG
      }

      /**
       * @brief Append one command-list node to the active command scope.
       *
       * @param scope - Command scope being updated.
       * @param command_id - Stored command item id referenced by the new node.
       * @return item_id_t - Stored `Cmds` node id.
       */
      template <typename D = Derived>
      static constexpr item_id_t append_command_node(D& scope, item_id_t command_id) {
        auto& enum_def{ scope.enum_ref() };
        auto& state{ scope.command_state() };
        item_id_t const cmds_id{ enum_def.add_item(Cmds<typename D::value_type>{ command_id, {} }) };

        if (!state.first_cmd_id) {
          state.first_cmd_id = cmds_id;
          scope.on_first_command(cmds_id);
        } else {
          enum_def.template item<Cmds<typename D::value_type>>(state.last_cmd_id).next_id = cmds_id;
        }

        state.last_cmd_id = cmds_id;
        return cmds_id;
      }

      /**
       * @brief Ensure that a matching implicit named command exists in the
       *   current command scope.
       *
       * @param scope - Command scope being updated.
       * @param has_mask - Whether the named command uses a command-local bitmask.
       * @param bitmask - Command-local bitmask when `has_mask` is true.
       * @return item_id_t - Stored named-command id.
       */
      template <typename D = Derived>
      static constexpr item_id_t ensure_named_target(
        D& scope,
        bool has_mask,
        typename D::value_type bitmask)
      {
        auto& state{ scope.command_state() };
        auto& implicit{ state.implicit_named };

        if (implicit.named_id) {
          if (implicit.has_mask == has_mask && (!has_mask || implicit.mask == bitmask)) {
            return implicit.named_id;
          }
          clear_implicit_named(scope);
        }

        auto& enum_def{ scope.enum_ref() };
        item_id_t const named_id{
          enum_def.add_item(Constexpr::impl::Named<typename D::value_type>{ has_mask, bitmask, {} })
        };
        append_command_node(scope, named_id);

        implicit.named_id = named_id;
        implicit.last_pair_id = {};
        implicit.has_mask = has_mask;
        implicit.mask = bitmask;
        return named_id;
      }

      /**
       * @brief Append one pair to the implicit named target, creating it when
       *   needed.
       *
       * @param scope - Command scope being updated.
       * @param has_mask - Whether the named command uses a command-local bitmask.
       * @param bitmask - Command-local bitmask when `has_mask` is true.
       * @param value - Enum value matched by the new pair.
       * @param name - Display name for the pair.
       */
      template <typename D = Derived>
      static constexpr void append_named_pair(
        D& scope,
        bool has_mask,
        typename D::value_type bitmask,
        typename D::value_type value,
        std::string_view name)
      {
        auto& enum_def{ scope.enum_ref() };
        auto& implicit{ scope.command_state().implicit_named };
        item_id_t const named_id{ ensure_named_target(scope, has_mask, bitmask) };
        verify_unique_named_value(scope, named_id, value);
        string_id_t const name_id{ enum_def.add_string(name) };
        item_id_t const pair_id{ enum_def.add_item(Pairs<typename D::value_type>{ value, name_id, {} }) };

        auto& named{ enum_def.template item<Constexpr::impl::Named<typename D::value_type>>(named_id) };
        if (!named.pairs_id) {
          named.pairs_id = pair_id;
        } else {
          enum_def.template item<Pairs<typename D::value_type>>(implicit.last_pair_id).next_pairs_id = pair_id;
        }

        implicit.last_pair_id = pair_id;
      }

      /**
       * @brief Append one numeric command to the active command scope.
       *
       * @param scope - Command scope being updated.
       * @param bitmask - Bitmask selecting the numeric field.
       * @param name - Display label for the numeric field.
       * @param format - Numeric formatting flags.
       */
      template <typename D = Derived>
      static constexpr void append_numeric(
        D& scope,
        typename D::value_type bitmask,
        std::string_view name,
        eEnumCommand format)
      {
        clear_implicit_named(scope);

        auto& enum_def{ scope.enum_ref() };
        string_id_t const name_id{ enum_def.add_string(name) };
        item_id_t const numeric_id{
          enum_def.add_item(Constexpr::impl::Numeric<typename D::value_type>{ bitmask, format, name_id })
        };
        append_command_node(scope, numeric_id);
      }

      /**
       * @brief Start one conditional branch scope and append the owning
       *   conditional command to the current scope.
       *
       * @param scope - Command scope being updated.
       * @param negate_first - Whether the first user-authored branch is the
       *   false branch.
       * @param group_bitmask - Single-bit selector for the conditional.
       * @param scope_bitmask - Scope bitmask applied inside the branch.
       * @param group_name - Optional group label for the first branch.
       * @param has_group_name - Whether `group_name` should be stored.
       * @return IfScope<Derived> - Builder state for the new branch.
       */
      template <typename D = Derived>
      static constexpr IfScope<D> begin_if_scope(
        D& scope,
        bool negate_first,
        typename D::value_type group_bitmask,
        typename D::value_type scope_bitmask,
        std::string_view group_name,
        bool has_group_name)
      {
        clear_implicit_named(scope);

        auto& enum_def{ scope.enum_ref() };
        string_id_t const group_name_id{
          has_group_name ? enum_def.add_string(group_name) : string_id_t{}
        };
        item_id_t const group_id{ enum_def.add_item(Group<typename D::value_type>{ group_name_id, {} }) };

        Conditional<typename D::value_type> conditional{};
        conditional.group_bitmask = group_bitmask;
        conditional.bitmask = scope_bitmask;
        if (negate_first) {
          conditional.false_group_id = group_id;
        } else {
          conditional.true_group_id = group_id;
        }

        item_id_t const conditional_id{ enum_def.add_item(conditional) };
        append_command_node(scope, conditional_id);
        return IfScope<D>{ scope, conditional_id, group_id };
      }
    };

    /**
     * @brief Builder state for the if branch of a conditional.
     *
     * @tparam Parent - Immediate parent builder scope.
     */
    template <typename Parent>
    class IfScope : public CommandScopeFacade<IfScope<Parent>> {
      Parent& m_parent;
      item_id_t m_conditional_id{};
      item_id_t m_group_id{};
      CommandScopeState<typename Parent::value_type> m_state{};

      template <typename Derived>
      friend class CommandScopeFacade;
      template <typename OtherParent>
      friend class IfScope;
      template <typename OtherParent>
      friend class ElseScope;

      /**
       * @brief Returns mutable access to the shared enum under construction.
       *
       * @return typename Parent::enum_type& - Shared mutable enum representation.
       */
      constexpr typename Parent::enum_type& enum_ref() noexcept {
        return m_parent.enum_ref();
      }

      /**
       * @brief Returns mutable access to this branch's command-scope state.
       *
       * @return CommandScopeState<typename Parent::value_type>& - Mutable scope state.
       */
      constexpr CommandScopeState<typename Parent::value_type>& command_state() noexcept {
        return m_state;
      }

      /**
       * @brief Records the first command node stored in this branch.
       *
       * @param cmds_id - First stored `Cmds` node id for the branch.
       */
      constexpr void on_first_command(item_id_t cmds_id) {
        enum_ref().template item<Group<typename Parent::value_type>>(m_group_id).cmds_id = cmds_id;
      }

      /**
       * @brief Append one named pair to this branch.
       *
       * @param has_mask - Whether the named command uses a command-local bitmask.
       * @param bitmask - Command-local bitmask when `has_mask` is true.
       * @param value - Enum value matched by the pair.
       * @param name - Display name for the pair.
       */
      constexpr void append_named_pair_impl(
        bool has_mask,
        typename Parent::value_type bitmask,
        typename Parent::value_type value,
        std::string_view name)
      {
        CommandScopeFacade<IfScope<Parent>>::append_named_pair(*this, has_mask, bitmask, value, name);
      }

      /**
       * @brief Append one numeric command to this branch.
       *
       * @param bitmask - Bitmask selecting the numeric field.
       * @param name - Display label for the numeric field.
       * @param format - Numeric formatting flags.
       */
      constexpr void append_numeric_impl(
        typename Parent::value_type bitmask,
        std::string_view name,
        eEnumCommand format)
      {
        CommandScopeFacade<IfScope<Parent>>::append_numeric(*this, bitmask, name, format);
      }

      /**
       * @brief Start one nested conditional scope inside this branch.
       *
       * @param negate_first - Whether the first user-authored branch is the
       *   false branch.
       * @param group_bitmask - Single-bit selector for the conditional.
       * @param scope_bitmask - Scope bitmask applied inside the branch.
       * @param group_name - Optional group label for the first branch.
       * @param has_group_name - Whether `group_name` should be stored.
       * @return IfScope<IfScope<Parent>> - Nested if-branch builder state.
       */
      constexpr auto begin_if_impl(
        bool negate_first,
        typename Parent::value_type group_bitmask,
        typename Parent::value_type scope_bitmask,
        std::string_view group_name,
        bool has_group_name)
      {
        return CommandScopeFacade<IfScope<Parent>>::begin_if_scope(
          *this, negate_first, group_bitmask, scope_bitmask, group_name, has_group_name);
      }

    public:
      using value_type = typename Parent::value_type;
      using enum_type = typename Parent::enum_type;
      using settings_type = typename Parent::settings_type;

      /**
       * @brief Construct one if-branch builder scope around its parent scope.
       *
       * @param parent - Parent scope snapshot to keep extending.
       * @param conditional_id - Stored conditional command id.
       * @param group_id - Stored group id for the active branch.
       */
      constexpr IfScope(Parent& parent, item_id_t conditional_id, item_id_t group_id) noexcept
      : m_parent{ parent }
      , m_conditional_id{ conditional_id }
      , m_group_id{ group_id }
      , m_state{}
      {
      }

      /**
       * @brief Switch this conditional from its if branch to its else branch.
       *
       * @return ElseScope<Parent> - Builder state for the else branch.
       */
      constexpr auto Else() {
        return make_else_scope_impl({}, false);
      }

      /**
       * @brief Switch this conditional from its if branch to a named else branch.
       *
       * @param group_name - Group label for the else branch.
       * @return ElseScope<Parent> - Builder state for the else branch.
       */
      constexpr auto Else(std::string_view group_name) {
        return make_else_scope_impl(group_name, true);
      }

      /**
       * @brief Finish this if scope and return to the exact parent scope type.
       *
       * @return Parent& - The parent scope, by reference.
       */
      constexpr Parent& End() {
        return finish_impl();
      }

    private:
      /**
       * @brief Returns whether the active branch has emitted at least one
       *   command node.
       *
       * @return bool - \c true when the active branch owns a command list.
       */
      constexpr bool branch_has_commands() noexcept {
        return enum_ref().template item<Group<typename Parent::value_type>>(m_group_id).cmds_id != 0u;
      }

      /**
       * @brief Rebinds an empty first branch so the same stored group becomes
       *   the else branch instead of leaving an empty if-side group behind.
       *
       * @param group_name - Optional group label for the else branch.
       * @param has_group_name - Whether `group_name` should be stored.
       * @return ElseScope<Parent> - Builder state for the normalized else branch.
       */
      constexpr ElseScope<Parent> reuse_empty_branch_as_else(
        std::string_view group_name,
        bool has_group_name)
      {
        auto& enum_def{ enum_ref() };
        auto& conditional{ enum_def.template item<Conditional<value_type>>(m_conditional_id) };
        auto& group{ enum_def.template item<Group<value_type>>(m_group_id) };

        group.name_id = has_group_name ? enum_def.add_string(group_name) : string_id_t{};

        if (conditional.true_group_id == m_group_id) {
          conditional.true_group_id = {};
          conditional.false_group_id = m_group_id;
        } else {
          assert(conditional.false_group_id == m_group_id || !"Active if branch must belong to the current conditional.");
          conditional.false_group_id = {};
          conditional.true_group_id = m_group_id;
        }

        return ElseScope<Parent>{ m_parent, m_conditional_id, m_group_id };
      }

      /**
       * @brief Finalizes this if scope while rejecting a fully empty leading
       *   branch that never transitioned into a non-empty else branch.
       *
       * @return Parent - Updated parent scope.
       */
      constexpr Parent& finish_impl() {
        assert(branch_has_commands() || !"Conditional first branches cannot be empty unless a later Else branch supplies the payload.");
        return m_parent;
      }

      /**
       * @brief Create the else-branch scope for this conditional.
       *
       * @param group_name - Optional group label for the else branch.
       * @param has_group_name - Whether `group_name` should be stored.
       * @return ElseScope<Parent> - Builder state for the else branch.
       */
      constexpr ElseScope<Parent> make_else_scope_impl(
        std::string_view group_name,
        bool has_group_name)
      {
        CommandScopeFacade<IfScope<Parent>>::clear_implicit_named(*this);

        if (!branch_has_commands()) {
          return reuse_empty_branch_as_else(group_name, has_group_name);
        }

        auto& enum_def{ enum_ref() };
        auto& conditional{ enum_def.template item<Conditional<value_type>>(m_conditional_id) };
        string_id_t const group_name_id{
          has_group_name ? enum_def.add_string(group_name) : string_id_t{}
        };
        item_id_t const group_id{ enum_def.add_item(Group<value_type>{ group_name_id, {} }) };

        if (conditional.true_group_id == m_group_id) {
          assert(!conditional.false_group_id || !"Else branch already exists.");
          conditional.false_group_id = group_id;
        } else {
          assert(conditional.false_group_id == m_group_id || !"Active if branch must belong to the current conditional.");
          assert(!conditional.true_group_id || !"Else branch already exists.");
          conditional.true_group_id = group_id;
        }

        return ElseScope<Parent>{ m_parent, m_conditional_id, group_id };
      }
    };

    /**
     * @brief Builder state for the else branch of a conditional.
     *
     * @tparam Parent - Immediate parent builder scope.
     */
    template <typename Parent>
    class ElseScope : public CommandScopeFacade<ElseScope<Parent>> {
      Parent& m_parent;
      item_id_t m_conditional_id{};
      item_id_t m_group_id{};
      CommandScopeState<typename Parent::value_type> m_state{};

      template <typename Derived>
      friend class CommandScopeFacade;
      template <typename OtherParent>
      friend class IfScope;
      template <typename OtherParent>
      friend class ElseScope;

      /**
       * @brief Returns mutable access to the shared enum under construction.
       *
       * @return typename Parent::enum_type& - Shared mutable enum representation.
       */
      constexpr typename Parent::enum_type& enum_ref() noexcept {
        return m_parent.enum_ref();
      }

      /**
       * @brief Returns mutable access to this branch's command-scope state.
       *
       * @return CommandScopeState<typename Parent::value_type>& - Mutable scope state.
       */
      constexpr CommandScopeState<typename Parent::value_type>& command_state() noexcept {
        return m_state;
      }

      /**
       * @brief Records the first command node stored in this branch.
       *
       * @param cmds_id - First stored `Cmds` node id for the branch.
       */
      constexpr void on_first_command(item_id_t cmds_id) {
        enum_ref().template item<Group<typename Parent::value_type>>(m_group_id).cmds_id = cmds_id;
      }

      /**
       * @brief Append one named pair to this branch.
       *
       * @param has_mask - Whether the named command uses a command-local bitmask.
       * @param bitmask - Command-local bitmask when `has_mask` is true.
       * @param value - Enum value matched by the pair.
       * @param name - Display name for the pair.
       */
      constexpr void append_named_pair_impl(
        bool has_mask,
        typename Parent::value_type bitmask,
        typename Parent::value_type value,
        std::string_view name)
      {
        CommandScopeFacade<ElseScope<Parent>>::append_named_pair(*this, has_mask, bitmask, value, name);
      }

      /**
       * @brief Append one numeric command to this branch.
       *
       * @param bitmask - Bitmask selecting the numeric field.
       * @param name - Display label for the numeric field.
       * @param format - Numeric formatting flags.
       */
      constexpr void append_numeric_impl(
        typename Parent::value_type bitmask,
        std::string_view name,
        eEnumCommand format)
      {
        CommandScopeFacade<ElseScope<Parent>>::append_numeric(*this, bitmask, name, format);
      }

      /**
       * @brief Start one nested conditional scope inside this branch.
       *
       * @param negate_first - Whether the first user-authored branch is the
       *   false branch.
       * @param group_bitmask - Single-bit selector for the conditional.
       * @param scope_bitmask - Scope bitmask applied inside the branch.
       * @param group_name - Optional group label for the first branch.
       * @param has_group_name - Whether `group_name` should be stored.
       * @return IfScope<ElseScope<Parent>> - Nested if-branch builder state.
       */
      constexpr auto begin_if_impl(
        bool negate_first,
        typename Parent::value_type group_bitmask,
        typename Parent::value_type scope_bitmask,
        std::string_view group_name,
        bool has_group_name)
      {
        return CommandScopeFacade<ElseScope<Parent>>::begin_if_scope(
          *this, negate_first, group_bitmask, scope_bitmask, group_name, has_group_name);
      }

    public:
      using value_type = typename Parent::value_type;
      using enum_type = typename Parent::enum_type;
      using settings_type = typename Parent::settings_type;

      /**
       * @brief Construct one else-branch builder scope around its parent scope.
       *
       * @param parent - Parent scope snapshot to keep extending.
       * @param conditional_id - Stored conditional command id.
       * @param group_id - Stored group id for the active else branch.
       */
      constexpr ElseScope(Parent& parent, item_id_t conditional_id, item_id_t group_id) noexcept
      : m_parent{ parent }
      , m_conditional_id{ conditional_id }
      , m_group_id{ group_id }
      , m_state{}
      {
      }

      /**
       * @brief Finish this else scope and return to the exact parent scope type.
       *
       * @return Parent& - The parent scope, by reference.
       */
      constexpr Parent& End() {
        return finish_impl();
      }

    private:
      /**
       * @brief Returns whether the active else branch has emitted at least one
       *   command node.
       *
       * @return bool - \c true when the active else branch owns a command list.
       */
      constexpr bool branch_has_commands() noexcept {
        return enum_ref().template item<Group<typename Parent::value_type>>(m_group_id).cmds_id != 0u;
      }

      /**
       * @brief Finalizes this else scope, dropping a trailing empty else branch
       *   or rejecting a fully empty conditional.
       *
       * @return Parent - Updated parent scope.
       */
      constexpr Parent& finish_impl() {
        if (branch_has_commands()) {
          return m_parent;
        }

        auto& enum_def{ enum_ref() };
        auto& conditional{ enum_def.template item<Conditional<value_type>>(m_conditional_id) };
        auto& group{ enum_def.template item<Group<value_type>>(m_group_id) };
        group.name_id = {};

        if (conditional.true_group_id == m_group_id) {
          conditional.true_group_id = {};
        } else {
          assert(conditional.false_group_id == m_group_id || !"Active else branch must belong to the current conditional.");
          conditional.false_group_id = {};
        }

        assert((conditional.true_group_id || conditional.false_group_id) || !"Conditionals cannot end with both branches empty.");

        return m_parent;
      }
    };

  } // namespace impl

  /**
   * @brief Immutable enum-definition storage plus builder-time mutation helpers.
   *
   * @tparam Settings - Representation settings.
   */
  template <typename Settings>
  class Enum {
  public:
    using value_type = typename Settings::value_type;
    using command_t = std::variant<
      impl::Pairs<value_type>, impl::Named<value_type>, impl::Numeric<value_type>,
      impl::Cmds<value_type>, impl::Group<value_type>, impl::Conditional<value_type>
    >;

  private:
    item_id_t m_cmds_id{};
    Strings<Settings::MAX_STRING_STORAGE> strings{};
    Items<Settings::MAX_ITEMS_STORAGE, command_t> items{};

    /**
     * @brief Emits the full headered program through one writer surface.
     *
     * @tparam WriterT - Writer type compatible with impl::EnumEncoder.
     * @param writer - Destination writer or counting sink.
     * @param compress - Whether constrained values should be dint-condensed.
     * @param append_terminate - Whether to append an explicit Terminate opcode.
     * @return WriterT& - The updated writer.
     */
    template <typename WriterT>
    constexpr WriterT& output_program_impl(
      WriterT& writer,
      bool compress = false,
      bool append_terminate = false) const
    {
      writer.write_int(impl::storage_header_for_value_type<typename Settings::value_type>(compress));

      impl::EnumEncoder<Enum<Settings>, WriterT> encoder{ *this, writer, compress };
      if (cmds_id()) {
        item<impl::Cmds<typename Settings::value_type>>(cmds_id()).encode(encoder);
      }
      if (append_terminate) {
        writer.write_opcode(eEnumCommand::Terminate);
      }

      return writer;
    }

  public:
    /**
     * @brief Packed value containing the amount of string and items currently being used.
     *
     * @return std::uint32_t - The packed used space.
     */
    constexpr std::uint32_t used_space() const {
      return Constexpr::pack_space(strings.used_space(), items.used_space());
    }

    /**
     * @brief Packed value containing the amount of string and items allocated.
     *
     * @return std::uint32_t - The packed allocated space.
     */
    constexpr std::uint32_t allocated_space() const {
      return Constexpr::pack_space(Settings::MAX_STRING_STORAGE, Settings::MAX_ITEMS_STORAGE);
    }

    /**
     * @brief Returns the exact encoded program size including the storage header.
     *
     * @param compress - Whether constrained values should be dint-condensed.
     * @param append_terminate - Whether to append an explicit Terminate opcode.
     * @return std::size_t - Exact number of bytes required for output_program().
     */
    constexpr std::size_t program_size(
      bool compress = false,
      bool append_terminate = false) const
    {
      impl::CountingProgramWriter writer{};
      return output_program_impl(writer, compress, append_terminate).size();
    }

    /**
     * @brief Encodes the full headered program into one caller-supplied buffer.
     *
     * @param begin - First writable byte in the destination buffer.
     * @param end - One-past-the-end of the destination buffer.
     * @param compress - Whether constrained values should be dint-condensed.
     * @param append_terminate - Whether to append an explicit Terminate opcode.
     * @return char* - Pointer one past the last byte written.
     */
    constexpr char* output_program(
      char* begin,
      char* end,
      bool compress = false,
      bool append_terminate = false) const
    {
      assert(begin <= end);
      assert(static_cast<std::size_t>(end - begin) >= program_size(compress, append_terminate)
        || !"Not enough space to store enum program.");

      impl::ProgramWriter writer{ begin, end };
      return output_program_impl(writer, compress, append_terminate).program_cursor();
    }

    /**
     * @brief Encodes the full headered program into one runtime-owned string.
     *
     * @param compress - Whether constrained values should be dint-condensed.
     * @param append_terminate - Whether to append an explicit Terminate opcode.
     * @return std::string - Runtime-owned encoded program bytes.
     */
    std::string output_program(
      bool compress = false,
      bool append_terminate = false) const
    {
      std::string program(program_size(compress, append_terminate), '\0');
      output_program(program.data(), program.data() + program.size(), compress, append_terminate);
      return program;
    }

    /**
     * @brief Encodes the full headered program into one output stream.
     *
     * @param os - Destination output stream.
     * @param compress - Whether constrained values should be dint-condensed.
     * @param append_terminate - Whether to append an explicit Terminate opcode.
     * @return std::ostream& - The same destination output stream.
     */
    std::ostream& output_program(
      std::ostream& os,
      bool compress = false,
      bool append_terminate = false) const
    {
      std::string const program{ output_program(compress, append_terminate) };
      os.write(program.data(), static_cast<std::streamsize>(program.size()));
      return os;
    }

    /**
     * @brief Returns the root command-list id for this enum description.
     *
     *   NOTE: Not going to be 0 because the first Cmds object is allocated when
     *         there is a command object to attach to it.  It's simpler to
     *         program it this way.
     *
     * @return item_id_t - Root `Cmds` node id or zero when empty.
     */
    constexpr item_id_t cmds_id() const noexcept {
      return m_cmds_id;
    }

    /**
     * @brief Records the root command-list id for this enum description.
     *
     * @param cmds_id - Root `Cmds` node id or zero when empty.
     */
    constexpr void set_cmds_id(item_id_t cmds_id) noexcept {
      m_cmds_id = cmds_id;
    }

    /**
     * @brief Registers a string in the backing string store.
     *
    * @param value - String to store.
     * @return string_id_t - Stable id for later lookups.
     */
    constexpr string_id_t add_string(std::string_view value) {
      return strings.add_string(value);
    }

    /**
     * @brief Registers an item in the backing item store.
     *
     * @tparam Item - Type assignable into the configured item variant.
     * @param item - Item to store.
     * @return item_id_t - Stable id for later lookups.
     */
    template <typename Item>
    constexpr item_id_t add_item(Item item) {
      return items.add_item(item);
    }

    /**
     * @brief Retrieves a stored string view by id.
     *
     * @param id - String id.
     * @return std::string_view - View of the stored string.
     */
    constexpr std::string_view get_string(string_id_t id) const {
      return strings.get_string(id);
    }

    /**
     * @brief Retrieves a stored item variant by id.
     *
     * @param item_id - Item id.
     * @return command_t& - Reference to the stored item variant.
     */
    constexpr command_t& item(item_id_t item_id) {
      return items.get_item(item_id);
    }

    /**
     * @brief Retrieves a stored item variant by id as const.
     *
     * @param item_id - Item id.
     * @return command_t const& - Reference to the stored item variant.
     */
    constexpr command_t const& item(item_id_t item_id) const {
      return items.get_item(item_id);
    }

    /**
     * @brief Retrieves a stored item as a specific type.
     *
     * @tparam Item - Requested stored item type.
     * @param item_id - Item id.
     * @return Item& - Reference to the stored item.
     */
    template <typename Item>
    constexpr Item& item(item_id_t item_id) {
      return items.template get_item<Item>(item_id);
    }

    /**
     * @brief Retrieves a stored item as a specific type with const access.
     *
     * @tparam Item - Requested stored item type.
     * @param item_id - Item id.
     * @return Item const& - Reference to the stored item.
     */
    template <typename Item>
    constexpr Item const& item(item_id_t item_id) const {
      return items.template get_item<Item>(item_id);
    }

    /**
     * @brief Retrieves a previously stored item as Item type pointer by id if
     *   correct type, otherwise return nullptr.
     *
     * @param item_id - Item id.
     * @return command_t const* - Pointer to the stored item variant or nullptr
     *   if not that type.
     */
    template <typename Item>
    constexpr auto* item_if(item_id_t item_id) {
      return items.template get_item_if<Item>(item_id);
    }

    /**
     * @brief Retrieves a previously stored item as Item type pointer by id if
     *   correct type, otherwise return nullptr.
     *
     * @param item_id - Item id.
     * @return command_t const* - Pointer to the stored item variant or nullptr
     *   if not that type.
     */
    template <typename Item>
    constexpr auto const* item_if(item_id_t item_id) const {
      return items.template get_item_if<Item>(item_id);
    }

    /**
     * @brief Binds one runtime value to this enum description for rendering.
     *
     * @param enum_value - Runtime value to interpret.
     * @return EnumValueView<Enum<Settings>> - Streamable render view.
     */
    constexpr EnumValueView<Enum<Settings>> value(value_type enum_value) const noexcept {
      return EnumValueView<Enum<Settings>>{ *this, enum_value };
    }

    /**
     * @brief Alias for `value(enum_value)`.
     *
     * @param enum_value - Runtime value to interpret.
     * @return EnumValueView<Enum<Settings>> - Streamable render view.
     */
    constexpr EnumValueView<Enum<Settings>> operator()(value_type enum_value) const noexcept {
      return value(enum_value);
    }
  };

  /**
   * @brief Streamable render view for one enum description and runtime value.
   *
   * @tparam EnumT - Immutable enum description type.
   */
  template <typename EnumT>
  class EnumValueView {
    EnumT const* m_enum{};
    typename EnumT::value_type m_value{};

  public:
    using value_type = typename EnumT::value_type;

    /**
     * @brief Constructs a render view for one enum description and runtime value.
     *
     * @param enum_def - Immutable enum description.
     * @param value - Runtime value to interpret.
     */
    constexpr EnumValueView(EnumT const& enum_def, value_type value) noexcept
    : m_enum(&enum_def)
    , m_value(value)
    {
    }

    /**
     * @brief Returns the referenced enum description.
     *
     * @return EnumT const& - Bound enum description.
     */
    constexpr EnumT const& enum_def() const noexcept {
      return *m_enum;
    }

    /**
     * @brief Returns the bound runtime value.
     *
     * @return value_type - Stored runtime value.
     */
    constexpr value_type value() const noexcept {
      return m_value;
    }

    /**
     * @brief Renders the bound enum description and runtime value into text.
     *
     * @return std::string - Final rendered text.
     */
    std::string to_string() const {
      return impl::EnumTextRenderer<EnumT>{ enum_def(), value() }.render();
    }
  };

  /**
   * @brief Streams one rendered enum value view.
   *
   * @tparam EnumT - Immutable enum description type.
   * @param stream - Destination output stream.
   * @param value_view - Enum description plus runtime value view.
   * @return std::ostream& - Destination output stream.
   */
  template <typename EnumT>
  std::ostream& operator<<(std::ostream& stream, EnumValueView<EnumT> const& value_view) {
    return stream << value_view.to_string();
  }

  namespace impl {

    /**
     * @brief Cursor-based decoder that rebuilds one stored enum graph from a
     *   definition stream.
     *
     * @tparam Settings - Destination enum storage settings.
     */
    template <typename Settings>
    class EnumDecoder {
      using enum_type = Enum<Settings>;
      using value_type = typename Settings::value_type;
      using underlying_value_type = underlying_equivalent_t<value_type>;
      using unsigned_value_type = unsigned_equivalent_t<value_type>;

      std::string_view m_program{};
      char const* m_cursor{};
      char const* m_end{};
      enum_type m_enum{};
      bool m_throw_on_terminate{ true };
      bool m_compress{};

      /**
       * @brief Returns the all-bits-set root scope for the configured value
       *   type.
       *
       * @return value_type - Root scope bitmask.
       */
      static constexpr value_type full_scope_bitmask() noexcept {
        return static_cast<value_type>(static_cast<unsigned_value_type>(~unsigned_value_type{}));
      }

      /**
       * @brief Returns whether the source cursor reached the end of the
       *   program.
       *
       * @return bool - \c true when no unread bytes remain.
       */
      constexpr bool at_end() const noexcept {
        return m_cursor == m_end;
      }

      /**
       * @brief Returns the next unread byte without consuming it.
       *
       * @return std::uint8_t - Next unread byte.
       * @throws EnumParseUnexpectedEof when no unread bytes remain.
       */
      constexpr std::uint8_t peek_byte() const {
        if (at_end()) {
          throw EnumParseUnexpectedEof("Unexpected end of enum program.");
        }
        return static_cast<std::uint8_t>(static_cast<unsigned char>(*m_cursor));
      }

      /**
       * @brief Consumes and returns the next unread byte.
       *
       * @return std::uint8_t - Consumed byte.
       * @throws EnumParseUnexpectedEof when no unread bytes remain.
       */
      constexpr std::uint8_t read_byte() {
        auto const byte{ peek_byte() };
        ++m_cursor;
        return byte;
      }

      /**
       * @brief Returns the opcode tag of the next unread command byte.
       *
       * @return eEnumCommand - Opcode tag with payload bits masked away.
       * @throws EnumParseUnexpectedEof when no unread bytes remain.
       */
      constexpr eEnumCommand peek_opcode() const {
        return static_cast<eEnumCommand>(peek_byte() & static_cast<std::uint8_t>(eEnumCommand::mOpCode));
      }

      /**
       * @brief Returns whether a value is a subset of its parent scope bitmask.
       *
       * @param value - Candidate constrained value.
       * @param scope_bitmask - Parent scope bitmask.
       * @return bool - \c true when \p value is fully contained in \p scope_bitmask.
       */
      static constexpr bool is_subset_of_scope(value_type value, value_type scope_bitmask) noexcept {
        auto const value_bits{ make_unsigned_equivalent(value) };
        auto const scope_bits{ make_unsigned_equivalent(scope_bitmask) };
        return (value_bits & scope_bits) == value_bits;
      }

      /**
       * @brief Validates that a constrained value fits inside the current
       *   parent scope.
       *
       * @param value - Candidate constrained value.
       * @param scope_bitmask - Parent scope bitmask.
       * @throws EnumParseInvalidStructure when \p value exceeds \p scope_bitmask.
       */
      static constexpr void verify_scope_subset(value_type value, value_type scope_bitmask) {
        if (!is_subset_of_scope(value, scope_bitmask)) {
          throw EnumParseInvalidStructure("Constrained value exceeds the parent scope bitmask.");
        }
      }

      /**
       * @brief Validates that one conditional selector mask has exactly one bit
       *   set.
       *
       * @param group_bitmask - Conditional selector bitmask.
       * @throws EnumParseInvalidStructure when \p group_bitmask does not
       *   contain exactly one set bit.
       */
      static constexpr void verify_group_bitmask(value_type group_bitmask) {
        auto const bits{ make_unsigned_equivalent(group_bitmask) };
        if (!bits || (bits & (bits - 1u)) != 0u) {
          throw EnumParseInvalidStructure("Conditional group_bitmask must contain exactly one bit.");
        }
      }

      /**
       * @brief Ensures the destination enum has enough remaining string space
       *   for a copied stream string.
       *
       * @param bytes_to_add - Number of bytes including the trailing NUL.
       * @throws EnumParseCapacityExceeded when the destination string heap
       *   would overflow.
       */
      constexpr void ensure_string_capacity(std::size_t bytes_to_add) const {
        auto const used_bytes{ static_cast<std::size_t>(string_space(m_enum.used_space())) };
        if (used_bytes + bytes_to_add > Settings::MAX_STRING_STORAGE) {
          throw EnumParseCapacityExceeded("Decoded strings exceed the destination enum capacity.");
        }
      }

      /**
       * @brief Ensures the destination enum has enough remaining item space
       *   for one more stored object.
       *
       * @throws EnumParseCapacityExceeded when the destination item heap would
       *   overflow.
       */
      constexpr void ensure_item_capacity() const {
        auto const used_items{ static_cast<std::size_t>(item_space(m_enum.used_space())) };
        if (used_items + 1u > Settings::MAX_ITEMS_STORAGE) {
          throw EnumParseCapacityExceeded("Decoded items exceed the destination enum capacity.");
        }
      }

      /**
       * @brief Copies one decoded string into the destination enum heap.
       *
       * @param value - Decoded transient string view.
       * @return string_id_t - Stored string id.
       */
      constexpr string_id_t store_string(std::string_view value) {
        ensure_string_capacity(value.size() + 1u);
        return m_enum.add_string(value);
      }

      /**
       * @brief Stores one decoded graph item into the destination enum heap.
       *
       * @tparam Item - Stored item type.
       * @param item - Item value to store.
       * @return item_id_t - Stored item id.
       */
      template <typename Item>
      constexpr item_id_t store_item(Item item) {
        ensure_item_capacity();
        return m_enum.add_item(item);
      }

      /**
       * @brief Reads one NUL-terminated string from the source without keeping a
       *   view alive beyond the current parse step.
       *
       * @return std::string_view - Transient view of the unread source buffer.
       * @throws EnumParseUnexpectedEof when the source string is not
       *   terminated before the end of the program.
       */
      constexpr std::string_view read_c_string_view() {
        auto const* const begin{ m_cursor };
        while (!at_end() && *m_cursor != '\0') {
          ++m_cursor;
        }

        if (at_end()) {
          throw EnumParseUnexpectedEof("Unterminated string in enum program.");
        }

        std::string_view const value{ begin, static_cast<std::size_t>(m_cursor - begin) };
        ++m_cursor;
        return value;
      }

      /**
       * @brief Reads one fixed-width integer from the source.
       *
       * @tparam T - Integral type to decode.
       * @return T - Decoded fixed-width value.
       * @throws EnumParseUnexpectedEof when the source ends before the full
       *   integer is available.
       */
      template <typename T>
      constexpr T read_fixed_width_integer() {
        if (static_cast<std::size_t>(m_end - m_cursor) < sizeof(T)) {
          throw EnumParseUnexpectedEof("Unexpected end while decoding a fixed-width integer.");
        }

        T value{};
        auto cursor{ m_cursor };
        Constexpr::decode_int(value, cursor, m_end);
        m_cursor = cursor;
        return value;
      }

      /**
       * @brief Reads one condensed dint integer from the source.
       *
       * @tparam T - Integral type to decode into.
       * @return T - Decoded dint value.
       * @throws EnumParseUnexpectedEof when the dint is not fully present in
       *   the source.
       */
      template <typename T>
      constexpr T read_dint_integer() {
        T value{};
        auto cursor{ m_cursor };
        Constexpr::decode_dint<NoThrow>(value, cursor, m_end);
        if (cursor == m_cursor) {
          throw EnumParseUnexpectedEof("Unexpected end while decoding a condensed integer.");
        }
        m_cursor = cursor;
        return value;
      }

      /**
       * @brief Reads in the scoped group_shift and return the corresponding bitmask.
       *
       * @param scope_bitmask - Parent scope bitmask that constrains the value.
       * @return value_type - Decoded constrained value.
       */
      constexpr value_type read_scoped_group_mask_value(value_type scope_bitmask) {
        auto const group_shift{ read_fixed_width_integer<std::uint8_t>() };
        if (group_shift >= std::numeric_limits<unsigned_value_type>::digits) {
          throw EnumParseInvalidStructure("group_shift value exceeds number of bits in value_type.");
        }
        value_type const group_mask{ static_cast<value_type>(1 << group_shift) };
        auto const recon{ scope_bitmask & group_mask };
        if (recon != group_mask) {
          throw EnumParseInvalidStructure("Bit value is not representable under the parent scope bitmask.");
        }
        return group_mask;
      }

      /**
       * @brief Reads one constrained value using the stream's compression mode.
       *
       * @param scope_bitmask - Parent scope bitmask that constrains the value.
       * @return value_type - Decoded constrained value.
       */
      constexpr value_type read_scoped_value(value_type scope_bitmask) {
        if (!m_compress) {
          auto const raw{ read_fixed_width_integer<underlying_value_type>() };
          value_type const value{ static_cast<value_type>(raw) };
          verify_scope_subset(value, scope_bitmask);
          return value;
        }

        auto const condensed{ read_dint_integer<unsigned_value_type>() };
        value_type const expanded{ expand(scope_bitmask, condensed, true) };
        auto const recon{ make_unsigned_equivalent(condense(scope_bitmask, expanded, true)) };
        if (recon != condensed) {
          throw EnumParseInvalidStructure("Condensed value is not representable under the parent scope bitmask.");
        }
        return expanded;
      }

      /**
       * @brief Appends one decoded command node to a linked command list.
       *
       * @param first_cmd_id - Head command-list id for the branch being built.
       * @param last_cmd_id - Tail command-list id for the branch being built.
       * @param command_id - Stored command item id to append.
       * @return item_id_t - Stored command-list node id.
       */
      constexpr item_id_t append_command_node(
        item_id_t& first_cmd_id,
        item_id_t& last_cmd_id,
        item_id_t command_id)
      {
        item_id_t const cmds_id{ store_item(Cmds<value_type>{ command_id, {} }) };
        if (!first_cmd_id) {
          first_cmd_id = cmds_id;
        } else {
          m_enum.template item<Cmds<value_type>>(last_cmd_id).next_id = cmds_id;
        }
        last_cmd_id = cmds_id;
        return cmds_id;
      }

      /**
       * @brief Verifies that one decoded named block does not reuse the same
       *   masked enum value.
       *
       * @param named_id - Stored named-command id being extended.
       * @param value - Candidate pair value.
       * @throws EnumParseInvalidStructure when \p value already exists in the
       *   named block.
       */
      constexpr void verify_unique_named_value(item_id_t named_id, value_type value) const {
        auto const& named{ m_enum.template item<Named<value_type>>(named_id) };
        for (item_id_t pair_id{ named.pairs_id }; pair_id != 0u;) {
          auto const& pair{ m_enum.template item<Pairs<value_type>>(pair_id) };
          if (pair.value == value) {
            throw EnumParseInvalidStructure("Named command cannot reuse the same masked enum value.");
          }
          pair_id = pair.next_pairs_id;
        }
      }

      /**
       * @brief Appends one decoded pair to a lazily-created named command.
       *
       * @param named_id - Stored named-command id being built. Created on first
       *   pair append.
       * @param last_pair_id - Tail pair id for the named command being built.
       * @param has_mask - Whether the named command owns a command-local mask.
       * @param command_mask - Stored command-local bitmask when \p has_mask is
       *   true.
       * @param value - Decoded pair value.
       * @param name_id - Stored string id for the pair name.
       */
      constexpr void append_pair(
        item_id_t& named_id,
        item_id_t& last_pair_id,
        bool has_mask,
        value_type command_mask,
        value_type value,
        string_id_t name_id)
      {
        if (!named_id) {
          named_id = store_item(Named<value_type>{ has_mask, command_mask, {} });
        }

        verify_unique_named_value(named_id, value);
        item_id_t const pair_id{ store_item(Pairs<value_type>{ value, name_id, {} }) };
        auto& named{ m_enum.template item<Named<value_type>>(named_id) };
        if (!named.pairs_id) {
          named.pairs_id = pair_id;
        } else {
          m_enum.template item<Pairs<value_type>>(last_pair_id).next_pairs_id = pair_id;
        }
        last_pair_id = pair_id;
      }

      /**
       * @brief Decodes a one-based item count from one opcode payload field.
       *
       * @param opcode - Raw opcode byte.
       * @param mask - Bitmask selecting the stored count bits.
       * @return std::size_t - Actual element count.
       */
      static constexpr std::size_t decode_count_plus_one(std::uint8_t opcode, std::uint8_t mask) noexcept {
        return static_cast<std::size_t>(opcode & mask) + 1u;
      }

      /**
       * @brief Decodes a zero-based item count from one opcode payload field.
       *
       * @param opcode - Raw opcode byte.
       * @param mask - Bitmask selecting the stored count bits.
       * @return std::size_t - Actual element count.
       */
      static constexpr std::size_t decode_count_direct(std::uint8_t opcode, std::uint8_t mask) noexcept {
        return static_cast<std::size_t>(opcode & mask);
      }

      /**
       * @brief Validates that a raw opcode byte uses only the supported numeric
       *   format bits.
       *
       * @param opcode - Raw numeric opcode byte.
       * @throws EnumParseInvalidOpcode when unsupported reserved bits are set.
       */
      static constexpr void validate_numeric_opcode(std::uint8_t opcode) {
        constexpr std::uint8_t allowed_bits{
          static_cast<std::uint8_t>(eEnumCommand::mOpCode) |
          static_cast<std::uint8_t>(eEnumCommand::fRightShiftBits) |
          static_cast<std::uint8_t>(eEnumCommand::fPackedBits) |
          static_cast<std::uint8_t>(eEnumCommand::fIsSigned)
        };
        if ((opcode & static_cast<std::uint8_t>(~allowed_bits)) != 0u) {
          throw EnumParseInvalidOpcode("Numeric opcode uses unsupported reserved bits.");
        }
      }

      /**
       * @brief Validates the exact one-byte storage-type header.
       *
       * @throws EnumParseEmptyInput when the source is empty.
       * @throws EnumParseHeaderMismatch when the width/sign discriminator does
       *   not match the requested destination enum value type.
       */
      constexpr void read_header() {
        auto const header{ impl::read_storage_header(m_program) };
        if (storage_type_base(header) != storage_type_for_value_type<value_type>()) {
          throw EnumParseHeaderMismatch("Enum program header does not match the requested enum value type.");
        }

        (void)read_byte();
        m_compress = storage_type_is_compressed(header);
      }

      /**
       * @brief Decodes one named-pair payload block plus any postfix pair
       *   continuations.
       *
       * @param initial_count - Number of pairs carried by the opening opcode.
       * @param pair_scope_bitmask - Active bitmask for the pair values.
       * @param has_mask - Whether the resulting named command carries a
       *   command-local bitmask.
       * @param command_mask - Stored command-local bitmask when \p has_mask is
       *   true.
       * @return item_id_t - Stored named-command id, or zero when the branch is
       *   empty.
       */
      constexpr item_id_t parse_pair_branch(
        std::size_t initial_count,
        value_type pair_scope_bitmask,
        bool has_mask,
        value_type command_mask)
      {
        item_id_t named_id{};
        item_id_t last_pair_id{};

        auto append_pairs = [&](std::size_t count) constexpr {
          for (std::size_t i{}; i < count; ++i) {
            auto const value{ read_scoped_value(pair_scope_bitmask) };
            string_id_t const name_id{ store_string(read_c_string_view()) };
            append_pair(named_id, last_pair_id, has_mask, command_mask, value, name_id);
          }
        };

        append_pairs(initial_count);

        while (!at_end() && peek_opcode() == eEnumCommand::ContinueScope) {
          std::uint8_t const opcode{ read_byte() };
          append_pairs(decode_count_plus_one(opcode, static_cast<std::uint8_t>(eEnumCommand::mCountLarge)));
        }

        return named_id;
      }

      /**
       * @brief Decodes one command-branch payload block plus any postfix
       *   command continuations.
       *
       * @param initial_count - Number of command constructs carried by the
       *   opening opcode.
       * @param scope_bitmask - Active scope bitmask for the branch.
       * @return item_id_t - Stored head command-list id, or zero when the
       *   branch is empty.
       */
      constexpr item_id_t parse_command_branch(std::size_t initial_count, value_type scope_bitmask) {
        item_id_t first_cmd_id{};
        item_id_t last_cmd_id{};

        auto append_commands = [&](std::size_t count) constexpr {
          for (std::size_t i{}; i < count; ++i) {
            item_id_t const command_id{ parse_command(scope_bitmask) };
            if (command_id) {
              append_command_node(first_cmd_id, last_cmd_id, command_id);
            }
          }
        };

        append_commands(initial_count);

        while (!at_end() && peek_opcode() == eEnumCommand::ContinueScope) {
          std::uint8_t const opcode{ read_byte() };
          append_commands(decode_count_plus_one(opcode, static_cast<std::uint8_t>(eEnumCommand::mCountLarge)));
        }

        return first_cmd_id;
      }

      /**
       * @brief Wraps one stored command item in a one-node command list.
       *
       * @param command_id - Stored command item id.
       * @return item_id_t - Stored command-list id.
       */
      constexpr item_id_t wrap_single_command(item_id_t command_id) {
        item_id_t first_cmd_id{};
        item_id_t last_cmd_id{};
        append_command_node(first_cmd_id, last_cmd_id, command_id);
        return first_cmd_id;
      }

      /**
       * @brief Decodes one optional postfix else branch for a conditional.
       *
       * @param scope_bitmask - Scope bitmask shared by the conditional
       *   branches.
       * @return item_id_t - Stored else-group id, or zero when no else branch
       *   follows.
       */
      constexpr item_id_t parse_optional_else_group(value_type scope_bitmask) {
        if (at_end() || peek_opcode() != eEnumCommand::Else) {
          return {};
        }

        std::uint8_t const opcode{ read_byte() };
        bool const has_group_name{ (opcode & static_cast<std::uint8_t>(eEnumCommand::fHasGroupName)) != 0u };
        bool const else_cmds{ (opcode & static_cast<std::uint8_t>(eEnumCommand::fElseCmds)) != 0u };
        string_id_t const group_name_id{
          has_group_name ? store_string(read_c_string_view()) : string_id_t{}
        };

        if (else_cmds) {
          item_id_t const cmds_id{
            parse_command_branch(
              decode_count_plus_one(opcode, static_cast<std::uint8_t>(eEnumCommand::mCountSmall)),
              scope_bitmask)
          };
          if (!cmds_id) {
            throw EnumParseInvalidStructure("Else command branches must contain at least one command.");
          }
          return store_item(Group<value_type>{ group_name_id, cmds_id });
        }

        item_id_t const named_id{
          parse_pair_branch(
            decode_count_plus_one(opcode, static_cast<std::uint8_t>(eEnumCommand::mCountSmall)),
            scope_bitmask,
            false,
            {})
        };
        if (!named_id) {
          throw EnumParseInvalidStructure("Else named branches must contain at least one pair.");
        }
        return store_item(Group<value_type>{ group_name_id, wrap_single_command(named_id) });
      }

      /**
       * @brief Decodes one Named command.
       *
       * @param opcode - Raw Named opcode byte.
       * @param scope_bitmask - Active parent scope bitmask.
       * @return item_id_t - Stored Named command id.
       */
      constexpr item_id_t parse_named_command(std::uint8_t opcode, value_type scope_bitmask) {
        bool const has_bitmask{ (opcode & static_cast<std::uint8_t>(eEnumCommand::fHasBitmask)) != 0u };
        value_type const command_mask{
          has_bitmask ? read_scoped_value(scope_bitmask) : value_type{}
        };
        value_type const pair_scope_bitmask{ has_bitmask ? command_mask : scope_bitmask };
        item_id_t const named_id{
          parse_pair_branch(
            decode_count_plus_one(opcode, static_cast<std::uint8_t>(eEnumCommand::mCountMedium)),
            pair_scope_bitmask,
            has_bitmask,
            command_mask)
        };
        if (!named_id) {
          throw EnumParseInvalidStructure("Named commands must contain at least one pair.");
        }
        return named_id;
      }

      /**
       * @brief Decodes one Numeric command.
       *
       * @param opcode - Raw Numeric opcode byte.
       * @param scope_bitmask - Active parent scope bitmask.
       * @return item_id_t - Stored Numeric command id.
       */
      constexpr item_id_t parse_numeric_command(std::uint8_t opcode, value_type scope_bitmask) {
        validate_numeric_opcode(opcode);
        value_type const bitmask{ read_scoped_value(scope_bitmask) };
        string_id_t const name_id{ store_string(read_c_string_view()) };
        eEnumCommand const format{
          static_cast<eEnumCommand>(opcode & static_cast<std::uint8_t>(
            eEnumCommand::fRightShiftBits |
            eEnumCommand::fPackedBits |
            eEnumCommand::fIsSigned))
        };
        return store_item(Numeric<value_type>{ bitmask, format, name_id });
      }

      /**
       * @brief Decodes one GroupIf, GroupIfNamed, or GroupIfNumeric command.
       *
       * @param opcode - Raw conditional opcode byte.
       * @param scope_bitmask - Active parent scope bitmask.
       * @return item_id_t - Stored Conditional command id.
       */
      constexpr item_id_t parse_conditional_command(std::uint8_t opcode, value_type scope_bitmask) {
        eEnumCommand const kind{
          static_cast<eEnumCommand>(opcode & static_cast<std::uint8_t>(eEnumCommand::mOpCode))
        };

        value_type const group_bitmask{ read_scoped_group_mask_value(scope_bitmask) };
        verify_group_bitmask(group_bitmask);
        value_type const branch_scope_bitmask{ read_scoped_value(scope_bitmask) };
        string_id_t const group_name_id{
          (opcode & static_cast<std::uint8_t>(eEnumCommand::fHasGroupName)) != 0u
            ? store_string(read_c_string_view())
            : string_id_t{}
        };

        item_id_t true_group_id{};
        item_id_t false_group_id{};

        if (kind == eEnumCommand::GroupIf) {
          item_id_t const cmds_id{
            parse_command_branch(
              decode_count_direct(opcode, static_cast<std::uint8_t>(eEnumCommand::mCountMedium)),
              branch_scope_bitmask)
          };
          if (cmds_id) {
            true_group_id = store_item(Group<value_type>{ group_name_id, cmds_id });
          }
          false_group_id = parse_optional_else_group(branch_scope_bitmask);
        } else if (kind == eEnumCommand::GroupIfNamed) {
          item_id_t const named_id{
            parse_pair_branch(
              decode_count_direct(opcode, static_cast<std::uint8_t>(eEnumCommand::mCountMedium)),
              branch_scope_bitmask,
              false,
              {})
          };
          if (named_id) {
            true_group_id = store_item(Group<value_type>{ group_name_id, wrap_single_command(named_id) });
          }
          false_group_id = parse_optional_else_group(branch_scope_bitmask);
        } else {
          bool const negate{ (opcode & static_cast<std::uint8_t>(eEnumCommand::fNegate)) != 0u };
          string_id_t const name_id{ store_string(read_c_string_view()) };
          eEnumCommand const format{
            static_cast<eEnumCommand>(opcode & static_cast<std::uint8_t>(
              eEnumCommand::fRightShiftBits |
              eEnumCommand::fPackedBits |
              eEnumCommand::fIsSigned))
          };
          item_id_t const numeric_id{
            store_item(Numeric<value_type>{ branch_scope_bitmask, format, name_id })
          };
          item_id_t const cmds_id{ wrap_single_command(numeric_id) };
          item_id_t const inline_group_id{ store_item(Group<value_type>{ group_name_id, cmds_id }) };
          item_id_t const else_group_id{ parse_optional_else_group(branch_scope_bitmask) };

          if (negate) {
            false_group_id = inline_group_id;
            true_group_id = else_group_id;
          } else {
            true_group_id = inline_group_id;
            false_group_id = else_group_id;
          }
        }

        if (!true_group_id && !false_group_id) {
          return {};
        }

        return store_item(Conditional<value_type>{
          group_bitmask,
          branch_scope_bitmask,
          true_group_id,
          false_group_id,
        });
      }

      /**
       * @brief Decodes one command construct at the current scope.
       *
       * @param scope_bitmask - Active parent scope bitmask.
       * @return item_id_t - Stored command item id.
       */
      constexpr item_id_t parse_command(value_type scope_bitmask) {
        std::uint8_t const opcode{ read_byte() };
        switch (static_cast<eEnumCommand>(opcode & static_cast<std::uint8_t>(eEnumCommand::mOpCode))) {
        case eEnumCommand::Named:
          return parse_named_command(opcode, scope_bitmask);
        case eEnumCommand::Numeric:
          return parse_numeric_command(opcode, scope_bitmask);
        case eEnumCommand::GroupIf:
        case eEnumCommand::GroupIfNamed:
        case eEnumCommand::GroupIfNumeric:
          return parse_conditional_command(opcode, scope_bitmask);
        case eEnumCommand::Terminate:
          throw EnumParseInvalidStructure("Terminate is only valid at the outermost stream level.");
        case eEnumCommand::Else:
          throw EnumParseInvalidStructure("Else is postfix-only and cannot appear as a standalone command.");
        case eEnumCommand::ContinueScope:
          throw EnumParseInvalidStructure("ContinueScope is postfix-only and cannot appear as a standalone command.");
        default:
          throw EnumParseInvalidOpcode("Unknown enum stream opcode.");
        }
      }

      /**
       * @brief Decodes the unbounded root command list until the program ends or
       *   an accepted Terminate opcode is reached.
       *
       * @return item_id_t - Stored head command-list id.
       */
      constexpr item_id_t parse_root_commands() {
        item_id_t first_cmd_id{};
        item_id_t last_cmd_id{};

        while (!at_end()) {
          if (peek_opcode() == eEnumCommand::Terminate) {
            std::uint8_t const opcode{ read_byte() };
            if (opcode != static_cast<std::uint8_t>(eEnumCommand::Terminate)) {
              throw EnumParseInvalidOpcode("Terminate opcode reserves all payload bits.");
            }
            if (m_throw_on_terminate) {
              throw EnumParseInvalidStructure("Terminate is not allowed when throw_on_terminate is true.");
            }
            if (!at_end()) {
              throw EnumParseInvalidStructure("Terminate must be the final byte in the enum program.");
            }
            break;
          }

          item_id_t const command_id{ parse_command(full_scope_bitmask()) };
          if (command_id) {
            append_command_node(first_cmd_id, last_cmd_id, command_id);
          }
        }

        return first_cmd_id;
      }

    public:
      /**
       * @brief Constructs a decoder over one immutable source program view.
       *
       * @param program - Source definition stream including the storage-type header.
       * @param throw_on_terminate - Whether a Terminate opcode is rejected as a
       *   parse error.
       */
      constexpr EnumDecoder(std::string_view program, bool throw_on_terminate) noexcept
      : m_program{ program }
      , m_cursor{ program.data() }
      , m_end{ program.data() + program.size() }
      , m_enum{}
      , m_throw_on_terminate{ throw_on_terminate }
      , m_compress{}
      {
      }

      /**
       * @brief Decodes the entire source program into one immutable enum
       *   representation.
       *
       * @return enum_type - Fully rebuilt enum graph.
       */
      constexpr enum_type decode() {
        read_header();
        m_enum.set_cmds_id(parse_root_commands());
        return m_enum;
      }
    };

  } // namespace impl

  /**
   * @brief Terminal builder wrapper returned after decoding a definition stream.
   *
   * @tparam Settings - Representation settings for the decoded enum graph.
   */
  template <typename Settings>
  class DecodedEnumBuilder {
    Enum<Settings> m_enum{};

  public:
    using enum_type = Enum<Settings>;

    /**
     * @brief Wraps one already-decoded enum for terminal builder-style access.
     *
     * @param enum_def - Decoded immutable enum representation.
     */
    constexpr explicit DecodedEnumBuilder(enum_type enum_def) noexcept
    : m_enum{ enum_def }
    {
    }

    /**
     * @brief Returns the decoded enum representation.
     *
     * @return enum_type - Built immutable enum representation.
     */
    constexpr enum_type Build() const {
      return m_enum;
    }

    /**
     * @brief Returns the used string/item space of the decoded enum.
     *
     * @return std::uint32_t - Packed used-space summary.
     */
    constexpr std::uint32_t used_space() const {
      return m_enum.used_space();
    }
  };

  /**
   * @brief Default fixed-capacity storage budget used by enum-description
   *   builders and wrappers when the caller does not override it.
   */
  constexpr std::uint32_t DefaultReserved{ pack_space(256, 128) };

  /**
   * @brief Immutable enum-representation settings bundling value type and
   *   fixed storage capacities.
   *
   * @tparam ValueT - Enum or integer value type stored by the representation.
   * @tparam StringAndItemCapacity - Packed storage reservation with string
   *   space in the low 16 bits and item space in the high 16 bits.
   */
  template <typename ValueT, std::uint32_t StringAndItemCapacity = DefaultReserved>
  struct EnumSettings {
    using value_type = ValueT;
    constexpr static std::uint16_t MAX_STRING_STORAGE { impl::string_space(StringAndItemCapacity) };
    constexpr static std::uint16_t MAX_ITEMS_STORAGE  { impl::item_space(StringAndItemCapacity)  };
  };


  /**
   * @brief Typed-chaining builder for immutable enum descriptions.
   *
   * @tparam Settings - Representation settings for the stored enum graph.
   */
  template <typename Settings>
  class EnumBuilder : public impl::CommandScopeFacade<EnumBuilder<Settings>> {
    Enum<Settings> m_enum{};
    impl::CommandScopeState<typename Settings::value_type> m_state{};

    friend class impl::CommandScopeFacade<EnumBuilder<Settings>>;
    template <typename Parent>
    friend class impl::IfScope;
    template <typename Parent>
    friend class impl::ElseScope;

    /**
     * @brief Returns mutable access to the enum under construction.
     *
     * @return Enum<Settings>& - Shared mutable enum representation.
     */
    constexpr Enum<Settings>& enum_ref() noexcept {
      return m_enum;
    }

    /**
     * @brief Returns mutable access to the root command-scope state.
     *
     * @return impl::CommandScopeState<typename Settings::value_type>& - Mutable root state.
     */
    constexpr impl::CommandScopeState<typename Settings::value_type>& command_state() noexcept {
      return m_state;
    }

    /**
     * @brief Records the first root command node stored in the enum.
     *
     * @param cmds_id - First stored root `Cmds` node id.
     */
    constexpr void on_first_command(item_id_t cmds_id) {
      m_enum.set_cmds_id(cmds_id);
    }

    /**
     * @brief Append one named pair to the root command scope.
     *
     * @param has_mask - Whether the named command uses a command-local bitmask.
     * @param bitmask - Command-local bitmask when `has_mask` is true.
     * @param value - Enum value matched by the pair.
     * @param name - Display name for the pair.
     */
    constexpr void append_named_pair_impl(
      bool has_mask,
      typename Settings::value_type bitmask,
      typename Settings::value_type value,
      std::string_view name)
    {
      impl::CommandScopeFacade<EnumBuilder<Settings>>::append_named_pair(
        *this, has_mask, bitmask, value, name);
    }

    /**
     * @brief Append one numeric command to the root command scope.
     *
     * @param bitmask - Bitmask selecting the numeric field.
     * @param name - Display label for the numeric field.
     * @param format - Numeric formatting flags.
     */
    constexpr void append_numeric_impl(
      typename Settings::value_type bitmask,
      std::string_view name,
      eEnumCommand format)
    {
      impl::CommandScopeFacade<EnumBuilder<Settings>>::append_numeric(*this, bitmask, name, format);
    }

    /**
     * @brief Start one conditional branch scope at the root command level.
     *
     * @param negate_first - Whether the first user-authored branch is the false branch.
     * @param group_bitmask - Single-bit selector for the conditional.
     * @param scope_bitmask - Scope bitmask applied inside the branch.
     * @param group_name - Optional group label for the first branch.
     * @param has_group_name - Whether `group_name` should be stored.
     * @return impl::IfScope<EnumBuilder<Settings>> - Builder state for the new branch.
     */
    constexpr auto begin_if_impl(
      bool negate_first,
      typename Settings::value_type group_bitmask,
      typename Settings::value_type scope_bitmask,
      std::string_view group_name,
      bool has_group_name)
    {
      return impl::CommandScopeFacade<EnumBuilder<Settings>>::begin_if_scope(
        *this, negate_first, group_bitmask, scope_bitmask, group_name, has_group_name);
    }

    /**
     * @brief Returns whether the builder still represents an empty root scope.
     *
     * @return bool - \c true when no root commands or stored payload exist yet.
     */
    constexpr bool is_empty_builder() const noexcept {
      return m_state.first_cmd_id == 0u && m_enum.used_space() == 0u;
    }

  public:
    using settings_type = Settings;
    using enum_type = Enum<Settings>;
    using value_type = typename Settings::value_type;

    /**
     * @brief Construct an empty enum builder.
     */
    constexpr EnumBuilder() noexcept = default;

    /**
     * @brief Finalize the current builder snapshot into an immutable enum.
     *
     * @return enum_type - Built immutable enum representation.
     */
    constexpr enum_type Build() const {
      auto result{ m_enum };
      result.set_cmds_id(m_state.first_cmd_id);
      return result;
    }

    /**
     * @brief Decode one definition stream as a terminal builder-chain step.
     *
     * After calling this function, only \c Build() and \c used_space() stay
     * available on the returned wrapper.
     *
     * @param program - Definition stream including the storage-type header.
     * @param throw_on_terminate - Whether a Terminate opcode is rejected as a
     *   parse error.
     * @return DecodedEnumBuilder<Settings> - Terminal wrapper around the
     *   decoded enum representation.
     */
    constexpr DecodedEnumBuilder<Settings> decode_program(
      std::string_view program,
      bool throw_on_terminate = true) const
    {
      assert(is_empty_builder() || !"decode_program() is only valid on an empty root builder.");

      return DecodedEnumBuilder<Settings>{
        impl::EnumDecoder<Settings>{ program, throw_on_terminate }.decode()
      };
    }

    /**
     * @brief Allows to specify a default for string and items for the Enum.
     *
     * NOTE: Use only at the very beginning as any previous commands will be
     *       lost.
     *
     * @tparam string_space - Number of string characters + NUL allocated for
     *   string storage.
     * @tparam item_space - Number of internal representational commands
     *   allocated (Named, Numeric, Pairs, Cmds, Conditional, Group)
     * @return EnumBuilder<NewSettings> - The new EnumBuilder.
     */
    template <std::uint16_t string_space, std::uint16_t item_space>
    constexpr auto reserve_space() {
      return EnumBuilder<EnumSettings<typename Settings::value_type, pack_space(string_space, item_space)>>{};
    }

    /**
     * @brief Returns the configured fixed-capacity storage budget.
     *
     * @return std::uint32_t - Packed string/item capacity summary.
     */
    constexpr std::uint32_t used_space() const {
      return m_enum.used_space();
    }
  };

  /**
   * @brief Create an empty typed-chaining enum builder.
   *
   * @tparam Settings - Representation settings for the stored enum graph.
   * @return EnumBuilder<Settings> - Empty builder.
   */
  template <typename Settings>
  constexpr auto build_enum_description() {
    return EnumBuilder<Settings>{};
  }

  /**
   * @brief Variant-backed runtime-selected wrapper around one decoded typed
   *   enum description.
   *
   * @tparam StringAndItemCapacity - Fixed backing storage capacity shared by
   *   all typed alternatives.
   */
  template <std::uint32_t StringAndItemCapacity = DefaultReserved>
  class AnyEnumDescription {
    using variant_type = std::variant<
      Enum<EnumSettings<std::int8_t,   StringAndItemCapacity>>,
      Enum<EnumSettings<std::int16_t,  StringAndItemCapacity>>,
      Enum<EnumSettings<std::int32_t,  StringAndItemCapacity>>,
      Enum<EnumSettings<std::int64_t,  StringAndItemCapacity>>,
      Enum<EnumSettings<std::uint8_t,  StringAndItemCapacity>>,
      Enum<EnumSettings<std::uint16_t, StringAndItemCapacity>>,
      Enum<EnumSettings<std::uint32_t, StringAndItemCapacity>>,
      Enum<EnumSettings<std::uint64_t, StringAndItemCapacity>>
    >;
    using value_view_type = AnyEnumValueView<AnyEnumDescription<StringAndItemCapacity>>;

    eEnumStorageType m_storage_type{};
    variant_type m_enum_desc;

  public:
    /**
     * @brief Wraps one decoded typed enum description with its selected storage
     *   discriminator.
     *
     * @tparam EnumT - Active typed enum alternative.
     * @param storage_type - Underlying width/sign discriminator chosen from the
     *   program header.
     * @param enum_desc - Decoded typed enum description.
     */
    template <typename EnumT>
    constexpr AnyEnumDescription(eEnumStorageType storage_type, EnumT enum_desc)
    : m_storage_type{ storage_type }
    , m_enum_desc{ enum_desc }
    {
    }

    /**
     * @brief Returns the active underlying width/sign discriminator.
     *
     * @return eEnumStorageType - Active width/sign discriminator without the
     *   Compress bit.
     */
    constexpr eEnumStorageType storage_type() const noexcept {
      return m_storage_type;
    }

    /**
     * @brief Returns whether the active decoded value type is signed.
     *
     * @return bool - \c true when the active by-wire value type is signed.
     */
    constexpr bool is_signed() const noexcept {
      return (storage_type() & eEnumStorageType::fIsSigned);
    }

    /**
     * @brief Visits the active typed enum description.
     *
     * @tparam Visitor - Callable visitor type accepted by `std::visit`.
     * @param visitor - Visitor invoked with the active typed enum alternative.
     * @return decltype(auto) - Result of invoking \p visitor.
     */
    template <class Visitor>
    decltype(auto) visit(Visitor&& visitor) const {
      return std::visit(std::forward<Visitor>(visitor), m_enum_desc);
    }

    /**
     * @brief Binds one runtime unsigned value to this runtime-selected enum
     *   description for rendering.
     *
     * @param enum_value - Unsigned runtime value to interpret.
     * @return value_view_type - Streamable render view.
     *
     * @throws std::invalid_argument when the decoded enum value type is signed.
     * @throws std::out_of_range when \p enum_value does not fit the decoded
     *   unsigned storage width.
     */
    value_view_type value_unsigned(std::uint64_t enum_value) const {
      return visit([&](auto const& typed_enum) -> value_view_type {
        using typed_enum_type = std::decay_t<decltype(typed_enum)>;
        using value_type = typename typed_enum_type::value_type;
        using unsigned_value_type = unsigned_equivalent_t<value_type>;

        if constexpr (std::is_signed_v<value_type>) {
          throw std::invalid_argument("value_unsigned() requires an unsigned runtime-selected enum description.");
        } else {
          if (enum_value > static_cast<std::uint64_t>(std::numeric_limits<unsigned_value_type>::max())) {
            throw std::out_of_range("Unsigned runtime value is out of range for the decoded enum storage width.");
          }
          return value_view_type{ *this, static_cast<std::uint64_t>(static_cast<unsigned_value_type>(enum_value)) };
        }
      });
    }

    /**
     * @brief Binds one runtime signed value to this runtime-selected enum
     *   description for rendering.
     *
     * @param enum_value - Signed runtime value to interpret.
     * @return value_view_type - Streamable render view.
     *
     * @throws std::invalid_argument when the decoded enum value type is
     *   unsigned.
     * @throws std::out_of_range when \p enum_value does not fit the decoded
     *   signed storage width.
     */
    value_view_type value_signed(std::int64_t enum_value) const {
      return visit([&](auto const& typed_enum) -> value_view_type {
        using typed_enum_type = std::decay_t<decltype(typed_enum)>;
        using value_type = typename typed_enum_type::value_type;
        using unsigned_value_type = unsigned_equivalent_t<value_type>;

        if constexpr (!std::is_signed_v<value_type>) {
          throw std::invalid_argument("value_signed() requires a signed runtime-selected enum description.");
        } else {
          if (enum_value < static_cast<std::int64_t>(std::numeric_limits<value_type>::min())
            || enum_value > static_cast<std::int64_t>(std::numeric_limits<value_type>::max()))
          {
            throw std::out_of_range("Signed runtime value is out of range for the decoded enum storage width.");
          }
          return value_view_type{
            *this,
            static_cast<std::uint64_t>(static_cast<unsigned_value_type>(static_cast<value_type>(enum_value)))
          };
        }
      });
    }

    /**
     * @brief Returns the exact encoded program size for the active typed enum
     *   description.
     *
     * @param compress - Whether constrained values should be dint-condensed.
     * @param append_terminate - Whether to append an explicit Terminate opcode.
     * @return std::size_t - Exact number of bytes required by
     *   `output_program(...)`.
     */
    std::size_t program_size(
      bool compress = false,
      bool append_terminate = false) const
    {
      return visit([&](auto const& enum_desc) {
        return enum_desc.program_size(compress, append_terminate);
      });
    }

    /**
     * @brief Encodes the active typed enum description into one caller-owned
     *   buffer.
     *
     * @param begin - First writable byte in the destination buffer.
     * @param end - One-past-the-end of the destination buffer.
     * @param compress - Whether constrained values should be dint-condensed.
     * @param append_terminate - Whether to append an explicit Terminate opcode.
     * @return char* - Pointer one past the last byte written.
     */
    char* output_program(
      char* begin,
      char* end,
      bool compress = false,
      bool append_terminate = false) const
    {
      return visit([&](auto const& enum_desc) {
        return enum_desc.output_program(begin, end, compress, append_terminate);
      });
    }

    /**
     * @brief Encodes the active typed enum description into one runtime-owned
     *   string.
     *
     * @param compress - Whether constrained values should be dint-condensed.
     * @param append_terminate - Whether to append an explicit Terminate opcode.
     * @return std::string - Runtime-owned encoded program bytes.
     */
    std::string output_program(
      bool compress = false,
      bool append_terminate = false) const
    {
      return visit([&](auto const& enum_desc) {
        return enum_desc.output_program(compress, append_terminate);
      });
    }

    /**
     * @brief Encodes the active typed enum description into one output stream.
     *
     * @param os - Destination output stream.
     * @param compress - Whether constrained values should be dint-condensed.
     * @param append_terminate - Whether to append an explicit Terminate opcode.
     * @return std::ostream& - The same destination output stream.
     */
    std::ostream& output_program(
      std::ostream& os,
      bool compress = false,
      bool append_terminate = false) const
    {
      return visit([&](auto const& enum_desc) -> std::ostream& {
        return enum_desc.output_program(os, compress, append_terminate);
      });
    }
  };

  /**
   * @brief Streamable render view for one runtime-selected enum description and
   *   bound runtime value.
   *
   * @tparam AnyEnumT - Immutable runtime-selected enum description type.
   */
  template <typename AnyEnumT>
  class AnyEnumValueView {
    AnyEnumT const* m_enum_def{};
    std::uint64_t m_value_bits{};

    /**
     * @brief Rebuilds one active typed integer value from stored raw bits.
     *
     * @tparam ValueT - Active integral value type selected by the wrapped
     *   description.
     * @param value_bits - Raw by-wire bits widened to 64 bits.
     * @return ValueT - Value narrowed back to the active typed width and
     *   signedness.
     */
    template <typename ValueT>
    static constexpr ValueT typed_value_from_bits(std::uint64_t value_bits) noexcept {
      static_assert(std::is_integral_v<ValueT>, "AnyEnumValueView requires integral runtime-selected value types.");
      using unsigned_value_type = unsigned_equivalent_t<ValueT>;

      unsigned_value_type const narrowed_bits{ static_cast<unsigned_value_type>(value_bits) };
      if constexpr (std::is_signed_v<ValueT>) {
        auto const signed_value{
          impl::sign_extend(narrowed_bits, std::numeric_limits<unsigned_value_type>::digits - 1u)
        };
        return static_cast<ValueT>(signed_value);
      } else {
        return static_cast<ValueT>(narrowed_bits);
      }
    }

  public:
    /**
     * @brief Constructs a render view for one runtime-selected enum
     *   description and runtime value bits.
     *
     * @param enum_def - Immutable runtime-selected enum description.
     * @param value_bits - Runtime value already normalized to the decoded
     *   storage width.
     */
    constexpr AnyEnumValueView(AnyEnumT const& enum_def, std::uint64_t value_bits) noexcept
    : m_enum_def(&enum_def)
    , m_value_bits(value_bits)
    {
    }

    /**
     * @brief Returns the referenced runtime-selected enum description.
     *
     * @return AnyEnumT const& - Bound runtime-selected enum description.
     */
    constexpr AnyEnumT const& enum_def() const noexcept {
      return *m_enum_def;
    }

    /**
     * @brief Returns the stored normalized value bits.
     *
     * @return std::uint64_t - Runtime value widened to 64 bits.
     */
    constexpr std::uint64_t value_bits() const noexcept {
      return m_value_bits;
    }

    /**
     * @brief Renders the bound runtime-selected enum description and runtime
     *   value into text.
     *
     * @return std::string - Final rendered text.
     */
    std::string to_string() const {
      return enum_def().visit([&](auto const& typed_enum) {
        using typed_enum_type = std::decay_t<decltype(typed_enum)>;
        using value_type = typename typed_enum_type::value_type;
        return typed_enum.value(typed_value_from_bits<value_type>(value_bits())).to_string();
      });
    }
  };

  /**
   * @brief Streams one rendered runtime-selected enum value view.
   *
   * @tparam AnyEnumT - Immutable runtime-selected enum description type.
   * @param stream - Destination output stream.
   * @param value_view - Runtime-selected enum description plus runtime value
   *   view.
   * @return std::ostream& - Destination output stream.
   */
  template <typename AnyEnumT>
  std::ostream& operator<<(std::ostream& stream, AnyEnumValueView<AnyEnumT> const& value_view) {
    return stream << value_view.to_string();
  }

  /**
   * @brief Builder-like entrypoint for decoding variant-backed
   *   runtime-selected enum descriptions from transmitted programs.
   *
   * @tparam StringAndItemCapacity - Fixed backing storage capacity shared by
   *   all typed alternatives.
   */
  template <std::uint32_t StringAndItemCapacity = DefaultReserved>
  class AnyEnumBuilder {
    std::string_view m_program{};
    bool m_throw_on_terminate{ true };
    bool m_has_program{ false };

    using any_enum_type = AnyEnumDescription<StringAndItemCapacity>;

    /**
     * @brief Decodes the stored program through one concrete typed enum
     *   alternative selected from the header.
     *
     * @tparam ValueT - Concrete signed or unsigned integral value type.
     * @param program - Definition stream including the storage header.
     * @param throw_on_terminate - Whether Terminate is rejected as a parse
     *   error.
     * @return any_enum_type - Variant-backed runtime-selected wrapper around
     *   the decoded typed enum description.
     */
    template <typename ValueT>
    static constexpr any_enum_type decode_as(
      std::string_view program,
      bool throw_on_terminate)
    {
      using Settings = EnumSettings<ValueT, StringAndItemCapacity>;
      return any_enum_type{
        impl::storage_type_for_value_type<ValueT>(),
        impl::EnumDecoder<Settings>{ program, throw_on_terminate }.decode()
      };
    }

  public:
    /**
     * @brief Stores one program to decode during `Build()`.
     *
     * @param program - Definition stream including the storage header.
     * @param throw_on_terminate - Whether a Terminate opcode is rejected as a
     *   parse error.
     * @return AnyEnumBuilder - Updated builder carrying the decode request.
     */
    constexpr AnyEnumBuilder decode_program(
      std::string_view program,
      bool throw_on_terminate = true) const
    {
      auto result{ *this };
      result.m_program = program;
      result.m_throw_on_terminate = throw_on_terminate;
      result.m_has_program = true;
      return result;
    }

    /**
     * @brief Decodes the stored program into one variant-backed
     *   runtime-selected enum description.
     *
     * @return any_enum_type - Decoded variant-backed runtime-selected enum
     *   description.
     */
    constexpr any_enum_type Build() const {
      assert(m_has_program || !"decode_program() must be called before Build().");

      auto const header{ impl::read_storage_header(m_program) };
      switch (impl::storage_type_base(header)) {
        case eEnumStorageType::Int8:
          return decode_as<std::int8_t>(m_program, m_throw_on_terminate);
        case eEnumStorageType::Int16:
          return decode_as<std::int16_t>(m_program, m_throw_on_terminate);
        case eEnumStorageType::Int32:
          return decode_as<std::int32_t>(m_program, m_throw_on_terminate);
        case eEnumStorageType::Int64:
          return decode_as<std::int64_t>(m_program, m_throw_on_terminate);
        case eEnumStorageType::UInt8:
          return decode_as<std::uint8_t>(m_program, m_throw_on_terminate);
        case eEnumStorageType::UInt16:
          return decode_as<std::uint16_t>(m_program, m_throw_on_terminate);
        case eEnumStorageType::UInt32:
          return decode_as<std::uint32_t>(m_program, m_throw_on_terminate);
        case eEnumStorageType::UInt64:
          return decode_as<std::uint64_t>(m_program, m_throw_on_terminate);
        case eEnumStorageType::Compress:
          assert(false && "Impossible: storage_type_base() masks out the Compress flag before dispatch.");
          break;
      }

      assert(false && "read_storage_header() already validated the storage-type discriminator.");
      throw EnumParseHeaderMismatch("Enum program header uses unsupported storage-type bits.");
    }

    /**
     * @brief Returns the configured fixed-capacity storage budget.
     *
     * @return std::uint32_t - Packed string/item capacity summary.
     */
    constexpr std::uint32_t allocated_space() const noexcept {
      return StringAndItemCapacity;
    }
  };

  /**
   * @brief Creates an empty variant-backed runtime-selected enum decode
   *   builder.
   *
   * @tparam StringAndItemCapacity - Fixed backing storage capacity shared by
   *   all typed alternatives.
   * @return AnyEnumBuilder<StringAndItemCapacity> - Empty variant-backed
   *   runtime-selected decode builder.
   */
  template <std::uint32_t StringAndItemCapacity = DefaultReserved>
  constexpr auto build_any_enum_description() {
    return AnyEnumBuilder<StringAndItemCapacity>{};
  }

} // namespace Constexpr

/**
 * @brief Build a Enum type as small as possible in compiler space.
 *
 * @example
 *
 * ```cpp
 * constexpr auto minimal_enum = BUILD_ENUM_DESCRIPTION(EnumType,
 *    .Named(TestEnum{ 0x01u }, "one")
 *    .Named(TestEnum{ 0x02u }, "two")
 * );
 * ```
 */
#define BUILD_ENUM_DESCRIPTION(enum_type, enum_description)                   \
  (Constexpr::build_enum_description<                                         \
    Constexpr::EnumSettings<                                                  \
      enum_type,                                                              \
      Constexpr::build_enum_description<Constexpr::EnumSettings<enum_type>>() \
        enum_description.used_space()                                         \
    >                                                                         \
  >() enum_description.Build())


#endif // CONSTEXPR_ENUM_HPP
