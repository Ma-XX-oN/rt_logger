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
#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <limits>
#include <variant>
// #include <iterator>
#include <functional>
#include <string_view>
#include <type_traits>
#include "bit.hpp"
#include "masked_bits.hpp"
#include "bitwise_enum.hpp"
#include "int_codec.hpp"
#include "string.hpp"
#include "bitwise_enum.hpp"
#include "type_traits.hpp"
#include "algorithm.hpp"
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

  namespace  impl {

    using size_t = std::uint16_t;
    using item_id_t = std::uint16_t;
    using string_id_t = std::uint16_t;

    /**
     * @brief Puts all Stream settings into one object to reduce clutter and noise in code.
     *
     * @tparam E - Enum type
     * @tparam MAX_STREAM_LEN_ - Maximum number of bytes for a stream.
     * @tparam MAX_STRING_STORAGE_ - Maximum number of byte for string storage.
     * @tparam MAX_ITEMS_STORAGE_ - Maximum number of items to store.
     * @tparam Variant - std::variant of all items to store.
     */
    template <typename E, size_t MAX_STREAM_LEN_, size_t MAX_STRING_STORAGE_, size_t MAX_ITEMS_STORAGE_, typename Variant>
    struct EncodingContextSettings {
      using Enum = E;
      constexpr static size_t MAX_STREAM_LEN     { MAX_STREAM_LEN_ };
      constexpr static size_t MAX_STRING_STORAGE { MAX_STRING_STORAGE_ };
      constexpr static size_t MAX_ITEMS_STORAGE  { MAX_ITEMS_STORAGE_ };
      using ItemVariant = Variant;
    };

    using program_cursor_t = char*;

    template <typename Settings>
    class EncodingContext {

      /// Stream being built
      string<Settings::MAX_STREAM_LEN+1> program{ Settings::MAX_STREAM_LEN };
      /// Where the current end of the stream is
      program_cursor_t program_end{ program.begin() };
      
      /// String storage
      Strings<Settings::MAX_STRING_STORAGE> strings{};
      
      /// Item storage
      using Variant = typename Settings::ItemVariant;
      Items<Settings::MAX_ITEMS_STORAGE, Variant> items{};
      
      // Are ints to be stored compressed?
      bool compress{};

    public:

     /**
      * @brief Returns a pointer to the next byte to be written in the program buffer.
      *
      * @return program_cursor_t - Current write position in the program buffer.
      */
      constexpr program_cursor_t program_cursor() {
        return program_end;
      }

      /**
       * @brief Registers a string in the backing string store.
       *
       * @param value - String to store.
       * @return string_id_t - Stable id for later lookups.
       */
      constexpr string_id_t add_string(std::string_view value) {
        return static_cast<string_id_t>(strings.add_string(value));
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
        return static_cast<item_id_t>(items.add_item(item));
      }

      /**
       * @brief Encodes the string into the stream.
       * 
       * @param id - String id.
       */
      constexpr void encode_string(string_id_t id) {
        std::string_view s { strings.get_string(id) };
        // +1 is valid for Constexpr::string template.  Allows copying the implicit NUL character.
        program_end = copy(s.begin(), s.end()+1, program_end);
      }

      /**
       * @brief Encodes a value in the stream, compressed as a condensed dnum if
       *   compress set, otherwise full width.
       *
       * @tparam T - Type of value.
       * @param value - Value to encode.
       * @param scope_bitmask - Bitmask to constrain and condense value.
       */
      template <typename T>
      constexpr void encode_int(T value, T scope_bitmask) {
        assert((value & scope_bitmask) == value || !"value must be a subset of scope_bitmask");
        if (compress) {
          assert(sizeof(T) > 1 || !"Can't compress a type that has a byte length of 1.");
          auto mask { make_unsigned_equivalent(scope_bitmask) };
          auto cvalue { condense(mask, make_unsigned_equivalent(value), true) };
          program_end = Constexpr::encode_dint(program_end, program.end(), cvalue);
        } else {
          program_end = Constexpr::encode_int(program_end, program.end(), value);
        }
      }

      /**
       * @brief Encodes eEnumCommand in the program.
       *
       * @param value - Value to encode.
       */
      constexpr void encode_int(eEnumCommand cmd) {
        program_end = Constexpr::encode_int(program_end, program.end(), cmd);
      }

      /**
       * @brief Gets an item who's type hasn't yet been resolved from storage.
       *
       * @param item_id - Id to reference requested item.
       * @return Variant& - Item retrieved.
       */
      constexpr Variant& item(item_id_t item_id) {
        return items.get_item(item_id);
      }

      /**
       * @brief Gets an item as a specific type from storage.
       * 
       * @param item_id - Id to reference requested item.
       * @return Item& - Item retrieved.
       */
      template <typename Item>
      constexpr Item& item(item_id_t item_id) {
        return items.template get_item<Item>(item_id);
      }
    };

    // This might not be needed.  Maybe useful for symbolic access to maximum number of distinct
    // items.  The rest...?  Meh.
    enum class eBlockType {
                                     // Range   Max Number of
                                     //         distinct items

      Pair           = 0b0'0'000'10, /// 1-16   16
      IfPair         = 0b0'0'001'01, /// 0-15   15
      ElsePair       = 0b0'0'010'00, /// 1-8    8
      ContPair       = 0b0'0'011'11, /// 1-32   32
      IfCmd          = 0b0'1'001'01, /// 0-15   15
      ElseCmd        = 0b0'1'010'00, /// 1-8    8
      ContCmd        = 0b0'1'011'11, /// 1-32   32

      mBlockType     = 0b0'0'111'00,
      Named          = 0b0'0'000'00, // Laying down pairs          for Named block
      If             = 0b0'0'001'00, // Laying down pairs/commands for If block
      Else           = 0b0'0'010'00, // Laying down pairs/commands for Else block
      Continue       = 0b0'0'011'00, // Laying down pairs/commands for Cont block
      Global         = 0b0'0'100'00, // Laying down       commands for Global block

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
      // eBlockType           part_of_block{ eBlockPartOf::Global };
      int remaining_items_allowed_in_block{}; // ignored for global block

      void set_scope_bitmask(E new_bitmask) {
        assert((scope_bitmask & new_bitmask) == new_bitmask || !"new_bitmask must be a subset of the scope_bitmask");
        scope_bitmask = new_bitmask;
      }
    };

    template <typename Settings, typename E, typename FnEncodeBlock>
    constexpr program_cursor_t encode_block(
      EncodingContext<Settings>& ec, ScopeData<E>& sd, eBlockType block_type, FnEncodeBlock fn_encode_block)
    {
      program_cursor_t pc { ec.program_cursor() };
      if (block_type != eBlockType::Global) {
        ec.encode_int(eEnumCommand::Terminate); // add an empty opcode
      }

      ScopeData<E> new_sd { sd };
      size_t max_items { max_items_for_block(block_type) };
      new_sd.remaining_items_allowed_in_block = max_items;

      fn_encode_block(new_sd);

      // Update the opcode's count parameter
      size_t encoded_pair_count { max_items - new_sd.remaining_items_allowed_in_block };
      if (does_count_start_from_one(block_type)) {
        encoded_pair_count -= 1;
      }
      *pc |= static_cast<char>(encoded_pair_count);
      return pc;
    }

    template <typename E>
    struct Pairs {
      E value{};
      string_id_t name_id{};
      item_id_t next_pairs_id{};

      template <typename Settings>
      constexpr auto encode(EncodingContext<Settings>& ec, ScopeData<E>& sd) {
        if (sd.remaining_items_allowed_in_block) {
          sd.remaining_items_allowed_in_block -= 1;
          ec.encode_int(value, sd.scope_bitmask);
          ec.encode_string(name_id);
          if (next_pairs_id) {
            ec.template item<Pairs<E>>(next_pairs_id).encode(ec, sd);
          }
        } else {
          program_cursor_t pc =
            encode_block(ec, sd, eBlockType::Continue,
              [&](ScopeData<E>& new_sd) { encode(ec, new_sd); });
          *pc |= eEnumCommand::ContinueScope;
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

      template <typename Settings>
      constexpr auto encode(EncodingContext<Settings>& ec, ScopeData<E>& sd) {
        assert(pairs_id || !"Can't define a Named block with no pairs.");

        program_cursor_t pc =
          encode_block(ec, sd, eBlockType::Named,
            [&](ScopeData<E>& new_sd) {
              if (has_mask) {
                ec.encode_int(mask, sd.scope_bitmask);
                new_sd.set_scope_bitmask(mask);
              }

              ec.template item<Pairs<E>>(pairs_id).encode(ec, new_sd);
          });

        // Update the opcode
        eEnumCommand opCode {};
        if (has_mask) {
          opCode = (eEnumCommand::Named | eEnumCommand::fHasBitmask);
        } else {
          opCode = (eEnumCommand::Named);
        }
        *pc |= opCode;
      }
    };

    template <typename E>
    struct Numeric {
      E mask{};
      eEnumCommand format{};
      string_id_t name_id{};

      template <typename Settings>
      constexpr auto encode(EncodingContext<Settings>& ec, ScopeData<E>& sd) {
        ec.encode_int(mask, sd.scope_bitmask);
        ec.encode_string(name_id);
      }
    };

    template <typename E>
    struct Cmds;

    enum class eGroupEncodingForm : std::uint8_t {
      None,
      NamedPairs,
      Numeric,
      Commands,
    };

    template <typename E>
    struct Group {
      string_id_t name_id{};
      item_id_t cmds_id{};   // Type: Cmds<E>

      /**
       * @brief Classifies how this group can be encoded inside a conditional.
       *
       * A missing command list has no branch body. A single unmasked
       * `Named` command can be emitted as pairs. A single `Numeric` command can
       * be emitted inline. Everything else requires a command-style branch.
       *
       * @tparam Settings - Encoding-context settings type.
       * @param ec - Encoding context that owns this group's items.
       * @return eGroupEncodingForm - Best conditional encoding form.
       */
      template <typename Settings>
      constexpr eGroupEncodingForm encoding_form(EncodingContext<Settings>& ec) const {
        if (!cmds_id) {
          return eGroupEncodingForm::None;
        }

        auto const& cmds { ec.template item<Cmds<E>>(cmds_id) };
        if (cmds.next_id) {
          return eGroupEncodingForm::Commands;
        }

        auto& cmd { ec.item(cmds.command_id) };
        if (auto const named { std::get_if<Named<E>>(&cmd) }) {
          return named->has_mask ? eGroupEncodingForm::Commands : eGroupEncodingForm::NamedPairs;
        }
        if (std::get_if<Numeric<E>>(&cmd)) {
          return eGroupEncodingForm::Numeric;
        }
        return eGroupEncodingForm::Commands;
      }

      // Group owner will add string if needed and will call encode_block as well.
      // This is just a placeholder for info, so not defining this function.
      //
      // template <typename Settings>
      // constexpr auto encode(EncodingContext<Settings>& ec, ScopeData<E>& sd) {
      // }
    };

    template <typename E>
    class Conditional {
      E group_bitmask; 
      item_id_t  true_group_id;
      item_id_t false_group_id;

      template <typename Settings>
      constexpr auto encode(EncodingContext<Settings>& ec, ScopeData<E>& sd) {
        ScopeData<E> new_sd { sd };

        // If true_cmds exist and have only one Numeric then
        //   encode true_cmds
        //   update opcode to GroupIfNumeric
        //   If false_cmds exist
        //     If false_cmds have only one Named without mask, then
        //       encode false_cmds
        //       update opcode to Else
        //     Else
        //       encode false_cmds
        //       update opcode to Else | fElseCmds
        // Else if false_cmds exist and have only one Numeric then
        //   encode false_cmds
        //   update opcode to GroupIfNumeric | Negate
        //   If true_cmds exist
        //     If true_cmds have only one Named without mask, then
        //       encode true_cmds
        //       update opcode to Else
        //     Else
        //       encode true_cmds
        //       update opcode to Else | fElseCmds
        // Else if true_cmds exist and have only one Named without mask, then
        //   encode true_cmds
        //   update opcode to GroupIfNamed
        //   If false_cmds exist
        //     If false_cmds have only one Named without mask, then
        //       encode false_cmds
        //       update opcode to Else
        //     Else
        //       encode false_cmds
        //       update opcode to Else | fElseCmds
        // else
        //   emit GroupIf
        //   encode true_cmds
        //   If false_cmds exist
        //     If false_cmds have only one Named without mask, then
        //       encode false_cmds
        //       update opcode to Else
        //     Else
        //       encode false_cmds
        //       update opcode to Else | fElseCmds
        
        // Encode true commands/pairs
        program_cursor_t pc =
          encode_block(ec, new_sd, eBlockType::If, [&](ScopeData<E> new_sd) {
            // need to add name for group if specified in true group
            (void)new_sd;
          });
        // update opcode to appropriate GroupIf* command code

        // Encode false commands/pairs (else)
        pc =
          encode_block(ec, new_sd, eBlockType::If, [&](ScopeData<E> new_sd) {
            // need to add name for group if specified in true group
            (void)new_sd;
          });
        // update opcode to appropriate GroupIf* command code
        
        (void)pc;
      }
    };

    template <typename E>
    struct Cmds {
      item_id_t command_id;
      item_id_t next_id;

      template <typename Settings>
      constexpr auto encode(EncodingContext<Settings>& ec, ScopeData<E>& sd) {
        auto dispatch = overload{
          [&](Named<E>&       cmd) { cmd.encode(ec, sd); },
          [&](Numeric<E>&     cmd) { cmd.encode(ec, sd); },
          [&](Conditional<E>& cmd) { cmd.encode(ec, sd); },
        };
        dispatch(ec.item(command_id));

        if (next_id) {
          ec.template item<Cmds<E>>(next_id).encode(ec, sd);
        }
      }
    };

    struct ConditionalEncodingPlan {
      item_id_t inline_group_id{};
      item_id_t else_group_id{};
      eEnumCommand if_opcode{};
      eEnumCommand else_opcode{};
      bool has_else_branch{};
    };

    /**
     * @brief Classifies a stored group by how a conditional can encode it.
     *
     * @tparam Settings - Encoding-context settings type.
     * @tparam E - Enum value type used by the stored items.
     * @param ec - Encoding context that owns the group item.
     * @param group_id - Group id to classify. Zero means the branch is absent.
     * @return eGroupEncodingForm - Conditional encoding form for the branch.
     */
    template <typename Settings, typename E>
    constexpr eGroupEncodingForm classify_group_encoding_form(
      EncodingContext<Settings>& ec, item_id_t group_id)
    {
      if (!group_id) {
        return eGroupEncodingForm::None;
      }
      return ec.template item<Group<E>>(group_id).encoding_form(ec);
    }

    /**
     * @brief Chooses the conditional opcodes that implement the documented branch-selection rules.
     *
     * @param true_form - Encoding form available for the true branch.
     * @param true_group_id - Item id for the true branch group.
     * @param false_form - Encoding form available for the false branch.
     * @param false_group_id - Item id for the false branch group.
     * @return ConditionalEncodingPlan - Selected inline/else groups and opcodes.
     */
    constexpr ConditionalEncodingPlan make_conditional_encoding_plan(
      eGroupEncodingForm true_form,
      item_id_t true_group_id,
      eGroupEncodingForm false_form,
      item_id_t false_group_id)
    {
      ConditionalEncodingPlan plan{};

      if (true_form == eGroupEncodingForm::Numeric) {
        plan.inline_group_id = true_group_id;
        plan.else_group_id = false_group_id;
        plan.if_opcode = eEnumCommand::GroupIfNumeric;
      } else if (false_form == eGroupEncodingForm::Numeric) {
        plan.inline_group_id = false_group_id;
        plan.else_group_id = true_group_id;
        plan.if_opcode = (eEnumCommand::GroupIfNumeric | eEnumCommand::fNegate);
      } else if (true_form == eGroupEncodingForm::NamedPairs) {
        plan.inline_group_id = true_group_id;
        plan.else_group_id = false_group_id;
        plan.if_opcode = eEnumCommand::GroupIfNamed;
      } else {
        plan.inline_group_id = true_group_id;
        plan.else_group_id = false_group_id;
        plan.if_opcode = eEnumCommand::GroupIf;
      }

      eGroupEncodingForm const else_form {
        plan.inline_group_id == true_group_id ? false_form : true_form
      };
      if (else_form != eGroupEncodingForm::None) {
        plan.has_else_branch = true;
        plan.else_opcode = eEnumCommand::Else;
        if (else_form != eGroupEncodingForm::NamedPairs) {
          plan.else_opcode = (plan.else_opcode | eEnumCommand::fElseCmds);
        }
      }

      return plan;
    }

    /**
     * @brief Chooses the conditional opcodes for two stored groups.
     *
     * @tparam Settings - Encoding-context settings type.
     * @tparam E - Enum value type used by the stored items.
     * @param ec - Encoding context that owns the referenced groups.
     * @param true_group_id - Item id for the true branch group.
     * @param false_group_id - Item id for the false branch group.
     * @return ConditionalEncodingPlan - Selected inline/else groups and opcodes.
     */
    template <typename Settings, typename E>
    constexpr ConditionalEncodingPlan make_conditional_encoding_plan(
      EncodingContext<Settings>& ec, item_id_t true_group_id, item_id_t false_group_id)
    {
      return make_conditional_encoding_plan(
        classify_group_encoding_form<Settings, E>(ec, true_group_id),
        true_group_id,
        classify_group_encoding_form<Settings, E>(ec, false_group_id),
        false_group_id);
    }


  }

} // namespace Constexpr
