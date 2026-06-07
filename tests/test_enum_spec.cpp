#include "constexpr/enum.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

using TestEnum = std::uint8_t;
using TestPairs = Constexpr::impl::Pairs<TestEnum>;
using TestNamed = Constexpr::impl::Named<TestEnum>;
using TestNumeric = Constexpr::impl::Numeric<TestEnum>;
using TestConditional = Constexpr::impl::Conditional<TestEnum>;
using TestGroup = Constexpr::impl::Group<TestEnum>;
using TestCmds = Constexpr::impl::Cmds<TestEnum>;
using TestSettings = Constexpr::EnumSettings<TestEnum, Constexpr::pack_space(128, 32)>;
using LargeTestSettings = Constexpr::EnumSettings<TestEnum, Constexpr::pack_space(256, 64)>;
using TestEnumDef = Constexpr::Enum<TestSettings>;
using LargeTestEnumDef = Constexpr::Enum<LargeTestSettings>;
using AnyTestEnumDef = Constexpr::AnyEnumDescription<Constexpr::pack_space(128, 32)>;
using TestBuilder = Constexpr::EnumBuilder<TestSettings>;
using TestEncoder = Constexpr::impl::EnumEncoder<TestEnumDef>;
using TestItemId = Constexpr::item_id_t;
using TestStringId = Constexpr::string_id_t;
using TestProgram = Constexpr::string<129>;
using TestScopeData = Constexpr::impl::ScopeData<TestEnum>;
using Constexpr::eEnumCommand;
using Constexpr::eEnumStorageType;

/**
 * @brief Returns the expected one-byte storage header for a value type used in
 * the enum stream tests.
 *
 * @tparam ValueT - Enum or integral value type.
 * @param compress - Whether to set the stream's compression flag.
 * @return std::uint8_t - Storage-type header byte.
 */
template <typename ValueT>
constexpr std::uint8_t storage_header_for(bool compress = false) {
  auto header{ static_cast<std::uint8_t>(Constexpr::impl::storage_type_for_value_type<ValueT>()) };
  if (compress) {
    header = static_cast<std::uint8_t>(header | static_cast<std::uint8_t>(eEnumStorageType::Compress));
  }
  return header;
}

constexpr char kDecodeProgram[]{
  static_cast<char>(storage_header_for<std::uint8_t>()),
  static_cast<char>(static_cast<std::uint8_t>(eEnumCommand::Named) | 0x01u), // Named with 2 elements
  static_cast<char>(0x01u), 'o', 'n', 'e', '\0',
  static_cast<char>(0x02u), 't', 'w', 'o', '\0',
};

constexpr std::string_view kDecodeProgramSv{ kDecodeProgram, sizeof(kDecodeProgram) };

constexpr bool builder_end_returns_exact_parent{
  std::is_same_v<
    decltype(Constexpr::build_enum_description<TestSettings>().If(TestEnum{ 0x80u }, TestEnum{ 0x0Fu }).End()),
    TestBuilder&>
};
static_assert(builder_end_returns_exact_parent);

// Prove that a fully built enum description can be materialized during constant evaluation.
constexpr bool builder_build_materializes_constexpr_enum{
  [] {
    constexpr auto enum_def{
      Constexpr::build_enum_description<Constexpr::EnumSettings<int>>()
        .Named(TestEnum{ 0x01u }, "one")
        .Named(TestEnum{ 0x02u }, "two")
        .Build()
    };

    static_assert(sizeof(enum_def) == 2336);  // assumes 8 byte alignment
    static_assert(Constexpr::impl::string_space(enum_def.allocated_space()) == 256); // default is 256 storable characters, even if not used.
    static_assert(Constexpr::impl::item_space(enum_def.allocated_space()) == 128);   // default is 128 storable items, even if not used.
    return enum_def.cmds_id() != 0u;
  }()
};
static_assert(builder_build_materializes_constexpr_enum);

// Prove that the minimal-size two-pass macro can also build an enum during constant evaluation.
constexpr bool build_enum_macro_materializes_constexpr_enum{
  [] {
    constexpr auto enum_def{
      BUILD_ENUM_DESCRIPTION(TestEnum,
        .Named(TestEnum{ 0x01u }, "one")
        .Named(TestEnum{ 0x02u }, "two"))
    };

    static_assert(sizeof(enum_def) == 88);  // assumes 8 byte alignment
    static_assert(Constexpr::impl::string_space(enum_def.allocated_space()) == 8); // 8 characters stored including NUL terminators
    static_assert(Constexpr::impl::item_space(enum_def.allocated_space()) == 4);   // 4 items stored, 2 Named and 2 Cmds.
                                                                                       // default is 256 and 256

    return enum_def.cmds_id() != 0u;
  }()
};
static_assert(build_enum_macro_materializes_constexpr_enum);

// Prove that the two-pass macro can also replay a multi-step conditional builder chain during constant evaluation.
constexpr bool build_enum_macro_materializes_conditional_constexpr_enum{
  [] {
    constexpr auto enum_def{
      BUILD_ENUM_DESCRIPTION(TestEnum,
        .If(TestEnum{ 0x80u }, TestEnum{ 0x0Fu })
          .Named(TestEnum{ 0x01u }, "if-one")
        .Else()
          .Named(TestEnum{ 0x02u }, "else-two")
        .End())
    };

    auto const& root_cmds{ enum_def.item<TestCmds>(enum_def.cmds_id()) };
    auto const& conditional{ enum_def.item<TestConditional>(root_cmds.command_id) };
    return enum_def.cmds_id() != 0u
      && conditional.true_group_id != 0u
      && conditional.false_group_id != 0u;
  }()
};
static_assert(build_enum_macro_materializes_conditional_constexpr_enum);

// Prove that a valid stream can decode directly into a constexpr enum during constant evaluation.
constexpr bool decode_program_materializes_constexpr_enum{
  [] {
    constexpr auto enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .decode_program(kDecodeProgramSv)
        .Build()
    };

    static_assert(Constexpr::impl::string_space(enum_def.used_space()) == 8u);
    static_assert(Constexpr::impl::item_space(enum_def.used_space()) == 4u);
    return enum_def.cmds_id() != 0u;
  }()
};
static_assert(decode_program_materializes_constexpr_enum);

// Prove that the two-pass macro can replay decode_program() and still shrink to the minimal enum size.
constexpr bool build_enum_macro_decodes_constexpr_program{
  [] {
    constexpr auto enum_def{
      BUILD_ENUM_DESCRIPTION(TestEnum,
        .decode_program(kDecodeProgramSv))
    };

    static_assert(Constexpr::impl::string_space(enum_def.allocated_space()) == 8u);
    static_assert(Constexpr::impl::item_space(enum_def.allocated_space()) == 4u);
    return enum_def.cmds_id() != 0u;
  }()
};
static_assert(build_enum_macro_decodes_constexpr_program);

// Prove that the public Enum output API can size and encode one full stream during constant evaluation.
constexpr bool enum_output_program_materializes_constexpr_stream{
  [] {
    constexpr auto enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .Named(TestEnum{ 0x01u }, "one")
        .Named(TestEnum{ 0x02u }, "two")
        .Build()
    };

    constexpr std::size_t program_size{ enum_def.program_size() };
    static_assert(program_size == sizeof(kDecodeProgram));

    char program[program_size]{};
    char* const end{ enum_def.output_program(program, program + program_size) };
    return end == program + program_size
      && std::string_view{ program, program_size } == kDecodeProgramSv;
  }()
};
static_assert(enum_output_program_materializes_constexpr_stream);

// Fixture-building helpers create stored group shapes that the runtime tests
// can classify without manually assembling ids inline in each case.

/**
 * @brief Streams one rendered enum value into a standard string.
 *
 * @tparam EnumT - Immutable enum description type.
 * @param enum_def - Immutable enum description.
 * @param value - Runtime value to render.
 * @return std::string - Rendered output text.
 */
template <typename EnumT>
std::string render_enum_value(EnumT const& enum_def, typename EnumT::value_type value) {
  std::ostringstream stream{};
  stream << enum_def(value);
  return stream.str();
}

/**
 * @brief Streams one rendered enum value into a standard string.
 *
 * @param enum_def - Immutable enum description.
 * @param value - Runtime value to render.
 * @return std::string - Rendered output text.
 */
std::string render_enum(TestEnumDef const& enum_def, TestEnum value) {
  return render_enum_value(enum_def, value);
}

/**
 * @brief Encodes one immutable enum description into a runtime-owned stream
 * with the required storage-type header byte.
 *
 * @tparam EnumT - Immutable enum description type.
 * @param enum_def - Immutable enum description to encode.
 * @param compress - Whether constrained values should be emitted as condensed
 *   dints.
 * @param append_terminate - Whether to append an explicit Terminate opcode.
 * @return std::string - Runtime-owned definition stream.
 */
template <typename EnumT>
std::string encode_program_with_header(
  EnumT const& enum_def,
  bool compress = false,
  bool append_terminate = false)
{
  return enum_def.output_program(compress, append_terminate);
}

/**
 * @brief Decodes one runtime enum stream through the builder-chain decoder.
 *
 * @tparam Settings - Destination enum storage settings.
 * @param program - Runtime-owned program bytes.
 * @param throw_on_terminate - Whether a Terminate opcode is rejected.
 * @return Constexpr::Enum<Settings> - Decoded immutable enum description.
 */
template <typename Settings = TestSettings>
Constexpr::Enum<Settings> decode_program(std::string_view program, bool throw_on_terminate = true) {
  return Constexpr::build_enum_description<Settings>()
    .decode_program(program, throw_on_terminate)
    .Build();
}

/**
 * @brief Decodes one runtime enum stream through the variant-backed
 *   runtime-selected builder chain.
 *
 * @tparam StringAndItemCapacity - Fixed backing storage capacity for the
 *   variant-backed runtime-selected decode result.
 * @param program - Runtime-owned program bytes.
 * @param throw_on_terminate - Whether a Terminate opcode is rejected.
 * @return Constexpr::AnyEnumDescription<StringAndItemCapacity> - Decoded
 *   variant-backed runtime-selected enum description.
 */
template <std::uint32_t StringAndItemCapacity = Constexpr::pack_space(128, 32)>
Constexpr::AnyEnumDescription<StringAndItemCapacity> decode_any_program(
  std::string_view program,
  bool throw_on_terminate = true)
{
  return Constexpr::build_any_enum_description<StringAndItemCapacity>()
    .decode_program(program, throw_on_terminate)
    .Build();
}

/**
 * @brief Writes one constrained integer in either full-width or condensed form
 *   for manual decoder tests.
 *
 * @tparam ValueT - Enum or integral value type.
 * @param writer - Destination program writer.
 * @param value - Constrained value to encode.
 * @param scope_bitmask - Parent scope bitmask used for condensed encoding.
 * @param compress - Whether the stream uses compressed constrained values.
 */
template <typename ValueT>
void write_scoped_value(
  Constexpr::impl::ProgramWriter& writer,
  ValueT value,
  ValueT scope_bitmask,
  bool compress)
{
  if (compress) {
    auto const condensed{ Constexpr::condense(scope_bitmask, value, true) };
    writer.write_dint(Constexpr::make_unsigned_equivalent(condensed));
    return;
  }

  writer.write_int(Constexpr::make_underlying_equivalent(value));
}

/**
 * @brief Writes one constrained integer bit as a bit offset count for manual decoder tests.
 *
 * @tparam ValueT - Enum or integral value type.
 * @param writer - Destination program writer.
 * @param value - Constrained value to encode.
 */
template <typename ValueT>
void write_group_shift(
  Constexpr::impl::ProgramWriter& writer,
  ValueT value)
{
  writer.write_int(Constexpr::make_underlying_equivalent(value));
}

/**
 * @brief Builds a runtime-owned manual program with the requested header byte.
 *
 * @tparam Fn - Body-writing callback type.
 * @param header - One-byte storage-type header.
 * @param fn - Callback that appends the program body.
 * @return std::string - Runtime-owned definition stream.
 */
template <typename Fn>
std::string build_manual_program(std::uint8_t header, Fn&& fn) {
  Constexpr::string<2048> program{};
  Constexpr::impl::ProgramWriter writer{ program };
  writer.write_int(header);
  fn(writer);
  writer.finish(program);
  return std::string{ program.data(), program.size() };
}

/**
 * @brief Stores one pair node for a named branch.
 *
 * @param enum_def - Test enum definition storage.
 * @param value - Enum value for the pair.
 * @param name - Pair name to store.
 * @return TestItemId - Stored pair id.
 */
constexpr TestItemId add_pair(TestEnumDef& enum_def, TestEnum value, char const* name) {
  return enum_def.add_item(TestPairs{ value, enum_def.add_string(name), 0 });
}

/**
 * @brief Stores one named command.
 *
 * @param enum_def - Test enum definition storage.
 * @param pairs_id - First pair id for the named command.
 * @param has_mask - Whether the named command carries its own mask.
 * @param mask - Optional explicit mask value.
 * @return TestItemId - Stored named-command id.
 */
constexpr TestItemId add_named(
  TestEnumDef& enum_def, TestItemId pairs_id, bool has_mask = false, TestEnum mask = 0)
{
  return enum_def.add_item(TestNamed{ has_mask, mask, pairs_id });
}

/**
 * @brief Stores one numeric command.
 *
 * @param enum_def - Test enum definition storage.
 * @param mask - Numeric mask value.
 * @param name - Numeric name to store.
 * @return TestItemId - Stored numeric-command id.
 */
constexpr TestItemId add_numeric(TestEnumDef& enum_def, TestEnum mask, char const* name) {
  return enum_def.add_item(TestNumeric{ mask, eEnumCommand{}, enum_def.add_string(name) });
}

/**
 * @brief Stores one command-list node.
 *
 * @param enum_def - Test enum definition storage.
 * @param command_id - Referenced command item id.
 * @param next_id - Optional next command node.
 * @return TestItemId - Stored command-list id.
 */
constexpr TestItemId add_cmd(TestEnumDef& enum_def, TestItemId command_id, TestItemId next_id = 0) {
  return enum_def.add_item(TestCmds{ command_id, next_id });
}

/**
 * @brief Stores one group referencing a command list.
 *
 * @param enum_def - Test enum definition storage.
 * @param cmds_id - Referenced command-list id.
 * @param name - Optional group name to store.
 * @return TestItemId - Stored group id.
 */
constexpr TestItemId add_group(TestEnumDef& enum_def, TestItemId cmds_id, char const* name = nullptr) {
  TestStringId const name_id{
    name ? enum_def.add_string(name) : static_cast<TestStringId>(0u)
  };
  return enum_def.add_item(TestGroup{ name_id, cmds_id });
}

/**
 * @brief Stores one conditional command.
 *
 * @param enum_def - Test enum definition storage.
 * @param group_bitmask - Single-bit conditional selector.
 * @param scope_bitmask - Child scope bitmask shared by both branches.
 * @param true_group_id - Group id for the true branch.
 * @param false_group_id - Group id for the false branch.
 * @return TestItemId - Stored conditional-command id.
 */
constexpr TestItemId add_conditional(
  TestEnumDef& enum_def,
  TestEnum group_bitmask,
  TestEnum scope_bitmask,
  TestItemId true_group_id,
  TestItemId false_group_id)
{
  return enum_def.add_item(TestConditional{
    group_bitmask,
    scope_bitmask,
    true_group_id,
    false_group_id,
  });
}

/**
 * @brief Stores a group that should encode as named pairs in a conditional branch.
 *
 * @param enum_def - Test enum definition storage.
 * @return TestItemId - Stored group id.
 */
constexpr TestItemId add_single_named_group(TestEnumDef& enum_def) {
  TestItemId const pair_id{ add_pair(enum_def, 0x01u, "one") };
  TestItemId const named_id{ add_named(enum_def, pair_id) };
  TestItemId const cmds_id{ add_cmd(enum_def, named_id) };
  return add_group(enum_def, cmds_id);
}

/**
 * @brief Stores a group that should encode as one inline numeric item.
 *
 * @param enum_def - Test enum definition storage.
 * @return TestItemId - Stored group id.
 */
constexpr TestItemId add_single_numeric_group(TestEnumDef& enum_def) {
  TestItemId const numeric_id{ add_numeric(enum_def, 0x03u, "bits") };
  TestItemId const cmds_id{ add_cmd(enum_def, numeric_id) };
  return add_group(enum_def, cmds_id);
}

/**
 * @brief Stores a group that must stay command-shaped because its named item has a mask.
 *
 * @param enum_def - Test enum definition storage.
 * @return TestItemId - Stored group id.
 */
constexpr TestItemId add_masked_named_group(TestEnumDef& enum_def) {
  TestItemId const pair_id{ add_pair(enum_def, 0x02u, "two") };
  TestItemId const named_id{ add_named(enum_def, pair_id, true, 0x03u) };
  TestItemId const cmds_id{ add_cmd(enum_def, named_id) };
  return add_group(enum_def, cmds_id);
}

/**
 * @brief Stores a group that must stay command-shaped because it contains multiple commands.
 *
 * @param enum_def - Test enum definition storage.
 * @return TestItemId - Stored group id.
 */
constexpr TestItemId add_multi_command_group(TestEnumDef& enum_def) {
  TestItemId const first_pair_id{ add_pair(enum_def, 0x04u, "four") };
  TestItemId const second_pair_id{ add_pair(enum_def, 0x08u, "eight") };
  TestItemId const first_named_id{ add_named(enum_def, first_pair_id) };
  TestItemId const second_named_id{ add_named(enum_def, second_pair_id) };
  TestItemId const tail_cmd_id{ add_cmd(enum_def, second_named_id) };
  TestItemId const head_cmd_id{ add_cmd(enum_def, first_named_id, tail_cmd_id) };
  return add_group(enum_def, head_cmd_id);
}

/**
 * @brief Stores a linked list of one-character named pairs.
 *
 * @param enum_def - Test enum definition storage.
 * @param count - Number of pairs to add.
 * @return TestItemId - Stored id of the first pair in the chain.
 */
inline TestItemId add_pair_chain(TestEnumDef& enum_def, int count) {
  TestItemId next_id{};

  for (int i{ count - 1 }; i >= 0; --i) {
    char const name_buffer[2]{
      static_cast<char>('a' + (i % 26)),
      '\0',
    };
    next_id = enum_def.add_item(TestPairs{
      static_cast<TestEnum>(i + 1),
      enum_def.add_string(std::string_view{ name_buffer, 1 }),
      next_id,
    });
  }

  return next_id;
}

/**
 * @brief Counts the number of stored pair nodes in a linked pair chain.
 *
 * @param enum_def - Test enum definition storage.
 * @param pair_id - First pair id in the chain.
 * @return std::size_t - Number of linked pair nodes.
 */
template <typename EnumT>
constexpr std::size_t count_pairs(EnumT const& enum_def, TestItemId pair_id) {
  std::size_t count{};
  for (; pair_id != 0u; pair_id = enum_def.template item<TestPairs>(pair_id).next_pairs_id) {
    ++count;
  }
  return count;
}

/**
 * @brief Counts the number of stored command-list nodes in a linked command chain.
 *
 * @param enum_def - Test enum definition storage.
 * @param cmds_id - First command-list id in the chain.
 * @return std::size_t - Number of linked command-list nodes.
 */
template <typename EnumT>
constexpr std::size_t count_commands(EnumT const& enum_def, TestItemId cmds_id) {
  std::size_t count{};
  for (; cmds_id != 0u; cmds_id = enum_def.template item<TestCmds>(cmds_id).next_id) {
    ++count;
  }
  return count;
}

TEST(EnumSpecGroup, DetectsInlineConditionalShapes)
{
  // Single-command numeric and plain named groups inline directly; masked or multi-command groups do not.
  TestEnumDef enum_def{};

  TestItemId const named_group_id{ add_single_named_group(enum_def) };
  TestItemId const numeric_group_id{ add_single_numeric_group(enum_def) };
  TestItemId const masked_named_group_id{ add_masked_named_group(enum_def) };
  TestItemId const multi_command_group_id{ add_multi_command_group(enum_def) };

  EXPECT_TRUE(enum_def.item<TestGroup>(named_group_id).has_only_one_Named_with_no_bitmask(enum_def));
  EXPECT_FALSE(enum_def.item<TestGroup>(named_group_id).has_only_one_Numeric(enum_def));

  EXPECT_TRUE(enum_def.item<TestGroup>(numeric_group_id).has_only_one_Numeric(enum_def));
  EXPECT_FALSE(enum_def.item<TestGroup>(numeric_group_id).has_only_one_Named_with_no_bitmask(enum_def));

  EXPECT_FALSE(enum_def.item<TestGroup>(masked_named_group_id).has_only_one_Numeric(enum_def));
  EXPECT_FALSE(enum_def.item<TestGroup>(masked_named_group_id).has_only_one_Named_with_no_bitmask(enum_def));

  EXPECT_FALSE(enum_def.item<TestGroup>(multi_command_group_id).has_only_one_Numeric(enum_def));
  EXPECT_FALSE(enum_def.item<TestGroup>(multi_command_group_id).has_only_one_Named_with_no_bitmask(enum_def));
}

TEST(EnumSpecWriter, AllowsPatchThroughCursorByteReference)
{
  // Keep the opcode patching surface low-level so callers can preserve or replace selected bits.
  TestProgram program{};
  Constexpr::impl::ProgramWriter writer{ program };

  auto const opcode_cursor{ writer.program_cursor() };
  writer.write_opcode(eEnumCommand::Terminate);

  writer.byte_at(opcode_cursor) = static_cast<char>(
    static_cast<std::uint8_t>(writer.byte_at(opcode_cursor)) | 0x03u);
  writer.byte_at(opcode_cursor) = static_cast<char>(
    (static_cast<std::uint8_t>(writer.byte_at(opcode_cursor))
      & static_cast<std::uint8_t>(~static_cast<std::uint8_t>(eEnumCommand::mOpCode)))
    | static_cast<std::uint8_t>(eEnumCommand::Named));

  writer.finish(program);

  EXPECT_EQ(
    static_cast<std::uint8_t>(program[0]),
    static_cast<std::uint8_t>(static_cast<std::uint8_t>(eEnumCommand::Named) | 0x03u));
}

TEST(EnumSpecEncoding, PublicOutputProgramOverloadsEmitConsistentHeaderedStreams)
{
  // The public Enum output API should size, buffer, string, and ostream-emit the same complete program stream.
  TestEnumDef const enum_def{ decode_program(kDecodeProgramSv) };
  std::string const expected{ kDecodeProgramSv };

  EXPECT_EQ(enum_def.program_size(), expected.size());
  EXPECT_EQ(enum_def.output_program(), expected);

  std::string expected_with_terminate{ expected };
  expected_with_terminate.push_back(static_cast<char>(eEnumCommand::Terminate));
  EXPECT_EQ(enum_def.program_size(false, true), expected_with_terminate.size());
  EXPECT_EQ(enum_def.output_program(false, true), expected_with_terminate);

  char buffer[32]{};
  char* const end{ enum_def.output_program(buffer, buffer + sizeof(buffer)) };
  std::string_view const buffered_program{ buffer, static_cast<std::size_t>(end - buffer) };
  std::string_view const expected_program{ expected };
  EXPECT_EQ(static_cast<std::size_t>(end - buffer), expected.size());
  EXPECT_EQ(buffered_program, expected_program);

  std::ostringstream stream{};
  EXPECT_EQ(&enum_def.output_program(stream), &stream);
  EXPECT_EQ(stream.str(), expected);
}

TEST(EnumSpecEncoding, CopiesChildScopeWithoutMutatingParentState)
{
  // A named child scope may narrow its mask and reset its block allowance without affecting the caller.
  TestEnumDef enum_def{};
  TestItemId const pair_id{ add_pair(enum_def, 0x01u, "one") };
  TestItemId const named_id{ add_named(enum_def, pair_id, true, 0x0Fu) };

  TestProgram program{};
  Constexpr::impl::ProgramWriter writer{ program };
  TestScopeData scope{};
  scope.scope_bitmask = 0xFFu;
  scope.remaining_items_allowed_in_block = 7;
  TestEncoder encoder{ enum_def, writer, scope };

  enum_def.item<TestNamed>(named_id).encode(encoder);
  writer.finish(program);

  EXPECT_EQ(encoder.scope_bitmask(), 0xFFu);
  EXPECT_EQ(encoder.remaining_items_allowed_in_block(), 7);
  EXPECT_EQ(
    static_cast<std::uint8_t>(program[0]),
    static_cast<std::uint8_t>(eEnumCommand::Named | eEnumCommand::fHasBitmask));
}

TEST(EnumSpecEncoding, EmitsContinueScopeWhenNamedPairsOverflowBlockBudget)
{
  // A named block should spill its seventeenth pair into a ContinueScope chunk.
  TestEnumDef enum_def{};
  TestItemId const pairs_id{ add_pair_chain(enum_def, 17) };
  TestItemId const named_id{ add_named(enum_def, pairs_id) };

  TestProgram program{};
  Constexpr::impl::ProgramWriter writer{ program };
  TestEncoder encoder{ enum_def, writer };

  enum_def.item<TestNamed>(named_id).encode(encoder);
  writer.finish(program);

  EXPECT_EQ(program.size(), 53u);
  EXPECT_EQ(
    static_cast<std::uint8_t>(program[0]),
    static_cast<std::uint8_t>(static_cast<std::uint8_t>(eEnumCommand::Named) | 0x0Fu));
  EXPECT_EQ(
    static_cast<std::uint8_t>(program[49]),
    static_cast<std::uint8_t>(eEnumCommand::ContinueScope));
}

constexpr std::uint8_t fGroup7thBitSet { 7 };

TEST(EnumSpecEncoding, ConditionalEmitsNamedIfAndPairElseBranches)
{
  // A plain named true branch should lower to GroupIfNamed, keep the conditional scope explicit,
  // and emit a pair-style Else branch when the false group is also plain named.
  TestEnumDef enum_def{};

  TestItemId const true_pair_id{ add_pair(enum_def, 0x01u, "one") };
  TestItemId const true_named_id{ add_named(enum_def, true_pair_id) };
  TestItemId const true_cmds_id{ add_cmd(enum_def, true_named_id) };
  TestItemId const true_group_id{ add_group(enum_def, true_cmds_id, "ifg") };

  TestItemId const false_pair_id{ add_pair(enum_def, 0x02u, "two") };
  TestItemId const false_named_id{ add_named(enum_def, false_pair_id) };
  TestItemId const false_cmds_id{ add_cmd(enum_def, false_named_id) };
  TestItemId const false_group_id{ add_group(enum_def, false_cmds_id) };

  TestItemId const conditional_id{
    add_conditional(enum_def, 0x80u, 0x0Fu, true_group_id, false_group_id)
  };

  TestProgram program{};
  Constexpr::impl::ProgramWriter writer{ program };
  TestEncoder encoder{ enum_def, writer };

  enum_def.item<TestConditional>(conditional_id).encode(encoder);
  writer.finish(program);

  ASSERT_EQ(program.size(), 18u);
  EXPECT_EQ(
    static_cast<std::uint8_t>(program[0]),
    static_cast<std::uint8_t>(static_cast<std::uint8_t>(eEnumCommand::GroupIfNamed)
      | static_cast<std::uint8_t>(eEnumCommand::fHasGroupName)
      | 0x01u));
  EXPECT_EQ(static_cast<std::uint8_t>(program[1]), fGroup7thBitSet);
  EXPECT_EQ(static_cast<std::uint8_t>(program[2]), 0x0Fu);
  EXPECT_EQ(std::string_view{ program.data() + 3u }, "ifg");
  EXPECT_EQ(static_cast<std::uint8_t>(program[7]), 0x01u);
  EXPECT_EQ(std::string_view{ program.data() + 8u }, "one");
  EXPECT_EQ(static_cast<std::uint8_t>(program[12]), static_cast<std::uint8_t>(eEnumCommand::Else));
  EXPECT_EQ(static_cast<std::uint8_t>(program[13]), 0x02u);
  EXPECT_EQ(std::string_view{ program.data() + 14u }, "two");
}

TEST(EnumSpecEncoding, ConditionalEmitsNumericIfAndCommandElseBranches)
{
  // A numeric true branch should inline under GroupIfNumeric and leave a command-shaped false
  // branch behind an Else | ElseCmds header.
  TestEnumDef enum_def{};

  TestItemId const true_numeric_id{ add_numeric(enum_def, 0x0Fu, "bits") };
  TestItemId const true_cmds_id{ add_cmd(enum_def, true_numeric_id) };
  TestItemId const true_group_id{ add_group(enum_def, true_cmds_id) };

  TestItemId const false_pair_id{ add_pair(enum_def, 0x02u, "two") };
  TestItemId const false_named_id{ add_named(enum_def, false_pair_id, true, 0x03u) };
  TestItemId const false_cmds_id{ add_cmd(enum_def, false_named_id) };
  TestItemId const false_group_id{ add_group(enum_def, false_cmds_id, "else") };

  TestItemId const conditional_id{
    add_conditional(enum_def, 0x80u, 0x0Fu, true_group_id, false_group_id)
  };

  TestProgram program{};
  Constexpr::impl::ProgramWriter writer{ program };
  TestEncoder encoder{ enum_def, writer };

  enum_def.item<TestConditional>(conditional_id).encode(encoder);
  writer.finish(program);

  ASSERT_EQ(program.size(), 21u);
  EXPECT_EQ(
    static_cast<std::uint8_t>(program[0]),
    static_cast<std::uint8_t>(eEnumCommand::GroupIfNumeric));
  EXPECT_EQ(static_cast<std::uint8_t>(program[1]), fGroup7thBitSet);
  EXPECT_EQ(static_cast<std::uint8_t>(program[2]), 0x0Fu);
  EXPECT_EQ(std::string_view{ program.data() + 3u }, "bits");
  EXPECT_EQ(
    static_cast<std::uint8_t>(program[8]),
    static_cast<std::uint8_t>(static_cast<std::uint8_t>(eEnumCommand::Else)
      | static_cast<std::uint8_t>(eEnumCommand::fHasGroupName)
      | static_cast<std::uint8_t>(eEnumCommand::fElseCmds)));
  EXPECT_EQ(std::string_view{ program.data() + 9u }, "else");
  EXPECT_EQ(
    static_cast<std::uint8_t>(program[14]),
    static_cast<std::uint8_t>(eEnumCommand::Named | eEnumCommand::fHasBitmask));
  EXPECT_EQ(static_cast<std::uint8_t>(program[15]), 0x03u);
  EXPECT_EQ(static_cast<std::uint8_t>(program[16]), 0x02u);
  EXPECT_EQ(std::string_view{ program.data() + 17u }, "two");
}

TEST(EnumSpecEncoding, ConditionalEmitsNegatedNumericWhenOnlyFalseBranchInlines)
{
  // A numeric false branch should inline under GroupIfNumeric | Negate and leave the true branch as Else pairs.
  TestEnumDef enum_def{};

  TestItemId const true_pair_id{ add_pair(enum_def, 0x01u, "one") };
  TestItemId const true_named_id{ add_named(enum_def, true_pair_id) };
  TestItemId const true_cmds_id{ add_cmd(enum_def, true_named_id) };
  TestItemId const true_group_id{ add_group(enum_def, true_cmds_id, "ifg") };

  TestItemId const false_numeric_id{ add_numeric(enum_def, 0x0Fu, "bits") };
  TestItemId const false_cmds_id{ add_cmd(enum_def, false_numeric_id) };
  TestItemId const false_group_id{ add_group(enum_def, false_cmds_id) };

  TestItemId const conditional_id{
    add_conditional(enum_def, 0x80u, 0x0Fu, true_group_id, false_group_id)
  };

  TestProgram program{};
  Constexpr::impl::ProgramWriter writer{ program };
  TestEncoder encoder{ enum_def, writer };

  enum_def.item<TestConditional>(conditional_id).encode(encoder);
  writer.finish(program);

  ASSERT_EQ(program.size(), 18u);
  EXPECT_EQ(
    static_cast<std::uint8_t>(program[0]),
    static_cast<std::uint8_t>(static_cast<std::uint8_t>(eEnumCommand::GroupIfNumeric)
      | static_cast<std::uint8_t>(eEnumCommand::fNegate)));
  EXPECT_EQ(static_cast<std::uint8_t>(program[1]), fGroup7thBitSet);
  EXPECT_EQ(static_cast<std::uint8_t>(program[2]), 0x0Fu);
  EXPECT_EQ(std::string_view{ program.data() + 3u }, "bits");
  EXPECT_EQ(
    static_cast<std::uint8_t>(program[8]),
    static_cast<std::uint8_t>(static_cast<std::uint8_t>(eEnumCommand::Else)
      | static_cast<std::uint8_t>(eEnumCommand::fHasGroupName)));
  EXPECT_EQ(std::string_view{ program.data() + 9u }, "ifg");
  EXPECT_EQ(static_cast<std::uint8_t>(program[13]), 0x01u);
  EXPECT_EQ(std::string_view{ program.data() + 14u }, "one");
}

TEST(EnumSpecEncoding, ConditionalAllowsOnlyFalseBranchWithoutMaterializingATrueGroup)
{
  // A stored conditional may legitimately omit its true branch and still encode as GroupIf with a zero-count if block plus Else.
  TestEnumDef enum_def{};

  TestItemId const false_pair_id{ add_pair(enum_def, 0x01u, "one") };
  TestItemId const false_named_id{ add_named(enum_def, false_pair_id) };
  TestItemId const false_cmds_id{ add_cmd(enum_def, false_named_id) };
  TestItemId const false_group_id{ add_group(enum_def, false_cmds_id) };

  TestItemId const conditional_id{
    add_conditional(enum_def, 0x80u, 0x0Fu, 0u, false_group_id)
  };

  TestProgram program{};
  Constexpr::impl::ProgramWriter writer{ program };
  TestEncoder encoder{ enum_def, writer };

  enum_def.item<TestConditional>(conditional_id).encode(encoder);
  writer.finish(program);

  ASSERT_EQ(program.size(), 9u);
  EXPECT_EQ(
    static_cast<std::uint8_t>(program[0]),
    static_cast<std::uint8_t>(eEnumCommand::GroupIf));
  EXPECT_EQ(static_cast<std::uint8_t>(program[1]), fGroup7thBitSet);
  EXPECT_EQ(static_cast<std::uint8_t>(program[2]), 0x0Fu);
  EXPECT_EQ(static_cast<std::uint8_t>(program[3]), static_cast<std::uint8_t>(eEnumCommand::Else));
  EXPECT_EQ(static_cast<std::uint8_t>(program[4]), 0x01u);
  EXPECT_EQ(std::string_view{ program.data() + 5u }, "one");
}

TEST(EnumSpecEncoding, ConditionalCommandBranchCountsStoredCommandsNotPairContinuations)
{
  // A command-style conditional branch counts stored command entries in its If_CmdCount field,
  // even when one child command later extends itself with ContinueScope.
  TestEnumDef enum_def{};

  TestItemId const long_pairs_id{ add_pair_chain(enum_def, 17) };
  TestItemId const masked_named_id{ add_named(enum_def, long_pairs_id, true, 0x1Fu) };
  TestItemId const true_cmds_id{ add_cmd(enum_def, masked_named_id) };
  TestItemId const true_group_id{ add_group(enum_def, true_cmds_id) };

  TestItemId const conditional_id{
    add_conditional(enum_def, 0x80u, 0x1Fu, true_group_id, 0)
  };

  TestProgram program{};
  Constexpr::impl::ProgramWriter writer{ program };
  TestEncoder encoder{ enum_def, writer };

  enum_def.item<TestConditional>(conditional_id).encode(encoder);
  writer.finish(program);

  EXPECT_EQ(
    static_cast<std::uint8_t>(program[0]),
    static_cast<std::uint8_t>(static_cast<std::uint8_t>(eEnumCommand::GroupIf) | 0x01u));
  EXPECT_EQ(
    static_cast<std::uint8_t>(program[3]),
    static_cast<std::uint8_t>(static_cast<std::uint8_t>(eEnumCommand::Named)
      | static_cast<std::uint8_t>(eEnumCommand::fHasBitmask)
      | 0x0Fu));
  EXPECT_EQ(
    static_cast<std::uint8_t>(program[53]),
    static_cast<std::uint8_t>(eEnumCommand::ContinueScope));
}

TEST(EnumSpecBuilder, BuildsRootNamedListAndStoresCmdsId)
{
  // Repeated Named() calls in one command scope should reuse the same implicit Named command and root Cmds anchor.
  TestEnumDef const enum_def{
    Constexpr::build_enum_description<TestSettings>()
      .Named(TestEnum{ 0x01u }, "one")
      .Named(TestEnum{ 0x02u }, "two")
      .Build()
  };

  ASSERT_NE(enum_def.cmds_id(), 0u);

  auto const& root_cmds{ enum_def.item<TestCmds>(enum_def.cmds_id()) };
  auto const& named{ enum_def.item<TestNamed>(root_cmds.command_id) };
  EXPECT_FALSE(named.has_mask);

  auto const& first_pair{ enum_def.item<TestPairs>(named.pairs_id) };
  EXPECT_EQ(first_pair.value, 0x01u);
  EXPECT_EQ(enum_def.get_string(first_pair.name_id), "one");
  ASSERT_NE(first_pair.next_pairs_id, 0u);

  auto const& second_pair{ enum_def.item<TestPairs>(first_pair.next_pairs_id) };
  EXPECT_EQ(second_pair.value, 0x02u);
  EXPECT_EQ(enum_def.get_string(second_pair.name_id), "two");
  EXPECT_EQ(second_pair.next_pairs_id, 0u);
  EXPECT_EQ(root_cmds.next_id, 0u);
}

TEST(EnumSpecBuilder, BuildsConditionalBranchesThroughTypedScopes)
{
  // IfNot() should store the first user-authored branch as false, then Else() should add the true branch and render the correct branch for bit-clear vs bit-set values.
  TestEnumDef const enum_def{
    Constexpr::build_enum_description<TestSettings>()
      .IfNot(TestEnum{ 0x80u }, TestEnum{ 0x0Fu }, "ifg")
        .Named(TestEnum{ 0x01u }, "one")
      .Else("elseg")
        .Numeric(TestEnum{ 0x0Fu }, "bits")
      .End()
      .Build()
  };

  ASSERT_NE(enum_def.cmds_id(), 0u);

  auto const& root_cmds{ enum_def.item<TestCmds>(enum_def.cmds_id()) };
  auto const& conditional{ enum_def.item<TestConditional>(root_cmds.command_id) };
  EXPECT_EQ(conditional.group_bitmask, 0x80u);
  EXPECT_EQ(conditional.bitmask, 0x0Fu);
  ASSERT_NE(conditional.false_group_id, 0u);
  ASSERT_NE(conditional.true_group_id, 0u);

  auto const& if_group{ enum_def.item<TestGroup>(conditional.false_group_id) };
  auto const& else_group{ enum_def.item<TestGroup>(conditional.true_group_id) };
  EXPECT_EQ(enum_def.get_string(if_group.name_id), "ifg");
  EXPECT_EQ(enum_def.get_string(else_group.name_id), "elseg");

  auto const& if_cmds{ enum_def.item<TestCmds>(if_group.cmds_id) };
  auto const& if_named{ enum_def.item<TestNamed>(if_cmds.command_id) };
  auto const& if_pair{ enum_def.item<TestPairs>(if_named.pairs_id) };
  EXPECT_EQ(if_pair.value, 0x01u);
  EXPECT_EQ(enum_def.get_string(if_pair.name_id), "one");

  auto const& else_cmds{ enum_def.item<TestCmds>(else_group.cmds_id) };
  auto const& else_numeric{ enum_def.item<TestNumeric>(else_cmds.command_id) };
  EXPECT_EQ(else_numeric.mask, 0x0Fu);
  EXPECT_EQ(enum_def.get_string(else_numeric.name_id), "bits");

  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x01u }), "one");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x80u }), "bits=0");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x81u }), "bits=1");
}

TEST(EnumSpecBuilder, BuildsNestedConditionalBranchesThroughBothBranchScopes)
{
  // Nested If() calls inside both an if branch and an else branch should build correctly and render through recursive conditional traversal.
  TestEnumDef const enum_def{
    Constexpr::build_enum_description<TestSettings>()
      .If(TestEnum{ 0x80u }, TestEnum{ 0x0Fu })
        .If(TestEnum{ 0x08u }, TestEnum{ 0x03u })
          .Named(TestEnum{ 0x01u }, "if-inner-one")
        .Else()
          .Named(TestEnum{ 0x02u }, "if-inner-two")
        .End()
      .Else()
        .If(TestEnum{ 0x04u }, TestEnum{ 0x03u })
          .Named(TestEnum{ 0x01u }, "else-inner-one")
        .Else()
          .Named(TestEnum{ 0x02u }, "else-inner-two")
        .End()
      .End()
      .Build()
  };

  auto const& root_cmds{ enum_def.item<TestCmds>(enum_def.cmds_id()) };
  auto const& outer{ enum_def.item<TestConditional>(root_cmds.command_id) };
  auto const& if_group{ enum_def.item<TestGroup>(outer.true_group_id) };
  auto const& else_group{ enum_def.item<TestGroup>(outer.false_group_id) };

  auto const& if_cmds{ enum_def.item<TestCmds>(if_group.cmds_id) };
  auto const& else_cmds{ enum_def.item<TestCmds>(else_group.cmds_id) };
  EXPECT_NE(enum_def.item_if<TestConditional>(if_cmds.command_id), nullptr);
  EXPECT_NE(enum_def.item_if<TestConditional>(else_cmds.command_id), nullptr);

  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x89u }), "if-inner-one");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x82u }), "if-inner-two");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x05u }), "else-inner-one");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x02u }), "else-inner-two");
}

TEST(EnumSpecBuilder, CanonicalizesEmptyConditionalBranches)
{
  // Empty leading branches should normalize to the opposite condition, empty trailing else branches should disappear, and fully empty conditionals should fail.
  {
    TestEnumDef const enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .If(TestEnum{ 0x80u }, TestEnum{ 0x0Fu })
        .Else()
          .Named(TestEnum{ 0x01u }, "one")
        .End()
        .Build()
    };

    auto const& root{ enum_def.item<TestCmds>(enum_def.cmds_id()) };
    auto const& conditional{ enum_def.item<TestConditional>(root.command_id) };
    EXPECT_EQ(conditional.true_group_id, 0u);
    EXPECT_NE(conditional.false_group_id, 0u);
    EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x01u }), "one");
  }

  {
    TestEnumDef const enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .IfNot(TestEnum{ 0x80u }, TestEnum{ 0x0Fu })
        .Else()
          .Named(TestEnum{ 0x01u }, "one")
        .End()
        .Build()
    };

    auto const& root{ enum_def.item<TestCmds>(enum_def.cmds_id()) };
    auto const& conditional{ enum_def.item<TestConditional>(root.command_id) };
    EXPECT_NE(conditional.true_group_id, 0u);
    EXPECT_EQ(conditional.false_group_id, 0u);
    EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x81u }), "one");
  }

  {
    TestEnumDef const enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .If(TestEnum{ 0x80u }, TestEnum{ 0x0Fu })
          .Named(TestEnum{ 0x01u }, "one")
        .Else()
        .End()
        .Build()
    };

    auto const& root{ enum_def.item<TestCmds>(enum_def.cmds_id()) };
    auto const& conditional{ enum_def.item<TestConditional>(root.command_id) };
    EXPECT_NE(conditional.true_group_id, 0u);
    EXPECT_EQ(conditional.false_group_id, 0u);
    EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x81u }), "one");
  }

#ifdef NDEBUG
  GTEST_SKIP() << "Assert-dependent tests are skipped in release builds.";
#else
  EXPECT_DEATH(
    (void)Constexpr::build_enum_description<TestSettings>()
      .If(TestEnum{ 0x80u }, TestEnum{ 0x0Fu })
      .End()
      .Build(),
    ""
  );

  EXPECT_DEATH(
    (void)Constexpr::build_enum_description<TestSettings>()
      .If(TestEnum{ 0x80u }, TestEnum{ 0x0Fu })
      .Else()
      .End()
      .Build(),
    ""
  );
#endif
}

TEST(EnumSpecAnyDecode, HeaderOnlyProgramsDispatchAllStorageKinds)
{
  // The variant-backed runtime-selected builder should dispatch each supported storage header into the matching typed alternative.
  auto expect_header_only = [](auto example_value) {
    using ValueT = decltype(example_value);

    std::string const program{
      static_cast<char>(storage_header_for<ValueT>())
    };

    auto const any_enum{ decode_any_program(program) };
    EXPECT_EQ(any_enum.storage_type(), Constexpr::impl::storage_type_for_value_type<ValueT>());
    EXPECT_EQ(any_enum.is_signed(), std::is_signed_v<ValueT>);
    EXPECT_EQ(any_enum.program_size(), program.size());
    EXPECT_EQ(any_enum.output_program(), program);
  };

  expect_header_only(std::int8_t{});
  expect_header_only(std::int16_t{});
  expect_header_only(std::int32_t{});
  expect_header_only(std::int64_t{});
  expect_header_only(std::uint8_t{});
  expect_header_only(std::uint16_t{});
  expect_header_only(std::uint32_t{});
  expect_header_only(std::uint64_t{});
}

TEST(EnumSpecAnyDecode, RoundTripsProgramsAndRendersThroughFacadeValueViews)
{
  // The variant-backed runtime-selected wrapper should re-emit the original stream and render through explicit unsigned value views without exposing visit() on the common path.
  std::string const program{ kDecodeProgramSv };
  AnyTestEnumDef const any_enum{ decode_any_program(program) };

  EXPECT_EQ(any_enum.storage_type(), eEnumStorageType::UInt8);
  EXPECT_FALSE(any_enum.is_signed());
  EXPECT_EQ(any_enum.output_program(), program);
  EXPECT_EQ(any_enum.value_unsigned(0x01u).to_string(), "one");
  EXPECT_EQ(any_enum.value_unsigned(0x02u).to_string(), "two");
  EXPECT_EQ(any_enum.visit([&](auto const& typed_enum) {
    using value_type = typename std::decay_t<decltype(typed_enum)>::value_type;
    return render_enum_value(typed_enum, static_cast<value_type>(0x02u));
  }), "two");

  std::ostringstream value_stream{};
  EXPECT_EQ(&(value_stream << any_enum.value_unsigned(0x02u)), &value_stream);
  EXPECT_EQ(value_stream.str(), "two");

  std::ostringstream stream{};
  EXPECT_EQ(&any_enum.output_program(stream), &stream);
  EXPECT_EQ(stream.str(), program);
}

TEST(EnumSpecAnyDecode, ValidatesSignednessAndRuntimeValueRanges)
{
  // The runtime-selected facade should require the matching signed or unsigned entrypoint and reject out-of-range values before rendering.
  std::string const unsigned_program{ kDecodeProgramSv };
  AnyTestEnumDef const unsigned_enum{ decode_any_program(unsigned_program) };

  EXPECT_THROW((void)unsigned_enum.value_signed(1), std::invalid_argument);
  EXPECT_THROW((void)unsigned_enum.value_unsigned(0x100u), std::out_of_range);

  std::string const signed_program{
    build_manual_program(storage_header_for<std::int8_t>(), [](auto& writer) {
      writer.write_opcode(eEnumCommand::Named);
      writer.write_int(static_cast<std::int8_t>(-1));
      writer.write_c_string("minus one");
    })
  };
  auto const signed_enum{ decode_any_program(signed_program) };

  EXPECT_TRUE(signed_enum.is_signed());
  EXPECT_EQ(signed_enum.value_signed(-1).to_string(), "minus one");
  EXPECT_THROW((void)signed_enum.value_unsigned(0xFFu), std::invalid_argument);
  EXPECT_THROW((void)signed_enum.value_signed(128), std::out_of_range);

  std::ostringstream signed_stream{};
  EXPECT_EQ(&(signed_stream << signed_enum.value_signed(-1)), &signed_stream);
  EXPECT_EQ(signed_stream.str(), "minus one");
}

TEST(EnumSpecAnyDecode, HonorsConfiguredStringAndItemCapacities)
{
  // The variant-backed runtime-selected builder should enforce the same fixed-capacity limits as the typed decoder.
  std::string oversized_name{
    static_cast<char>(storage_header_for<TestEnum>())
  };
  oversized_name.push_back(static_cast<char>(eEnumCommand::Named));
  oversized_name.push_back(static_cast<char>(0x01u));
  oversized_name.append(128u, 'x');
  oversized_name.push_back('\0');

  EXPECT_THROW(
    (void)decode_any_program<Constexpr::pack_space(4, 32)>(oversized_name),
    Constexpr::EnumParseCapacityExceeded);

  EXPECT_THROW(
    (void)decode_any_program<Constexpr::pack_space(128, 3)>(kDecodeProgramSv),
    Constexpr::EnumParseCapacityExceeded);

  auto const any_enum{
    decode_any_program<Constexpr::pack_space(128, 4)>(kDecodeProgramSv)
  };
  EXPECT_EQ(any_enum.output_program(), std::string{ kDecodeProgramSv });
}

TEST(EnumSpecAnyDecode, ReportsParseErrorsAndTerminatePolicy)
{
  // The variant-backed runtime-selected decode path should surface the same parse errors as the typed decoder and honor throw_on_terminate.
  EXPECT_THROW((void)decode_any_program(std::string_view{}), Constexpr::EnumParseEmptyInput);

  std::string const unsupported_header{
    static_cast<char>(0xF0u)
  };
  EXPECT_THROW((void)decode_any_program(unsupported_header), Constexpr::EnumParseHeaderMismatch);

  std::string const invalid_numeric_opcode{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(static_cast<eEnumCommand>(
        static_cast<std::uint8_t>(eEnumCommand::Numeric) | 0x08u));
      writer.write_int(static_cast<std::uint8_t>(0x0Fu));
      writer.write_c_string("bits");
    })
  };
  EXPECT_THROW((void)decode_any_program(invalid_numeric_opcode), Constexpr::EnumParseInvalidOpcode);

  std::string const terminated_program{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(eEnumCommand::Terminate);
    })
  };
  EXPECT_THROW((void)decode_any_program(terminated_program, true), Constexpr::EnumParseInvalidStructure);

  AnyTestEnumDef const any_enum{ decode_any_program(terminated_program, false) };
  EXPECT_EQ(any_enum.output_program(false, true), terminated_program);
}

TEST(EnumSpecDecode, HeaderOnlyProgramBuildsEmptyEnum)
{
  // The one-byte storage header is the minimum valid program and should decode to an empty enum.
  std::string const program{
    static_cast<char>(storage_header_for<TestEnum>())
  };

  TestEnumDef const enum_def{ decode_program(program) };

  EXPECT_EQ(enum_def.cmds_id(), 0u);
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x00u }), "");
}

TEST(EnumSpecDecode, RejectsEmptyInputAndHeaderMismatch)
{
  // Decoding requires at least the header byte, and that header must match the destination value type.
  EXPECT_THROW((void)decode_program(std::string_view{}), Constexpr::EnumParseEmptyInput);

  std::string const wrong_header{
    static_cast<char>(storage_header_for<std::uint16_t>())
  };
  EXPECT_THROW((void)decode_program(wrong_header), Constexpr::EnumParseHeaderMismatch);
}

TEST(EnumSpecDecode, DecodesSimpleNamedMaskedNamedAndNumericCommands)
{
  // The decoder should rebuild the direct Named/Numeric command forms and preserve their stored payloads.
  std::string const named_program{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(static_cast<eEnumCommand>(
        static_cast<std::uint8_t>(eEnumCommand::Named) | 0x01u));
      writer.write_int(static_cast<std::uint8_t>(0x01u));
      writer.write_c_string("one");
      writer.write_int(static_cast<std::uint8_t>(0x02u));
      writer.write_c_string("two");
    })
  };

  TestEnumDef const named_enum{ decode_program(named_program) };
  auto const& named_root_cmds{ named_enum.item<TestCmds>(named_enum.cmds_id()) };
  auto const& named{ named_enum.item<TestNamed>(named_root_cmds.command_id) };
  EXPECT_FALSE(named.has_mask);
  EXPECT_EQ(count_pairs(named_enum, named.pairs_id), 2u);
  EXPECT_EQ(named_enum.get_string(named_enum.item<TestPairs>(named.pairs_id).name_id), "one");

  std::string const masked_named_program{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(static_cast<eEnumCommand>(
        static_cast<std::uint8_t>(eEnumCommand::Named) |
        static_cast<std::uint8_t>(eEnumCommand::fHasBitmask)));
      writer.write_int(static_cast<std::uint8_t>(0x03u));
      writer.write_int(static_cast<std::uint8_t>(0x02u));
      writer.write_c_string("two");
    })
  };

  TestEnumDef const masked_named_enum{ decode_program(masked_named_program) };
  auto const& masked_root_cmds{ masked_named_enum.item<TestCmds>(masked_named_enum.cmds_id()) };
  auto const& masked_named{ masked_named_enum.item<TestNamed>(masked_root_cmds.command_id) };
  EXPECT_TRUE(masked_named.has_mask);
  EXPECT_EQ(masked_named.mask, 0x03u);
  EXPECT_EQ(render_enum(masked_named_enum, TestEnum{ 0x02u }), "two");

  std::string const numeric_program{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(static_cast<eEnumCommand>(
        static_cast<std::uint8_t>(eEnumCommand::Numeric) |
        static_cast<std::uint8_t>(eEnumCommand::fRightShiftBits)));
      writer.write_int(static_cast<std::uint8_t>(0x30u));
      writer.write_c_string("bits");
    })
  };

  TestEnumDef const numeric_enum{ decode_program(numeric_program) };
  auto const& numeric_root_cmds{ numeric_enum.item<TestCmds>(numeric_enum.cmds_id()) };
  auto const& numeric{ numeric_enum.item<TestNumeric>(numeric_root_cmds.command_id) };
  EXPECT_EQ(numeric.mask, 0x30u);
  EXPECT_EQ(render_enum(numeric_enum, TestEnum{ 0x10u }), "bits=1");
}

TEST(EnumSpecDecode, DecodesGroupIfCommandBranches)
{
  // Command-shaped conditional branches should rebuild as stored Groups containing command lists.
  TestEnumDef const source{
    Constexpr::build_enum_description<TestSettings>()
      .If(TestEnum{ 0x80u }, TestEnum{ 0x0Fu })
        .Named(TestEnum{ 0x01u }, "one", TestEnum{ 0x03u })
      .End()
      .Build()
  };

  TestEnumDef const decoded{ decode_program(encode_program_with_header(source)) };
  auto const& root{ decoded.item<TestCmds>(decoded.cmds_id()) };
  auto const& conditional{ decoded.item<TestConditional>(root.command_id) };
  ASSERT_NE(conditional.true_group_id, 0u);
  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x81u }), "one");
}

TEST(EnumSpecDecode, DecodesGroupIfNamedBranchesAndPairStyleElse)
{
  // Inline named conditional branches and pair-style else branches should both rebuild into stored group wrappers.
  TestEnumDef const source{
    Constexpr::build_enum_description<TestSettings>()
      .If(TestEnum{ 0x80u }, TestEnum{ 0x0Fu }, "ifg")
        .Named(TestEnum{ 0x01u }, "one")
      .Else("elseg")
        .Named(TestEnum{ 0x02u }, "two")
      .End()
      .Build()
  };

  TestEnumDef const decoded{ decode_program(encode_program_with_header(source)) };
  auto const& root{ decoded.item<TestCmds>(decoded.cmds_id()) };
  auto const& conditional{ decoded.item<TestConditional>(root.command_id) };
  ASSERT_NE(conditional.true_group_id, 0u);
  ASSERT_NE(conditional.false_group_id, 0u);
  EXPECT_NE(decoded.item<TestGroup>(conditional.true_group_id).cmds_id, 0u);
  EXPECT_NE(decoded.item<TestGroup>(conditional.false_group_id).cmds_id, 0u);
  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x81u }), "one");
  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x02u }), "two");
}

TEST(EnumSpecDecode, DecodesGroupIfNumericBranchesAndCommandElse)
{
  // Inline numeric branches should decode back into Group -> Cmds -> Numeric, with the else branch still attached to the same conditional.
  TestEnumDef const source{
    Constexpr::build_enum_description<TestSettings>()
      .If(TestEnum{ 0x80u }, TestEnum{ 0x30u })
        .Numeric(TestEnum{ 0x30u }, "bits", eEnumCommand::fRightShiftBits)
      .Else("elseg")
        .Named(TestEnum{ 0x00u }, "zero")
      .End()
      .Build()
  };

  TestEnumDef const decoded{ decode_program(encode_program_with_header(source)) };
  auto const& root{ decoded.item<TestCmds>(decoded.cmds_id()) };
  auto const& conditional{ decoded.item<TestConditional>(root.command_id) };
  auto const& numeric_group{ decoded.item<TestGroup>(conditional.true_group_id) };
  auto const& numeric_cmds{ decoded.item<TestCmds>(numeric_group.cmds_id) };
  EXPECT_NE(decoded.item_if<TestNumeric>(numeric_cmds.command_id), nullptr);
  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x90u }), "bits=1");
  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x00u }), "zero");
}

TEST(EnumSpecDecode, DecodesPairContinueScopeExtensions)
{
  // A Named branch that overflowed its first pair block should decode back into one linked pair chain and still match the rebuilt pairs when rendered.
  TestEnumDef const source{
    [] {
      TestEnumDef enum_def{};
      TestItemId const pairs_id{ add_pair_chain(enum_def, 17) };
      TestItemId const named_id{ add_named(enum_def, pairs_id) };
      enum_def.set_cmds_id(add_cmd(enum_def, named_id));
      return enum_def;
    }()
  };

  TestEnumDef const decoded{ decode_program(encode_program_with_header(source)) };
  auto const& root{ decoded.item<TestCmds>(decoded.cmds_id()) };
  auto const& named{ decoded.item<TestNamed>(root.command_id) };
  EXPECT_EQ(count_pairs(decoded, named.pairs_id), 17u);
  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x01u }), "a");
  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x11u }), "q");
}

TEST(EnumSpecDecode, DecodesCommandContinueScopeExtensions)
{
  // Command branches that overflow their first GroupIf block should decode back into one linked command chain.
  LargeTestEnumDef const source{
    [] {
      LargeTestEnumDef enum_def{};
      TestItemId first_cmd_id{};
      TestItemId last_cmd_id{};

      for (int i{}; i < 17; ++i) {
        char const name[]{
          static_cast<char>('a' + (i % 26)),
          '\0',
        };
        TestItemId const numeric_id{
          enum_def.add_item(TestNumeric{
            static_cast<TestEnum>(1u << (i % 7)),
            eEnumCommand{},
            enum_def.add_string(std::string_view{ name, 1 }),
          })
        };
        TestItemId const cmds_id{ enum_def.add_item(TestCmds{ numeric_id, {} }) };
        if (!first_cmd_id) {
          first_cmd_id = cmds_id;
        } else {
          enum_def.item<TestCmds>(last_cmd_id).next_id = cmds_id;
        }
        last_cmd_id = cmds_id;
      }

      TestItemId const group_id{ enum_def.add_item(TestGroup{ {}, first_cmd_id }) };
      TestItemId const conditional_id{
        enum_def.add_item(TestConditional{ 0x80u, 0xFFu, group_id, 0u })
      };
      enum_def.set_cmds_id(enum_def.add_item(TestCmds{ conditional_id, {} }));
      return enum_def;
    }()
  };

  LargeTestEnumDef const decoded{ decode_program<LargeTestSettings>(encode_program_with_header(source)) };
  auto const& root{ decoded.item<TestCmds>(decoded.cmds_id()) };
  auto const& conditional{ decoded.item<TestConditional>(root.command_id) };
  auto const& group{ decoded.item<TestGroup>(conditional.true_group_id) };
  EXPECT_EQ(count_commands(decoded, group.cmds_id), 17u);
}

TEST(EnumSpecDecode, DecodesCompressedNestedValuesAndZeroCountConditionalBranches)
{
  // Manual streams should decode compressed constrained values, and zero-count GroupIf* branches should not materialize empty Group objects.
  bool constexpr compress{ true };
  TestEnum const root_scope{ static_cast<TestEnum>(~TestEnum{ 0u }) };

  std::string const compressed_program{
    build_manual_program(storage_header_for<TestEnum>(compress), [&](auto& writer) {
      writer.write_opcode(static_cast<eEnumCommand>(
        static_cast<std::uint8_t>(eEnumCommand::GroupIfNamed) | 0x01u));
      write_group_shift(writer, fGroup7thBitSet);
      write_scoped_value(writer, TestEnum{ 0x70u }, root_scope, compress);
      write_scoped_value(writer, TestEnum{ 0x10u }, TestEnum{ 0x70u }, compress);
      writer.write_c_string("hi");
    })
  };

  TestEnumDef const compressed_enum{ decode_program(compressed_program) };
  EXPECT_EQ(render_enum(compressed_enum, TestEnum{ 0x90u }), "hi");

  std::string const zero_count_cmd_program{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(eEnumCommand::GroupIf);
      writer.write_int(static_cast<std::uint8_t>(fGroup7thBitSet));
      writer.write_int(static_cast<std::uint8_t>(0x0Fu));
    })
  };

  TestEnumDef const zero_count_cmd_enum{ decode_program(zero_count_cmd_program) };
  EXPECT_EQ(zero_count_cmd_enum.cmds_id(), 0u);
  EXPECT_EQ(render_enum(zero_count_cmd_enum, TestEnum{ 0x80u }), "");

  std::string const zero_count_named_program{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(eEnumCommand::GroupIfNamed);
      writer.write_int(static_cast<std::uint8_t>(fGroup7thBitSet));
      writer.write_int(static_cast<std::uint8_t>(0x0Fu));
    })
  };

  TestEnumDef const zero_count_named_enum{ decode_program(zero_count_named_program) };
  EXPECT_EQ(zero_count_named_enum.cmds_id(), 0u);
  EXPECT_EQ(render_enum(zero_count_named_enum, TestEnum{ 0x80u }), "");

  std::string const zero_count_with_else_program{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(eEnumCommand::GroupIf);
      writer.write_int(static_cast<std::uint8_t>(fGroup7thBitSet));
      writer.write_int(static_cast<std::uint8_t>(0x0Fu));
      writer.write_opcode(eEnumCommand::Else);
      writer.write_int(static_cast<std::uint8_t>(0x01u));
      writer.write_c_string("one");
    })
  };

  TestEnumDef const zero_count_with_else_enum{ decode_program(zero_count_with_else_program) };
  auto const& zero_count_with_else_root{ zero_count_with_else_enum.item<TestCmds>(zero_count_with_else_enum.cmds_id()) };
  auto const& zero_count_with_else_conditional{
    zero_count_with_else_enum.item<TestConditional>(zero_count_with_else_root.command_id)
  };
  EXPECT_EQ(zero_count_with_else_conditional.true_group_id, 0u);
  EXPECT_NE(zero_count_with_else_conditional.false_group_id, 0u);
  EXPECT_EQ(render_enum(zero_count_with_else_enum, TestEnum{ 0x01u }), "one");
}

TEST(EnumSpecDecode, RoundTripsStoredFalseOnlyConditionalBranches)
{
  // A stored conditional with no true group should round-trip through the encoder and decoder without inventing a fake empty group.
  TestEnumDef const source{
    [] {
      TestEnumDef enum_def{};

      TestItemId const false_pair_id{ add_pair(enum_def, 0x01u, "one") };
      TestItemId const false_named_id{ add_named(enum_def, false_pair_id) };
      TestItemId const false_cmds_id{ add_cmd(enum_def, false_named_id) };
      TestItemId const false_group_id{ add_group(enum_def, false_cmds_id) };
      TestItemId const conditional_id{
        add_conditional(enum_def, 0x80u, 0x0Fu, 0u, false_group_id)
      };
      enum_def.set_cmds_id(add_cmd(enum_def, conditional_id));
      return enum_def;
    }()
  };

  TestEnumDef const decoded{ decode_program(encode_program_with_header(source)) };
  auto const& root{ decoded.item<TestCmds>(decoded.cmds_id()) };
  auto const& conditional{ decoded.item<TestConditional>(root.command_id) };
  EXPECT_EQ(conditional.true_group_id, 0u);
  EXPECT_NE(conditional.false_group_id, 0u);
  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x01u }), "one");
}

TEST(EnumSpecDecode, RoundTripsNestedConditionalBranches)
{
  // Nested conditionals stored inside both parent branches should survive encode/decode and still render through recursive command traversal.
  TestEnumDef const source{
    Constexpr::build_enum_description<TestSettings>()
      .If(TestEnum{ 0x80u }, TestEnum{ 0x0Fu })
        .If(TestEnum{ 0x08u }, TestEnum{ 0x03u })
          .Named(TestEnum{ 0x01u }, "if-inner-one")
        .Else()
          .Named(TestEnum{ 0x02u }, "if-inner-two")
        .End()
      .Else()
        .If(TestEnum{ 0x04u }, TestEnum{ 0x03u })
          .Named(TestEnum{ 0x01u }, "else-inner-one")
        .Else()
          .Named(TestEnum{ 0x02u }, "else-inner-two")
        .End()
      .End()
      .Build()
  };

  TestEnumDef const decoded{ decode_program(encode_program_with_header(source)) };
  auto const& root{ decoded.item<TestCmds>(decoded.cmds_id()) };
  auto const& outer{ decoded.item<TestConditional>(root.command_id) };

  auto const& if_group{ decoded.item<TestGroup>(outer.true_group_id) };
  auto const& else_group{ decoded.item<TestGroup>(outer.false_group_id) };
  auto const& if_cmds{ decoded.item<TestCmds>(if_group.cmds_id) };
  auto const& else_cmds{ decoded.item<TestCmds>(else_group.cmds_id) };
  EXPECT_NE(decoded.item_if<TestConditional>(if_cmds.command_id), nullptr);
  EXPECT_NE(decoded.item_if<TestConditional>(else_cmds.command_id), nullptr);

  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x89u }), "if-inner-one");
  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x82u }), "if-inner-two");
  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x05u }), "else-inner-one");
  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x02u }), "else-inner-two");
}

TEST(EnumSpecDecode, OwnsCopiedStringsAndRoundTripsRenderedBehavior)
{
  // The decoder should copy incoming strings into the enum heap and preserve rendered behavior after a headered round-trip.
  TestEnumDef const source{
    Constexpr::build_enum_description<TestSettings>()
      .Named(TestEnum{ 0x01u }, "A", TestEnum{ 0x01u })
      .If(TestEnum{ 0x80u }, TestEnum{ 0x30u })
        .Numeric(TestEnum{ 0x30u }, "bits", eEnumCommand::fRightShiftBits)
      .Else("elseg")
        .Named(TestEnum{ 0x00u }, "ZERO")
      .End()
      .Build()
  };

  std::string program{ encode_program_with_header(source) };
  TestEnumDef const decoded{ decode_program(program) };

  program.assign(program.size(), 'x');

  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x91u }), "A, bits=1");
  EXPECT_EQ(render_enum(decoded, TestEnum{ 0x00u }), "ZERO");
}

TEST(EnumSpecDecode, ReportsRuntimeParseErrors)
{
  // Malformed streams should surface through the public parse-error family with specific diagnostics where practical.
  std::string const truncated_program{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(eEnumCommand::Named);
      writer.write_int(static_cast<std::uint8_t>(0x01u));
    })
  };
  EXPECT_THROW((void)decode_program(truncated_program), Constexpr::EnumParseUnexpectedEof);

  std::string const invalid_numeric_opcode{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(static_cast<eEnumCommand>(
        static_cast<std::uint8_t>(eEnumCommand::Numeric) | 0x08u));
      writer.write_int(static_cast<std::uint8_t>(0x0Fu));
      writer.write_c_string("bits");
    })
  };
  EXPECT_THROW((void)decode_program(invalid_numeric_opcode), Constexpr::EnumParseInvalidOpcode);

  std::string const pair_value_outside_named_mask{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(static_cast<eEnumCommand>(
        static_cast<std::uint8_t>(eEnumCommand::Named) |
        static_cast<std::uint8_t>(eEnumCommand::fHasBitmask) |
        0x01u));
      writer.write_int(static_cast<std::uint8_t>(0x03u));
      writer.write_int(static_cast<std::uint8_t>(0x04u));
      writer.write_c_string("bad");
    })
  };
  EXPECT_THROW((void)decode_program(pair_value_outside_named_mask), Constexpr::EnumParseInvalidStructure);

  std::string const misplaced_else{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(static_cast<eEnumCommand>(
        static_cast<std::uint8_t>(eEnumCommand::Else) | 0x00u));
      writer.write_int(static_cast<std::uint8_t>(0x01u));
      writer.write_c_string("one");
    })
  };
  EXPECT_THROW((void)decode_program(misplaced_else), Constexpr::EnumParseInvalidStructure);

  std::string const misplaced_continue{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(eEnumCommand::ContinueScope);
      writer.write_int(static_cast<std::uint8_t>(0x01u));
      writer.write_c_string("one");
    })
  };
  EXPECT_THROW((void)decode_program(misplaced_continue), Constexpr::EnumParseInvalidStructure);

  std::string const bytes_after_terminate{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(eEnumCommand::Terminate);
      writer.write_opcode(eEnumCommand::Named);
      writer.write_int(static_cast<std::uint8_t>(0x01u));
      writer.write_c_string("one");
    })
  };
  EXPECT_THROW((void)decode_program(bytes_after_terminate, false), Constexpr::EnumParseInvalidStructure);

  std::string const else_command_branch_with_only_noop_conditional{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(static_cast<eEnumCommand>(
        static_cast<std::uint8_t>(eEnumCommand::GroupIf) | 0x01u));
      writer.write_int(static_cast<std::uint8_t>(fGroup7thBitSet));
      writer.write_int(static_cast<std::uint8_t>(0x0Fu));
      writer.write_opcode(static_cast<eEnumCommand>(
        static_cast<std::uint8_t>(eEnumCommand::Named) | 0x01u));
      writer.write_int(static_cast<std::uint8_t>(0x01u));
      writer.write_c_string("one");
      writer.write_opcode(static_cast<eEnumCommand>(
        static_cast<std::uint8_t>(eEnumCommand::Else) |
        static_cast<std::uint8_t>(eEnumCommand::fElseCmds)));
      writer.write_opcode(eEnumCommand::GroupIf);
      writer.write_int(static_cast<std::uint8_t>(fGroup7thBitSet));
      writer.write_int(static_cast<std::uint8_t>(0x03u));
    })
  };
  EXPECT_THROW((void)decode_program(else_command_branch_with_only_noop_conditional), Constexpr::EnumParseInvalidStructure);

  std::string const invalid_compressed_scoped_value{
    build_manual_program(storage_header_for<TestEnum>(true), [](auto& writer) {
      writer.write_opcode(static_cast<eEnumCommand>(
        static_cast<std::uint8_t>(eEnumCommand::GroupIfNamed) | 0x01u));
      writer.write_dint(static_cast<std::uint8_t>(fGroup7thBitSet));
      writer.write_dint(static_cast<std::uint8_t>(0x0Au));
      writer.write_dint(static_cast<std::uint8_t>(0x04u));
      writer.write_c_string("bad");
    })
  };
  EXPECT_THROW((void)decode_program(invalid_compressed_scoped_value), Constexpr::EnumParseInvalidStructure);

  std::string const duplicate_masked_values{
    build_manual_program(storage_header_for<TestEnum>(), [](auto& writer) {
      writer.write_opcode(static_cast<eEnumCommand>(
        static_cast<std::uint8_t>(eEnumCommand::Named) |
        static_cast<std::uint8_t>(eEnumCommand::fHasBitmask) |
        0x01u));
      writer.write_int(static_cast<std::uint8_t>(0x03u));
      writer.write_int(static_cast<std::uint8_t>(0x00u));
      writer.write_c_string("D");
      writer.write_int(static_cast<std::uint8_t>(0x00u));
      writer.write_c_string("E");
    })
  };
  EXPECT_THROW((void)decode_program(duplicate_masked_values), Constexpr::EnumParseInvalidStructure);

  std::string oversized_name{
    static_cast<char>(storage_header_for<TestEnum>())
  };
  oversized_name.push_back(static_cast<char>(eEnumCommand::Named));
  oversized_name.push_back(static_cast<char>(0x01u));
  oversized_name.append(128u, 'x');
  oversized_name.push_back('\0');
  EXPECT_THROW((void)decode_program(oversized_name), Constexpr::EnumParseCapacityExceeded);

  std::string const header_only{
    static_cast<char>(storage_header_for<TestEnum>())
  };
#ifdef NDEBUG
  GTEST_SKIP() << "Assert-dependent tests are skipped in release builds.";
#else
  EXPECT_DEATH(
    (void)Constexpr::build_enum_description<TestSettings>()
      .Named(TestEnum{ 0x01u }, "one")
      .decode_program(header_only)
      .Build(),
    ""
  );
#endif
}

TEST(EnumSpecRender, RendersNonzeroNamedCountsWithoutExtraSeparators)
{
  // Zero matching non-zero named items should render nothing.
  {
    TestEnumDef const enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .Build()
    };

    EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x00u }), "");
  }

  // One matching non-zero named item should render without any separator noise.
  {
    TestEnumDef const enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .Named(TestEnum{ 0x01u }, "A", TestEnum{ 0x01u })
        .Build()
    };

    EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x01u }), "A");
  }

  // Two matching non-zero named items should render with exactly one vertical-bar separator.
  {
    TestEnumDef const enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .Named(TestEnum{ 0x01u }, "A", TestEnum{ 0x01u })
        .Named(TestEnum{ 0x02u }, "B", TestEnum{ 0x02u })
        .Build()
    };

    EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x03u }), "A | B");
  }
}

TEST(EnumSpecRender, RendersZeroNamedCountsWithoutExtraSeparators)
{
  // Zero matching zero-valued named items should render nothing.
  {
    TestEnumDef const enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .Named(TestEnum{ 0x00u }, "D", TestEnum{ 0x0Cu })
        .Build()
    };

    EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x04u }), "");
  }

  // One matching zero-valued named item should render without any separator noise.
  {
    TestEnumDef const enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .Named(TestEnum{ 0x00u }, "D", TestEnum{ 0x0Cu })
        .Build()
    };

    EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x00u }), "D");
  }

  // Two matching zero-valued named items should render with exactly one comma separator.
  {
    TestEnumDef const enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .Named(TestEnum{ 0x00u }, "D", TestEnum{ 0x0Cu })
        .Named(TestEnum{ 0x00u }, "E", TestEnum{ 0x30u })
        .Build()
    };

    EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x00u }), "D, E");
  }
}

TEST(EnumSpecRender, RendersNumericCountsWithoutExtraSeparators)
{
  // Zero numeric items should render nothing.
  {
    TestEnumDef const enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .Build()
    };

    EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x00u }), "");
  }

  // One numeric item should render without any separator noise.
  {
    TestEnumDef const enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .Numeric(TestEnum{ 0x30u }, "F", eEnumCommand::fRightShiftBits)
        .Build()
    };

    EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x10u }), "F=1");
  }

  // Two numeric items should render with exactly one comma separator.
  {
    TestEnumDef const enum_def{
      Constexpr::build_enum_description<TestSettings>()
        .Numeric(TestEnum{ 0x30u }, "F", eEnumCommand::fRightShiftBits)
        .Numeric(TestEnum{ 0xC0u }, "G", eEnumCommand::fRightShiftBits)
        .Build()
    };

    EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x90u }), "F=1, G=2");
  }
}

TEST(EnumSpecRender, RightShiftBitsPreservesSparseBitGapsWhilePackedBitsCondenseThem)
{
  // RightShiftBits should move the selected field to bit zero without packing away the zeros between sparse mask bits.
  TestEnumDef const enum_def{
    Constexpr::build_enum_description<TestSettings>()
      .Numeric(TestEnum{ 0x0Au }, "shifted", eEnumCommand::fRightShiftBits)
      .Numeric(TestEnum{ 0x0Au }, "packed", eEnumCommand::fPackedBits)
      .Build()
  };

  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x0Au }), "shifted=5, packed=3");
}

TEST(EnumSpecRender, SignExtendsPlainShiftedAndPackedNumericFields)
{
  // Signed numeric rendering should extend the extracted sign bit after plain masking, right shifting, or sparse-bit packing.
  TestEnumDef const enum_def{
    Constexpr::build_enum_description<TestSettings>()
      .Numeric(TestEnum{ 0x30u }, "plain", eEnumCommand::fIsSigned)
      .Numeric(TestEnum{ 0x30u }, "shifted", eEnumCommand::fIsSigned | eEnumCommand::fRightShiftBits)
      .Numeric(TestEnum{ 0x0Au }, "packed", eEnumCommand::fIsSigned | eEnumCommand::fPackedBits)
      .Build()
  };

  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x10u }), "plain=16, shifted=1, packed=0");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x38u }), "plain=-16, shifted=-1, packed=-2");
}

TEST(EnumSpecRender, ProducesExpectedNamedOutputsAcrossValuesZeroThroughFour)
{
  // One named group with zero and one-bit alternatives should produce the expected text for each value from 0 through 4.
  TestEnumDef const enum_def{
    Constexpr::build_enum_description<TestSettings>()
      .Named(TestEnum{ 0x00u }, "D", TestEnum{ 0x07u })
      .Named(TestEnum{ 0x01u }, "A", TestEnum{ 0x01u })
      .Named(TestEnum{ 0x02u }, "B", TestEnum{ 0x02u })
      .Named(TestEnum{ 0x04u }, "C", TestEnum{ 0x04u })
      .Build()
  };

  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x00u }), "D");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x01u }), "A");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x02u }), "B");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x03u }), "A | B");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x04u }), "C");
}

TEST(EnumSpecRender, OrdersNamedAndNumericBucketsWithoutExtraCrossSeparators)
{
  // Matching non-zero names should come first, then zero-valued names, then numeric items.
  TestEnumDef const enum_def{
    Constexpr::build_enum_description<TestSettings>()
      .Named(TestEnum{ 0x01u }, "A", TestEnum{ 0x01u })
      .Named(TestEnum{ 0x02u }, "B", TestEnum{ 0x02u })
      .Named(TestEnum{ 0x00u }, "D", TestEnum{ 0x08u })
      .Named(TestEnum{ 0x00u }, "E", TestEnum{ 0x04u })
      .Numeric(TestEnum{ 0x30u }, "F", eEnumCommand::fRightShiftBits)
      .Numeric(TestEnum{ 0xC0u }, "G", eEnumCommand::fRightShiftBits)
      .Build()
  };

  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x93u }), "A | B, D, E, F=1, G=2");
}

TEST(EnumSpecRender, RendersConditionalGroupsUsingSharedScopeBitmask)
{
  // Conditional groups should pick the active branch and evaluate unmasked Named items as exact values within the branch scope bitmask.
  TestEnumDef const enum_def{
    Constexpr::build_enum_description<TestSettings>()
      .If(TestEnum{ 0x80u }, TestEnum{ 0x0Fu }, "ifg")
        .Named(TestEnum{ 0x01u }, "if-one")
        .Named(TestEnum{ 0x02u }, "if-two")
      .Else("elseg")
        .Named(TestEnum{ 0x00u }, "else-zero")
        .Named(TestEnum{ 0x01u }, "else-one")
        .Named(TestEnum{ 0x04u }, "else-four")
      .End()
      .Build()
  };

  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x81u }), "if-one");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x82u }), "if-two");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x00u }), "else-zero");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x01u }), "else-one");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x04u }), "else-four");
}

TEST(EnumSpecRender, MergesConditionalGroupOutputWithoutRenderingGroupNames)
{
  // Group contents should merge into the shared output buckets while the stored group labels stay invisible in rendered text.
  TestEnumDef const enum_def{
    Constexpr::build_enum_description<TestSettings>()
      .Named(TestEnum{ 0x40u }, "ROOT", TestEnum{ 0x40u })
      .If(TestEnum{ 0x80u }, TestEnum{ 0x0Fu }, "ifg")
        .Named(TestEnum{ 0x01u }, "IF")
        .Numeric(TestEnum{ 0x30u }, "bits", eEnumCommand::fRightShiftBits)
      .Else("elseg")
        .Named(TestEnum{ 0x00u }, "ELSE")
      .End()
      .Build()
  };

  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0xD1u }), "ROOT | IF, bits=1");
  EXPECT_EQ(render_enum(enum_def, TestEnum{ 0x40u }), "ROOT, ELSE");
}

TEST(EnumSpecBuilder, RejectsDuplicateNamedValuesWithinOneMaskedGroup)
{
  // Reusing one masked enum value inside the same Named block should be rejected instead of producing two names.
#ifdef NDEBUG
  GTEST_SKIP() << "Assert-dependent tests are skipped in release builds.";
#else
  EXPECT_DEATH(
    (void)Constexpr::build_enum_description<TestSettings>()
      .Named(TestEnum{ 0x00u }, "D", TestEnum{ 0x0Cu })
      .Named(TestEnum{ 0x00u }, "E", TestEnum{ 0x0Cu })
      .Build(),
    ""
  );
#endif
}

} // namespace
