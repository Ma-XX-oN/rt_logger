#include "constexpr/enum.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <variant>

namespace {

using TestEnum = std::uint8_t;
using TestPairs = Constexpr::impl::Pairs<TestEnum>;
using TestNamed = Constexpr::impl::Named<TestEnum>;
using TestNumeric = Constexpr::impl::Numeric<TestEnum>;
using TestConditional = Constexpr::impl::Conditional<TestEnum>;
using TestGroup = Constexpr::impl::Group<TestEnum>;
using TestCmds = Constexpr::impl::Cmds<TestEnum>;
using TestItemVariant = std::variant<TestPairs, TestNamed, TestNumeric, TestConditional, TestGroup, TestCmds>;
using TestSettings = Constexpr::impl::EnumSettings<TestEnum, Constexpr::reserve_space(128, 32), TestItemVariant>;
using TestEnumDef = Constexpr::Enum<TestSettings>;
using TestBuilder = Constexpr::EnumBuilder<TestSettings>;
using TestEncoder = Constexpr::impl::EnumEncoder<TestEnumDef>;
using TestItemId = Constexpr::item_id_t;
using TestStringId = Constexpr::string_id_t;
using TestProgram = Constexpr::string<129>;
using TestScopeData = Constexpr::impl::ScopeData<TestEnum>;
using Constexpr::eEnumCommand;

constexpr bool builder_end_returns_exact_parent{
  std::is_same_v<
    decltype(Constexpr::build_enum_description<TestSettings>().If(TestEnum{ 0x80u }, TestEnum{ 0x0Fu }).End()),
    TestBuilder>
};
static_assert(builder_end_returns_exact_parent);

// Prove that a fully built enum description can be materialized during constant evaluation.
constexpr bool builder_build_materializes_constexpr_enum{
  [] {
    constexpr auto enum_def{
      Constexpr::build_enum_description<Constexpr::DefaultEnumSettings<int>>()
        .Name(TestEnum{ 0x01u }, "one")
        .Name(TestEnum{ 0x02u }, "two")
        .Build()
    };

    static_assert(sizeof(enum_def) == 4384);  // assumes 8 byte alignment
    static_assert(Constexpr::impl::string_space(enum_def.actual_space()) == 256); // default is 256 storable characters, even if not used.
    static_assert(Constexpr::impl::item_space(enum_def.actual_space()) == 256);   // default is 256 storable items, even if not used.
    return enum_def.cmds_id() != 0u;
  }()
};
static_assert(builder_build_materializes_constexpr_enum);

// Prove that the minimal-size two-pass macro can also build an enum during constant evaluation.
constexpr bool build_enum_macro_materializes_constexpr_enum{
  [] {
    constexpr auto enum_def{
      BUILD_ENUM_DESCRIPTION(TestEnum,
        .Name(TestEnum{ 0x01u }, "one")
        .Name(TestEnum{ 0x02u }, "two"))
    };

    static_assert(sizeof(enum_def) == 88);  // assumes 8 byte alignment
    static_assert(Constexpr::impl::string_space(enum_def.actual_space()) == 8); // 8 characters stored including NUL terminators
    static_assert(Constexpr::impl::item_space(enum_def.actual_space()) == 4);   // 4 items stored, 2 Named and 2 Cmds.
                                                                                       // default is 256 and 256

    return enum_def.cmds_id() != 0u;
  }()
};
static_assert(build_enum_macro_materializes_constexpr_enum);

// Fixture-building helpers create stored group shapes that the runtime tests
// can classify without manually assembling ids inline in each case.

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
    static_cast<unsigned char>(writer.byte_at(opcode_cursor)) | 0x03u);
  writer.byte_at(opcode_cursor) = static_cast<char>(
    (static_cast<unsigned char>(writer.byte_at(opcode_cursor))
      & static_cast<unsigned char>(~static_cast<unsigned char>(eEnumCommand::mOpCode)))
    | static_cast<unsigned char>(eEnumCommand::Named));

  writer.finish(program);

  EXPECT_EQ(
    static_cast<unsigned char>(program[0]),
    static_cast<unsigned char>(static_cast<unsigned char>(eEnumCommand::Named) | 0x03u));
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
    static_cast<unsigned char>(program[0]),
    static_cast<unsigned char>(eEnumCommand::Named | eEnumCommand::fHasBitmask));
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
    static_cast<unsigned char>(program[0]),
    static_cast<unsigned char>(static_cast<unsigned char>(eEnumCommand::Named) | 0x0Fu));
  EXPECT_EQ(
    static_cast<unsigned char>(program[49]),
    static_cast<unsigned char>(eEnumCommand::ContinueScope));
}

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
    static_cast<unsigned char>(program[0]),
    static_cast<unsigned char>(static_cast<unsigned char>(eEnumCommand::GroupIfNamed)
      | static_cast<unsigned char>(eEnumCommand::fHasGroupName)
      | 0x01u));
  EXPECT_EQ(static_cast<unsigned char>(program[1]), 0x80u);
  EXPECT_EQ(static_cast<unsigned char>(program[2]), 0x0Fu);
  EXPECT_EQ(std::string_view{ program.data() + 3u }, "ifg");
  EXPECT_EQ(static_cast<unsigned char>(program[7]), 0x01u);
  EXPECT_EQ(std::string_view{ program.data() + 8u }, "one");
  EXPECT_EQ(static_cast<unsigned char>(program[12]), static_cast<unsigned char>(eEnumCommand::Else));
  EXPECT_EQ(static_cast<unsigned char>(program[13]), 0x02u);
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
    static_cast<unsigned char>(program[0]),
    static_cast<unsigned char>(eEnumCommand::GroupIfNumeric));
  EXPECT_EQ(static_cast<unsigned char>(program[1]), 0x80u);
  EXPECT_EQ(static_cast<unsigned char>(program[2]), 0x0Fu);
  EXPECT_EQ(std::string_view{ program.data() + 3u }, "bits");
  EXPECT_EQ(
    static_cast<unsigned char>(program[8]),
    static_cast<unsigned char>(static_cast<unsigned char>(eEnumCommand::Else)
      | static_cast<unsigned char>(eEnumCommand::fHasGroupName)
      | static_cast<unsigned char>(eEnumCommand::fElseCmds)));
  EXPECT_EQ(std::string_view{ program.data() + 9u }, "else");
  EXPECT_EQ(
    static_cast<unsigned char>(program[14]),
    static_cast<unsigned char>(eEnumCommand::Named | eEnumCommand::fHasBitmask));
  EXPECT_EQ(static_cast<unsigned char>(program[15]), 0x03u);
  EXPECT_EQ(static_cast<unsigned char>(program[16]), 0x02u);
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
    static_cast<unsigned char>(program[0]),
    static_cast<unsigned char>(static_cast<unsigned char>(eEnumCommand::GroupIfNumeric)
      | static_cast<unsigned char>(eEnumCommand::fNegate)));
  EXPECT_EQ(static_cast<unsigned char>(program[1]), 0x80u);
  EXPECT_EQ(static_cast<unsigned char>(program[2]), 0x0Fu);
  EXPECT_EQ(std::string_view{ program.data() + 3u }, "bits");
  EXPECT_EQ(
    static_cast<unsigned char>(program[8]),
    static_cast<unsigned char>(static_cast<unsigned char>(eEnumCommand::Else)
      | static_cast<unsigned char>(eEnumCommand::fHasGroupName)));
  EXPECT_EQ(std::string_view{ program.data() + 9u }, "ifg");
  EXPECT_EQ(static_cast<unsigned char>(program[13]), 0x01u);
  EXPECT_EQ(std::string_view{ program.data() + 14u }, "one");
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
    static_cast<unsigned char>(program[0]),
    static_cast<unsigned char>(static_cast<unsigned char>(eEnumCommand::GroupIf) | 0x01u));
  EXPECT_EQ(
    static_cast<unsigned char>(program[3]),
    static_cast<unsigned char>(static_cast<unsigned char>(eEnumCommand::Named)
      | static_cast<unsigned char>(eEnumCommand::fHasBitmask)
      | 0x0Fu));
  EXPECT_EQ(
    static_cast<unsigned char>(program[53]),
    static_cast<unsigned char>(eEnumCommand::ContinueScope));
}

TEST(EnumSpecBuilder, BuildsRootNamedListAndStoresCmdsId)
{
  // Repeated Name() calls in one command scope should reuse the same implicit Named command and root Cmds anchor.
  TestEnumDef const enum_def{
    Constexpr::build_enum_description<TestSettings>()
      .Name(TestEnum{ 0x01u }, "one")
      .Name(TestEnum{ 0x02u }, "two")
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
  // IfNot() should store the first user-authored branch as false, then Else() should add the true branch and return to root on End().
  TestEnumDef const enum_def{
    Constexpr::build_enum_description<TestSettings>()
      .IfNot(TestEnum{ 0x80u }, TestEnum{ 0x0Fu }, "ifg")
        .Name(TestEnum{ 0x01u }, "one")
      .Else("elseg")
        .Number(TestEnum{ 0x0Fu }, "bits")
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
}

} // namespace
