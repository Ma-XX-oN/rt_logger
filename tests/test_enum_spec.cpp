#include "constexpr/enum_spec.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>
#include <variant>

namespace {

using TestEnum = std::uint8_t;
using TestPairs = Constexpr::impl::Pairs<TestEnum>;
using TestNamed = Constexpr::impl::Named<TestEnum>;
using TestNumeric = Constexpr::impl::Numeric<TestEnum>;
using TestConditional = Constexpr::impl::Conditional<TestEnum>;
using TestGlobal = Constexpr::impl::Global<TestEnum>;
using TestGroup = Constexpr::impl::Group<TestEnum>;
using TestCmds = Constexpr::impl::Cmds<TestEnum>;
using TestGroupEncodingForm = Constexpr::impl::eGroupEncodingForm;
using TestConditionalEncodingPlan = Constexpr::impl::ConditionalEncodingPlan;
using TestItemVariant = std::variant<TestPairs, TestNamed, TestNumeric, TestConditional, TestGlobal, TestGroup, TestCmds>;
using TestSettings = Constexpr::impl::EnumSettings<TestEnum, 128, 32, TestItemVariant>;
using TestEnumDef = Constexpr::impl::Enum<TestSettings>;
using TestEncoder = Constexpr::impl::EnumEncoder<TestEnumDef>;
using TestItemId = Constexpr::impl::item_id_t;
using TestProgram = Constexpr::string<129>;
using TestScopeData = Constexpr::impl::ScopeData<TestEnum>;
using Constexpr::eEnumCommand;

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
 * @brief Stores one global node referencing a command list.
 *
 * @param enum_def - Test enum definition storage.
 * @param cmds_id - Referenced command-list id.
 * @return TestItemId - Stored global id.
 */
constexpr TestItemId add_global(TestEnumDef& enum_def, TestItemId cmds_id) {
  return enum_def.add_item(TestGlobal{ cmds_id });
}

/**
 * @brief Stores one group referencing a command list.
 *
 * @param enum_def - Test enum definition storage.
 * @param cmds_id - Referenced command-list id.
 * @return TestItemId - Stored group id.
 */
constexpr TestItemId add_group(TestEnumDef& enum_def, TestItemId cmds_id) {
  return enum_def.add_item(TestGroup{ 0, cmds_id });
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

// Compile-time scenarios pin down the conditional opcode choice before the
// full encoder is finished.

constexpr bool kConditionalPlanUsesInlineTrueNumeric{ []() constexpr {
  // A true-side inline numeric with no false branch should stay as bare GroupIfNumeric.
  TestConditionalEncodingPlan const plan{
    Constexpr::impl::make_conditional_encoding_plan(
      TestGroupEncodingForm::Numeric,
      11,
      TestGroupEncodingForm::None,
      0)
  };

  return plan.inline_group_id == 11
    && plan.else_group_id == 0
    && plan.if_opcode == eEnumCommand::GroupIfNumeric
    && !plan.has_else_branch;
}() };
static_assert(kConditionalPlanUsesInlineTrueNumeric);

constexpr bool kConditionalPlanUsesNamedElseAfterTrueNumeric{ []() constexpr {
  // A plain named false branch after inline true numeric should use a pair-style Else.
  TestConditionalEncodingPlan const plan{
    Constexpr::impl::make_conditional_encoding_plan(
      TestGroupEncodingForm::Numeric,
      11,
      TestGroupEncodingForm::NamedPairs,
      12)
  };

  return plan.inline_group_id == 11
    && plan.else_group_id == 12
    && plan.if_opcode == eEnumCommand::GroupIfNumeric
    && plan.has_else_branch
    && plan.else_opcode == eEnumCommand::Else;
}() };
static_assert(kConditionalPlanUsesNamedElseAfterTrueNumeric);

constexpr bool kConditionalPlanUsesNegatedFalseNumeric{ []() constexpr {
  // A false-side inline numeric should become Negate, leaving a command-style true else branch.
  TestConditionalEncodingPlan const plan{
    Constexpr::impl::make_conditional_encoding_plan(
      TestGroupEncodingForm::Commands,
      21,
      TestGroupEncodingForm::Numeric,
      22)
  };

  return plan.inline_group_id == 22
    && plan.else_group_id == 21
    && plan.if_opcode == (eEnumCommand::GroupIfNumeric | eEnumCommand::fNegate)
    && plan.has_else_branch
    && plan.else_opcode == (eEnumCommand::Else | eEnumCommand::fElseCmds);
}() };
static_assert(kConditionalPlanUsesNegatedFalseNumeric);

constexpr bool kConditionalPlanUsesGroupIfNamedWhenTrueBranchIsPlainNamed{ []() constexpr {
  // A plain named true branch should use GroupIfNamed and keep a command-shaped else branch explicit.
  TestConditionalEncodingPlan const plan{
    Constexpr::impl::make_conditional_encoding_plan(
      TestGroupEncodingForm::NamedPairs,
      31,
      TestGroupEncodingForm::Commands,
      32)
  };

  return plan.inline_group_id == 31
    && plan.else_group_id == 32
    && plan.if_opcode == eEnumCommand::GroupIfNamed
    && plan.has_else_branch
    && plan.else_opcode == (eEnumCommand::Else | eEnumCommand::fElseCmds);
}() };
static_assert(kConditionalPlanUsesGroupIfNamedWhenTrueBranchIsPlainNamed);

constexpr bool kConditionalPlanFallsBackToGroupIfForCommandBranches{ []() constexpr {
  // Two command-shaped branches should fall back to a general GroupIf plus command Else.
  TestConditionalEncodingPlan const plan{
    Constexpr::impl::make_conditional_encoding_plan(
      TestGroupEncodingForm::Commands,
      41,
      TestGroupEncodingForm::Commands,
      42)
  };

  return plan.inline_group_id == 41
    && plan.else_group_id == 42
    && plan.if_opcode == eEnumCommand::GroupIf
    && plan.has_else_branch
    && plan.else_opcode == (eEnumCommand::Else | eEnumCommand::fElseCmds);
}() };
static_assert(kConditionalPlanFallsBackToGroupIfForCommandBranches);

// Runtime tests keep the compile-time proofs visible in the normal test runner
// and verify the stored fixture shapes classify the way the planner expects.

TEST(EnumSpecConditionalPlan, SupportsCompileTimeSelection)
{
  // Surface the constexpr plan-selection proofs in the regular GTest output.
  EXPECT_TRUE(kConditionalPlanUsesInlineTrueNumeric);
  EXPECT_TRUE(kConditionalPlanUsesNamedElseAfterTrueNumeric);
  EXPECT_TRUE(kConditionalPlanUsesNegatedFalseNumeric);
  EXPECT_TRUE(kConditionalPlanUsesGroupIfNamedWhenTrueBranchIsPlainNamed);
  EXPECT_TRUE(kConditionalPlanFallsBackToGroupIfForCommandBranches);
}

TEST(EnumSpecGroup, ClassifiesStoredGroupsByConditionalForm)
{
  // Distinguish empty, named-pair, numeric, and command-shaped stored groups.
  TestEnumDef enum_def{};

  // An empty group contributes no branch body.
  TestItemId const empty_group_id{ add_group(enum_def, 0) };

  // A single unmasked named command can lower directly as pairs.
  TestItemId const named_group_id{ add_single_named_group(enum_def) };

  // A single numeric command can lower inline.
  TestItemId const numeric_group_id{ add_single_numeric_group(enum_def) };

  // Masked or multi-command groups must remain command lists.
  TestItemId const masked_named_group_id{ add_masked_named_group(enum_def) };
  TestItemId const multi_command_group_id{ add_multi_command_group(enum_def) };

  TestGroupEncodingForm const empty_form{
    Constexpr::impl::classify_group_encoding_form(enum_def, empty_group_id)
  };
  TestGroupEncodingForm const named_form{
    Constexpr::impl::classify_group_encoding_form(enum_def, named_group_id)
  };
  TestGroupEncodingForm const numeric_form{
    Constexpr::impl::classify_group_encoding_form(enum_def, numeric_group_id)
  };
  TestGroupEncodingForm const masked_named_form{
    Constexpr::impl::classify_group_encoding_form(enum_def, masked_named_group_id)
  };
  TestGroupEncodingForm const multi_command_form{
    Constexpr::impl::classify_group_encoding_form(enum_def, multi_command_group_id)
  };

  EXPECT_EQ(empty_form, TestGroupEncodingForm::None);
  EXPECT_EQ(named_form, TestGroupEncodingForm::NamedPairs);
  EXPECT_EQ(numeric_form, TestGroupEncodingForm::Numeric);
  EXPECT_EQ(masked_named_form, TestGroupEncodingForm::Commands);
  EXPECT_EQ(multi_command_form, TestGroupEncodingForm::Commands);
}

TEST(EnumSpecConditionalPlan, BuildsPlanFromStoredGroups)
{
  // Build plans from stored groups to prove classification and planning agree.
  TestEnumDef enum_def{};

  // True-side numeric should inline and the masked named false side should become a command else branch.
  TestItemId const true_numeric_group_id{ add_single_numeric_group(enum_def) };
  TestItemId const false_command_group_id{ add_masked_named_group(enum_def) };
  TestConditionalEncodingPlan const numeric_then_else_plan{
    Constexpr::impl::make_conditional_encoding_plan(
      enum_def,
      true_numeric_group_id,
      false_command_group_id)
  };

  EXPECT_EQ(numeric_then_else_plan.inline_group_id, true_numeric_group_id);
  EXPECT_EQ(numeric_then_else_plan.else_group_id, false_command_group_id);
  EXPECT_EQ(numeric_then_else_plan.if_opcode, eEnumCommand::GroupIfNumeric);
  EXPECT_TRUE(numeric_then_else_plan.has_else_branch);
  EXPECT_EQ(numeric_then_else_plan.else_opcode, (eEnumCommand::Else | eEnumCommand::fElseCmds));

  // False-side numeric should inline under Negate and a plain named true side should become a pair else branch.
  TestItemId const true_named_group_id{ add_single_named_group(enum_def) };
  TestItemId const false_numeric_group_id{ add_single_numeric_group(enum_def) };
  TestConditionalEncodingPlan const negated_numeric_plan{
    Constexpr::impl::make_conditional_encoding_plan(
      enum_def,
      true_named_group_id,
      false_numeric_group_id)
  };

  EXPECT_EQ(negated_numeric_plan.inline_group_id, false_numeric_group_id);
  EXPECT_EQ(negated_numeric_plan.else_group_id, true_named_group_id);
  EXPECT_EQ(negated_numeric_plan.if_opcode, (eEnumCommand::GroupIfNumeric | eEnumCommand::fNegate));
  EXPECT_TRUE(negated_numeric_plan.has_else_branch);
  EXPECT_EQ(negated_numeric_plan.else_opcode, eEnumCommand::Else);
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

TEST(EnumSpecEncoding, GlobalEncodesRootCommandList)
{
  // A stored global list should provide the root encode entry point for commands.
  TestEnumDef enum_def{};
  TestItemId const pair_id{ add_pair(enum_def, 0x01u, "one") };
  TestItemId const named_id{ add_named(enum_def, pair_id) };
  TestItemId const cmds_id{ add_cmd(enum_def, named_id) };
  TestItemId const global_id{ add_global(enum_def, cmds_id) };

  TestProgram program{};
  Constexpr::impl::ProgramWriter writer{ program };
  TestEncoder encoder{ enum_def, writer };

  enum_def.item<TestGlobal>(global_id).encode(encoder);
  writer.finish(program);

  EXPECT_EQ(program.size(), 6u);
  EXPECT_EQ(
    static_cast<unsigned char>(program[0]),
    static_cast<unsigned char>(eEnumCommand::Named));
}

} // namespace
