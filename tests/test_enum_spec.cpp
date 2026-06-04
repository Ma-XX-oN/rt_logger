#include "constexpr/enum_spec.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <variant>

namespace {

using TestEnum = std::uint8_t;
using TestPairs = Constexpr::impl::Pairs<TestEnum>;
using TestNamed = Constexpr::impl::Named<TestEnum>;
using TestNumeric = Constexpr::impl::Numeric<TestEnum>;
using TestGroup = Constexpr::impl::Group<TestEnum>;
using TestCmds = Constexpr::impl::Cmds<TestEnum>;
using TestGroupEncodingForm = Constexpr::impl::eGroupEncodingForm;
using TestConditionalEncodingPlan = Constexpr::impl::ConditionalEncodingPlan;
using TestItemVariant = std::variant<TestPairs, TestNamed, TestNumeric, TestGroup, TestCmds>;
using TestSettings = Constexpr::impl::EncodingContextSettings<TestEnum, 128, 128, 32, TestItemVariant>;
using TestEncodingContext = Constexpr::impl::EncodingContext<TestSettings>;
using TestItemId = Constexpr::impl::item_id_t;
using Constexpr::eEnumCommand;

// Fixture-building helpers create stored group shapes that the runtime tests
// can classify without manually assembling ids inline in each case.

/**
 * @brief Stores one pair node for a named branch.
 *
 * @param ec - Test encoding context.
 * @param value - Enum value for the pair.
 * @param name - Pair name to store.
 * @return TestItemId - Stored pair id.
 */
constexpr TestItemId add_pair(TestEncodingContext& ec, TestEnum value, char const* name) {
  return ec.add_item(TestPairs{ value, ec.add_string(name), 0 });
}

/**
 * @brief Stores one named command.
 *
 * @param ec - Test encoding context.
 * @param pairs_id - First pair id for the named command.
 * @param has_mask - Whether the named command carries its own mask.
 * @param mask - Optional explicit mask value.
 * @return TestItemId - Stored named-command id.
 */
constexpr TestItemId add_named(
  TestEncodingContext& ec, TestItemId pairs_id, bool has_mask = false, TestEnum mask = 0)
{
  return ec.add_item(TestNamed{ has_mask, mask, pairs_id });
}

/**
 * @brief Stores one numeric command.
 *
 * @param ec - Test encoding context.
 * @param mask - Numeric mask value.
 * @param name - Numeric name to store.
 * @return TestItemId - Stored numeric-command id.
 */
constexpr TestItemId add_numeric(TestEncodingContext& ec, TestEnum mask, char const* name) {
  return ec.add_item(TestNumeric{ mask, eEnumCommand{}, ec.add_string(name) });
}

/**
 * @brief Stores one command-list node.
 *
 * @param ec - Test encoding context.
 * @param command_id - Referenced command item id.
 * @param next_id - Optional next command node.
 * @return TestItemId - Stored command-list id.
 */
constexpr TestItemId add_cmd(TestEncodingContext& ec, TestItemId command_id, TestItemId next_id = 0) {
  return ec.add_item(TestCmds{ command_id, next_id });
}

/**
 * @brief Stores one group referencing a command list.
 *
 * @param ec - Test encoding context.
 * @param cmds_id - Referenced command-list id.
 * @return TestItemId - Stored group id.
 */
constexpr TestItemId add_group(TestEncodingContext& ec, TestItemId cmds_id) {
  return ec.add_item(TestGroup{ 0, cmds_id });
}

/**
 * @brief Stores a group that should encode as named pairs in a conditional branch.
 *
 * @param ec - Test encoding context.
 * @return TestItemId - Stored group id.
 */
constexpr TestItemId add_single_named_group(TestEncodingContext& ec) {
  TestItemId const pair_id{ add_pair(ec, 0x01u, "one") };
  TestItemId const named_id{ add_named(ec, pair_id) };
  TestItemId const cmds_id{ add_cmd(ec, named_id) };
  return add_group(ec, cmds_id);
}

/**
 * @brief Stores a group that should encode as one inline numeric item.
 *
 * @param ec - Test encoding context.
 * @return TestItemId - Stored group id.
 */
constexpr TestItemId add_single_numeric_group(TestEncodingContext& ec) {
  TestItemId const numeric_id{ add_numeric(ec, 0x03u, "bits") };
  TestItemId const cmds_id{ add_cmd(ec, numeric_id) };
  return add_group(ec, cmds_id);
}

/**
 * @brief Stores a group that must stay command-shaped because its named item has a mask.
 *
 * @param ec - Test encoding context.
 * @return TestItemId - Stored group id.
 */
constexpr TestItemId add_masked_named_group(TestEncodingContext& ec) {
  TestItemId const pair_id{ add_pair(ec, 0x02u, "two") };
  TestItemId const named_id{ add_named(ec, pair_id, true, 0x03u) };
  TestItemId const cmds_id{ add_cmd(ec, named_id) };
  return add_group(ec, cmds_id);
}

/**
 * @brief Stores a group that must stay command-shaped because it contains multiple commands.
 *
 * @param ec - Test encoding context.
 * @return TestItemId - Stored group id.
 */
constexpr TestItemId add_multi_command_group(TestEncodingContext& ec) {
  TestItemId const first_pair_id{ add_pair(ec, 0x04u, "four") };
  TestItemId const second_pair_id{ add_pair(ec, 0x08u, "eight") };
  TestItemId const first_named_id{ add_named(ec, first_pair_id) };
  TestItemId const second_named_id{ add_named(ec, second_pair_id) };
  TestItemId const tail_cmd_id{ add_cmd(ec, second_named_id) };
  TestItemId const head_cmd_id{ add_cmd(ec, first_named_id, tail_cmd_id) };
  return add_group(ec, head_cmd_id);
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
  TestEncodingContext ec{};

  // An empty group contributes no branch body.
  TestItemId const empty_group_id{ add_group(ec, 0) };

  // A single unmasked named command can lower directly as pairs.
  TestItemId const named_group_id{ add_single_named_group(ec) };

  // A single numeric command can lower inline.
  TestItemId const numeric_group_id{ add_single_numeric_group(ec) };

  // Masked or multi-command groups must remain command lists.
  TestItemId const masked_named_group_id{ add_masked_named_group(ec) };
  TestItemId const multi_command_group_id{ add_multi_command_group(ec) };

  TestGroupEncodingForm const empty_form{
    Constexpr::impl::classify_group_encoding_form<TestSettings, TestEnum>(ec, empty_group_id)
  };
  TestGroupEncodingForm const named_form{
    Constexpr::impl::classify_group_encoding_form<TestSettings, TestEnum>(ec, named_group_id)
  };
  TestGroupEncodingForm const numeric_form{
    Constexpr::impl::classify_group_encoding_form<TestSettings, TestEnum>(ec, numeric_group_id)
  };
  TestGroupEncodingForm const masked_named_form{
    Constexpr::impl::classify_group_encoding_form<TestSettings, TestEnum>(ec, masked_named_group_id)
  };
  TestGroupEncodingForm const multi_command_form{
    Constexpr::impl::classify_group_encoding_form<TestSettings, TestEnum>(ec, multi_command_group_id)
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
  TestEncodingContext ec{};

  // True-side numeric should inline and the masked named false side should become a command else branch.
  TestItemId const true_numeric_group_id{ add_single_numeric_group(ec) };
  TestItemId const false_command_group_id{ add_masked_named_group(ec) };
  TestConditionalEncodingPlan const numeric_then_else_plan{
    Constexpr::impl::make_conditional_encoding_plan<TestSettings, TestEnum>(
      ec,
      true_numeric_group_id,
      false_command_group_id)
  };

  EXPECT_EQ(numeric_then_else_plan.inline_group_id, true_numeric_group_id);
  EXPECT_EQ(numeric_then_else_plan.else_group_id, false_command_group_id);
  EXPECT_EQ(numeric_then_else_plan.if_opcode, eEnumCommand::GroupIfNumeric);
  EXPECT_TRUE(numeric_then_else_plan.has_else_branch);
  EXPECT_EQ(numeric_then_else_plan.else_opcode, (eEnumCommand::Else | eEnumCommand::fElseCmds));

  // False-side numeric should inline under Negate and a plain named true side should become a pair else branch.
  TestItemId const true_named_group_id{ add_single_named_group(ec) };
  TestItemId const false_numeric_group_id{ add_single_numeric_group(ec) };
  TestConditionalEncodingPlan const negated_numeric_plan{
    Constexpr::impl::make_conditional_encoding_plan<TestSettings, TestEnum>(
      ec,
      true_named_group_id,
      false_numeric_group_id)
  };

  EXPECT_EQ(negated_numeric_plan.inline_group_id, false_numeric_group_id);
  EXPECT_EQ(negated_numeric_plan.else_group_id, true_named_group_id);
  EXPECT_EQ(negated_numeric_plan.if_opcode, (eEnumCommand::GroupIfNumeric | eEnumCommand::fNegate));
  EXPECT_TRUE(negated_numeric_plan.has_else_branch);
  EXPECT_EQ(negated_numeric_plan.else_opcode, eEnumCommand::Else);
}

} // namespace
