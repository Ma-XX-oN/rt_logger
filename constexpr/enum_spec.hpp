/**
 * @file enum_spec.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Type specifier for enums to allow for safer enum usage and clearer
 *   display.
 * @version 0.1
 * @date 2026-05-28
 *
 * @copyright Copyright (c) 2026
 *
 * TODO: Need to flesh this out more.
 */
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <type_traits>
#include "bitwise_enum.hpp"
#include "constexpr/string.hpp"
#include "constexpr/bitwise_enum.hpp"
#include "enum_registry.hpp"

namespace Constexpr {

constexpr std::size_t MAX_NAME_LEN { 20 };

enum eEnumCommand : std::uint8_t {
  OpCodeMask         = 0b1110'0000,
  PayloadMask        = 0b0001'1111,
  FlagMask           = 0b0001'0000,
  BigCountMask       = 0b0001'1111,
  SmallCountMask     = 0b0000'1111,

  Terminate          = 0 << 5,  // End of stream if stream length not known.

  Named              = 1 << 5,  // Specifies pairs; compares (value & current or stated bitmask) to enum_value.
   HasBitmask        =  1 << 4, //   States if bitmask is specified.
  
  Numeric            = 2 << 5,  // Specifies name for bits for stated bitmask.
   FmtRightShiftBits =  1 << 0, //   Shift bits so least significant bits coinciding with bitmask are at the 0th bit.
   FmtPackBits       =  1 << 1, //   Condense bits coinciding with bitmask.
   FmtIsSigned       =  1 << 2, //   Sign extend bit coinciding with most significant bit of bitmask.

  GroupIf            = 3 << 5,  // If group_bitmask set, use bitmask on following commands.
  GroupIfNamed       = 4 << 5,  // If group_bitmask set, use bitmask on following pairs.
  Else               = 5 << 5,  // Continue the current conditional scope as else group.
   HasGroupName      =  1 << 4, // States if group name is specified.
  ContinueScope      = 6 << 5,  // Continue the current named or command branch.

  GroupIfNumeric     = 7 << 5,  // If group_bitmask set, specify numeric output for stated bitmask.
   Negate            =  1 << 3, //   Inverts the inline numeric condition, so the numeric item belongs to the else case.
   ElseCmd           =  1 << 4, //   If followed by Else, that Else is a command list; otherwise it is a pair list.
   // GroupIfNumeric also can take Fmt* flags.
};
} // namespace Constexpr

template <>
struct BitwiseOps<Constexpr::eEnumCommand> : std::true_type {};



// What we need:
//
// 1. Enum definition container instance that will have the specification of the enum.
// 2. Item 1 implies that we need a class to create the container instance.
// 3. Will need a function to start creating the byte stream, which will return a type instance that
//    will chain and produce the byte stream.  This means that the builder and the definition holder
//    needs to be the same object, unless I have a terminator for the chain.  Separating out the
//    concerns is probably for the best and will prevent users from seeing builder functions when
//    they just want access functions.
// 4. There is a value() member that weill return an EnumInstance container, which is just a class
//    wrapping around the integer.
// 5. To be able to set a value, the EnumInstance object will overload the operator=() to take a
//    some `set` and `force_set` object to allow it to work as I've designed.
// 6. When building with EnumBuilder, it either has to know the max allocation size and reuse the
//    EnumBuilder or it'll have to create a new EnumBuilder at every step, copying the previous
//    data.  Although the latter is a bit abhorrent to me, it is the best way through and the
//    intermediate object will vaporise soon enough anyway.
//

// Types needed:
//
// 1. EnumBuilder - Builds the EnumType
// 2. EnumType - defines the enum, the names it uses, the underlying type, and the dependencies.
// 2. EnumInstance - Wrapper around the underlying type.
// 3. EnumSet - Used to set EnumInstance's underlying value.
// 4. EnumForceSet - Used to set EnumInstance's underlying value, but reduces restrictions.
//
// using E = std::int8_t;

namespace Constexpr {

template <typename E, std::size_t N>
class EnumType;

/**
 * @brief Encode a integer value in binary in a string (little endian)
 * 
 * @tparam T - Type to store.
 * @param dst_begin - Start of stream to add to.
 * @param dst_end - One past the end of the stream.
 * @param value - Value to encode.
 * @return constexpr char* - New dst_begin for next write.
 */
template <typename T>
constexpr char* encode_int(char* dst_begin, char* dst_end, T value) {
  assert(dst_end - dst_begin >= sizeof(T));
  
  auto end{ dst_begin + sizeof(T) };
  for(; dst_begin != end; ++dst_begin) {
    *dst_begin = static_cast<char>(value & 0xff);
    value >>= 8;
  }
  return dst_begin;
}

/**
 * @brief Decode a integer in binary from a string (little endian).
 * 
 * @tparam T - Type to get as.
 * @param dst_begin - Start of stream to get from.  Updated for next read.
 * @param dst_end - One past end of stream.
 * @return T - value decoded.
 */
template <typename T>
constexpr T decode_int(char const*& dst_begin, char const* dst_end) {
  assert(dst_end - dst_begin >= sizeof(T));

  auto begin { std::make_reverse_iterator(dst_begin + sizeof(T) ) };
  auto end { std::make_reverse_iterator(dst_begin) };
  using UT = std::make_unsigned_t<T>;
  UT value{ static_cast<unsigned char>(*begin++) };
  
  while(begin != end) {
    value <<= 8;
    value |= static_cast<unsigned char>(*begin++);
  }
  dst_begin += sizeof(T);
  return static_cast<T>(value);
}

namespace impl {
  enum eMaking : char {
    none,
    pairs,
    commands,
  };

  using offset_t = std::uint16_t;

  /// Size of each stack entry
  constexpr int STACK_ENTRY_SIZE {
    sizeof(eMaking)     // the making operation
    + sizeof(offset_t)  // current program offset
    + sizeof(offset_t)  // number of objects made
  };

  constexpr int OBJS_MADE_I  { sizeof(offset_t) };                // Objects made entry index from end of stack
  constexpr int PRG_OFFSET_I { sizeof(offset_t) + OBJS_MADE_I  }; // Program offset entry index from end of stack
  constexpr int MAKING_I     { sizeof(eMaking)  + PRG_OFFSET_I }; // eMakeing enum entry index from end of stack

  /**
   * @brief Push stack entry onto stack.  That's comprised of types { eMaking, offset_t, offset_t }.
   *
   * @tparam N - Total number of bytes that can be placed on the stack.
   * @param stack - Stack for knowing what is being made and where it was started being made in the
   *   program.
   * @param making - What is begin made (either pairs or commands).
   * @param prg_offset - Where the offset is where this was started.  This is so that it can be
   *   updated when scope has finished.
   * @return auto - New instance of stack where entry was added.
   */
  template <std::size_t N>
  constexpr auto push(string<N> const& stack, eMaking making, offset_t prg_offset)
  {
    string<N+STACK_ENTRY_SIZE> result { stack };
    result.resize(result.size() + STACK_ENTRY_SIZE );

    result.end()[-MAKING_I] = making;

    auto next { encode_int(result.end()-PRG_OFFSET_I, result.end(), prg_offset) };
    encode_int(next, result.end(), offset_t{0}) ;
    return result;
  }

  /**
   * @brief Pop stack entry off stack.
   * 
   * @tparam N - Total number of bytes that can be placed on the stack.
   * @param stack - Stack to pop off an entry from.
   * @return auto - New instance of stack with entry popped off.
   */
  template <std::size_t N>
  constexpr auto pop(string<N> const& stack) {
    string<N-STACK_ENTRY_SIZE> result{
      std::string_view{ stack.begin(), stack.end()-STACK_ENTRY_SIZE } };
    return result;
  }

  /**
   * @brief Get the last making object.
   * 
   * @tparam N - Total number of bytes that can be placed on the stack.
   * @param stack - Stack to get the data from.
   * @return impl::eMaking - What was being made last. 
   */
  template <std::size_t N>
  constexpr eMaking get_last_making(string<N> const& stack) {
    if constexpr (N) {
      assert(N > STACK_ENTRY_SIZE);
      return static_cast<eMaking>(stack.end()[-STACK_ENTRY_SIZE]);
    } else {
      assert(!"Nothing left on stack to get.");
    }
  }

  /**
   * @brief Get the last prg offset object
   *
   * @tparam N - Total number of bytes that can be placed on the stack.
   * @param stack - Stack to get the data from.
   * @return impl::offset_t - The byte offset for the command that needs the eMaking object.
   */
  template <std::size_t N>
  constexpr offset_t get_last_prg_offset(string<N> const& stack) {
    if constexpr (N) {
      assert(N > STACK_ENTRY_SIZE);
      auto begin { stack.end()-PRG_OFFSET_I };
      return decode_int<offset_t>(begin, stack.end());
    } else {
      assert(!"Nothing left on stack to get.");
    }
  }

  /**
   * @brief Get the last objs made object
   * 
   * @tparam N - Total number of bytes that can be placed on the stack.
   * @param stack - Stack to get the data from.
   * @return impl::offset_t - The count of the number of objects made.
   */
  template <std::size_t N>
  constexpr offset_t get_last_objs_made(string<N> const& stack) {
    if constexpr (N) {
      assert(N > STACK_ENTRY_SIZE);
      auto begin { stack.end()-OBJS_MADE_I };
      return decode_int<offset_t>(begin, stack.end());
    } else {
      assert(!"Nothing left on stack to get.");
    }
  }

  /**
   * @brief Increment the number of objects made.
   * 
   * @tparam N - Total number of bytes that can be placed on the stack.
   * @param stack - Stack to get the data from.
   */
  template <std::size_t N>
  constexpr offset_t inc_last_objs_made(string<N> const& stack) {
    if constexpr (N) {
      assert(N > STACK_ENTRY_SIZE);
      auto begin { stack.end()-OBJS_MADE_I };
      auto count { decode_int<offset_t>(begin, stack.end()) };
      encode_int(stack.end()-OBJS_MADE_I, stack.end(), ++count);
      return count;
    } else {
      assert(!"Nothing left on stack to get.");
    }
  }

} // namespace impl

/**
 * @brief Builds an EnumType
 * 
 * @tparam E - enum type or int type.  This is the type that build and accessor's will expect.
 * @tparam N - Size of the enum descriptor stream so far.
 */
template <typename E, std::size_t N = 0, std::size_t S = 0>
class EnumBuilder {
  /// The byte program being compiled
  string<N> m_program;
  
  /// Making stack - what is currently being made
  string<N> m_stack;

  /**
   * @brief Construct a new Enum Builder object
   * 
   * @tparam N_ - Number of bytes used by program.
   * @tparam S_ - Number of bytes used by stack.
   * @param program - Program compiled so far.
   * @param stack - Current making stack.
   */
  template<std::size_t N_, std::size_t S_>
  EnumBuilder(E, string<N_> const& program, string<S_> const& stack)
  : m_program(program)
  , m_stack(stack)
  {
  }

  public:
  using string_view = std::string_view;
  using last_scope_offset_t = std::uint16_t;

  /**
   * @brief Starts a Names section in the program.
   * 
   * @return A new EnumBuilder instance with an updated program and stack.
   */
  constexpr auto NameStart() {
    auto stack { impl::push(m_stack, impl::eMaking::pairs, m_program.size()) };
    string<N+sizeof(eEnumCommand)> program { m_program };
    program.push_back(eEnumCommand::Named);
    return EnumBuilder{ E{}, program, stack };
  }

  /**
   * @brief Starts a Names section in the program which includes a bitmask.
   * 
   * @param bitmask - bitmask to constrain bits to.
   * @return A new EnumBuilder instance with an updated program and stack.
   */
  constexpr auto NameStart(E bitmask) {
    auto stack { impl::push(m_stack, impl::eMaking::pairs, m_program.size()) };
    string<N+sizeof(eEnumCommand)+sizeof(E)> program { m_program };
    auto begin { program.end() };
    program.resize(program.size()+sizeof(eEnumCommand)+sizeof(E));
    *begin++ = eEnumCommand::Named | eEnumCommand::HasBitmask;
    encode_int(begin, program.end(), bitmask);
    return EnumBuilder{ E{}, program, stack };
  }

  /**
   * @brief Create a pair for the program.
   * 
   * @param value - Value to relate to name.
   * @param name - Name to be related by the value.
   * @return A new EnumBuilder instance with an updated program and stack.
   */
  constexpr auto Pair(E value, std::string_view name) {
    assert(impl::get_last_making(m_stack) == impl::eMaking::pairs);
    impl::inc_last_objs_made(m_stack);

    auto length { name.size() };
    assert(length < MAX_NAME_LEN);
    
    string<N+sizeof(E)+MAX_NAME_LEN> program { m_program };
    auto begin { program.begin() };
    program.resize(program.size() + sizeof(E) + name.size() + 1); // +1 for NUL

    begin = encode_int(begin, program.end(), value);
    begin = copy(name.begin(), name.end(), begin);
    assert(begin != program.end());
    *begin++ = 0;

    program.resize(begin - program.begin());
    return EnumBuilder{ E{}, program, m_stack };
  }

  /**
   * @brief Ends any scoping block for any command.
   * 
   * @return A new EnumBuilder instance with an updated program and stack.
   */
  constexpr auto End() {
    if (impl::get_last_making(m_stack) == impl::eMaking::pairs) {

    }
  }
};

template<typename E, std::size_t N, std::size_t S>
EnumBuilder(E, string<N>, string<S>) -> EnumBuilder<E, N, S>;

/**
 * @brief Type to build values with.
 * 
 * @tparam E 
 * @tparam N 
 */
template <typename E, std::size_t N>
class EnumType {
  Constexpr::string<N> m_definition;

  /**
   * @brief Accepts the definition.
   * 
   */
  constexpr EnumType(std::string_view& definition)
  : m_definition(definition)
  {
  }

  
};

} // namespace Constexpr
