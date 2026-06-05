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
#include <variant>
#include <string_view>
#include <type_traits>

#include "masked_bits.hpp"
#include "bitwise_enum.hpp"
#include "int_codec.hpp"
#include "string.hpp"
#include "bitwise_enum.hpp"
#include "type_traits.hpp"
#include "heap.hpp"
#include "int_codec.hpp"

namespace Constexpr {

constexpr std::size_t MAX_NAME_LEN { 20 };

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

  GroupIf            = 3 << 5,  // | M | If group_bitmask set, use bitmask on following commands.
  GroupIfNamed       = 4 << 5,  // | M | If group_bitmask set, use bitmask on following pairs.
  Else               = 5 << 5,  // | S | Continue the current conditional scope as else group.
   fHasGroupName     =  1 << 4, // |   | States if group name is specified for GroupIf* and Else.
   fElseCmds         =  1 << 3, // |   | Used only by Else.  If set, Else is for commands otherwise pairs.
  ContinueScope      = 6 << 5,  // | L | Continue the current named or command branch.

  GroupIfNumeric     = 7 << 5,  // | - | If group_bitmask set, specify numeric output for stated bitmask.
   fNegate           =  1 << 3, // |   |   Inverts the inline numeric condition, so the numeric item belongs to the else case.
   // GroupIfNumeric also can take fRightShiftBits, fPackedBits, fIsSigned and fHasGroupName flags.
};
} // namespace Constexpr

template <>
struct BitwiseOps<Constexpr::eEnumCommand> : std::true_type {};



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

  constexpr std::uint32_t reserve_space(std::uint16_t string_space, std::uint16_t item_space) {
    return static_cast<std::uint32_t>(string_space) | (static_cast<std::uint32_t>(item_space) << 16);
  }
  
  namespace impl {

    template <typename Parent>
    class IfScope;

    template <typename Parent>
    class ElseScope;

    template <typename Derived>
    class CommandScopeFacade;

    using size_t = std::uint16_t;

    constexpr std::uint16_t string_space(std::uint32_t value) {
      return static_cast<std::uint16_t>(value);
    }

    constexpr std::uint16_t item_space(std::uint32_t value) {
      return static_cast<std::uint16_t>(value >> 16);
    }

    /**
     * @brief Collect immutable enum-representation settings in one place.
     *
     * @tparam E - Enum or integer value type stored by the representation.
     * @tparam StringAndItemStorage - Packed storage reservation with string
     *   space in the low 16 bits and item space in the high 16 bits.
     * @tparam Variant - Variant type used to store every graph item.
     */
    template <typename E, std::uint32_t StringAndItemStorage, typename Variant>
    struct EnumSettings {
      using Value = E;
      constexpr static size_t MAX_STRING_STORAGE { string_space(StringAndItemStorage) };
      constexpr static size_t MAX_ITEMS_STORAGE  {   item_space(StringAndItemStorage) };
      using ItemVariant = Variant;
    };

    using program_cursor_t = char*;

    /**
     * @brief Non-owning byte sink for building a definition stream.
     */
    class ProgramWriter {
      program_cursor_t m_begin{};
      program_cursor_t m_end{};
      program_cursor_t m_cursor{};

    public:
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

    private:
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
      constexpr void verify_scope_bitmask(E new_bitmask) {
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

    template <typename EnumT>
    class EnumEncoder {
      EnumT const* m_enum{};
      ProgramWriter* m_writer{};
      ScopeData<typename EnumT::value_type> m_scope{};
      bool m_compress{};

    public:
      using enum_type = EnumT;
      using value_type = typename EnumT::value_type;

      /**
       * @brief Constructs an encoder for one definition-stream emission pass.
       *
       * @param enum_def - Immutable enum representation to read from.
       * @param writer - Writable stream sink.
       * @param compress - Whether scoped integer values should be condensed as dints.
       */
      constexpr EnumEncoder(EnumT const& enum_def, ProgramWriter& writer, bool compress = false) noexcept
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
        ProgramWriter& writer,
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
       * @return ProgramWriter& - Referenced output sink.
       */
      constexpr ProgramWriter& writer() const noexcept {
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
       * @return program_cursor_t - Current write position in the program buffer.
       */
      constexpr program_cursor_t program_cursor() const noexcept {
        return m_writer->program_cursor();
      }

      /**
       * @brief Returns a pointer to the next byte to be written in the program buffer.
       *
       * @return program_cursor_t - Current write position in the program buffer.
       */
      constexpr program_cursor_t reserve_byte() noexcept {
        auto cursor = m_writer->program_cursor();
        encode_int(eEnumCommand::Terminate);
        return cursor;
      }

      /**
       * @brief Returns a writable reference to a previously written byte.
       *
       * @param cursor - Pointer to the byte to patch.
       * @return char& - Writable byte reference.
       */
      constexpr char& byte_at(program_cursor_t cursor) {
        return m_writer->byte_at(cursor);
      }

      /**
       * @brief Returns a const reference to a previously written byte.
       *
       * @param cursor - Pointer to the byte to inspect.
       * @return char const& - Const byte reference.
       */
      constexpr char const& byte_at(program_cursor_t cursor) const {
        return m_writer->byte_at(cursor);
      }

      /**
       * @brief Bitwise-ORs selected bits into a previously written byte.
       *
       * @tparam T - Integral or enum byte-sized source of bits.
       * @param cursor - Pointer to the byte to update.
       * @param bits - Bits to OR into the byte.
       */
      template <typename T>
      constexpr void or_byte_at(program_cursor_t cursor, T bits) {
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
       * @brief Retrieves an unresolved stored item variant by id.
       *
       * @param item_id - Item id to look up.
       * @return typename EnumT::item_variant const& - Stored item variant.
       */
      constexpr typename EnumT::item_variant const& item(item_id_t item_id) const {
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
     * generated in a first block and returns how many were.
     *
     * @tparam EnumT - Referenced enum representation type.
     * @tparam FnEncodeBlock - Callback type used to encode the child block.
     * @param ec - Active encoder for the parent scope.
     * @param block_type - Shape of the block being emitted.
     * @param fn_encode_block - Callback that emits the child block using a
     * copied encoder.
     * @return size_t - Number of elements stored in first block.
     */
    template <typename EnumT, typename FnEncodeBlock>
    constexpr size_t encode_block(
      EnumEncoder<EnumT>& ec, eBlockType block_type, FnEncodeBlock fn_encode_block)
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
      template <typename EnumT>
      constexpr void encode(EnumEncoder<EnumT>& ec) const {
        if (ec.remaining_items_allowed_in_block() > 0) {
          ec.remaining_items_allowed_in_block() -= 1;
          ec.encode_int(value, ec.scope_bitmask());
          ec.encode_string(name_id);
          if (next_pairs_id) {
            ec.template item<Pairs<E>>(next_pairs_id).encode(ec);
          }
        } else {
          program_cursor_t pc { ec.reserve_byte() };
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
      template <typename EnumT>
      constexpr void encode(EnumEncoder<EnumT>& ec) const {
        assert(pairs_id || !"Can't define a Named block with no pairs.");

        program_cursor_t pc { ec.reserve_byte() };
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
      template <typename EnumT>
      constexpr size_t encode_body(EnumEncoder<EnumT>& ec, eBlockType block_type = eBlockType::Pair) const {
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
      template <typename EnumT>
      constexpr void encode(EnumEncoder<EnumT>& ec) const {
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
      template <typename EnumT>
      constexpr eEnumCommand encode_inline_body(EnumEncoder<EnumT>& ec) const {
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
      template <typename EnumT>
      constexpr void encode(EnumEncoder<EnumT>& ec) const {
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

        assert(if_group || !"Conditional requires a true group unless the false group can inline as negated numeric.");

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
        program_cursor_t const if_pc { ec.reserve_byte() };
        ec.encode_int(group_bitmask, ec.scope_bitmask());
        ec.encode_int(bitmask, ec.scope_bitmask());
        if (if_group->name_id) {
          ec.encode_string(if_group->name_id);
        }
        
        
        // EMITTING IF BLOCK
        auto if_ec { ec };
        if_ec.set_scope_bitmask(bitmask);

        if (if_opcode == eEnumCommand::GroupIf) {
          size_t const stored {
            encode_block(if_ec, eBlockType::IfCmd,
              [&](auto& child_ec) {
                child_ec.template item<Cmds<E>>(if_group->cmds_id).encode(child_ec);
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
        program_cursor_t const else_pc { ec.reserve_byte() };

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
      template <typename EnumT>
      constexpr void encode_current(EnumEncoder<EnumT>& ec) const {
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
      template <typename EnumT>
      constexpr void encode(EnumEncoder<EnumT>& ec) const {
        if (ec.remaining_items_allowed_in_block() < 0) {
          encode_current(ec);
        } else if (ec.remaining_items_allowed_in_block() > 0) {
          ec.remaining_items_allowed_in_block() -= 1;
          encode_current(ec);
        } else {
          program_cursor_t pc { ec.reserve_byte() };
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
     *       Name/Named
     *       Number/Numeric
     *       If / IfNot
     *       NameIf / NameIfNot
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
     * command scope while building an enum graph.
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
       * @return auto - Updated command scope.
       */
      template <typename D = Derived>
      constexpr auto Name(typename D::value_type value, std::string_view name) const {
        auto next{ derived() };
        next.append_named_pair_impl(false, typename D::value_type{}, value, name);
        return next;
      }

      /**
       * @brief Add one named value under an explicit command-local bitmask.
       *
       * @param bitmask - Command-local bitmask for the named block.
       * @param value - Masked enum value matched by the pair.
       * @param name - Display name for the pair.
       * @return auto - Updated command scope.
       */
      template <typename D = Derived>
      constexpr auto Name(
        typename D::value_type bitmask,
        typename D::value_type value,
        std::string_view name) const
      {
        auto next{ derived() };
        next.append_named_pair_impl(true, bitmask, value, name);
        return next;
      }

      /**
       * @brief Alias for `Name(value, name)`.
       *
       * @param value - Enum value matched by the pair.
       * @param name - Display name for the pair.
       * @return auto - Updated command scope.
       */
      template <typename D = Derived>
      constexpr auto Named(typename D::value_type value, std::string_view name) const {
        return Name(value, name);
      }

      /**
       * @brief Alias for `Name(bitmask, value, name)`.
       *
       * @param bitmask - Command-local bitmask for the named block.
       * @param value - Masked enum value matched by the pair.
       * @param name - Display name for the pair.
       * @return auto - Updated command scope.
       */
      template <typename D = Derived>
      constexpr auto Named(
        typename D::value_type bitmask,
        typename D::value_type value,
        std::string_view name) const
      {
        return Name(bitmask, value, name);
      }

      /**
       * @brief Add one numeric command to the current command scope.
       *
       * @param bitmask - Bitmask selecting the numeric field.
       * @param name - Display label for the numeric field.
       * @param format - Numeric formatting flags.
       * @return auto - Updated command scope.
       */
      template <typename D = Derived>
      constexpr auto Number(
        typename D::value_type bitmask,
        std::string_view name,
        eEnumCommand format = eEnumCommand{}) const
      {
        auto next{ derived() };
        next.append_numeric_impl(bitmask, name, format);
        return next;
      }

      /**
       * @brief Alias for `Number(bitmask, name, format)`.
       *
       * @param bitmask - Bitmask selecting the numeric field.
       * @param name - Display label for the numeric field.
       * @param format - Numeric formatting flags.
       * @return auto - Updated command scope.
       */
      template <typename D = Derived>
      constexpr auto Numeric(
        typename D::value_type bitmask,
        std::string_view name,
        eEnumCommand format = eEnumCommand{}) const
      {
        return Number(bitmask, name, format);
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
        typename D::value_type scope_bitmask) const
      {
        auto next{ derived() };
        return next.begin_if_impl(false, group_bitmask, scope_bitmask, {}, false);
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
        std::string_view group_name) const
      {
        auto next{ derived() };
        return next.begin_if_impl(false, group_bitmask, scope_bitmask, group_name, true);
      }

      /**
       * @brief Start a conditional scope whose first user-authored branch is the
       * false case.
       *
       * @param group_bitmask - Single-bit selector for the branch.
       * @param scope_bitmask - Scope bitmask to apply inside the branch.
       * @return IfScope<Derived> - Builder state for the first user-authored branch.
       */
      template <typename D = Derived>
      constexpr auto IfNot(
        typename D::value_type group_bitmask,
        typename D::value_type scope_bitmask) const
      {
        auto next{ derived() };
        return next.begin_if_impl(true, group_bitmask, scope_bitmask, {}, false);
      }

      /**
       * @brief Start a named conditional scope whose first user-authored branch
       * is the false case.
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
        std::string_view group_name) const
      {
        auto next{ derived() };
        return next.begin_if_impl(true, group_bitmask, scope_bitmask, group_name, true);
      }

      /**
       * @brief Alias for `If(group_bitmask, scope_bitmask)`.
       *
       * @param group_bitmask - Single-bit selector for the branch.
       * @param scope_bitmask - Scope bitmask to apply inside the branch.
       * @return IfScope<Derived> - Builder state for the if branch.
       */
      template <typename D = Derived>
      constexpr auto NameIf(
        typename D::value_type group_bitmask,
        typename D::value_type scope_bitmask) const
      {
        return If(group_bitmask, scope_bitmask);
      }

      /**
       * @brief Alias for `If(group_bitmask, scope_bitmask, group_name)`.
       *
       * @param group_bitmask - Single-bit selector for the branch.
       * @param scope_bitmask - Scope bitmask to apply inside the branch.
       * @param group_name - Group label for the if branch.
       * @return IfScope<Derived> - Builder state for the if branch.
       */
      template <typename D = Derived>
      constexpr auto NameIf(
        typename D::value_type group_bitmask,
        typename D::value_type scope_bitmask,
        std::string_view group_name) const
      {
        return If(group_bitmask, scope_bitmask, group_name);
      }

      /**
       * @brief Alias for `IfNot(group_bitmask, scope_bitmask)`.
       *
       * @param group_bitmask - Single-bit selector for the branch.
       * @param scope_bitmask - Scope bitmask to apply inside the branch.
       * @return IfScope<Derived> - Builder state for the first user-authored branch.
       */
      template <typename D = Derived>
      constexpr auto NameIfNot(
        typename D::value_type group_bitmask,
        typename D::value_type scope_bitmask) const
      {
        return IfNot(group_bitmask, scope_bitmask);
      }

      /**
       * @brief Alias for `IfNot(group_bitmask, scope_bitmask, group_name)`.
       *
       * @param group_bitmask - Single-bit selector for the branch.
       * @param scope_bitmask - Scope bitmask to apply inside the branch.
       * @param group_name - Group label for the first user-authored branch.
       * @return IfScope<Derived> - Builder state for the first user-authored branch.
       */
      template <typename D = Derived>
      constexpr auto NameIfNot(
        typename D::value_type group_bitmask,
        typename D::value_type scope_bitmask,
        std::string_view group_name) const
      {
        return IfNot(group_bitmask, scope_bitmask, group_name);
      }

    protected:
      /**
       * @brief Returns the concrete command-scope object.
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
       * current command scope.
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
       * needed.
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
       * conditional command to the current scope.
       *
       * @param scope - Command scope being updated.
       * @param negate_first - Whether the first user-authored branch is the
       * false branch.
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
      Parent m_parent{};
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
       * false branch.
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
      constexpr IfScope(Parent parent, item_id_t conditional_id, item_id_t group_id) noexcept
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
      constexpr auto Else() const {
        auto next{ *this };
        return next.make_else_scope_impl({}, false);
      }

      /**
       * @brief Switch this conditional from its if branch to a named else branch.
       *
       * @param group_name - Group label for the else branch.
       * @return ElseScope<Parent> - Builder state for the else branch.
       */
      constexpr auto Else(std::string_view group_name) const {
        auto next{ *this };
        return next.make_else_scope_impl(group_name, true);
      }

      /**
       * @brief Finish this if scope and return to the exact parent scope type.
       *
       * @return Parent - Updated parent scope.
       */
      constexpr Parent End() const {
        return m_parent;
      }

    private:
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

        return ElseScope<Parent>{ m_parent, group_id };
      }
    };

    /**
     * @brief Builder state for the else branch of a conditional.
     *
     * @tparam Parent - Immediate parent builder scope.
     */
    template <typename Parent>
    class ElseScope : public CommandScopeFacade<ElseScope<Parent>> {
      Parent m_parent{};
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
       * false branch.
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
       * @param group_id - Stored group id for the active else branch.
       */
      constexpr ElseScope(Parent parent, item_id_t group_id) noexcept
      : m_parent{ parent }
      , m_group_id{ group_id }
      , m_state{}
      {
      }

      /**
       * @brief Finish this else scope and return to the exact parent scope type.
       *
       * @return Parent - Updated parent scope.
       */
      constexpr Parent End() const {
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
    using Variant = typename Settings::ItemVariant;

    item_id_t m_cmds_id{};
    Strings<Settings::MAX_STRING_STORAGE> strings{};
    Items<Settings::MAX_ITEMS_STORAGE, Variant> items{};

  public:
    using value_type = typename Settings::Value;
    using item_variant = Variant;

    constexpr std::uint32_t reserve_space() const {
      return Constexpr::reserve_space(strings.used_space(), items.used_space());
    }

    constexpr std::uint32_t actual_space() const {
      return Constexpr::reserve_space(Settings::MAX_STRING_STORAGE, Settings::MAX_ITEMS_STORAGE);
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
     * @return Variant& - Reference to the stored item variant.
     */
    constexpr Variant& item(item_id_t item_id) {
      return items.get_item(item_id);
    }

    /**
     * @brief Retrieves a stored item variant by id as const.
     *
     * @param item_id - Item id.
     * @return Variant const& - Reference to the stored item variant.
     */
    constexpr Variant const& item(item_id_t item_id) const {
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
     * @return Variant const* - Pointer to the stored item variant or nullptr
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
     * @return Variant const* - Pointer to the stored item variant or nullptr
     *   if not that type.
     */
    template <typename Item>
    constexpr auto const* item_if(item_id_t item_id) const {
      return items.template get_item_if<Item>(item_id);
    }
  };

  /**
   * @brief Typed-chaining builder for immutable enum descriptions.
   *
   * @tparam Settings - Representation settings for the stored enum graph.
   */
  template <typename Settings>
  class EnumBuilder : public impl::CommandScopeFacade<EnumBuilder<Settings>> {
    Enum<Settings> m_enum{};
    impl::CommandScopeState<typename Settings::Value> m_state{};

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
     * @return impl::CommandScopeState<typename Settings::Value>& - Mutable root state.
     */
    constexpr impl::CommandScopeState<typename Settings::Value>& command_state() noexcept {
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
      typename Settings::Value bitmask,
      typename Settings::Value value,
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
      typename Settings::Value bitmask,
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
      typename Settings::Value group_bitmask,
      typename Settings::Value scope_bitmask,
      std::string_view group_name,
      bool has_group_name)
    {
      return impl::CommandScopeFacade<EnumBuilder<Settings>>::begin_if_scope(
        *this, negate_first, group_bitmask, scope_bitmask, group_name, has_group_name);
    }

  public:
    using settings_type = Settings;
    using enum_type = Enum<Settings>;
    using value_type = typename Settings::Value;

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

    constexpr std::uint32_t reserve_space() const {
      return m_enum.reserve_space();
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

  template <typename E, std::uint32_t StringAndItemCapacity = reserve_space(256, 256)>
  using DefaultEnumSettings = impl::EnumSettings<E, StringAndItemCapacity
  , std::variant<
      impl::Pairs<E>, impl::Named<E>, impl::Numeric<E>, impl::Cmds<E>, impl::Group<E>, impl::Conditional<E>
    >
  >;

} // namespace Constexpr

/**
 * @brief Build a Enum type as small as possible in compiler space.
 * 
 * @example
 *
 * ```cpp
 * constexpr auto minimal_enum = BUILD_ENUM_DESCRIPTION(EnumType,
 *    .Name(TestEnum{ 0x01u }, "one")
 *    .Name(TestEnum{ 0x02u }, "two")
 * );
 */
#define BUILD_ENUM_DESCRIPTION(enum_type, enum_description)                          \
  (Constexpr::build_enum_description<                                                \
    Constexpr::DefaultEnumSettings<                                                  \
      enum_type,                                                                     \
      Constexpr::build_enum_description<Constexpr::DefaultEnumSettings<enum_type>>() \
        enum_description.reserve_space()                                             \
    >                                                                                \
  >() enum_description.Build())


#endif // CONSTEXPR_ENUM_HPP
