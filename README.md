# A C++ RT Logger <!-- omit in toc -->

- [Purpose](#purpose)
  - [Small](#small)
  - [Fast](#fast)
  - [Resilient](#resilient)
- [Dependencies to build](#dependencies-to-build)
- [Design](#design)
  - [Packet Design](#packet-design)
    - [Dynamic Numbers (dnum)](#dynamic-numbers-dnum)
    - [C-String Definition](#c-string-definition)
    - [General Block Format](#general-block-format)
    - [Payload specifications](#payload-specifications)
      - [1. StringBlock Payload](#1-stringblock-payload)
      - [2. EnumBlock Payload](#2-enumblock-payload)
      - [3. DataBlock Payload](#3-datablock-payload)
      - [4. TimeAnchorBlock Payload](#4-timeanchorblock-payload)
      - [5. DropCountBlock Payload](#5-dropcountblock-payload)

## Purpose

When compiled, this is to be a very small, fast and resilient logger.

### Small

Data stored as minimally as possible with little overhead.

I'm hoping to have minimal code binary overhead from the logger.

### Fast

The logger writes the string formatting sparingly, by stating the string and an id that represents it once before use and occasionally in case of data corruption.

All parameters are to be stored in the logger machine's binary format.  (May have a test pattern block to help automate reconstruction).  The reduces conversion to sting overhead.

### Resilient

The data is in a binary format, so there are fences and CRCs to help with recovery.  If data crosses blocks, the CRC's seed starts where the previous block leaves off.

## Dependencies to build

```bash
# Install cmake, ninga and gtest dependencies
pacman -S \
  mingw-w64-clang-x86_64-cmake \
  mingw-w64-clang-x86_64-ninja \
  mingw-w64-clang-x86_64-gtest
```

## Design

### Packet Design

The data is stored in packets with a maximum payload of 256 (I don't see any use
for an empty packet).  I may make this configurable, but will start with 256
byte payload.

> **UPDATE:**
>
> Given my current packet definitions, the minimum payload is 5.  The makes for
> 5-260 bytes per packet.  4 extra bytes doesn't sound like a lot, but it might
> be useful.

#### Dynamic Numbers (dnum)

A dynamic number is a sequence of consecutive 8 bit bytes where the most
significant is the continuation flag.  If set, then it is left shifted 7 bit and
the next 7 bits are ORed onto the shifted one.  This keeps happening until the
continuation flag is clear.

This makes 1 byte hold 128 values, 2 bytes can hold 16k values and so on.  This
is a very fast and effective compression scheme if most of the values expected
are to be lov values.

#### C-String Definition

The characters in non-format c-strings are just NUL terminated character
streams.

Format strings are also NUL terminated but have control codes embedded in them
to represent the stored type and how it should be output.  Type specification is
key as the data isn't translated to text at the time of writing, but at the time
of reading.  All values are stored in the machine's representation, meaning that
the endianness of the values are kept as is and has to be fixed to the reader's
end.  This is done to reduce the writer's latency as much as possible.

Any value in a string that is between [1, 31] or [0x01, 0x1F] inclusive are
considered control codes. Subtract 1 from that value results in an inclusive
range of [0, 30] or [0x00, 0x1E].

Backslash, tab and LF are encoded in the string as R"(\\\\)", R"(\n)" and
R"(\t)" respectively.  I.e.  Those characters are stored as escaped
characters and are represented by 2 characters.

These specifications prevents embedding a NUL into a string and make it
easier to use flags on the integer set for specific operations.

There are 3 different control sequences.

1. TYPE_SPEC

   This specifies the binary representation of the type being printed.  This is
   because the values are not translated to text, but are stored as the machine
   loggers binary format to be converted later to reduce logging latency.

   1. Numbers and strings

      Numbers and string types are specified as one byte.

   2. Enum

      Enums are followed by a dnum (dynamic number) to specify which registered
      enum to use.  A dnum is a 7 bit number with a continuation bit to allow a
      number to be as small as 1 byte for small numbers but can be theoretically
      represent any sized number.

   3. Array

      All of those described previously can be prefixed with an Array MODIFIER.
      That consists of Array, followed by a dnum and can be stacked 3 levels
      deep. That limitation only exists is only because I'm not sure how to
      represent 4D arrays on output.

2. FORMATTING INFO

   The formatting info consists of 1-6 bytes (2-12 if using arrays), depending
   on what is requested. This excludes the starting eType byte.  A sequence
   always ends with a eFmtLetter byte.  Sizes stated are minimal values.  If
   embedded dnums specified are greater than 127, then it could mean a larger
   size.  Embedded dnums of 0 are considered an error.  If done as text, it
   would easily be twice the size.

                11111             111111111122222
       12345678901234    123456789012345678901234 // Arrays are deduced, not
       {:0^+z#10.10b}    {:0^+z#10.10!a10a10a10d} // specified as shown here.

   When arrays are displayed, they are space delimited when minimum width
   formatting is specified, otherwise is it comma delimited.  This is actually
   dependent on the renderer side, so it could be configurable there.

   ```text
   | byte 1    | byte 2     | byte 3   | byte 4     | byte 5     | byte 6     | byte 7     |
   |-----------|------------|----------|------------|------------|------------|------------|
   | eType     | eFmtLetter |          |            |            |            |            |
   | eType     | eFmt0      | eFmt1    | eFmtLetter |            |            |            |
   | eType     | eFmt0      | eFmt1    | MIN_DNUM   | eFmtLetter |            |            |
   | eType     | eFmt0      | eFmt1    | PREC_DNUM  | eFmtLetter |            |            |
   | eType     | eFmt0      | eFmt1    | MIN_DNUM   | PREC_DNUM  | eFmtLetter |            |
   | eType     | eFmt0      | eFmt1    | FILL_CHAR  | eFmtLetter |            |            |
   | eType     | eFmt0      | eFmt1    | MIN_DNUM   | FILL_CHAR  | eFmtLetter |            |
   | eType     | eFmt0      | eFmt1    | PREC_DNUM  | FILL_CHAR  | eFmtLetter |            |
   | eType     | eFmt0      | eFmt1    | MIN_DNUM   | PREC_DNUM  | FILL_CHAR  | eFmtLetter |
   | Enum      | enum id    | ...      |            |            |            |            |
   | Array     | DNUM_SIZE  | eType ...|            |            |            |            |
   | Array     | DNUM_SIZE  | Array    | DNUM_SIZE  | eType ...  |            |            |
   | Array     | DNUM_SIZE  | Array    | DNUM_SIZE  | Array      | DNUM_SIZE  | eType ...  |
   ```

3. End of an fstring

   All fstrings are NUL terminated.

   1. SALT - Visible String Terminator

      All fstrings have a CRC16 with them to distinguish them from each other.
      If an fstring has the same CRC16 as another, it is considered an error and
      can be forced to be made different by adding a salt to the end of the
      string, which is the Salt value followed by nothing or a dnum THAT IS NOT
      ZERO.

      This adds one or more bytes to the string. A DNUM_SALT value of 0 is an
      error, but it's equivalent is to have no DNUM_SALT at all.  A dnum is used
      to ensure that the salt is always part of the fstring and doesn't
      prematurely terminate it with an inadvertent NUL. The salt shouldn't be
      printed on the displaying end.

      ```text
      | byte 1    | byte 2        | byte 3   |
      |-----------|---------------|----------|
      | NUL       |               |          |
      | Salt      | NUL           |          |
      | Salt      | DNUM_SALT     | NUL      |
      ```

#### General Block Format

| field | bytes | description                                                                    |
|------:|------:|--------------------------------------------------------------------------------|
| 1     | 1     | Block type: StringBlock, EnumBlock, DataBlock, TimeAnchorBlock, DropCountBlock |
| 2     | 1     | Sequence number (6 bit), and two flags (continuation and continues).           |
| 4     | 1     | payload length-5:  minimum payload is 5, so 5-260 bytes are possible.          |
| 5     | 5-260 | payload                                                                        |
| 6     | 2     | CRC16: Should be good enough for 256 byte + 4 byte header blocks.              |
| 7     | 4     | Fence: "####"                                                                  |

Fields 1-4 are the header, 5 is the payload, 6 is the CRC and 7 ends the block.

The fence is at the end because a block isn't finished unless a fence is found.
Before the first block is emitted, there will have an explicit fence to start
the blocks off.

The 6-bit sequence number detects short discontinuities. It is not intended to
count long loss intervals.

| field | bit   | description                   |
|------:|------:|-------------------------------|
| 1     | 0-5   | Sequence number (64 values).  |
| 2     | 6     | Block continues.              |
| 3     | 7     | Block is continuing.          |

The CRC is based on fields 1-5.

A continued block's CRC is seeded by the previous block's CRC.

#### Payload specifications

Payload tables with a "repeat" column will have a count field prior to repeated
fields.  So any fields with a ✅ after a count field will be repeated that many
times.  If necessary, for nested repeat fields, there will be one ✅ for the
first level, two for the next, etc.

##### 1. StringBlock Payload

Contains 1 or more strings, with IDs and CRC16 for each.

| repeats | field | bytes    | description                                 |
|:-------:|------:|---------:|---------------------------------------------|
|         | 1     | 1        | Count of strings in block.                  |
|   ✅    | 2     | variable | String ID (dynamic number).                 |
|   ✅    | 3     | 1        | Severity (Info, Warn, Error, Debug)         |
|   ✅    | 4     | variable | NUL terminated format string (c-string)     |
|   ✅    | 5     | 2        | The CRC for the severity and string (CRC16) |

Format string contains embedded info that represents the type and how it's to be
displayed.  Any character with a value less than 32 indicates the start of a
field.

See eType for more information.

##### 2. EnumBlock Payload

```c++
enum eEnumStorageType : std::uint8_t {
  // Integer types
   Int8 = 0x00,  Int16 = 0x01,  Int32 = 0x02,  Int64 = 0x03,
  UInt8 = 0x04, UInt16 = 0x05, UInt32 = 0x06, UInt64 = 0x07,
};
```

| repeats | field | bytes    | description                             |
|:-------:|------:|---------:|-----------------------------------------|
|         | 1     | variable | Enum ID (dynamic number)                |
|         | 2     | 1        | Storage type (eEnumStorageType)         |
|         | 3     | 1        | Count of number masks                   |
|   ✅    | 4     | variable | Mask (could be sizeof storage or dnum)  |
|         | 5     | variable | Count of named masks                    |
|   ✅    | 6     | variable | Mask (could be sizeof storage or dnum)  |
|         | 7     | variable | Count of enums values encoded (dnum)    |
|   ✅    | 8     | variable | Value (could be sizeof storage or dnum) |
|   ✅    | 9     | variable | String for that value (c-string)        |

Number mask means that area is to be thought of as a number.  Possible
formatting strategies:

- Display as stated.
- Right shift value of bits to least significant bit and report that value.
- If not contiguous make them contiguous.

May specify formatting options for this and put it in the eEnumStorageType.

Named masks means that if a mask is applied and it gives a non-zero value, find
value based on that value and output the string for that value.

Mask and value are stored as unsigned dnums unless they are a byte in size.

##### 3. DataBlock Payload

This is the actual logged data.

| field | bytes    | description                                |
|------:|---------:|--------------------------------------------|
| 1     | 2        | Timestamp (offset from TimeAnchorBlock)    |
| 2     | 2        | Event count (rolls over every 64k events)  |
| 3     | variable | String ID (dnum)                           |
| 4     | variable | Arguments (binary payload based on string) |

There is only one event per block.

Each event's event count is taken from a global counter that gets incremented
for every event sent out.  The CRC for this block is seeded from the CRC of the
string pointed at by the string ID. This helps verify that the correct string is
used with the payload.

##### 4. TimeAnchorBlock Payload

| field | bytes    | description                                |
|------:|---------:|--------------------------------------------|
| 1     | 8        | Timestamp (machine timestamp)              |

This is outputted periodically to keep the DataBlock's timestamp relevant.
Maybe every 10 minutes?

##### 5. DropCountBlock Payload

If the log buffers can't be emptied fast enough, some events will have to be
dropped.  First, repeated StringBlock and EnumBlock packets will be postponed.
Then Info packets will be the first to be dropped, then Debug, Warnings and
finally Error.

This packet will be periodically be output if there are any dropped packets.

| field | bytes    | description                                |
|------:|---------:|--------------------------------------------|
| 1     | 4        | Number of dropped debug events.            |
| 2     | 4        | Number of dropped info events.             |
| 3     | 4        | Number of dropped warning events.          |
| 4     | 4        | Number of dropped error events.            |
