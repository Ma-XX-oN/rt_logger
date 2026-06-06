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
    - [Runtime Type Selection](#runtime-type-selection)
- [Usage Examples](#usage-examples)
  - [Naming Convention Used In These Examples](#naming-convention-used-in-these-examples)
  - [1. Build An Enum Description](#1-build-an-enum-description)
  - [2. Render A Runtime Value With That Description](#2-render-a-runtime-value-with-that-description)
  - [3. Let A Flag Change How Other Bits Are Interpreted](#3-let-a-flag-change-how-other-bits-are-interpreted)
  - [4. Transmit Enum Semantics To Another Process](#4-transmit-enum-semantics-to-another-process)
  - [5. Re-Emit A Program Into A Fixed Buffer](#5-re-emit-a-program-into-a-fixed-buffer)
  - [6. Re-Emit A Program As `std::string` Or To `std::ostream`](#6-re-emit-a-program-as-stdstring-or-to-stdostream)
  - [7. Reduce Compile-Time Storage With The Macro](#7-reduce-compile-time-storage-with-the-macro)
- [Future Possible Semantics](#future-possible-semantics)
  - [1. Reflect Names Back Into Values](#1-reflect-names-back-into-values)
  - [2. Add Group-Aware Safety Helpers](#2-add-group-aware-safety-helpers)
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

The minimum valid program is exactly one byte: the storage-type header. After that header, the program body may be
empty. The stream is processed sequentially. If the stream length is known externally, decoding may stop at the end of
the stream. Otherwise the stream may use `Terminate` as an explicit end marker when the caller chooses to allow it.

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
- compile-time and runtime description building starts from:
  - `build_enum_description<Settings>()`
  - `BUILD_ENUM_DESCRIPTION(EnumType, ...)` when the default two-pass shrink-to-fit macro is desired
- runtime and constexpr stream decoding is exposed through the builder chain:
  - `build_enum_description<Settings>().decode_program(program_sv, throw_on_terminate).Build()`
  - `build_any_enum_description<StringAndItemCapacity>().decode_program(program_sv, throw_on_terminate).Build()`
- stream emission is exposed on `Enum` itself:
  - `program_size(compress, append_terminate)`
  - `output_program(char* begin, char* end, compress, append_terminate)`
  - `output_program(std::ostream& os, compress, append_terminate)`
  - `output_program(compress, append_terminate)` returning `std::string`
- `string_id_t` and `item_id_t` are public storage ids in `Constexpr`, defined with the storage layer rather than inside
  enum-encoding internals.
- `ProgramWriter` and `EnumEncoder` remain internal low-level helpers. Most callers should use the `Enum` output
  functions instead of driving the writer and encoder manually.
- Stored enum items continue to own their `encode(...)` member functions. Scope-introducing block logic remains in a
  free helper instead of being spread across ad-hoc policy objects.

### Builder Shape

The builder direction is typed chaining rather than lambdas or variadic free-item factories.

Reasons:

- chaining keeps builder operations scoped on the builder object, which avoids namespace-lookup friction for helpers
  such as `Named`, `Numeric`, `If`, and `Else`
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

#### Runtime Type Selection

The by-wire program already carries the underlying width and signedness in its one-byte storage header. The missing
piece is a variant-backed runtime-selected decoded holder for receivers that do not know the enum value type at
compile time.

`build_any_enum_description<...>().decode_program(...).Build()` fills that gap. It reads the header once, dispatches to
the matching typed decoder, and stores the result in `AnyEnumDescription<StringAndItemCapacity>`, which is backed by a
private `std::variant` of the 8 concrete signed and unsigned integer enum-description types.

That means the receiver parses the transmitted program once, keeps the decoded description, and then reuses it through
`is_signed()`, `value_unsigned(...)`, `value_signed(...)`, `program_size(...)`, and `output_program(...)` without
reparsing the original bytes. `visit(...)` remains available as an advanced escape hatch when a caller truly needs the
active typed description.

A future implementation could still move to a single max-width erased representation if that ever becomes more
ergonomic than the visitor-based wrapper.

## Usage Examples

The examples in this section describe the implemented public surface for
building, rendering, decoding, and re-emitting enum definition streams.

### Naming Convention Used In These Examples

The examples below use this naming convention:

- `eType` for the enum type itself
- `TypeDesc` for an enum description object
- `Value` for ordinary enum values
- `fValue` for flag bits
- `mValue` for masks

### 1. Build An Enum Description

If you are only naming distinct enum values, use the plain `Named(value, name)`
form. Add a mask only when the same storage really contains a masked field.

```cpp
enum class eMode : std::uint8_t {
  Idle = 0x00,
  Busy = 0x01,
  Error = 0x02,
};

constexpr auto ModeDesc = Constexpr::build_enum_description<
  Constexpr::EnumSettings<eMode>
>()
  .Named(eMode::Idle, "IDLE")
  .Named(eMode::Busy, "BUSY")
  .Named(eMode::Error, "ERROR")
  .Build();
```

This builds a description of the enum type itself. The resulting `ModeDesc`
can then interpret runtime values of `eMode` into human readable text.

### 2. Render A Runtime Value With That Description

`Enum::value(...)` returns a streamable view, and `Enum::operator()(...)` is an
alias for the same thing.

```cpp
eMode const value{ eMode::Busy };

std::string text{ ModeDesc.value(value).to_string() };
// text == "BUSY"

std::ostringstream os{};
os << ModeDesc(value);
// os.str() == "BUSY"
```

### 3. Let A Flag Change How Other Bits Are Interpreted

Use `If(...)`, `Else(...)`, and `End()` when one flag bit changes how the rest
of the stored bits should be interpreted.

```cpp
enum class ePacketState : std::uint8_t {
  fPrimary = 0x80,
  mKindMask = 0x0F,
};

auto PacketDesc = Constexpr::build_enum_description<Constexpr::EnumSettings<ePacketState>>()
  .If(ePacketState::fPrimary, ePacketState::mKindMask, "primary")
    .Named(ePacketState{ 0x01 }, "ONE")
    .Named(ePacketState{ 0x02 }, "TWO")
  .Else("secondary")
    .Named(ePacketState{ 0x01 }, "HELLO")
    .Named(ePacketState{ 0x02 }, "GOODBYE")
  .End()
  .Build();

std::string primary_text{ PacketDesc(ePacketState{ 0x81 }).to_string() };
std::string secondary_text{ PacketDesc(ePacketState{ 0x01 }).to_string() };
std::string secondary_goodbye_text{ PacketDesc(ePacketState{ 0x02 }).to_string() };
// primary_text           == "ONE"
// secondary_text         == "HELLO"
// secondary_goodbye_text == "GOODBYE"
```

Here the same low field value `0x01` means different things depending on the
flag bit:

- `0x81` means `"ONE"` because `fPrimary` selects the primary interpretation
- `0x01` means `"HELLO"` because `fPrimary` is clear and the secondary
  interpretation applies instead

The branch names `"primary"` and `"secondary"` are optional. They are not
rendered today, but they may be useful for future group-oriented APIs or
tooling.

### 4. Transmit Enum Semantics To Another Process

You can emit the description program from one process and reconstruct the same
enum semantics in another process that does not have the enum type compiled in.

```cpp
// Sender side: serialize the enum description semantics.
std::string const program{ ModeDesc.output_program() };

// Receiver side: reconstruct the same description from the transmitted bytes.
auto decoded = Constexpr::build_enum_description<Constexpr::EnumSettings<eMode>>()
  .decode_program(program)
  .Build();

std::string text{ decoded(eMode::Busy).to_string() };
// text == "BUSY"
```

Pass `false` for `throw_on_terminate` when `Terminate` should be treated as a
legal end-of-program marker instead of a parse error.

When the receiver does not know the enum value type at compile time, use the
variant-backed runtime-selected decode path instead:

```cpp
auto AnyDesc = Constexpr::build_any_enum_description<Constexpr::reserve_space(512, 512)>()
  .decode_program(program)
  .Build();

std::string text;
if (AnyDesc.is_signed()) {
  text = AnyDesc.value_signed(0x01).to_string();
} else {
  text = AnyDesc.value_unsigned(0x01u).to_string();
}
// text == "BUSY"
```

Use `value_unsigned(...)` for unsigned-by-wire descriptions and `value_signed(...)`
for signed-by-wire descriptions. `visit(...)` is still there for advanced typed
access, but it does not need to be the normal rendering path.

### 5. Re-Emit A Program Into A Fixed Buffer

Use `program_size()` first, then write into caller-owned storage with
`output_program(begin, end, ...)`.

```cpp
std::size_t const bytes_needed{ ModeDesc.program_size() };
std::vector<char> buffer(bytes_needed);

char* const end{
  ModeDesc.output_program(buffer.data(), buffer.data() + buffer.size())
};

assert(static_cast<std::size_t>(end - buffer.data()) == bytes_needed);
```

### 6. Re-Emit A Program As `std::string` Or To `std::ostream`

These overloads are runtime conveniences layered over the same encode walk.

```cpp
std::string const program{ ModeDesc.output_program() };

ModeDesc.output_program(std::cout) << '\n';

std::string const terminated_program{
  ModeDesc.output_program(false, true)
};
```

Use `append_terminate = true` when the receiver does not already know the
program length and needs an explicit end marker.

### 7. Reduce Compile-Time Storage With The Macro

If you want the stored enum description itself to be made smaller at compile
time, define it with `BUILD_ENUM_DESCRIPTION(...)` instead of the explicit
builder form. The two-pass macro can shrink the backing storage to the used
size.

```cpp
constexpr auto ModeDesc = BUILD_ENUM_DESCRIPTION(eMode,
  .Named(eMode::Idle, "IDLE")
  .Named(eMode::Busy, "BUSY")
  .Named(eMode::Error, "ERROR")
);
```

## Future Possible Semantics

The ideas in this section are intentionally beyond the currently implemented
surface.

### 1. Reflect Names Back Into Values

One possible extension would be to let a description map names back into enum
values.

For grouped descriptions, that probably should not return the bare enum type in
all cases. The same underlying numeric value can belong to more than one group,
so a description-aware token or wrapped value is safer than pretending the raw
enum value is always enough context.

If such a lookup is performed during constant evaluation on a `constexpr`
description object, the compiler can fold the result down to the numeric value.
At runtime, the generated code would then see only the resulting number instead
of performing a string search.

```cpp
// Not implemented today.
constexpr auto busy_token{ ModeDesc["BUSY"] };
```

### 2. Add Group-Aware Safety Helpers

Another possible extension would be helper APIs that make grouped enum values
safer to manipulate.

When the same numeric sub-value means different things in different groups, a
plain assignment can be ambiguous. Future helpers could require an explicit
group switch or reject writes that do not match the active group.

Using the `PacketDesc` example from
[Let A Flag Change How Other Bits Are Interpreted](#3-let-a-flag-change-how-other-bits-are-interpreted):

```cpp
// Not implemented today.
auto value = PacketDesc.value(ePacketState{ 0x01 });

value = PacketDesc.set("HELLO");               // ok if already in the secondary group
value = PacketDesc.set("primary", "ONE");      // explicit group switch
value = PacketDesc.force_set("GOODBYE");       // explicit unchecked switch
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
|------------------|---------------------------------------------------------------------------------|------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
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
12. `Else` is postfix-only. It is never a standalone command. A decoder recognizes it only immediately after finishing
    the `if` body of a `GroupIf`, `GroupIfNamed`, or `GroupIfNumeric` command.
13. `ContinueScope` is postfix-only. It is never a standalone command. A decoder recognizes it only immediately after
    finishing the branch it extends, i.e. `Named`, `GroupIf`, `GroupIfNamed`, `Else` or another `ContinueScope`. A named
    branch may be extended only by more `pair`s, and a command branch may be extended only by more `command`s.
14. A `GroupIf*` command and its immediately following valid `Else`, if present, count as one parent-scope command
    construct. `Else` does not begin a second sibling command.
15. The embedded branch counts are what make `Else` and `ContinueScope` unambiguous. A decoder only considers those
    opcodes in the postfix positions created by the immediately preceding branch structure.
16. A command-local `bitmask`, such as `Named` with `Has_bitmask` or `Numeric`, affects only that command. Only
    `GroupIf*` changes `scope_bitmask`.

Because nested scope bitmasks must always narrow their parent scope, malformed or corrupted streams are easier to detect
than with a model that relies on a separate mutable mask-setting command. More compact streams are also possible because
the scope bitmask is carried directly by the `GroupIf*` block that uses it.

## Possible Render Mechanism

Here is some pseudo-C++ for one possible rendering policy:

```cpp
render(value, stream):
  scope_bitmask = ~0

  // ContinueScope: extends the preceding Named or branch command list inline.
  // Handle it as part of reading that branch's pairs/commands, not as a top-level command.
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
