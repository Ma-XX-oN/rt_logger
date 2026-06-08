/**
 * @file enum_ir.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Opcode enums, parse-error hierarchy, stream writer types, and IR graph-node types for the enum description system.
 * @version 0.1
 * @date 2026-05-28
 *
 * @copyright Copyright (c) 2026
 */
#ifndef CONSTEXPR_ENUM_IR_HPP
#define CONSTEXPR_ENUM_IR_HPP

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <variant>

#include "masked_bits.hpp"
#include "bitwise_enum.hpp"
#include "int_codec.hpp"
#include "string.hpp"
#include "type_traits.hpp"
#include "heap.hpp"

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

template <>
struct enable_bitwise_ops<Constexpr::eEnumCommand> : std::true_type {};

template <>
struct enable_bitwise_ops<Constexpr::eEnumStorageType> : std::true_type {};

template <>
struct enable_bitwise_ops<Constexpr::impl::eBlockType> : std::true_type {};

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
//    intermediate objects will vaporise soon enough anyway.
//

// Types needed:
//
// 1. EnumBuilder - Builds the Enum
// 2. Enum - defines the enum, the names it uses, the underlying type, and the dependencies.
// 3. EnumInstance - Wrapper around the underlying type.
// 4. EnumSet - Used to set EnumInstance's underlying value.
// 5. EnumForceSet - Used to set EnumInstance's underlying value, but reduces restrictions.
// 6. Abstraction Intermediate Representation
//
//  ## Graph Classes
//
//  Each class below is basically a set of integers. The items below each is the number of aggregate integers in each
//
//    Pairs {
//      E value
//      Int name_string_id
//      Int next_id
//
//      build_stream(string current) : string
//    } 3 items
//
//    Cmd {
//      get_string(E value) : string
//    } Conceptual base class
//
//    Named : Cmd {
//      bool has_mask
//      E mask
//      Pairs first_pair_id
//      Pairs last_pair_id  // optimisation
//
//      build_stream(string current) : string
//    } 5 items + 1
//
//    Numeric : Cmd {
//      E mask
//      Format format
//      Str name_string_id
//
//      build_stream(string current) : string
//    } 3 items
//
//    Cmds {
//      Cmd command_id
//      Int next_id
//
//      build_stream(string current) : string
//    } 2 items
//
//    Group {
//      Str name_string_id
//      Cmds first_cmd_id
//      Cmds last_cmd_id    // optimisation
//
//      build_stream(string current) : string
//    } 3 items + 1
//
//    Conditional : Cmd {
//      E group_bitmask
//      Group true_commands_id
//      Group false_commands_id
//
//      build_stream(string current) : string
//    } 7 items
//
//  ## Storage classes
//
//  These are storage classes. The indices in the previous classes refer to an object or string here.   This reduces
//  the class template count parameters to 2 values. The total length of strings bytes stored and the number of
//  objects.  This makes it simple to manage and since nothing is ever deleted, we don't even have to track that
//  anywhere.
//
//    Strings {
//      String memory
//
//      add_string(String str_to_register) : index
//    }
//
//    Items {
//      array<variant<Pairs, Named, Numeric, Group, Conditional>> memory
//
//      add_item(T item) : index
//    }
//

// using E = std::int8_t;

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
      unsigned_equivalent_t<E> scope_bitmask{ std::numeric_limits<unsigned_equivalent_t<E>>::max() };
      int remaining_items_allowed_in_block{ -1 }; // Negative means the current scope has no block-local limit.

      /**
       * @brief Validates that one bitmask is a subset of the active scope bitmask.
       *
       * @param new_bitmask - Candidate bitmask that must remain inside the current scope.
       */
      constexpr void verify_scope_bitmask([[maybe_unused]] unsigned_equivalent_t<E> new_bitmask) {
        assert((scope_bitmask & new_bitmask) == new_bitmask || !"new_bitmask must be a subset of the scope_bitmask");
      }

      /**
       * @brief Narrows the active scope bitmask.
       *
       * @param new_bitmask - Replacement bitmask that must remain inside the current scope.
       */
      constexpr void set_scope_bitmask(unsigned_equivalent_t<E> new_bitmask) {
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
      // Enum description type
      using enum_type = EnumT;
      // Underlying enum or integral value type
      using value_type = typename EnumT::value_type;
      // unsigned underlying integral value type
      using unsigned_value_type = unsigned_equivalent_t<value_type>;
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
       * @return unsigned_value_type - Current scope bitmask.
       */
      constexpr unsigned_value_type scope_bitmask() const noexcept {
        return m_scope.scope_bitmask;
      }

      /**
       * @brief Validates that one bitmask is a subset of the active scope bitmask.
       *
       * @param new_bitmask - Candidate bitmask that must remain inside the current scope.
       */
      constexpr void verify_scope_bitmask(unsigned_value_type new_bitmask) {
        m_scope.verify_scope_bitmask(new_bitmask);
      }

      /**
       * @brief Narrows the current scope bitmask.
       *
       * @param new_bitmask - Replacement bitmask that must remain inside the current scope.
       */
      constexpr void set_scope_bitmask(unsigned_value_type new_bitmask) {
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
       * @tparam T - Value type (enum or integral).
       * @param value - Value to encode.
       * @param scope_bitmask - Active scope bitmask for validation and condensation (unsigned).
       */
      template <typename T>
      constexpr void encode_int(T value, unsigned_equivalent_t<T> scope_bitmask) {
        assert((make_unsigned_equivalent(value) & scope_bitmask) == make_unsigned_equivalent(value)
          || !"value must be a subset of scope_bitmask");
        if (m_compress) {
          assert(sizeof(T) > 1 || !"Can't compress a type that has a byte length of 1.");
          auto const cvalue { condense(scope_bitmask, make_unsigned_equivalent(value), true) };
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
      unsigned_equivalent_t<E> mask{};
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
      unsigned_equivalent_t<E> mask{};
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
      unsigned_equivalent_t<E> group_bitmask{};
      unsigned_equivalent_t<E> bitmask{};
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
     * @brief Validates that one conditional selector mask has exactly one bit
     *   set.
     *
     * @param group_bitmask - Conditional selector bitmask.
     * @throws EnumParseInvalidStructure when \p group_bitmask does not
     *   contain exactly one set bit.
     */
    template <typename ValueT>
    constexpr void verify_group_bitmask(ValueT group_bitmask) {
      if (!has_only_one_bit_set(group_bitmask)) {
        throw EnumParseInvalidStructure("Conditional group_bitmask must contain exactly one bit.");
      }
    }

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
      assert(sign_bit_index < digits || !"Sign bit exceeds value's bit positions.");

      std::size_t const width{ sign_bit_index + 1u };
      UInt const width_mask{
        width >= digits
          ? std::numeric_limits<UInt>::max()
          : static_cast<UInt>((UInt{ 1u } << width) - 1u)
      };

      UInt const sign_bit{ static_cast<UInt>(UInt{ 1u } << sign_bit_index) };
      if ((value & sign_bit) == 0u) {
        return static_cast<std::make_signed_t<UInt>>(value & width_mask);
      }

      return bit_cast<std::make_signed_t<UInt>>(
        static_cast<UInt>(value | static_cast<UInt>(~width_mask))
      );
    }
  } // namespace impl

} // namespace Constexpr

#endif // CONSTEXPR_ENUM_IR_HPP
