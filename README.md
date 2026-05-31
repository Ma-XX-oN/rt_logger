# A C++ RT Logger <!-- omit in toc -->

- [Purpose](#purpose)
  - [Small](#small)
  - [Fast](#fast)
  - [Resilient](#resilient)
- [Dependencies to build](#dependencies-to-build)
- [VS Code Configuration](#vs-code-configuration)
- [Design](#design)
  - [Type Definitions](#type-definitions)
    - [Dynamic Integers (dint)](#dynamic-integers-dint)
    - [C-String](#c-string)
      - [Ensuring Generated FString CRCs Are Unique](#ensuring-generated-fstring-crcs-are-unique)
    - [Enum Definition Stream](#enum-definition-stream)
      - [Enum Stream Specification](#enum-stream-specification)
    - [Possible Render Mechanism](#possible-render-mechanism)
  - [Packet Design](#packet-design)
    - [General Block Format](#general-block-format)
      - [Field 2: Sequence And Continuation Flags](#field-2-sequence-and-continuation-flags)
      - [Field 3: Payload Length](#field-3-payload-length)
      - [Field 5: CRC16](#field-5-crc16)
      - [Field 6: Fence](#field-6-fence)
    - [Payload specifications](#payload-specifications)
      - [1. String Block Payload](#1-string-block-payload)
      - [2. Enum Block Payload](#2-enum-block-payload)
      - [3. Data Block Payload](#3-data-block-payload)
      - [4. Time Anchor Block Payload](#4-time-anchor-block-payload)
      - [5. Drop Count Block Payload](#5-drop-count-block-payload)
      - [6. Test Pattern](#6-test-pattern)

## Purpose

When compiled, this is to be a very small, fast and resilient logger.

### Small

Data stored as minimally as possible with little overhead.

I'm hoping to have minimal binary footprint.

### Fast

The logger writes the string formatting sparingly, by stating the string and an
id that represents it once before use and occasionally in case of data
corruption.

All parameters are to be stored in the logger machine's binary format.  (May
have a test pattern block to help automate reconstruction).  This reduces
conversion-to-string overhead.

### Resilient

The data is in a binary format, so there are fences and CRCs to help with
recovery.  If data crosses blocks, the CRC's seed starts where the previous
block leaves off.

## Dependencies to build

```bash
# Install cmake, ninja and gtest dependencies
pacman -S \
  mingw-w64-clang-x86_64-cmake \
  mingw-w64-clang-x86_64-ninja \
  mingw-w64-clang-x86_64-gtest
```

## VS Code Configuration

This repo keeps the build truth in `CMakeLists.txt` and `CMakePresets.json`.
That includes the CLANG64 toolchain, the test targets, and exporting
`build-clang/compile_commands.json` for editor tooling.

`.vscode/settings.json` is only the editor-side consumer of that build data. It
points the C/C++ extension at the CLANG64 compiler, the CMake Tools provider,
and `build-clang/compile_commands.json` so IntelliSense uses the same context
as the real build.

If IntelliSense starts showing bogus include or language-mode errors, refresh
the build metadata with:

```bash
cmake --preset clang64-debug-ubsan -D DINT_BUILD_TESTS=ON
```

If VS Code still shows stale squiggles after that, reload the window or run
`C/C++: Reset IntelliSense Database`.

## Design

### Type Definitions

#### Dynamic Integers (dint)

A dynamic integer is a sequence of consecutive 8 bit bytes where the most
significant bit is the continuation flag. If that bit is set, another dint byte
follows. If it is clear, the current byte is the last byte in the integer
sequence. The remaining 7 bits carry payload.

Unsigned values are stored in 7-bit chunks starting from the least significant
bits. That means small unsigned values fit in a single byte, and larger values
grow by one byte for each additional 7 bits of payload they need.

Signed values use the same 7-bit chunking, but the most significant payload
chunk also carries the sign in bit `0x40`. Negative values are first bitwise
inverted, then encoded as payload, and finally marked negative by setting that
sign bit in the most significant chunk. During decode, the payload is
reconstructed first, the sign bit in the terminal chunk is checked, and if the
value was marked negative the bits are inverted.

There is one extra signed case. If the most significant payload chunk already
needs bit `0x40` as data, that chunk cannot also store the sign. When that
happens, the encoder marks that byte as a continuation byte and appends one
extra terminal byte whose only job is to carry the sign. This keeps the
encoding unambiguous while still keeping small signed values compact.

In practice, this means dints are a compact representation for values that are
usually small, while still allowing larger integers to grow naturally in 7-bit
steps instead of always paying the full fixed-width storage cost.

#### C-String

The characters in non-format c-strings are just NUL terminated character
streams.

Format strings are also NUL terminated but have control codes embedded in them
to represent the stored type and how it should be output.  Type specification is
key as the data isn't translated to text at the time of writing, but at the time
of reading.  All values are stored in the machine's representation, meaning that
the endianness of the values are kept as is and has to be fixed to the reader's
end.  This is done to reduce the writer's latency as much as possible.

Any value in a string that is between [1, 31] or [0x01, 0x1F] inclusive is
considered a control code. Subtracting 1 from that value results in an inclusive
range of [0, 30] or [0x00, 0x1E].

Backslash, tab and LF are encoded in the string as R"(\\\\)", R"(\n)" and
R"(\t)" respectively.  I.e.  those characters are stored as escaped characters
and are represented by 2 characters.

These specifications prevent embedding a NUL into a string and make it easier to
use flags on the integer set for specific operations.

There are 3 different control sequences.

1. TYPE_SPEC

   This specifies the binary representation of the type being printed.  This is
   because the values are not translated to text, but are stored as the machine
   logger's binary format to be converted later to reduce logging latency.

   1. Numbers and strings

      Numbers and string types are specified as one byte.

   2. Enum

      Enums are followed by a dint (dynamic integer) to specify which registered
      enum to use.  A dint is a 7 bit number with a continuation bit to allow a
      number to be as small as 1 byte for small numbers but can theoretically
      represent any sized number.

   3. Array

      All of those described previously can be prefixed with an Array MODIFIER.
      That consists of Array, followed by a dint and can be stacked 3 levels
      deep. That limitation only exists because I'm not sure how to represent 4D
      arrays on output.

2. FORMATTING INFO

   The formatting info consists of 1-6 bytes (2-12 if using arrays), depending
   on what is requested. This excludes the starting eType byte.  A sequence
   always ends with an eFmtLetter byte.  Sizes stated are minimal values.  If
   embedded dints specified are greater than 127, then it could mean a larger
   size.  Embedded dints of 0 are considered an error.  If done as text, it
   would easily be twice the size.

   ```cpp
            11111             111111111122222
   12345678901234    123456789012345678901234 // Arrays are deduced, not
   {:0^+z#10.10b}    {:0^+z#10.10!a10a10a10d} // specified as shown here.
   ```

   When arrays are displayed, they are space delimited when minimum width
   formatting is specified, otherwise it is comma delimited.  This is actually
   dependent on the renderer side, so it could be configurable there.

   ```text
   | byte 1    | byte 2     | byte 3   | byte 4     | byte 5     | byte 6     | byte 7     |
   |-----------|------------|----------|------------|------------|------------|------------|
   | eType     | eFmtLetter |          |            |            |            |            |
   | eType     | eFmt0      | eFmt1    | eFmtLetter |            |            |            |
   | eType     | eFmt0      | eFmt1    | MIN_DINT   | eFmtLetter |            |            |
   | eType     | eFmt0      | eFmt1    | PREC_DINT  | eFmtLetter |            |            |
   | eType     | eFmt0      | eFmt1    | MIN_DINT   | PREC_DINT  | eFmtLetter |            |
   | eType     | eFmt0      | eFmt1    | FILL_CHAR  | eFmtLetter |            |            |
   | eType     | eFmt0      | eFmt1    | MIN_DINT   | FILL_CHAR  | eFmtLetter |            |
   | eType     | eFmt0      | eFmt1    | PREC_DINT  | FILL_CHAR  | eFmtLetter |            |
   | eType     | eFmt0      | eFmt1    | MIN_DINT   | PREC_DINT  | FILL_CHAR  | eFmtLetter |
   | Enum      | enum id    | ...      |            |            |            |            |
   | Array     | DINT_SIZE  | eType ...|            |            |            |            |
   | Array     | DINT_SIZE  | Array    | DINT_SIZE  | eType ...  |            |            |
   | Array     | DINT_SIZE  | Array    | DINT_SIZE  | Array      | DINT_SIZE  | eType ...  |
   ```

3. End of an fstring

   All fstrings are NUL terminated.

   1. SALT - Visible String Terminator

      All fstrings have a CRC16 with them to distinguish them from each other.
      If an fstring has the same CRC16 as another, it is considered an error and
      can be forced to be made different by adding a salt to the end of the
      string, which is the Salt value followed by nothing or a dint THAT IS NOT
      ZERO.

      This adds one or more bytes to the string. A DINT_SALT value of 0 is an
      error, but its equivalent is to have no DINT_SALT at all.  A dint is used
      to ensure that the salt is always part of the fstring and doesn't
      prematurely terminate it with an inadvertent NUL. The salt shouldn't be
      printed on the displaying end.

      ```text
      | byte 1    | byte 2        | byte 3   |
      |-----------|---------------|----------|
      | NUL       |               |          |
      | Salt      | NUL           |          |
      | Salt      | DINT_SALT     | NUL      |
      ```

##### Ensuring Generated FString CRCs Are Unique

To ensure that all FStrings generate a unique CRC, run the compiler on each
source file in preprocess-only mode.  Just call the compiler with `-DSOURCE_FILE
<source_file>` on the following code to do the first pass.  Append all of these
together.

```cpp

#define FIRST_ARG_(first, ...) first
#define FIRST_ARG(...) FIRST_ARG_(__VA_ARGS__, unused)

// Override the default RTLOG macro to do a transform to make it easier to
// extract all of the rt logs.  That way the preprocessor does the heavy lifting
// on first pass.
#define RTLOG_INFO(...)  RTLOG(__FILE__, __LINE__, info,  FIRST_ARG(__VA_ARGS__))
#define RTLOG_DEBUG(...) RTLOG(__FILE__, __LINE__, debug, FIRST_ARG(__VA_ARGS__))
#define RTLOG_WARN(...)  RTLOG(__FILE__, __LINE__, warn,  FIRST_ARG(__VA_ARGS__))
#define RTLOG_ERROR(...) RTLOG(__FILE__, __LINE__, error, FIRST_ARG(__VA_ARGS__))

#include SOURCE_FILE
```

Then filter using:

```bash
sed -E -n 's/^[ \t]*(RTLOG\(.*)/\1/p' \
  | LC_ALL=C sort -u \
  | sed -E -n 'h; s/^RTLOG\([ \t]*("[^"]*"),[ \t]*([0-9]+),.*/#line \2 \1/p; x; p'
```

That does:

1. Remove all text other than the log entries.
2. Remove any duplicates caused by including the same header that logs.
3. Add `#line` entries before each line.

Then do a second compile with `-DCONCAT_SOURCE_FILES <concat_source_files>` on
the resulting file.

```cpp
#include <cstdint>

enum severity {
  info,
  debug,
  warn,
  error,
};

constexpr std::uint16_t crc16(char const* s) {
  std::uint16_t h = 0;

  for (; *s; ++s) {
    h = std::uint16_t(
      h * 131u + static_cast<unsigned char>(*s)
    );
  }

  return h;
}

constexpr std::uint16_t gen_crc(severity sev, char const* s) {
  return std::uint16_t(
    crc16(s) ^ (static_cast<unsigned>(sev) << 8)
  );
}

#define STRINGIZE_(val) #val
#define STRINGIZE(val) STRINGIZE_(val)

#define RTLOG(file, line, severity, ...) \
  case gen_crc(severity, file ":" STRINGIZE(line) ": " __VA_ARGS__):

void verify_log_crc_uniqueness() {
  switch (0) {

#include CONCAT_SOURCE_FILES

  }
}
```

Voila!  Should now give a compile-time error showing where the CRCs are the
same.

> **NOTE:**
>
> This will not work with raw string literals that are multiline.  E.g.  The
> following will not properly work.
>
> ```cpp
> RTLOG_DEBUG( R"(Line 1
> Line 2)" );
> ```
>
> Do this instead:
>
> ```cpp
> RTLOG_DEBUG( "Line 1\n"
> "Line 2" );
> ```

#### Enum Definition Stream

```c++
enum eEnumStorageType : std::uint8_t {
  // Integer types
   Int8 = 0x00,  Int16 = 0x01,  Int32 = 0x02,  Int64 = 0x03,
  UInt8 = 0x04, UInt16 = 0x05, UInt32 = 0x06, UInt64 = 0x07,

  // States how constrained group_bitmask, bitmask and enum_value values are stored
  Compress          = 0x08, // Encode constrained values as condensed dints under the parent scope
};
```

An enum definition stream uses this storage type and a compact descriptor
program to describe how enum values are named, grouped, and formatted.

The stream is processed sequentially. If the stream length is known externally,
decoding may stop at the end of the stream. Otherwise the stream must end with a
`Terminate` command.

This format makes scope explicit in each `GroupIf*` command. The block's
`bitmask` replaces the earlier idea of a separate `SetMaskValue` command, so the
scope used by a conditional block is visible at the block header instead of
being ambient mutable state.

`Compress` changes only how constrained values are encoded, not the semantics
below. If `Compress` is clear, constrained `group_bitmask`, `bitmask`, and
`enum_value` values are stored full width. If `Compress` is set, those same
values are stored as condensed dints relative to the parent
`current_scope_bitmask` and are decoded back before the execution rules below
are applied.

The command set is:

```c++
enum eEnumCommand : std::uint8_t {
  OpCodeMask         = 0b1110'0000,
  PayloadMask        = 0b0001'1111,

  Terminate          = 0 << 5,  // End of stream if stream length not known.

  Named              = 1 << 5,  // Specifies pairs; compares (value & current or stated bitmask) to enum_value.
   HasBitmask        =  1 << 4, //   States if bitmask is specified.
  
  Numeric            = 2 << 5,  // Specifies name for bits for stated bitmask.
   FmtRightShiftBits =  1 << 0, //   Shift bits so least significant bits coinciding with bitmask are at the 0th bit.
   FmtPackBits       =  1 << 1, //   Condense bits coinciding with bitmask.
   FmtIsSigned       =  1 << 2, //   Sign extend bit coinciding with most significant bit of bitmask.

  GroupIf            = 3 << 5,  // If group_bitmask set, use bitmask on following commands.
  GroupIfNamed       = 4 << 5,  // If group_bitmask set, use bitmask on following pairs.
  ContinueScope      = 5 << 5,  // Continue the current named or command branch.
  Else               = 6 << 5,  // Continue the current conditional scope as else group.

  GroupIfNumeric     = 7 << 5,  // If group_bitmask set, specify numeric output for stated bitmask.
   Negate            =  1 << 3, //   Inverts the inline numeric condition, so the numeric item belongs to the else case.
   ElseCmd           =  1 << 4, //   If followed by Else, that Else is a command list; otherwise it is a pair list.
   // Can also take Fmt* flags.
};
```

##### Enum Stream Specification

An enum definition stream consists of one command following another.

The basic data items are:

| item            | meaning                                     |
|-----------------|---------------------------------------------|
| `bitmask`       | constrains comparisons or numeric output    |
| `enum_value`    | masked enum value to compare against        |
| `name`          | emitted name or numeric field label         |
| `pair`          | (`enum_value`, `name`)                      |
| `group_bitmask` | single selector bit for a conditional group |

The commands are:

| command          | encoded parameters                           | external parameters                        | meaning                                                                                                                                                                                                                                                                                                                         |
|------------------|----------------------------------------------|--------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Terminate`      | `Unused`                                     | none                                       | Ends stream processing. Required when the stream length is not known externally.                                                                                                                                                                                                                                                |
| `Named`          | `Pair_count` and optional `Has_bitmask`      | `pair`, ... or `bitmask`, `pair`, ...      | Uses the effective bitmask for that command, compares the current value against each `enum_value`, and emits the matching `name`.                                                                                                                                                                                               |
| `Numeric`        | `Format`                                     | `bitmask`, `name`                          | Emits a numeric representation of `value & bitmask` using the selected format.                                                                                                                                                                                                                                                  |
| `GroupIf`        | `If_cmd_count`                               | `group_bitmask`, `bitmask`, `command`, ... | If `(value & group_bitmask) != 0`, enters a nested scope with the given `bitmask` and executes the enclosed commands.                                                                                                                                                                                                           |
| `GroupIfNamed`   | `If_pair_count`                              | `group_bitmask`, `bitmask`, `pair`, ...    | Conditional `Named` block in a nested scope.                                                                                                                                                                                                                                                                                    |
| `GroupIfNumeric` | `Format` and optional `Negate` and `ElseCmd` | `group_bitmask`, `bitmask`, `name`         | Conditional `Numeric` block in a nested scope. `Negate` makes the inline numeric output belong to the `else` case instead of the `if` case. A following `Else` is allowed; with `ElseCmd` clear it behaves like the `Else` branch of a `GroupIfNamed`, and with `ElseCmd` set it behaves like the `Else` branch of a `GroupIf`. |
| `Else`           | `Else_pair_count` or `Else_cmd_count`        | `pair`, ... or `command`, ...              | Alternate branch for the immediately preceding `GroupIf`, `GroupIfNamed`, or `GroupIfNumeric` block. The `Else` branch uses the same block scope `bitmask` as the corresponding `if` branch.                                                                                                                                    |
| `ContinueScope`  | `Cont_pair_count` or `Cont_cmd_count`        | `pair`, ... or `command`, ...              | Extends the immediately preceding branch when more pairs or commands are needed than fit in the originating count field.                                                                                                                                                                                                        |

The encoded parameters are:

| parameter           | meaning                                                                                                                                           |
|---------------------|---------------------------------------------------------------------------------------------------------------------------------------------------|
| `Unused`            | Reserved. Must be zero.                                                                                                                           |
| `Has_bitmask`       | Makes `Named` take a `bitmask` for that command.                                                                                                  |
| `Pair_count`        | Number of pairs following `Named` (1-16).                                                                                                         |
| `If_pair_count`     | Number of pairs following `GroupIfNamed` (0-31).                                                                                                  |
| `Else_pair_count`   | Number of pairs following `Else` for a named branch (1-32).                                                                                       |
| `Cont_pair_count`   | Number of pairs following `ContinueScope` for a named branch (1-32).                                                                              |
| `If_cmd_count`      | Number of commands following `GroupIf` (0-31).                                                                                                    |
| `Else_cmd_count`    | Number of commands following `Else` for a command branch (1-32).                                                                                  |
| `Cont_cmd_count`    | Number of commands following `ContinueScope` for a command branch (1-32).                                                                         |
| `Format`            | Numeric formatting mode. A bitwise OR of the `Fmt*` flags below.                                                                                  |
| `FmtRightShiftBits` | If the least significant bit selected by `bitmask` is not bit 0, shift the masked value right until the selected range starts at bit 0.           |
| `FmtPackBits`       | Pack the selected bits densely into the low bits in least-significant-bit order.                                                                  |
| `FmtIsSigned`       | After shifting or packing, treat the most significant extracted bit as the sign bit and sign-extend it.                                           |
| `Negate`            | Inverts the inline numeric condition of `GroupIfNumeric`, so the numeric item belongs to the `else` case.                                         |
| `ElseCmd`           | If `GroupIfNumeric` is followed by `Else`, makes that `Else` use `Else_cmd_count` and `command`, ... instead of `Else_pair_count` and `pair`, ... |

> 💡NOTE:
>
> Count fields that allow zero are stored directly. Count fields that do not
> allow zero are stored as one less than the actual count, so the full payload
> bit range can be used.

Execution rules:

1. Processing begins with `current_scope_bitmask = ~0`.
2. `Named` uses an effective bitmask.
   If `Has_bitmask` is clear, `effective_bitmask = current_scope_bitmask`.
   If `Has_bitmask` is set, the next item is a `bitmask`, and
   `effective_bitmask = bitmask` for that `Named` command only.
   `Named` compares real masked enum values. Values are not shifted before
   comparing: `(value & effective_bitmask) == enum_value`.
3. `Numeric` formats `value & bitmask`. `Format` controls how the masked bits
   are interpreted.
4. `group_bitmask` must contain exactly one bit. It selects which branch of a
   conditional group is used.
5. Entering `GroupIf`, `GroupIfNamed`, or `GroupIfNumeric` evaluates whether
   `(value & group_bitmask) != 0`.
6. Each `GroupIf*` command introduces a new scope bitmask. On entry, the
   current scope bitmask is pushed and replaced with the block's `bitmask`. On
   exit, the previous scope bitmask is restored.
7. `group_bitmask`, `bitmask`, and `enum_value` are all constrained by the
   parent `current_scope_bitmask` and must be subsets of it. This allows
   `Compress` to encode them in condensed form. A stream that violates these
   constraints is invalid.
8. The matching `Else` branch reuses the same scope bitmask as the
   corresponding conditional branch.
9. `ContinueScope` continues the current branch at the same nesting level and
   with the same scope bitmask. It does not introduce a new condition.
10. `GroupIfNumeric` always carries its numeric `if` branch inline. If it is
    followed by `Else`, that `Else` uses the same scope bitmask. With `ElseCmd`
    clear, the `Else` branch is a named branch as if continuing a
    `GroupIfNamed`. With `ElseCmd` set, the `Else` branch is a command branch as
    if continuing a `GroupIf`.
11. A `GroupIf` or `GroupIfNamed` `if` branch may be followed immediately by
    `Else` or `ContinueScope`. A `GroupIfNumeric` inline numeric branch may be
    followed immediately by `Else`. An `Else` branch may be followed
    immediately by `ContinueScope`. Once the next command is neither a valid
    `Else` nor a valid `ContinueScope` for that branch, the branch ends and the
    previous scope bitmask is restored.
12. `Else` is only valid immediately after `GroupIf`, `GroupIfNamed`, or
    `GroupIfNumeric`.
13. `ContinueScope` is only valid immediately after the branch it extends, i.e.
    `Named`, `GroupIf`, `GroupIfNamed`, `Else` or another `ContinueScope`.  A
    named branch may be extended only by more `pair`s, and a command branch may
    be extended only by more `command`s.
14. A command-local `bitmask`, such as `Named` with `Has_bitmask` or `Numeric`,
    affects only that command. Only `GroupIf*` changes `current_scope_bitmask`.

Because nested scope bitmasks must always narrow their parent scope, malformed
or corrupted streams are easier to detect than with a model that relies on a
separate mutable mask-setting command. More compact streams are also possible
because the scope bitmask is carried directly by the `GroupIf*` block that uses
it.

#### Possible Render Mechanism

Here is some pseudo-C++ for one possible rendering policy:

```cpp
render(value, stream):
  current_scope_bitmask = ~0

  for each command in stream:
    if (command == Terminate)
      break;

    switch (command):
      case Named:
        effective_bitmask = HasBitmask ? bitmask : current_scope_bitmask
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
          push current_scope_bitmask
          current_scope_bitmask = bitmask
          evaluate if_branch
          pop current_scope_bitmask
        else if next command is Else:
          push current_scope_bitmask
          current_scope_bitmask = bitmask
          evaluate else_branch
          pop current_scope_bitmask
        break

      case GroupIfNumeric:
        if (((value & group_bitmask) != 0) != (Negate != 0)):
          push current_scope_bitmask
          current_scope_bitmask = bitmask
          add_to_num_queue(name, format(value & bitmask, Format))
          pop current_scope_bitmask
        else if next command is Else:
          push current_scope_bitmask
          current_scope_bitmask = bitmask
          evaluate else_branch // commands if ElseCmd, pairs otherwise
          pop current_scope_bitmask
        break

  // non-zero values are separated with '|' (they have bits set); zero values are not (they don't).
  emit_nonzero() // separates items with '|'
  emit_zero()    // separates items with ',' or ' '
  emit_num()     // separates items with ',' or ' '
```

### Packet Design

The data is stored in packets with a maximum payload of 256 (I don't see any use
for an empty packet).  I may make this configurable, but will start with 256
byte payload.

> **UPDATE:**
>
> Given my current packet definitions, the minimum payload is 5.  That makes for
> 5-260 bytes per packet.  4 extra bytes doesn't sound like a lot, but it might
> be useful.

#### General Block Format

| field | bytes | description                                                                    |
|------:|------:|--------------------------------------------------------------------------------|
| 1     | 1     | Block type: StringBlock, EnumBlock, DataBlock, TimeAnchorBlock, DropCountBlock |
| 2     | 1     | Sequence number and continuation flags                                         |
| 3     | 1     | Payload length minus 5                                                         |
| 4     | 5-260 | Payload                                                                        |
| 5     | 2     | CRC16                                                                          |
| 6     | 4     | Fence                                                                          |

Fields 1-3 are the header, 4 is the payload, 5 is the CRC and 6 ends the block.

##### Field 2: Sequence And Continuation Flags

The 6-bit sequence number detects short discontinuities. It is not intended to
count long loss intervals.

| field | bit   | description                   |
|------:|------:|-------------------------------|
| 1     | 0-5   | Sequence number (64 values).  |
| 2     | 6     | Block continues.              |
| 3     | 7     | Block is continuing.          |

##### Field 3: Payload Length

Field 3 stores the payload length minus 5.  Since the minimum payload is 5
bytes, valid payload sizes are 5-260 bytes.

##### Field 5: CRC16

The CRC is based on [General Block](#general-block-format)'s fields 1-4.

A continued block's CRC is seeded by the previous block's CRC.  A
[Data Block](#3-data-block-payload) uses the string's CRC from the
[String Block](#1-string-block-payload) as the seed.  Otherwise it's seeded with
0.

##### Field 6: Fence

The fence is at the end because a block isn't finished unless a fence is found.
Before the first block is emitted, there will be an explicit fence to start the
blocks off.  Might be a different sequence like "!!!!" to show that this is the
very first of a logging run.

#### Payload specifications

Payload tables with a "repeat" column will have a count field prior to repeated
fields.  So any fields with a ✅ after a count field will be repeated that many
times.  If necessary, for nested repeat fields, there will be one ✅ for the
first level, two for the next, etc.

##### 1. String Block Payload

Contains 1 or more strings, with IDs and CRC16 for each.

| repeats | field | bytes    | description                                 |
|:-------:|------:|---------:|---------------------------------------------|
|         | 1     | 1        | Count of strings in block.                  |
|   ✅    | 2     | variable | String ID (dint).                           |
|   ✅    | 3     | 1        | Severity (Info, Warn, Error, Debug)         |
|   ✅    | 4     | variable | NUL terminated format string (c-string)     |
|   ✅    | 5     | 2        | The CRC for the severity and string (CRC16) |

The format string contains embedded info that represents the type and how it's
to be displayed.  Any character with a value less than 32 indicates the start of
a field.

See eType for more information.

##### 2. Enum Block Payload

| field | bytes     | description                                        |
|------:|----------:|----------------------------------------------------|
|  1    | variable  | Enum ID (dint)                                     |
|  2    | 1         | Underlying enum storage type (eEnumStorageType)    |
|  3    | variable  | Enum definition bytecode stream                    |

Field 3 is an enum definition bytecode stream. Its storage type, command set,
and interpretation rules are specified in [Enum Definition
Stream](#enum-definition-stream).

##### 3. Data Block Payload

This is the actual logged data.

| field | bytes    | description                                |
|------:|---------:|--------------------------------------------|
| 1     | 2        | Timestamp (offset from TimeAnchorBlock)    |
| 2     | 2        | Event count (rolls over every 64k events)  |
| 3     | variable | String ID (dint)                           |
| 4     | variable | Arguments (binary payload based on string) |

There is only one event per block.

Each event's event count is taken from a global counter that gets incremented
for every event sent out.  The CRC for this block is seeded from the CRC of the
string pointed at by the string ID. This helps verify that the correct string is
used with the payload.

##### 4. Time Anchor Block Payload

| field | bytes    | description                                |
|------:|---------:|--------------------------------------------|
| 1     | 8        | Timestamp (machine timestamp)              |

This is output periodically to keep the DataBlock's timestamp relevant. Maybe
every 10 minutes?

##### 5. Drop Count Block Payload

If the log buffers can't be emptied fast enough, some events will have to be
dropped.  First, repeated StringBlock and EnumBlock packets will be postponed.
Then Info packets will be the first to be dropped, then Debug, Warning and
finally Error.

This packet will periodically be output if there are any dropped packets.

| field | bytes    | description                                |
|------:|---------:|--------------------------------------------|
| 1     | 4        | Number of dropped debug events.            |
| 2     | 4        | Number of dropped info events.             |
| 3     | 4        | Number of dropped warning events.          |
| 4     | 4        | Number of dropped error events.            |

##### 6. Test Pattern

This specifies the basic information needed to identify the binary
representation used by the logging machine.

All integers are 2's complement and must be 1, 2, 4 or 8 bytes long.  Enums must
be based on those representations.  Attempting to log anything else will result
in a compile time error.

By definition of this library, `CHAR_BIT` must equal 8, making chars equal to
bytes.

> To think about:
>
> Not sure about NaN.  This could be useful or not.  Might instead have float
> and double and long double use an enum to specify a particular standard
> representation.

| field | bytes      | description                                       |
|------:|-----------:|---------------------------------------------------|
| 1     | 2          | Int16                       (0x0201)              |
| 1     | 4          | Int32                   (0x04030201)              |
| 1     | 8          | Int64           (0x0807060504030201)              |
| 1     | 1          | float                (sizeof(float))              |
| 1     | sizeof(f)  | float                   (positive 0)              |
| 1     | sizeof(f)  | float                   (negative 0)              |
| 1     | sizeof(f)  | float                   (positive ∞)              |
| 1     | sizeof(f)  | float                   (negative ∞)              |
| 1     | sizeof(f)  | float                 (not a number)              |
| 1     | sizeof(f)  | float                          (1e7)              |
| 1     | 1          | double              (sizeof(double))              |
| 1     | sizeof(d)  | double                  (positive 0)              |
| 1     | sizeof(d)  | double                  (negative 0)              |
| 1     | sizeof(d)  | double                  (positive ∞)              |
| 1     | sizeof(d)  | double                  (negative ∞)              |
| 1     | sizeof(d)  | double                (not a number)              |
| 1     | sizeof(d)  | double                         (1e7)              |
| 1     | 1          | Has long double test block (0/1)                  |
| 1     | 1          | long double    (sizeof(long double)) **optional** |
| 1     | sizeof(ld) | long double             (positive 0) **optional** |
| 1     | sizeof(ld) | long double             (negative 0) **optional** |
| 1     | sizeof(ld) | long double             (positive ∞) **optional** |
| 1     | sizeof(ld) | long double             (negative ∞) **optional** |
| 1     | sizeof(ld) | long double           (not a number) **optional** |
| 1     | sizeof(ld) | long double                    (1e7) **optional** |
