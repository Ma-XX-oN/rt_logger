/**
 * @file enum_any.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Type-erased (variant-backed) wrappers for runtime-selected enum descriptions.
 * @version 0.1
 * @date 2026-05-28
 *
 * @copyright Copyright (c) 2026
 */
#ifndef CONSTEXPR_ENUM_ANY_HPP
#define CONSTEXPR_ENUM_ANY_HPP

#include "enum_render.hpp"
#include "enum_builder.hpp"

namespace Constexpr {

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

#endif // CONSTEXPR_ENUM_ANY_HPP
