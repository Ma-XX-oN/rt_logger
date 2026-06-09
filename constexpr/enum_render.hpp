/**
 * @file enum_render.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Text renderer and value-view for typed enum descriptions.
 * @version 0.1
 * @date 2026-05-28
 *
 * @copyright Copyright (c) 2026
 */
#ifndef CONSTEXPR_ENUM_RENDER_HPP
#define CONSTEXPR_ENUM_RENDER_HPP

#include "enum_core.hpp"
#include <string>
#include <ostream>

namespace Constexpr {
  namespace impl {

    /**
     * @brief Renders one enum description plus runtime value into output text.
     *
     * TODO: May want to make this constexpr.  Have to think about it.
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
       * @return unsigned_value_type - All-bits-set scope bitmask for \c
       *   value_type.
       */
      static constexpr unsigned_value_type full_scope_bitmask() noexcept {
        return ~unsigned_value_type{};
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
       * @param scope_bitmask - Active scope bitmask (unsigned) for the current command list.
       */
      void render_named(Named<value_type> const& named, unsigned_value_type scope_bitmask) {
        auto const effective_bitmask{
          named.has_mask
            ? named.mask
            : scope_bitmask
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
       * @param scope_bitmask - Parent scope bitmask (unsigned); present for signature symmetry.
       */
      void render_conditional(Conditional<value_type> const& conditional, unsigned_value_type scope_bitmask) {
        (void)scope_bitmask;

        bool const condition_met{
          (make_unsigned_equivalent(m_value) & conditional.group_bitmask) != 0u
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
       * @param scope_bitmask - Active scope bitmask (unsigned) for that command list.
       */
      void render_cmds(item_id_t cmds_id, unsigned_value_type scope_bitmask) {
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

  } // namespace impl

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

  /**
  * @brief Streamable render view for one static constexpr enum definition.
  *
  * The enum definition is supplied as a non-type template parameter (NTTP), so
  * the view only stores the runtime enum value.
  *
  * @tparam EnumDef - Static constexpr enum definition object.
  */
  template <auto const& EnumDef>
  class ConstexprEnumValueView {
    using enum_type = std::remove_cv_t<
      std::remove_reference_t<decltype(EnumDef)>>;

    typename enum_type::value_type m_value{};

  public:
    using value_type = typename enum_type::value_type;

    /**
    * @brief Constructs a render view for one runtime value.
    *
    * @param value - Runtime value to interpret.
    */
    constexpr explicit ConstexprEnumValueView(value_type value) noexcept
    : m_value{ value }
    {
    }

    /**
    * @brief Returns the static enum definition bound to this view type.
    *
    * @return enum_type const& - Static enum definition.
    */
    static constexpr enum_type const& enum_def() noexcept {
      return EnumDef;
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
    * @brief Renders the bound enum definition and runtime value into text.
    *
    * @return std::string - Final rendered text.
    */
    std::string to_string() const {
      return impl::EnumTextRenderer<enum_type>{ enum_def(), value() }.render();
    }
  };

  /**
  * @brief Callable wrapper for one static constexpr enum definition.
  *
  * \c operator() and \c value() returns a one-member view because the enum
  * definition is carried by the view type.
  *
  * @tparam EnumDef - Static constexpr enum definition object.
  */
  template <auto const& EnumDef>
  class ConstexprEnum {
    using enum_type = std::remove_cv_t<
      std::remove_reference_t<decltype(EnumDef)>>;

  public:
    using value_type = typename enum_type::value_type;
    using constexpr_view_type = ConstexprEnumValueView<EnumDef>;

    /**
    * @brief Returns the static enum definition wrapped by this object.
    *
    * @return enum_type const& - Static enum definition.
    */
    static constexpr enum_type const& enum_def() noexcept {
      return EnumDef;
    }

    /**
    * @brief Binds a value using a one-member constexpr enum-value view.
    *
    * @param enum_value - Runtime value to interpret.
    * @return constexpr_view_type - One-member streamable render view.
    */
    constexpr constexpr_view_type operator()(value_type const& enum_value) const
      noexcept
    {
      return constexpr_view_type{ enum_value };
    }

    /**
    * @brief Binds a value using a one-member constexpr enum-value view.
    *
    * @param enum_value - Runtime value to interpret.
    * @return constexpr_view_type - One-member streamable render view.
    */
    constexpr constexpr_view_type value(value_type const& enum_value) const noexcept {
      return constexpr_view_type{ enum_value };
    }

    /**
    * @brief Returns the packed used string/item space of the enum definition.
    *
    * @return std::uint32_t - Packed used-space summary.
    */
    constexpr std::uint32_t used_space() const noexcept {
      return enum_def().used_space();
    }

    /**
    * @brief Returns the packed allocated string/item capacity of the enum definition.
    *
    * @return std::uint32_t - Packed allocated-space summary.
    */
    constexpr std::uint32_t allocated_space() const noexcept {
      return enum_def().allocated_space();
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
      return enum_def().program_size(compress, append_terminate);
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
      return enum_def().output_program(begin, end, compress, append_terminate);
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
      return enum_def().output_program(compress, append_terminate);
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
      return enum_def().output_program(os, compress, append_terminate);
    }
  };

  /**
  * @brief Streams one rendered constexpr enum value view.
  *
  * @tparam EnumDef - Static constexpr enum definition object.
  * @param stream - Destination output stream.
  * @param value_view - Enum value view to render.
  * @return std::ostream& - Destination output stream.
  */
  template <auto const& EnumDef>
  std::ostream& operator<<(
    std::ostream& stream,
    ConstexprEnumValueView<EnumDef> const& value_view)
  {
    return stream << value_view.to_string();
  }

} // namespace Constexpr

#endif // CONSTEXPR_ENUM_RENDER_HPP
