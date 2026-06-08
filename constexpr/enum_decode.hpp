/**
 * @file enum_decode.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Stream decoder that rebuilds typed enum graphs from definition programs.
 * @version 0.1
 * @date 2026-05-28
 *
 * @copyright Copyright (c) 2026
 */
#ifndef CONSTEXPR_ENUM_DECODE_HPP
#define CONSTEXPR_ENUM_DECODE_HPP

#include "enum_core.hpp"
#include "ThrowNoThrow.hpp"

namespace Constexpr { namespace impl {

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
       * @return unsigned_value_type - Root scope bitmask.
       */
      static constexpr unsigned_value_type full_scope_bitmask() noexcept {
        return ~unsigned_value_type{};
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
       * @param scope_bitmask - Parent scope bitmask (unsigned).
       * @return bool - \c true when \p value is fully contained in \p scope_bitmask.
       */
      static constexpr bool is_subset_of_scope(value_type value, unsigned_value_type scope_bitmask) noexcept {
        auto const value_bits{ make_unsigned_equivalent(value) };
        return (value_bits & scope_bitmask) == value_bits;
      }

      /**
       * @brief Validates that a constrained value fits inside the current
       *   parent scope.
       *
       * @param value - Candidate constrained value.
       * @param scope_bitmask - Parent scope bitmask (unsigned).
       * @throws EnumParseInvalidStructure when \p value exceeds \p scope_bitmask.
       */
      static constexpr void verify_scope_subset(value_type value, unsigned_value_type scope_bitmask) {
        if (!is_subset_of_scope(value, scope_bitmask)) {
          throw EnumParseInvalidStructure("Constrained value exceeds the parent scope bitmask.");
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
       * @param scope_bitmask - Parent scope bitmask that constrains the value (unsigned).
       * @return unsigned_value_type - Decoded single-bit group mask.
       */
      constexpr unsigned_value_type read_scoped_group_mask_value(unsigned_value_type scope_bitmask) {
        auto const group_shift{ read_fixed_width_integer<std::uint8_t>() };
        if (group_shift >= std::numeric_limits<unsigned_value_type>::digits) {
          throw EnumParseInvalidStructure("group_shift value exceeds number of bits in value_type.");
        }
        auto const group_mask{ static_cast<unsigned_value_type>(unsigned_value_type{1} << group_shift) };
        auto const recon{ scope_bitmask & group_mask };
        if (recon != group_mask) {
          throw EnumParseInvalidStructure("Bit value is not representable under the parent scope bitmask.");
        }
        return group_mask;
      }

      /**
       * @brief Reads one constrained value using the stream's compression mode.
       *
       * @param scope_bitmask - Parent scope bitmask that constrains the value (unsigned).
       * @return value_type - Decoded constrained value.
       */
      constexpr value_type read_scoped_value(unsigned_value_type scope_bitmask) {
        if (!m_compress) {
          auto const raw{ read_fixed_width_integer<underlying_value_type>() };
          value_type const value{ static_cast<value_type>(raw) };
          verify_scope_subset(value, scope_bitmask);
          return value;
        }

        auto const condensed{ read_dint_integer<unsigned_value_type>() };
        value_type const expanded{ expand<value_type>(scope_bitmask, condensed, true) };
        auto const recon{ make_unsigned_equivalent(condense(scope_bitmask, expanded, true)) };
        if (recon != condensed) {
          throw EnumParseInvalidStructure("Condensed value is not representable under the parent scope bitmask.");
        }
        return expanded;
      }

      /**
       * @brief Reads one constrained bitmask using the stream's compression mode.
       *
       * Same wire format as \c read_scoped_value but returns an unsigned result
       * for storage in mask fields.
       *
       * @param scope_bitmask - Parent scope bitmask that constrains the value (unsigned).
       * @return unsigned_value_type - Decoded constrained bitmask.
       */
      constexpr unsigned_value_type read_scoped_bitmask(unsigned_value_type scope_bitmask) {
        return make_unsigned_equivalent(read_scoped_value(scope_bitmask));
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
       * @param command_mask - Stored command-local bitmask (unsigned) when \p
       *   has_mask is true.
       * @param value - Decoded pair value.
       * @param name_id - Stored string id for the pair name.
       */
      constexpr void append_pair(
        item_id_t& named_id,
        item_id_t& last_pair_id,
        bool has_mask,
        unsigned_value_type command_mask,
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
       * @param command_mask - Stored command-local bitmask (unsigned) when \p
       *   has_mask is true.
       * @return item_id_t - Stored named-command id, or zero when the branch is
       *   empty.
       */
      constexpr item_id_t parse_pair_branch(
        std::size_t initial_count,
        unsigned_value_type pair_scope_bitmask,
        bool has_mask,
        unsigned_value_type command_mask)
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
      constexpr item_id_t parse_command_branch(std::size_t initial_count, unsigned_value_type scope_bitmask) {
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
      constexpr item_id_t parse_optional_else_group(unsigned_value_type scope_bitmask) {
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
      constexpr item_id_t parse_named_command(std::uint8_t opcode, unsigned_value_type scope_bitmask) {
        bool const has_bitmask{ (opcode & static_cast<std::uint8_t>(eEnumCommand::fHasBitmask)) != 0u };
        unsigned_value_type const command_mask{
          has_bitmask ? read_scoped_bitmask(scope_bitmask) : unsigned_value_type{}
        };
        unsigned_value_type const pair_scope_bitmask{ has_bitmask ? command_mask : scope_bitmask };
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
      constexpr item_id_t parse_numeric_command(std::uint8_t opcode, unsigned_value_type scope_bitmask) {
        validate_numeric_opcode(opcode);
        unsigned_value_type const bitmask{ read_scoped_bitmask(scope_bitmask) };
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
      constexpr item_id_t parse_conditional_command(std::uint8_t opcode, unsigned_value_type scope_bitmask) {
        eEnumCommand const kind{
          static_cast<eEnumCommand>(opcode & static_cast<std::uint8_t>(eEnumCommand::mOpCode))
        };

        unsigned_value_type const group_bitmask{ read_scoped_group_mask_value(scope_bitmask) };
        verify_group_bitmask(group_bitmask);
        unsigned_value_type const branch_scope_bitmask{ read_scoped_bitmask(scope_bitmask) };
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
      constexpr item_id_t parse_command(unsigned_value_type scope_bitmask) {
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

} // namespace Constexpr

#endif // CONSTEXPR_ENUM_DECODE_HPP
