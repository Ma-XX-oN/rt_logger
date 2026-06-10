# A C++ RT Logger <!-- omit in toc -->

- [Purpose](#purpose)
  - [Small](#small)
  - [Fast](#fast)
  - [Resilient](#resilient)
- [Dependencies to build](#dependencies-to-build)
- [VS Code Configuration](#vs-code-configuration)
- [Design](#design)
  - [CRC16 Used](#crc16-used)
  - [Type Definitions](#type-definitions)
    - [Dynamic Integers (dint)](#dynamic-integers-dint)
    - [C-String](#c-string)
      - [Ensuring Generated FString CRCs Are Unique](#ensuring-generated-fstring-crcs-are-unique)
    - [Enum Definition Stream](#enum-definition-stream)
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
      - [6. Test Pattern - Possibility](#6-test-pattern---possibility)
  - [Packet Usage](#packet-usage)
    - [Life-cycle](#life-cycle)
    - [Producer Thread](#producer-thread)
    - [Consumer Thread](#consumer-thread)

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
cmake --preset clang64-debug-ubsan -D BUILD_TESTS=ON
```

If VS Code still shows stale squiggles after that, reload the window or run
`C/C++: Reset IntelliSense Database`.

## Design

### CRC16 Used

The CRC16 algorithm is CRC-16/XMODEM:

```text
width  = 16
poly   = 0x1021
init   = caller-provided seed, default 0x0000
refin  = false
refout = false
xorout = 0x0000
check  = 0x31C3 for "123456789" with seed 0
```

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

      | byte 1    | byte 2        | byte 3   |
      |-----------|---------------|----------|
      | NUL       |               |          |
      | Salt      | NUL           |          |
      | Salt      | DINT_SALT     | NUL      |

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

Possible implementation:

```cpp
#include <cstdint>

enum severity {
  info,
  debug,
  warn,
  error,
};

constexpr std::uint16_t crc16(char const* s) {
  // ... XMODEM CRC16 algorithm ...
  return crc16_result;
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

The enum definition stream design, builder-facing usage sketches, bytecode
format, and rendering notes now live in [Enum.md](Enum.md).

The short version is:

- an enum definition stream stores enum naming/grouping rules as a compact
  bytecode program
- the stream carries explicit conditional scope through `GroupIf*` block headers
- the packet format stores that program in the enum block payload
- the public round-trip surface is:
  - `BUILD_ENUM_DESCRIPTION(...)` or `build_enum_description<Settings>() ... .Build()`
  - `.decode_program(program_sv, throw_on_terminate).Build()` for decode
  - `build_any_enum_description<StringAndItemCapacity>() ... .decode_program(...).Build()` for variant-backed runtime-selected decode
  - `Enum::program_size()` and `Enum::output_program(...)` for re-emission

### Packet Design

The data is stored in packets with a minimum to maximum payload of 5 to 260
since the smallest packet payload is 5 bytes.

#### General Block Format

| field | bytes | description                                                                    |
| ----: | ----: | ------------------------------------------------------------------------------ |
|     1 |     1 | Block type: StringBlock, EnumBlock, DataBlock, TimeAnchorBlock, DropCountBlock |
|     2 |     1 | Sequence number and continuation flags                                         |
|     3 |     1 | Payload length minus 5                                                         |
|     4 | 5-260 | Payload                                                                        |
|     5 |     2 | CRC16                                                                          |
|     6 |     4 | Fence                                                                          |

Fields 1-3 are the header, 4 is the payload, 5 is the CRC and 6 ends the block.

##### Field 2: Sequence And Continuation Flags

The 6-bit sequence number identifies packet order in the global reservation
stream.  Since packets from different producers may be physically interleaved,
a non-adjacent sequence number does not by itself prove packet loss.  It only
indicates a possible gap until the reader has enough later packets to determine
whether the missing sequence number was dropped, delayed, or lost in transfer.
If packets were actually dropped, then a `Drop Count` packet will be emitted.
It is not intended to count long loss intervals.

| field |  bit | description                  |
| ----: | ---: | ---------------------------- |
|     1 |  0-5 | Sequence number (64 values). |
|     2 |    6 | Block continues.             |
|     3 |    7 | Block is continuing.         |

| bit 7 | bit 6 | Meaning                            |
| ----: | ----: | ---------------------------------- |
|     0 |     0 | single-packet block                |
|     0 |     1 | first packet of multipacket block  |
|     1 |     1 | middle packet of multipacket block |
|     1 |     0 | final packet of multipacket block  |

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

| repeats | field |    bytes | description                                 |
| :-----: | ----: | -------: | ------------------------------------------- |
|         |     1 |        1 | Count of strings in block.                  |
|    ✅   |     2 | variable | String ID (dint).                           |
|    ✅   |     3 |        1 | Severity (Info, Warn, Error, Debug)         |
|    ✅   |     4 | variable | NUL terminated format string (c-string)     |
|    ✅   |     5 |        2 | The CRC for the severity and string (CRC16) |

The format string contains embedded info that represents the type and how it's
to be displayed.  Any character with a value less than 32 indicates the start of
a field.

See eType for more information.

##### 2. Enum Block Payload

| field |    bytes | description                                     |
| ----: | -------: | ----------------------------------------------- |
|     1 | variable | Enum ID (dint)                                  |
|     2 |        1 | Underlying enum storage type (eEnumStorageType) |
|     3 | variable | Enum definition bytecode stream                 |

Field 3 is an enum definition bytecode stream. Its storage type, command set,
and interpretation rules are specified in [Enum.md](Enum.md).

##### 3. Data Block Payload

This is the actual logged data.

| field |    bytes | description                                |
| ----: | -------: | ------------------------------------------ |
|     1 |        2 | Timestamp (offset from TimeAnchorBlock)    |
|     2 |        2 | Event count (rolls over every 64k events)  |
|     3 | variable | String ID (dint)                           |
|     4 | variable | Arguments (binary payload based on string) |

There is only one event per block.

Each event's event count is taken from a global counter that gets incremented
for every event sent out.  The CRC for this block is seeded from the CRC of the
string pointed at by the string ID. This helps verify that the correct string is
used with the payload.

##### 4. Time Anchor Block Payload

| field | bytes | description                   |
| ----: | ----: | ----------------------------- |
|     1 |     8 | Timestamp (machine timestamp) |

This is output periodically to keep the DataBlock's timestamp relevant. Maybe
every 10 minutes?

##### 5. Drop Count Block Payload

If the log buffers can't be emptied fast enough, some events will have to be
dropped.  First, repeated StringBlock and EnumBlock packets will be postponed.
Then Info packets will be the first to be dropped, then Debug, Warning and
finally Error.

This packet will periodically be output if there are any dropped packets.

| field | bytes | description                                                |
| ----: | ----: | ---------------------------------------------------------- |
|     1 |     2 | Total number of dropped info packets.                      |
|     2 |     2 | Total number of published info, but incomplete packets.    |
|     3 |     2 | Total number of dropped debug packets.                     |
|     4 |     2 | Total number of published debug, but incomplete packets.   |
|     5 |     2 | Total number of dropped warning packets.                   |
|     6 |     2 | Total number of published warning, but incomplete packets. |
|     7 |     2 | Total number of dropped error packets.                     |
|     8 |     2 | Total number of published error, but incomplete packets.   |

##### 6. Test Pattern - Possibility

This is a possible specification to show the basic information needed to
identify the binary representation used by the logging machine.

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

### Packet Usage

Data spanning multiple packets is guaranteed to reserve contiguous sequence
numbers.  Packets from a single thread have ascending sequence numbers.
However, if multiple threads are sending messages, packets from different
threads may interleave in the output stream.

For example, if thread 1 has data that spans 2 packets and thread 2 has data
that spans 1 packet, then the output may appear as:

```text
[ t1:p1, t2:p1, t1:p2 ]
```

Or as a possible concrete sequence where thread 2 was reserved 3 and thread 1
was reserved 4 and 5:

```text
[ 4, 3, 5 ]
```

The packets for thread 1 still have contiguous sequence numbers, but they are
not guaranteed to be physically adjacent in the output stream.  This is handled
by each producer reserving a sequence span before writing the packets.

The sequence field must be wide enough that the global sequence number cannot
wrap while any earlier reserved sequence number may still be relevant to packet
ordering, continuation, or drop reporting.

Since each producer can have only one active multipacket reservation at a time,
the required bound is:

```text
sum(max_packet_span_per_producer) < 2^sequence_bits
```

With the current 6-bit sequence field:

```text
sum(max_packet_span_per_producer) < 64
```

If all producers use the same maximum packet span, this simplifies to:

```text
producer_thread_count * max_packet_span < 64
```

To prevent blocking, each producer thread has its own pool of packets to draw
from using a circular queue.  The pool should be large enough for the largest
multipacket set plus enough extra packets to absorb bursts if the consumer is
not returning packets fast enough.

The consumer queue must be large enough to hold all packet pointers from all
producer pools, so enqueueing a filled packet pointer cannot fail due to queue
capacity.

DropCountBlock packets, and all other non-event packets, use dedicated packet
storage.  They are not dropped because of event-packet pool exhaustion.  They
may still be suppressed deliberately to reduce output bandwidth.

#### Life-cycle

```text
producer free queue -> producer -> consumer queue -> consumer -> producer free queue
```

#### Producer Thread

1. The producer determines how many bytes are needed and calculates how many
   packets are required.  Since splitting a single value across a packet
   boundary is allowed, this calculates to:

   ```cpp
   packet_span = (bytes_needed + payload_size - 1) / payload_size;
   ```

   `bytes_needed` includes the space needed for the `Timestamp`, `Event count`,
   `String ID` and `Arguments`.

   The calculation is performed in a type wide enough to represent
   `bytes_needed + payload_size - 1`; the resulting packet count must fit in the
   type used for `packet_span`.

   The producer then reserves the next `packet_span` sequence numbers.

2. Set `packets_published` to `0`.

3. If a packet is available from the free queue:
   1. Extract a packet from the free queue.
   2. Set the packet's sequence to the next reserved sequence number in
      `packet_span`.
   3. Write the packet.
   4. Send the packet to be consumed by adding it to the consume queue and
      signalling the consumer.
   5. Increment `packets_published`.
   6. If this was not the last packet in the multipacket data, go to step 3.
      Otherwise, done.

4. Else:
   1. Let:

      ```cpp
      packets_dropped = packet_span - packets_published;
      ```

   2. Add `packets_dropped` to the dropped-packet counter for the packet
      severity.

   3. If `packets_published != 0`, add `packets_published` to the published but
      incomplete counter for the packet severity.

   4. Report the updated drop counts to the consumer.

#### Consumer Thread

1. Wait for a packet or drop-count report to be ready to consume.

2. If a packet is ready:
   1. Send packet.
   2. Reset packet's sequence/continue markers, write pointer/length to an empty
      packet state.
   3. Put packet in the free queue.

3. If the invalid/dropped count has changed:
   1. Report the new drop count.

4. Go to step 1.
