/**
 * @file enum_builder.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Fluent builder API for constructing typed enum descriptions.
 * @version 0.1
 * @date 2026-05-28
 *
 * @copyright Copyright (c) 2026
 */
#ifndef CONSTEXPR_ENUM_BUILDER_HPP
#define CONSTEXPR_ENUM_BUILDER_HPP

#include "enum_decode.hpp"

namespace Constexpr {
  namespace impl {

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
     *       Named
     *       Numeric
     *       If / IfNot
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
     *   command scope while building an enum graph.
     *
     * @tparam E - Enum or integer value type.
     */
    template <typename E>
    struct ImplicitNamedState {
      item_id_t named_id{};
      item_id_t last_pair_id{};
      bool has_mask{};
      unsigned_equivalent_t<E> mask{};
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
       * @return Derived& - Updated command scope.
       */
      template <typename D = Derived>
      constexpr Derived& Named(typename D::value_type value, std::string_view name) {
        derived().append_named_pair_impl(false, typename D::value_type{}, value, name);
        return derived();
      }

      /**
       * @brief Add one named value under an explicit command-local bitmask.
       *
       * @param value - Masked enum value matched by the pair.
       * @param name - Display name for the pair.
       * @param bitmask - Command-local bitmask for the named block.
       * @return Derived& - This scope, mutated in-place.
       */
      template <typename D = Derived>
      constexpr Derived& Named(
        typename D::value_type value,
        std::string_view name,
        typename D::value_type bitmask)
      {
        derived().append_named_pair_impl(true, bitmask, value, name);
        return derived();
      }

      /**
       * @brief Add one numeric command to the current command scope.
       *
       * @param bitmask - Bitmask selecting the numeric field.
       * @param name - Display label for the numeric field.
       * @param format - Numeric formatting flags.
       * @return Derived& - This scope, mutated in-place.
       */
      template <typename D = Derived>
      constexpr Derived& Numeric(
        typename D::value_type bitmask,
        std::string_view name,
        eEnumCommand format = eEnumCommand{})
      {
        derived().append_numeric_impl(bitmask, name, format);
        return derived();
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
        typename D::value_type scope_bitmask)
      {
        return derived().begin_if_impl(false, group_bitmask, scope_bitmask, {}, false);
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
        std::string_view group_name)
      {
        return derived().begin_if_impl(false, group_bitmask, scope_bitmask, group_name, true);
      }

      /**
       * @brief Start a conditional scope whose first user-authored branch is the
       *   false case.
       *
       * @param group_bitmask - Single-bit selector for the branch.
       * @param scope_bitmask - Scope bitmask to apply inside the branch.
       * @return IfScope<Derived> - Builder state for the first user-authored branch.
       */
      template <typename D = Derived>
      constexpr auto IfNot(
        typename D::value_type group_bitmask,
        typename D::value_type scope_bitmask)
      {
        return derived().begin_if_impl(true, group_bitmask, scope_bitmask, {}, false);
      }

      /**
       * @brief Start a named conditional scope whose first user-authored branch
       *   is the false case.
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
        std::string_view group_name)
      {
        return derived().begin_if_impl(true, group_bitmask, scope_bitmask, group_name, true);
      }

    protected:
      /**
       * @brief Returns whether \p value is a bitwise subset of \p mask.
       *
       * Works for both integral types and enum classes via
       * \c make_unsigned_equivalent.
       *
       * @tparam T - Value type (integral or enum).
       * @tparam U - Mask type (integral or enum); may differ from \p T.
       * @param value - Candidate subset value.
       * @param mask - Superset mask to test against.
       * @return bool - \c true when every set bit in \p value is also set in \p mask.
       */
      template <typename T, typename U>
      static constexpr bool is_subset_of(T value, U mask) noexcept {
        auto const uv{ make_unsigned_equivalent(value) };
        auto const um{ make_unsigned_equivalent(mask) };
        return (uv & um) == uv;
      }

      /**
       * @brief Returns the concrete command-scope object.
       *
       * @return Derived& - Concrete scope reference.
       */
      constexpr Derived& derived() noexcept {
        return static_cast<Derived&>(*this);
      }

      /**
       * @brief Returns the concrete command-scope object (const overload).
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
       * @brief Verifies that one named block does not reuse the same masked value.
       *
       * @param scope - Command scope being updated.
       * @param named_id - Stored named-command id being extended.
       * @param value - Candidate masked enum value for the new pair.
       */
      template <typename D = Derived>
      static constexpr void verify_unique_named_value(
        D& scope,
        item_id_t named_id,
        [[maybe_unused]] typename D::value_type value)
      {
        #ifndef NDEBUG
        auto& enum_def{ scope.enum_ref() };
        auto const& named{ enum_def.template item<Constexpr::impl::Named<typename D::value_type>>(named_id) };

        for (item_id_t pair_id{ named.pairs_id }; pair_id != 0u;) {
          auto const& pair{ enum_def.template item<Pairs<typename D::value_type>>(pair_id) };
          assert(pair.value != value || !"Named command cannot reuse the same masked enum value.");
          pair_id = pair.next_pairs_id;
        }
        #endif // NDEBUG
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
       *   current command scope.
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
        unsigned_equivalent_t<typename D::value_type> bitmask)
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
       *   needed.
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
        auto const u_bitmask{ make_unsigned_equivalent(bitmask) };
        if (has_mask) {
          assert(is_subset_of(u_bitmask, scope.scope_bitmask()) || !"Named bitmask must be a subset of the parent scope_bitmask");
          assert(is_subset_of(value, u_bitmask) || !"Named value must be a subset of its command bitmask");
        } else {
          assert(is_subset_of(value, scope.scope_bitmask()) || !"Named value must be a subset of the parent scope_bitmask");
        }

        auto& enum_def{ scope.enum_ref() };
        auto& implicit{ scope.command_state().implicit_named };
        item_id_t const named_id{ ensure_named_target(scope, has_mask, u_bitmask) };
        verify_unique_named_value(scope, named_id, value);
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
        auto const u_bitmask{ make_unsigned_equivalent(bitmask) };
        assert(is_subset_of(u_bitmask, scope.scope_bitmask()) || !"Numeric bitmask must be a subset of the parent scope_bitmask");
        clear_implicit_named(scope);

        auto& enum_def{ scope.enum_ref() };
        string_id_t const name_id{ enum_def.add_string(name) };
        item_id_t const numeric_id{
          enum_def.add_item(Constexpr::impl::Numeric<typename D::value_type>{ u_bitmask, format, name_id })
        };
        append_command_node(scope, numeric_id);
      }

      /**
       * @brief Start one conditional branch scope and append the owning
       *   conditional command to the current scope.
       *
       * @param scope - Command scope being updated.
       * @param negate_first - Whether the first user-authored branch is the
       *   false branch.
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

        auto const u_group_bitmask{ make_unsigned_equivalent(group_bitmask) };
        auto const u_scope_bitmask{ make_unsigned_equivalent(scope_bitmask) };

        auto& enum_def{ scope.enum_ref() };
        string_id_t const group_name_id{
          has_group_name ? enum_def.add_string(group_name) : string_id_t{}
        };
        item_id_t const group_id{ enum_def.add_item(Group<typename D::value_type>{ group_name_id, {} }) };

        Conditional<typename D::value_type> conditional{};
        verify_group_bitmask(u_group_bitmask);
        assert(is_subset_of(u_group_bitmask, scope.scope_bitmask()) || !"group_bitmask must be a subset of the parent scope_bitmask");
        assert(is_subset_of(u_scope_bitmask, scope.scope_bitmask()) || !"scope_bitmask must be a subset of the parent scope_bitmask");
        conditional.group_bitmask = u_group_bitmask;
        conditional.bitmask = u_scope_bitmask;
        if (negate_first) {
          conditional.false_group_id = group_id;
        } else {
          conditional.true_group_id = group_id;
        }

        item_id_t const conditional_id{ enum_def.add_item(conditional) };
        append_command_node(scope, conditional_id);
        return IfScope<D>{ scope, conditional_id, group_id, u_scope_bitmask };
      }
    };

    /**
     * @brief Builder state for the if branch of a conditional.
     *
     * @tparam Parent - Immediate parent builder scope.
     */
    template <typename Parent>
    class IfScope : public CommandScopeFacade<IfScope<Parent>> {
      Parent& m_parent;
      item_id_t m_conditional_id{};
      item_id_t m_group_id{};
      unsigned_equivalent_t<typename Parent::value_type> m_scope_bitmask{};
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
       *   false branch.
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
      using unsigned_value_type = unsigned_equivalent_t<value_type>;
      using enum_type = typename Parent::enum_type;
      using settings_type = typename Parent::settings_type;

      /**
       * @brief Construct one if-branch builder scope around its parent scope.
       *
       * @param parent - Parent scope snapshot to keep extending.
       * @param conditional_id - Stored conditional command id.
       * @param group_id - Stored group id for the active branch.
       * @param scope_bitmask - Active scope bitmask (unsigned) constraining values inside this branch.
       */
      constexpr IfScope(
        Parent& parent,
        item_id_t conditional_id,
        item_id_t group_id,
        unsigned_value_type scope_bitmask) noexcept
      : m_parent{ parent }
      , m_conditional_id{ conditional_id }
      , m_group_id{ group_id }
      , m_scope_bitmask{ scope_bitmask }
      , m_state{}
      {
      }

      /**
       * @brief Returns the active scope bitmask constraining values in this branch.
       *
       * @return unsigned_value_type - Scope bitmask for this if branch.
       */
      constexpr unsigned_value_type scope_bitmask() const noexcept { return m_scope_bitmask; }

      /**
       * @brief Switch this conditional from its if branch to its else branch.
       *
       * @return ElseScope<Parent> - Builder state for the else branch.
       */
      constexpr auto Else() {
        return make_else_scope_impl({}, false);
      }

      /**
       * @brief Switch this conditional from its if branch to a named else branch.
       *
       * @param group_name - Group label for the else branch.
       * @return ElseScope<Parent> - Builder state for the else branch.
       */
      constexpr auto Else(std::string_view group_name) {
        return make_else_scope_impl(group_name, true);
      }

      /**
       * @brief Finish this if scope and return to the exact parent scope type.
       *
       * @return Parent& - The parent scope, by reference.
       */
      constexpr Parent& End() {
        return finish_impl();
      }

    private:
      /**
       * @brief Returns whether the active branch has emitted at least one
       *   command node.
       *
       * @return bool - \c true when the active branch owns a command list.
       */
      constexpr bool branch_has_commands() noexcept {
        return enum_ref().template item<Group<typename Parent::value_type>>(m_group_id).cmds_id != 0u;
      }

      /**
       * @brief Rebinds an empty first branch so the same stored group becomes
       *   the else branch instead of leaving an empty if-side group behind.
       *
       * @param group_name - Optional group label for the else branch.
       * @param has_group_name - Whether `group_name` should be stored.
       * @return ElseScope<Parent> - Builder state for the normalized else branch.
       */
      constexpr ElseScope<Parent> reuse_empty_branch_as_else(
        std::string_view group_name,
        bool has_group_name)
      {
        auto& enum_def{ enum_ref() };
        auto& conditional{ enum_def.template item<Conditional<value_type>>(m_conditional_id) };
        auto& group{ enum_def.template item<Group<value_type>>(m_group_id) };

        group.name_id = has_group_name ? enum_def.add_string(group_name) : string_id_t{};

        if (conditional.true_group_id == m_group_id) {
          conditional.true_group_id = {};
          conditional.false_group_id = m_group_id;
        } else {
          assert(conditional.false_group_id == m_group_id || !"Active if branch must belong to the current conditional.");
          conditional.false_group_id = {};
          conditional.true_group_id = m_group_id;
        }

        return ElseScope<Parent>{ m_parent, m_conditional_id, m_group_id, m_scope_bitmask };
      }

      /**
       * @brief Finalizes this if scope while rejecting a fully empty leading
       *   branch that never transitioned into a non-empty else branch.
       *
       * @return Parent - Updated parent scope.
       */
      constexpr Parent& finish_impl() {
        assert(branch_has_commands() || !"Conditional first branches cannot be empty unless a later Else branch supplies the payload.");
        return m_parent;
      }

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

        if (!branch_has_commands()) {
          return reuse_empty_branch_as_else(group_name, has_group_name);
        }

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

        return ElseScope<Parent>{ m_parent, m_conditional_id, group_id, m_scope_bitmask };
      }
    };

    /**
     * @brief Builder state for the else branch of a conditional.
     *
     * @tparam Parent - Immediate parent builder scope.
     */
    template <typename Parent>
    class ElseScope : public CommandScopeFacade<ElseScope<Parent>> {
      Parent& m_parent;
      item_id_t m_conditional_id{};
      item_id_t m_group_id{};
      unsigned_equivalent_t<typename Parent::value_type> m_scope_bitmask{};
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
       *   false branch.
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
      using unsigned_value_type = unsigned_equivalent_t<value_type>;
      using enum_type = typename Parent::enum_type;
      using settings_type = typename Parent::settings_type;

      /**
       * @brief Construct one else-branch builder scope around its parent scope.
       *
       * @param parent - Parent scope snapshot to keep extending.
       * @param conditional_id - Stored conditional command id.
       * @param group_id - Stored group id for the active else branch.
       * @param scope_bitmask - Active scope bitmask (unsigned) constraining values inside this branch.
       */
      constexpr ElseScope(
        Parent& parent,
        item_id_t conditional_id,
        item_id_t group_id,
        unsigned_value_type scope_bitmask) noexcept
      : m_parent{ parent }
      , m_conditional_id{ conditional_id }
      , m_group_id{ group_id }
      , m_scope_bitmask{ scope_bitmask }
      , m_state{}
      {
      }

      /**
       * @brief Returns the active scope bitmask constraining values in this branch.
       *
       * @return unsigned_value_type - Scope bitmask for this else branch.
       */
      constexpr unsigned_value_type scope_bitmask() const noexcept { return m_scope_bitmask; }

      /**
       * @brief Finish this else scope and return to the exact parent scope type.
       *
       * @return Parent& - The parent scope, by reference.
       */
      constexpr Parent& End() {
        return finish_impl();
      }

    private:
      /**
       * @brief Returns whether the active else branch has emitted at least one
       *   command node.
       *
       * @return bool - \c true when the active else branch owns a command list.
       */
      constexpr bool branch_has_commands() noexcept {
        return enum_ref().template item<Group<typename Parent::value_type>>(m_group_id).cmds_id != 0u;
      }

      /**
       * @brief Finalizes this else scope, dropping a trailing empty else branch
       *   or rejecting a fully empty conditional.
       *
       * @return Parent - Updated parent scope.
       */
      constexpr Parent& finish_impl() {
        if (branch_has_commands()) {
          return m_parent;
        }

        auto& enum_def{ enum_ref() };
        auto& conditional{ enum_def.template item<Conditional<value_type>>(m_conditional_id) };
        auto& group{ enum_def.template item<Group<value_type>>(m_group_id) };
        group.name_id = {};

        if (conditional.true_group_id == m_group_id) {
          conditional.true_group_id = {};
        } else {
          assert(conditional.false_group_id == m_group_id || !"Active else branch must belong to the current conditional.");
          conditional.false_group_id = {};
        }

        assert((conditional.true_group_id || conditional.false_group_id) || !"Conditionals cannot end with both branches empty.");

        return m_parent;
      }
    };

  } // namespace impl

  /**
   * @brief Default fixed-capacity storage budget used by enum-description
   *   builders and wrappers when the caller does not override it.
   */
  constexpr std::uint32_t DefaultReserved{ pack_space(256, 128) };

  /**
   * @brief Immutable enum-representation settings bundling value type and
   *   fixed storage capacities.
   *
   * @tparam ValueT - Enum or integer value type stored by the representation.
   * @tparam StringAndItemCapacity - Packed storage reservation with string
   *   space in the low 16 bits and item space in the high 16 bits.
   */
  template <typename ValueT, std::uint32_t StringAndItemCapacity = DefaultReserved>
  struct EnumSettings {
    using value_type = ValueT;
    constexpr static std::uint16_t MAX_STRING_STORAGE { impl::string_space(StringAndItemCapacity) };
    constexpr static std::uint16_t MAX_ITEMS_STORAGE  { impl::item_space(StringAndItemCapacity)  };
  };


  /**
   * @brief Typed-chaining builder for immutable enum descriptions.
   *
   * @tparam Settings - Representation settings for the stored enum graph.
   */
  template <typename Settings>
  class EnumBuilder : public impl::CommandScopeFacade<EnumBuilder<Settings>> {
    Enum<Settings> m_enum{};
    impl::CommandScopeState<typename Settings::value_type> m_state{};

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
     * @return impl::CommandScopeState<typename Settings::value_type>& - Mutable root state.
     */
    constexpr impl::CommandScopeState<typename Settings::value_type>& command_state() noexcept {
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
      typename Settings::value_type bitmask,
      typename Settings::value_type value,
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
      typename Settings::value_type bitmask,
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
      typename Settings::value_type group_bitmask,
      typename Settings::value_type scope_bitmask,
      std::string_view group_name,
      bool has_group_name)
    {
      return impl::CommandScopeFacade<EnumBuilder<Settings>>::begin_if_scope(
        *this, negate_first, group_bitmask, scope_bitmask, group_name, has_group_name);
    }

  public:
    using settings_type = Settings;
    using enum_type = Enum<Settings>;
    using value_type = typename Settings::value_type;
    using unsigned_value_type = unsigned_equivalent_t<value_type>;

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

    /**
     * @brief Returns the scope bitmask for the root builder.
     *
     * The root scope has no parent constraint, so all bits are valid.
     *
     * @return unsigned_value_type - All-ones mask.
     */
    constexpr unsigned_value_type scope_bitmask() const noexcept {
      return ~unsigned_value_type{};
    }

    /**
     * @brief Returns the configured fixed-capacity storage budget.
     *
     * @return std::uint32_t - Packed string/item capacity summary.
     */
    constexpr std::uint32_t used_space() const {
      return m_enum.used_space();
    }
  };

  /**
   * @brief Entry-point builder returned by \c build_enum_description().
   *
   * Inherits all fluent chaining ops from \c EnumBuilder<Settings> (via
   * \c CommandScopeFacade<EnumBuilder<Settings>>).  Because the CRTP target
   * is \c EnumBuilder<Settings>, every fluent call returns
   * \c EnumBuilder<Settings>& — dropping the root wrapper automatically after
   * the first entry is added.
   *
   * Adds two operations that are only valid before any entries are appended:
   *   - \c reserve_space<S,I>() — returns a fresh \c EnumBuilderRoot with
   *     reconfigured storage capacities.
   *   - \c decode_program()    — decodes a serialised definition stream and
   *     returns a \c DecodedEnumBuilder terminal.
   *
   * @tparam Settings - Representation settings matching the enclosed builder.
   */
  template <typename Settings>
  class EnumBuilderRoot : public EnumBuilder<Settings> {
  public:
    using settings_type = Settings;
    using value_type = typename Settings::value_type;

    /**
     * @brief Construct an empty root enum builder.
     */
    constexpr EnumBuilderRoot() noexcept = default;

    /**
     * @brief Return a fresh root builder with reconfigured storage capacities.
     *
     * @tparam string_space - Characters (+ NUL) allocated for string storage.
     * @tparam item_space - Slots for internal representational commands.
     * @return EnumBuilderRoot<NewSettings> - Empty root builder with new capacity.
     */
    template <std::uint16_t string_space, std::uint16_t item_space>
    constexpr auto reserve_space() const {
      return EnumBuilderRoot<EnumSettings<value_type, pack_space(string_space, item_space)>>{};
    }

    /**
     * @brief Decode a serialised definition stream as a terminal chain step.
     *
     * After calling this function only \c Build() and \c used_space() are
     * available on the returned wrapper.
     *
     * @param program - Definition stream including the storage-type header.
     * @param throw_on_terminate - Whether a Terminate opcode is a parse error.
     * @return DecodedEnumBuilder<Settings> - Terminal wrapper around the decoded enum.
     */
    constexpr DecodedEnumBuilder<Settings> decode_program(
      std::string_view program,
      bool throw_on_terminate = true) const
    {
      return DecodedEnumBuilder<Settings>{
        impl::EnumDecoder<Settings>{ program, throw_on_terminate }.decode()
      };
    }
  };

  /**
   * @brief Create an empty typed-chaining enum builder.
   *
   * @tparam Settings - Representation settings for the stored enum graph.
   * @return EnumBuilderRoot<Settings> - Empty root builder.
   */
  template <typename Settings>
  constexpr auto build_enum_description() {
    return EnumBuilderRoot<Settings>{};
  }

} // namespace Constexpr

/**
 * @brief Build a Enum type as small as possible in compiler space.
 *
 * @example
 *
 * ```cpp
 * constexpr auto minimal_enum = BUILD_ENUM_DESCRIPTION(EnumType,
 *    .Named(TestEnum{ 0x01u }, "one")
 *    .Named(TestEnum{ 0x02u }, "two")
 * );
 * ```
 */
#define BUILD_ENUM_DESCRIPTION(enum_type, enum_description)                   \
  (Constexpr::build_enum_description<                                         \
    Constexpr::EnumSettings<                                                  \
      enum_type,                                                              \
      Constexpr::build_enum_description<Constexpr::EnumSettings<enum_type>>() \
        enum_description.used_space()                                         \
    >                                                                         \
  >() enum_description.Build())

#endif // CONSTEXPR_ENUM_BUILDER_HPP
