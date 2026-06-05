# Enum Stream Definition <!-- omit in toc -->

- [Purpose](#purpose)
- [Stream Overview](#stream-overview)
- [API Direction](#api-direction)
  - [Builder Shape](#builder-shape)
  - [Builder States](#builder-states)
  - [Parent-Typed Nesting](#parent-typed-nesting)
  - [Future Enhancements](#future-enhancements)
    - [Output Format](#output-format)
    - [Encoding Format](#encoding-format)
- [Potential Usage Examples](#potential-usage-examples)
- [Enum Stream Specification](#enum-stream-specification)
- [Possible Render Mechanism](#possible-render-mechanism)

## Purpose

This document is the authoritative design note for the enum-description subsystem.

It records:

- the public API direction for enum description and encoding
- the intended builder shape and builder-state model
- the enum definition stream wire format and its execution rules
- the rendering model that interprets a stored enum stream against a value

`README.md` keeps only the project-level overview and links here when enum-specific detail is needed.

## Stream Overview

```c++
enum eEnumStorageType : std::uint8_t {
  // Integer types
   Int8 = 0x00,  Int16 = 0x01,  Int32 = 0x02,  Int64 = 0x03,
  UInt8 = 0x04, UInt16 = 0x05, UInt32 = 0x06, UInt64 = 0x07,

  // States how constrained group_bitmask, bitmask and enum_value values are stored
  Compress          = 0x08, // Encode constrained values as condensed dints under the parent scope
};
```

An enum definition stream uses this storage type and a compact descriptor program to describe how enum values are named,
grouped, and formatted.

The stream is processed sequentially. If the stream length is known externally, decoding may stop at the end of the
stream. Otherwise the stream must end with a `Terminate` command.

This format makes scope explicit in each `GroupIf*` command. The block's `bitmask` replaces the earlier idea of a
separate `SetMaskValue` command, so the scope used by a conditional block is visible at the block header instead of
being ambient mutable state.

`Compress` changes only how constrained values are encoded, not the semantics below. If `Compress` is clear, constrained
`group_bitmask`, `bitmask`, and `enum_value` values are stored full width. If `Compress` is set, those same values are
stored as condensed dints relative to the parent `scope_bitmask` and are decoded back before the execution rules
below are applied.

The command set is:

```c++
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
```

## API Direction

The accepted API direction for enum descriptions is:

- `Constexpr::Enum<Settings>` is the public immutable representation type.
- `string_id_t` and `item_id_t` are public storage ids in `Constexpr`, defined with the storage layer rather than inside
  enum-encoding internals.
- The encoding path is split into three roles:
  - `Enum` for immutable representation storage
  - `ProgramWriter` for the writable byte sink
  - `EnumEncoder` for one encoding pass over an `Enum` into a `ProgramWriter`
- Stored enum items continue to own their `encode(...)` member functions. Scope-introducing block logic remains in a
  free helper instead of being spread across ad-hoc policy objects.

### Builder Shape

The builder direction is typed chaining rather than lambdas or variadic free-item factories.

Reasons:

- chaining keeps builder operations scoped on the builder object, which avoids namespace-lookup friction for helpers
  such as `Name`, `Number`, `If`, and `Else`
- chaining reads naturally for a declarative enum-description DSL
- state-typed chaining allows IntelliSense to expose only valid next operations

### Builder States

The accepted builder-state model is:

- `GlobalScope`
  - allows `If`, `IfNot`, `Named`, and `Numeric`
- `IfScope<Parent>`
  - allows `If`, `IfNot`, `Named`, `Numeric`, `Else`, and `End`
- `ElseScope<Parent>`
  - allows `If`, `IfNot`, `Named`, `Numeric`, and `End`
  - does not allow `Else`

`ElseScope` is separate because the presence of `Else` changes what operations are valid.

### Parent-Typed Nesting

Builder nesting is parent-typed rather than depth-typed.

That means:

- `End()` returns the exact parent scope type
- the builder models structural nesting directly instead of only tracking a numeric level
- a global builder state is an API concept, not a stored `Global` node in the enum representation

So the intended shape is closer to `IfScope<Parent>` and `ElseScope<Parent>` than to `IfScope<N>` or `ElseScope<N>`.

### Future Enhancements

#### Output Format

One accepted future enhancement is richer numeric output formatting without widening the common `Numeric` command form.

For plain `Numeric`, the remaining two format bits may be used as a compact numeric display mode:

- `0` = decimal
- `1` = hex
- `2` = hex with prefix
- `3` = extended-format-byte follows

`GroupIfNumeric` stays the restricted compact inline form for simple decimal numeric output only.

If a conditional numeric branch needs any non-default numeric presentation, it should not use `GroupIfNumeric`. It
should encode as `GroupIf` with a normal `Numeric` command inside the branch instead.

#### Encoding Format

Since a group mask shall only be one bit, instead of encoding the bitmask as the underlying type or dint-condensed
underlying type, we can replace that with an unsigned char to signify which bit is set.

## Potential Usage Examples

The examples in this section describe the intended builder-facing API for enum
descriptions. The currently tested low-level stream representation and encoder
live in `constexpr/enum.hpp`; these examples are design-level usage, not a
guarantee that this exact front-end surface is available in the current build.

Simple:

```cpp
enum eTest : std::uint8_t {
  nothing, something
};
constexpr auto eDisc = build_enum_description<eTest>()
  .Named(eTest::nothing, "NOTHING")       // named value
  .Named(eTest::something, "Something")   // another named value
  .Build();

auto eValue = eDisc.value(eDisc["NOTHING"]);

// string value "NOTHING". std::string type.
auto eText = eValue.to_string();

// Constexpr::string<N> type
auto sDefStream = eDisc.definition_stream();

auto eNewDesc = build_enum_description<std::uint8_t>(sDefStream);

// This does runtime evaluation, so is slower than the assignment to eValue above.
// Still it's usable and has the same definition as eDisc and the same size.
auto eNewValue = eNewDesc.value(eNewDesc["NOTHING"]);

assert(eValue.to_int() == eNewValue.to_int());

// Constexpr::string<N> type
// Gets the stream as if it were an escaped c-string, which can be pasted into a
// source file to build_enum_description in a constexpr object.
auto sEscapedDefStream = eDisc.definition_stream_escaped();
```

A little more complicated:

```cpp
enum eTest : std::uint8_t {
  nothing = 0, something = 1,  // this group of values are for group1
  hello   = 0, goodbye   = 1,  // this group of values are for group2
  group2 = 0x80, mask = 0x7f
};
constexpr auto eDisc = build_enum_description<eTest>()
  .IfNot(eTest::group2, eTest::mask)
    .Named(eTest::nothing, "NOTHING")
    .Named(eTest::something, "Something")
  .Else()
    .Named(eTest::hello, "hello")
    .Named(eTest::goodbye, "goodbye")
    .Numeric(eTest::mask, "value")
  .End()
  .Build();

// Using enums to get and set named values is unsafe when working with groups.
// Consider:
//
//   auto eUnsafe = eDisc.value(eTest::goodbye); // didn't set the group2 bit
//
// So we disallow that and use the names instead.

// this is safer as it would set group2 mask bit
auto eValue = eDisc.value(eDisc["goodbye"]);
eValue = eDisc["hello"];                          // ok, right group
eValue = eDisc["NOTHING"];                        // error since not right group
eValue = set(eTest::group2, 0, eDisc["NOTHING"]); // Set group2 bit to 0 and setting to "NOTHING" value.
eValue = force_set(eDisc["hello"]);               // force changing group and setting to "hello" value.

assert(eValue.is_active(eTest::group2));
assert(eValue.from_group(eTest::group2) == eTest::nothing); // true
assert(eValue.from_group(eTest::group2) == eTest::hello);   // this is also true
assert(eValue != eDisc["NOTHING"]);                         // safer
assert(eValue == eDisc["hello"]);

eValue = set(eDisc["value"], 3);       // ok
eValue = set(eDisc["value"], 1);       // error since 1 is a named value "Something".
eValue = force_set(eDisc["value"], 1); // ok, forcing set to value 1.
```

Same complexity but allows direct access to groups by name.

```cpp
enum eTest : std::uint8_t {
  nothing, something,
  hello = 0, goodbye,
  group2 = 0x80, mask = 0x7f
};
auto eDisc = build_enum_description<eTest>()
  .IfNot(eTest::group2, eTest::mask, "group1")
    .Named(eTest::nothing, "NOTHING")
    .Named(eTest::something, "Something")
  .Else("group2")
    .Named(eTest::hello, "hello")
    .Named(eTest::goodbye, "goodbye")
    .Numeric(eTest::mask, "value")
  .End()
  .Build();
auto eValue = eDisc.value(eDisc["goodbye"]);

eValue = set(eDisc["NOTHING"]);                // ok because bit 7 is 0
eValue = set(eDisc["hello"]);                  // error: not right group
eValue = set(eDisc["group2"], eDisc["hello"]); // ok because explicitly stating to switch to group2

assert(eValue.is_active(eDisc["group2"]));
// Can't get a value from a group unless it has no subgroups
assert(eValue.get_string(eDisc["group2"]) == "hello");
assert(eValue.get(eDisc["group2"]) == eTest::hello);   // true
assert(eValue.get(eDisc["group2"]) == eTest::nothing); // this is also true

```

Don't actually need an enum:

```cpp
auto eDisc = build_enum_description<std::int8_t>()
  .Named(0, "NOTHING")
  .Named(1, "Something")
  .Build();

auto eValue = eDisc.value(); // zero initialised
auto eText = eValue.to_string();
auto sDefStream = eDisc.definition_stream();
```

## Enum Stream Specification

An enum definition stream consists of one command following another.

The basic data items are:

| item            | meaning                                     | type                                                            |
|-----------------|---------------------------------------------|-----------------------------------------------------------------|
| `bitmask`       | constrains comparisons or numeric output    | integer size of type or dint if compressed                      |
| `group_bitmask` | single selector bit for a conditional group | integer size of type or dint if compressed                      |
| `enum_value`    | masked enum value to compare against        | integer size of type or dint if compressed                      |
| `name`          | emitted name or numeric field label         | c-string                                                        |
| `gName`         | group name label                            | c-string                                                        |
| `pair`          | (`enum_value`, `name`)                      | integer size of type or dint if compressed followed by c-string |

The commands are:

| command          | encoded parameters                                                              | external parameters                                  | meaning                                                                                                                                                                                                                                                                                                                         |
|------------------|------------------------------------------------------------------ --------------|------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Terminate`      | `Unused`                                                                        | none                                                 | Ends stream processing. Required when the stream length is not known externally.                                                                                                                                                                                                                                                |
| `Named`          | `PairCount` and optional `Has_bitmask`                                          | `pair`, ... or `bitmask`, `pair`, ...                | Uses the effective bitmask for that command, compares the current value against each `enum_value`, and emits the matching `name`.                                                                                                                                                                                               |
| `Numeric`        | `Format`                                                                        | `bitmask`, `name`                                    | Emits a numeric representation of `value & bitmask` using the selected format.                                                                                                                                                                                                                                                  |
| `GroupIf`        | `If_CmdCount`  and optional `Has_GroupName`                                     | `group_bitmask`, `bitmask`, `*gName`, `command`, ... | If `(value & group_bitmask) != 0`, enters a nested scope with the given `bitmask` and executes the enclosed commands. If `Has_GroupName` set, then `*gName` is specified, otherwise not.                                                                                                                                        |
| `GroupIfNamed`   | `If_PairCount` and optional `Has_GroupName`                                     | `group_bitmask`, `bitmask`, `*gName`, `pair`, ...    | Conditional `Named` block in a nested scope. If `Has_GroupName` set, then `*gName` is specified, otherwise not.                                                                                                                                                                                                                 |
| `GroupIfNumeric` | `Format` and optional `Negate` and `Has_GroupName`                              | `group_bitmask`, `bitmask`, `*gName`, `name`         | Conditional `Numeric` block in a nested scope. `Negate` makes the inline numeric output belong to the `else` case instead of the `if` case. A following `Else` is allowed.  If `Has_GroupName` set, then `*gName` is specified, otherwise not.                                                                                  |
| `Else`           | `Else_PairCount` or `Else_CmdCount` and optional `Has_GroupName` and `ElseCmds` | `*gName`, `pair`, ... or `*gName`, `command`, ...    | Alternate branch for the immediately preceding `GroupIf`, `GroupIfNamed`, or `GroupIfNumeric` block. The `Else` branch uses the same block scope `bitmask` as the corresponding `if` branch. If `Has_GroupName` set, then `*gName` is specified, otherwise not.  If `ElseCmds` set, then uses `command`, ... otherwise `pairs`. |
| `ContinueScope`  | `Cont_PairCount` or `Cont_CmdCount`                                             | `pair`, ... or `command`, ...                        | Extends the immediately preceding branch when more pairs or commands are needed than fit in the originating count field.                                                                                                                                                                                                        |

The encoded parameters are:

| parameter           | meaning                                                                                                                                           |
|---------------------|---------------------------------------------------------------------------------------------------------------------------------------------------|
| `Unused`            | Reserved. Must be zero.                                                                                                                           |
| `Has_bitmask`       | Makes `Named` take a `bitmask` for that command.                                                                                                  |
| `PairCount`         | Number of pairs following `Named` (1-16).                                                                                                         |
| `If_PairCount`      | Number of pairs following `GroupIfNamed` (0-15).                                                                                                  |
| `Else_PairCount`    | Number of pairs following `Else` for a named branch (1-8).                                                                                        |
| `Cont_PairCount`    | Number of pairs following `ContinueScope` for a named branch (1-32).                                                                              |
| `If_CmdCount`       | Number of commands following `GroupIf` (0-15).                                                                                                    |
| `Else_CmdCount`     | Number of commands following `Else` for a command branch (1-8).                                                                                   |
| `Cont_CmdCount`     | Number of commands following `ContinueScope` for a command branch (1-32).                                                                         |
| `Format`            | Numeric formatting mode. A bitwise OR of the `Fmt*` flags below.                                                                                  |
| `FmtRightShiftBits` | If the least significant bit selected by `bitmask` is not bit 0, shift the masked value right until the selected range starts at bit 0.           |
| `FmtPackBits`       | Pack the selected bits densely into the low bits in least-significant-bit order.                                                                  |
| `FmtIsSigned`       | After shifting or packing, treat the most significant extracted bit as the sign bit and sign-extend it.                                           |
| `Negate`            | Inverts the inline numeric condition of `GroupIfNumeric`, so the numeric item belongs to the `else` case.                                         |
| `ElseCmds`          | When combined with `Else`, makes that `Else` use `Else_CmdCount` and `command`, ... instead of `Else_PairCount` and `pair`, ...                   |

> 💡NOTE:
>
> Count fields that allow zero are stored directly. Count fields that do not allow zero are stored as one less than the
> actual count, so that the full payload bit range can be used.

Execution rules:

1. Processing begins with `scope_bitmask = ~0`.
2. `Named` uses an effective bitmask. If `Has_bitmask` is clear, `effective_bitmask = scope_bitmask`. If
   `Has_bitmask` is set, the next item is a `bitmask`, and `effective_bitmask = bitmask` for that `Named` command only.
   `Named` compares real masked enum values. Values are not shifted before comparing: `(value & effective_bitmask) ==
   enum_value`.
3. `Numeric` formats `value & bitmask`. `Format` controls how the masked bits are interpreted.
4. `group_bitmask` must contain exactly one bit. It selects which branch of a conditional group is used.
5. Entering `GroupIf`, `GroupIfNamed`, or `GroupIfNumeric` evaluates whether `(value & group_bitmask) != 0`.
6. Each `GroupIf*` command introduces a new scope bitmask. On entry, the current scope bitmask is pushed and replaced
   with the block's `bitmask`. On exit, the previous scope bitmask is restored.
7. `group_bitmask`, `bitmask`, and `enum_value` are all constrained by the parent `scope_bitmask` and must be subsets of
   it. This allows `Compress` to encode them in condensed form. A stream that violates these constraints is invalid.
8. The matching `Else` branch reuses the same scope bitmask as the corresponding conditional branch.
9. `ContinueScope` continues the current branch at the same nesting level and with the same scope bitmask. It does not
   introduce a new condition.
10. `GroupIfNumeric` always carries its numeric `if` branch inline.
11. A `GroupIf` or `GroupIfNamed` `if` branch may be followed immediately by `Else` or `ContinueScope`.  A
    `GroupIfNumeric` `if` branch may only be followed immediately by `Else`.  An `Else` branch may be followed
    immediately by `ContinueScope`. Once the next command is neither a valid `Else` nor a valid `ContinueScope` for that
    branch, the branch ends and the previous scope bitmask is restored.
12. `Else` is only valid immediately after `GroupIf`, `GroupIfNamed` or `GroupIfNumeric`.
13. `ContinueScope` is only valid immediately after the branch it extends, i.e. `Named`, `GroupIf`, `GroupIfNamed`,
    `Else` or another `ContinueScope`.  A named branch may be extended only by more `pair`s, and a command branch may be
    extended only by more `command`s.
14. A command-local `bitmask`, such as `Named` with `Has_bitmask` or `Numeric`, affects only that command. Only
    `GroupIf*` changes `scope_bitmask`.

Because nested scope bitmasks must always narrow their parent scope, malformed or corrupted streams are easier to detect
than with a model that relies on a separate mutable mask-setting command. More compact streams are also possible because
the scope bitmask is carried directly by the `GroupIf*` block that uses it.

## Possible Render Mechanism

Here is some pseudo-C++ for one possible rendering policy:

```cpp
render(value, stream):
  scope_bitmask = ~0

  for each command in stream:
    if (command == Terminate)
      break;

    switch (command):
      case Named:
        effective_bitmask = HasBitmask ? bitmask : scope_bitmask
        if any pair matches (value & effective_bitmask) == enum_value:
          if enum_value == 0:
            add_to_zero_queue(name)
          else:
            add_to_nonzero_queue(name)
        break

      case Numeric:
        add_to_num_queue(name, format(value & bitmask, Format))
        break

      case GroupIf:
      case GroupIfNamed:
        if (value & group_bitmask) != 0:
          push scope_bitmask
          scope_bitmask = bitmask
          evaluate if_branch
          pop scope_bitmask
        else if next command is Else:
          push scope_bitmask
          scope_bitmask = bitmask
          evaluate else_branch
          pop scope_bitmask
        break

      case GroupIfNumeric:
        if (((value & group_bitmask) != 0) != (Negate != 0)):
          push scope_bitmask
          scope_bitmask = bitmask
          add_to_num_queue(name, format(value & bitmask, Format))
          pop scope_bitmask
        else if next command is Else:
          push scope_bitmask
          scope_bitmask = bitmask
          evaluate else_branch // commands if Else has fElseCmd set, pairs otherwise
          pop scope_bitmask
        break

  // non-zero values are separated with '|' (they have bits set); zero values are not (they don't).
  emit_nonzero() // separates items with '|'
  emit_zero()    // separates items with ',' or ' '
  emit_num()     // separates items with ',' or ' '
```
