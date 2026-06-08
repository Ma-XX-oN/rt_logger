/**
 * @file enum_core.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Central Enum description container.
 * @version 0.1
 * @date 2026-05-28
 *
 * @copyright Copyright (c) 2026
 */
#ifndef CONSTEXPR_ENUM_CORE_HPP
#define CONSTEXPR_ENUM_CORE_HPP

#include "enum_ir.hpp"
#include <ostream>
#include <variant>

namespace Constexpr {

  template <typename EnumT>
  class EnumValueView;

  template <std::uint32_t StringAndItemCapacity>
  class AnyEnumDescription;

  template <typename AnyEnumT>
  class AnyEnumValueView;

  /**
   * @brief Immutable enum-definition storage plus builder-time mutation helpers.
   *
   * @tparam Settings - Representation settings.
   */
  template <typename Settings>
  class Enum {
  public:
      // Underlying enum or integral value type
    using value_type = typename Settings::value_type;
    // Command type stored in items storage
    using command_t = std::variant<
      impl::Pairs<value_type>, impl::Named<value_type>, impl::Numeric<value_type>,
      impl::Cmds<value_type>, impl::Group<value_type>, impl::Conditional<value_type>
    >;

  private:
    // Command id where the program starts
    item_id_t m_cmds_id{};
    // Storage for all strings
    Strings<Settings::MAX_STRING_STORAGE> strings{};
    // Storage for all command items
    Items<Settings::MAX_ITEMS_STORAGE, command_t> items{};

    /**
     * @brief Emits the full headered program through one writer surface.
     *
     * @tparam WriterT - Writer type compatible with impl::EnumEncoder.
     * @param writer - Destination writer or counting sink.
     * @param compress - Whether constrained values should be dint-condensed.
     *   Ignored if \c sizeof(value_type)==1 and implicitly set to \c false.
     * @param append_terminate - Whether to append an explicit Terminate opcode.
     * @return WriterT& - The updated writer.
     */
    template <typename WriterT>
    constexpr WriterT& output_program_impl(
      WriterT& writer,
      bool compress = false,
      bool append_terminate = false) const
    {
      compress = compress && sizeof(value_type) > 1;
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

} // namespace Constexpr

#endif // CONSTEXPR_ENUM_CORE_HPP
