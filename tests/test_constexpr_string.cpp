#include "constexpr/string.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string_view>
#include <utility>

using Constexpr::string;

constexpr bool kConstexprDefaultConstructionWorks{ []() constexpr {
  // Start from the empty-string state.
  string<8> str{};

  return str.empty()
    && str.size() == 0u
    && str.length() == 0u
    && str.capacity() == 7u
    && str.max_size() == 7u
    && str.begin() == str.end()
    && str.cbegin() == str.cend()
    && str.view().empty();
}() };
static_assert(kConstexprDefaultConstructionWorks);

constexpr bool kConstexprLiteralConstructionWorks{ []() constexpr {
  // Array construction preserves the full literal payload.
  string<8> str{"alpha"};

  return !str.empty()
    && str.size() == 5u
    && str.length() == 5u
    && str.capacity() == 7u
    && str.front() == 'a'
    && str.back() == 'a'
    && str[1] == 'l'
    && str.begin()[2] == 'p'
    && *(str.end() - 1) == 'a'
    && *(str.rbegin()) == 'a'
    && *(str.crbegin()) == 'a'
    && str.view() == std::string_view("alpha")
    && static_cast<std::string_view>(str) == std::string_view("alpha");
}() };
static_assert(kConstexprLiteralConstructionWorks);

constexpr bool kConstexprEmbeddedNulConstructionWorks{ []() constexpr {
  // Array construction keeps embedded NUL bytes as logical data.
  string<4> str{"a\0b"};

  return !str.empty()
    && str.size() == 3u
    && str.length() == 3u
    && str[0] == 'a'
    && str[1] == '\0'
    && str[2] == 'b'
    && str.view() == std::string_view("a\0b", 3);
}() };
static_assert(kConstexprEmbeddedNulConstructionWorks);

constexpr bool kConstexprCtadWorks{ []() constexpr {
  // Deduction should still produce the exact-capacity string type.
  string str{"hi"};

  return str.size() == 2u
    && str.length() == 2u
    && str.capacity() == 2u
    && str.max_size() == 2u;
}() };
static_assert(kConstexprCtadWorks);

constexpr bool kConstexprCompareWorks{ []() constexpr {
  // Prepare lexicographic comparison inputs.
  string<8> alpha{"alpha"};
  string<4> bet{"bet"};
  string<6> also_alpha{"alpha"};

  return alpha.compare(bet) < 0
    && bet.compare(alpha) > 0
    && alpha.compare(also_alpha) == 0
    && alpha == also_alpha
    && alpha != bet
    && alpha < bet
    && alpha <= also_alpha
    && bet > alpha
    && bet >= alpha;
}() };
static_assert(kConstexprCompareWorks);

constexpr bool kConstexprAtWorks{ []() constexpr {
  // Mutate through bounds-checked access.
  string<8> str{"alpha"};
  str.at(1) = 'o';

  // Re-read the result through a const reference.
  string<8> const& const_str{ str };

  return str.view() == std::string_view("aopha")
    && const_str.at(0) == 'a'
    && const_str.at(4) == 'a';
}() };
static_assert(kConstexprAtWorks);

constexpr bool kConstexprMutationWorks{ []() constexpr {
  // Interior NUL writes do not change the tracked size.
  string<8> str{"abc"};
  str[1] = '\0';

  return str.view() == std::string_view("a\0c", 3)
    && str.size() == 3u
    && str.back() == 'c';
}() };
static_assert(kConstexprMutationWorks);

constexpr bool kConstexprResizeWorks{ []() constexpr {
  // Grow with a fill character.
  string<8> str{"ab"};
  str.resize(4, 'x');

  bool const grew_ok{
    str.size() == 4u
    && str.view() == std::string_view("abxx")
  };

  // Shrink back down without rescanning for NUL.
  str.resize(1);

  return grew_ok
    && str.size() == 1u
    && str.view() == std::string_view("a");
}() };
static_assert(kConstexprResizeWorks);

constexpr bool kConstexprAppendWorks{ []() constexpr {
  // Start from a short logical string.
  string<8> str{"a"};

  // Appending counted data preserves embedded NUL bytes.
  constexpr char suffix[]{ 'x', '\0', 'y' };
  str.append(std::string_view("b\0c", 3));
  str.append(suffix, 3u);

  return str.size() == 7u
    && str.view() == std::string_view("ab\0cx\0y", 7);
}() };
static_assert(kConstexprAppendWorks);

constexpr bool kConstexprAssignWorks{ []() constexpr {
  // Replace with a counted string_view payload first.
  string<8> str{"alpha"};
  str.assign(std::string_view("m\0n", 3));

  bool const view_assign_ok{
    str.size() == 3u
    && str.view() == std::string_view("m\0n", 3)
  };

  // Replace with a counted raw buffer next.
  constexpr char raw[]{ 'q', '\0', 'r' };
  str.assign(raw, 3u);

  bool const raw_assign_ok{
    str.size() == 3u
    && str.view() == std::string_view("q\0r", 3)
  };

  // Finally replace from another counted string object.
  string<4> rhs{"u\0v"};
  str.assign(rhs);

  return view_assign_ok
    && raw_assign_ok
    && str.size() == 3u
    && str.view() == std::string_view("u\0v", 3);
}() };
static_assert(kConstexprAssignWorks);

constexpr bool kConstexprCopyWorks{ []() constexpr {
  // Copy construct from embedded-NUL content.
  string<8> original{"a\0b"};
  string<8> copied{ original };

  // Copy assign the same logical content into a different destination.
  string<8> assigned{"zzz"};
  assigned = original;

  return copied.size() == 3u
    && copied.view() == std::string_view("a\0b", 3)
    && assigned.size() == 3u
    && assigned.view() == std::string_view("a\0b", 3);
}() };
static_assert(kConstexprCopyWorks);

constexpr bool kConstexprConvertingCopyWorks{ []() constexpr {
  // Copy into a smaller type that still has enough logical capacity.
  string<8> large{"alpha"};
  string<6> small{ large };

  // Copy back into a larger destination through converting assignment.
  string<8> assigned{};
  assigned = small;

  return small.size() == 5u
    && small.view() == std::string_view("alpha")
    && assigned.size() == 5u
    && assigned.view() == std::string_view("alpha");
}() };
static_assert(kConstexprConvertingCopyWorks);

constexpr bool kConstexprMoveWorks{ []() constexpr {
  // Move construction should transfer the logical content.
  string<8> source{"move"};
  string<8> moved{ std::move(source) };

  // Move assignment should do the same and leave the source empty.
  string<8> assigned{"xx"};
  assigned = std::move(moved);

  return source.empty()
    && moved.empty()
    && assigned.size() == 4u
    && assigned.view() == std::string_view("move");
}() };
static_assert(kConstexprMoveWorks);

constexpr bool kConstexprStringViewWorks{ []() constexpr {
  // Construct directly from a counted string_view.
  string<8> constructed{ std::string_view("a\0b", 3) };

  // Then assign another counted string_view payload.
  string<8> assigned{"zzz"};
  assigned = std::string_view("q\0r", 3);

  return constructed.size() == 3u
    && constructed.view() == std::string_view("a\0b", 3)
    && assigned.size() == 3u
    && assigned.view() == std::string_view("q\0r", 3);
}() };
static_assert(kConstexprStringViewWorks);

constexpr bool kConstexprCStringSemanticsWork{ []() constexpr {
  // Show pointer construction will stop at first NUL character.
  constexpr char raw[]{ 'a', '\0', 'b', '\0' };
  char const* ptr{ raw };
  string<8> constructed{ ptr };

  // Same as above and then appends after that.
  string<8> appended{"x"};
  appended.append(ptr);
  appended.append("bc");

  // Shows assignment stops at first NUL character.
  string<8> assigned{"zzz"};
  assigned = ptr;

  return constructed.size() == 1u
    && constructed.view() == std::string_view("a")
    && appended.size() == 4u
    && appended.view() == std::string_view("xabc")
    && assigned.size() == 1u
    && assigned.view() == std::string_view("a");
}() };
static_assert(kConstexprCStringSemanticsWork);

constexpr bool kConstexprMutableDataWorks{ []() constexpr {
  // Non-const data() provides a raw mutable view into logical bytes.
  string<8> str{"abc"};
  char* ptr{ str.data() };
  ptr[1] = 'x';

  return str.size() == 3u
    && str.view() == std::string_view("axc");
}() };
static_assert(kConstexprMutableDataWorks);

constexpr bool kConstexprCountAssignAndAppendWork{ []() constexpr {
  // Assign repeated characters first.
  string<8> str{};
  str.assign(3u, 'q');

  // Then extend with another repeated sequence.
  str.append(2u, 'r');

  return str.size() == 5u
    && str.view() == std::string_view("qqqrr");
}() };
static_assert(kConstexprCountAssignAndAppendWork);

constexpr bool kConstexprPlusEqualsWorks{ []() constexpr {
  // Start with one logical character.
  string<9> str{"a"};
  str += 'b';
  str += std::string_view("c\0d", 3);
  str += "e";

  // Finish by appending another counted string object.
  string<3> tail{"f"};
  str += tail;

  return str.size() == 7u
    && str.view() == std::string_view("abc\0def", 7);
}() };
static_assert(kConstexprPlusEqualsWorks);

constexpr bool kConstexprClearWorks{ []() constexpr {
  // Clear should reset the logical state back to empty.
  string<8> str{"clear"};
  str.clear();

  return str.empty()
    && str.size() == 0u
    && str.view().empty();
}() };
static_assert(kConstexprClearWorks);

constexpr bool kConstexprMemberSwapWorks{ []() constexpr {
  // Swap two strings with different logical sizes.
  string<8> lhs{"left"};
  string<8> rhs{"a"};
  lhs.swap(rhs);

  return lhs.size() == 1u
    && lhs.view() == std::string_view("a")
    && rhs.size() == 4u
    && rhs.view() == std::string_view("left");
}() };
static_assert(kConstexprMemberSwapWorks);

constexpr bool kConstexprFreeSwapWorks{ []() constexpr {
  // Exercise the free swap overload too.
  string<8> lhs{"one"};
  string<8> rhs{"xy"};
  swap(lhs, rhs);

  return lhs.size() == 2u
    && lhs.view() == std::string_view("xy")
    && rhs.size() == 3u
    && rhs.view() == std::string_view("one");
}() };
static_assert(kConstexprFreeSwapWorks);

constexpr bool kConstexprSubstringAndSearchWork{ []() constexpr {
  // Start from a string with repeated structure for search coverage.
  string<8> str{"abcabc"};

  // Substring extraction should preserve counted semantics.
  string<8> sub{ str.substr(2u, 3u) };

  // copy() writes only the requested logical slice.
  char buffer[3]{};
  std::size_t const copied{ str.copy(buffer, 3u, 1u) };

  bool const copy_ok{
    copied == 3u
    && buffer[0] == 'b'
    && buffer[1] == 'c'
    && buffer[2] == 'a'
  };

  return sub.view() == std::string_view("cab")
    && str.compare(std::string_view("abcabc")) == 0
    && str.find(std::string_view("ca")) == 2u
    && str.find('b', 2u) == 4u
    && str.rfind(std::string_view("ab")) == 3u
    && str.rfind('a') == 3u
    && str.find_first_of(std::string_view("zc")) == 2u
    && str.find_first_of('c') == 2u
    && str.find_last_of(std::string_view("ab")) == 4u
    && str.find_last_of('a') == 3u
    && str.find_first_not_of(std::string_view("ab")) == 2u
    && str.find_first_not_of('a') == 1u
    && str.find_last_not_of(std::string_view("bc")) == 3u
    && str.find_last_not_of('c') == 4u
    && str.starts_with(std::string_view("abc"))
    && str.starts_with('a')
    && str.ends_with(std::string_view("abc"))
    && str.ends_with('c')
    && copy_ok;
}() };
static_assert(kConstexprSubstringAndSearchWork);

TEST(StringConstexpr, SupportsCompileTimeEvaluation)
{
  EXPECT_TRUE(kConstexprDefaultConstructionWorks);
  EXPECT_TRUE(kConstexprLiteralConstructionWorks);
  EXPECT_TRUE(kConstexprEmbeddedNulConstructionWorks);
  EXPECT_TRUE(kConstexprCtadWorks);
  EXPECT_TRUE(kConstexprCompareWorks);
  EXPECT_TRUE(kConstexprAtWorks);
  EXPECT_TRUE(kConstexprMutationWorks);
  EXPECT_TRUE(kConstexprResizeWorks);
  EXPECT_TRUE(kConstexprAppendWorks);
  EXPECT_TRUE(kConstexprAssignWorks);
  EXPECT_TRUE(kConstexprCopyWorks);
  EXPECT_TRUE(kConstexprConvertingCopyWorks);
  EXPECT_TRUE(kConstexprMoveWorks);
  EXPECT_TRUE(kConstexprStringViewWorks);
  EXPECT_TRUE(kConstexprCStringSemanticsWork);
  EXPECT_TRUE(kConstexprMutableDataWorks);
  EXPECT_TRUE(kConstexprCountAssignAndAppendWork);
  EXPECT_TRUE(kConstexprPlusEqualsWorks);
  EXPECT_TRUE(kConstexprClearWorks);
  EXPECT_TRUE(kConstexprMemberSwapWorks);
  EXPECT_TRUE(kConstexprFreeSwapWorks);
  EXPECT_TRUE(kConstexprSubstringAndSearchWork);
}

TEST(StringRuntime, PreservesStringSemantics)
{
  // Start from a normal runtime string.
  string<8> str{"hello"};

  EXPECT_EQ(5u, str.size());
  EXPECT_EQ(5u, str.length());
  EXPECT_EQ(7u, str.capacity());
  EXPECT_EQ(7u, str.max_size());
  EXPECT_EQ(std::string_view("hello"), str.view());
  EXPECT_STREQ("hello", str.c_str());
}

TEST(StringRuntime, AtRejectsEndIndexAndThrowsPastIt)
{
  // at() follows std::string and rejects the end index.
  string<8> str{"hello"};
  std::size_t end_index{ str.size() };

  EXPECT_THROW(
    static_cast<void>(str.at(end_index)),
    std::out_of_range
  );
  EXPECT_THROW(
    static_cast<void>(str.at(end_index + 1u)),
    std::out_of_range
  );
}

#ifndef NDEBUG
TEST(StringRuntime, SubscriptRejectsEndIndexAndPastItInDebug)
{
  // operator[] guards the logical boundary with debug assertions.
  EXPECT_DEATH(
    []() {
      string<8> str{"hello"};
      std::size_t end_index{ str.size() };

      str[end_index] = 'x';
    }(),
    ""
  );

  EXPECT_DEATH(
    []() {
      string<8> str{"hello"};
      std::size_t end_index{ str.size() };

      str[end_index + 1u] = 'x';
    }(),
    ""
  );
}
#endif

TEST(StringRuntime, OverflowingMutatorsThrowLengthError)
{
  // Each mutator should reject growth past fixed capacity.
  string<4> resize_str{"abc"};
  string<4> push_str{"abc"};
  string<4> append_str{"ab"};
  string<4> assign_str{};

  EXPECT_THROW(
    resize_str.resize(4u),
    std::length_error
  );
  EXPECT_THROW(
    push_str.push_back('d'),
    std::length_error
  );
  EXPECT_THROW(
    append_str.append(std::string_view("cd", 2)),
    std::length_error
  );
  EXPECT_THROW(
    assign_str.assign(std::string_view("abcd", 4)),
    std::length_error
  );
}

TEST(StringRuntime, ConvertingConstructionAndAssignmentThrowWhenTooLarge)
{
  // Converting operations still enforce fixed destination capacity.
  string<8> source{"hello"};
  string<4> destination{};

  EXPECT_THROW(
    static_cast<void>(string<4>{ source }),
    std::length_error
  );
  EXPECT_THROW(
    destination = source,
    std::length_error
  );
}

TEST(StringRuntime, MutableDataWritesLogicalBytes)
{
  // Mutate an existing logical byte through the raw buffer API.
  string<8> str{"hello"};
  char* ptr{ str.data() };

  ptr[1] = 'a';

  EXPECT_EQ(std::string_view("hallo"), str.view());
}

TEST(StringRuntime, CStringOverloadsUseNullTerminatedSemantics)
{
  // Pointer-based overloads stop at the first NUL terminator.
  char const raw[]{ 'a', '\0', 'b', '\0' };
  char const* ptr{ raw };
  string<8> constructed{ ptr };

  // Appending from a C string uses the same rule.
  string<8> appended{"x"};
  appended.append(ptr);

  // Assignment from a C string does too.
  string<8> assigned{"zzz"};
  assigned = ptr;

  EXPECT_EQ(std::string_view("a"), constructed.view());
  EXPECT_EQ(std::string_view("xa"), appended.view());
  EXPECT_EQ(std::string_view("a"), assigned.view());
}
