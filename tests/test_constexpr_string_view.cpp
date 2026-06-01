/**
 * @file test_constexpr_string_view.cpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Test that string_view works as expected.
 * @version 0.1
 * @date 2026-05-31
 *
 * @copyright Copyright (c) 2026
 *
 * This is a hold over from when I implemented my own version of this.  I still
 * think that the tests are useful so I'm keeping them for now.
 */
#include "constexpr/string.hpp"
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>

// Intellisense in VsCode didn't like this when using clang to build, so had to
// install clangd extention and add these to the .vscode/settings.json file.
//
//   "clangd.arguments": [
//     "--compile-commands-dir=${workspaceFolder}/build-clang",
//   ],
//   "clangd.path": "C:/msys64/clang64/bin/clangd.exe",

// Compile-time coverage
constexpr bool kConstexprDefaultConstructionWorks{ []() constexpr {
  // Default construction should produce an empty counted C string.
  std::string_view str{""};
  return str.empty()
    && str.size() == 0u
    && str.length() == 0u
    && str.begin() == str.end()
    && str.cbegin() == str.cend()
    && str.data()[0] == '\0';
}() };
static_assert(kConstexprDefaultConstructionWorks);

constexpr bool kConstexprLiteralConstructionWorks{ []() constexpr {
  // Construction from a literal should expose the full logical payload.
  std::string_view str{"alpha"};

  // Accessors and iterators should all agree on the five-character contents.
  bool const access_ok{
    !str.empty()
    && str.size() == 5u
    && str.length() == 5u
    && str.front() == 'a'
    && str.back() == 'a'
    && str[1] == 'l'
    && str.begin()[2] == 'p'
    && *(str.end() - 1) == 'a'
    && *(str.rbegin()) == 'a'
    && *(str.crbegin()) == 'a'
  };

  // View-style conversions should expose the same logical text too.
  bool const view_ok{
    str.max_size() == std::string_view::npos
    && str == std::string_view("alpha")
    && static_cast<std::string_view>(str) == std::string_view("alpha")
  };

  return access_ok && view_ok;
}() };
static_assert(kConstexprLiteralConstructionWorks);

constexpr bool kConstexprCompareWorks{ []() constexpr {
  // Compare equal and ordered strings through both compare() and operators.
  std::string_view alpha{"alpha"};
  std::string_view beta{"beta"};
  std::string_view also_alpha{"alpha"};

  // compare() should agree with lexical ordering.
  bool const compare_ok{
    alpha.compare(beta) < 0
    && beta.compare(alpha) > 0
    && alpha.compare(also_alpha) == 0
  };

  // The relational operators should expose the same ordering contract.
  bool const relation_ok{
    alpha == also_alpha
    && alpha != beta
    && alpha < beta
    && alpha <= also_alpha
    && beta > alpha
    && beta >= alpha
  };

  return compare_ok && relation_ok;
}() };
static_assert(kConstexprCompareWorks);

constexpr bool kConstexprAtWorks{ []() constexpr {
  // at() should read valid logical characters at both ends of the string.
  std::string_view str{"alpha"};
  return str.at(0) == 'a'
    && str.at(4) == 'a';
}() };
static_assert(kConstexprAtWorks);

constexpr bool kConstexprMemberSwapWorks{ []() constexpr {
  // Member swap should exchange both logical contents and sizes.
  std::string_view lhs{"left"};
  std::string_view rhs{"right"};
  lhs.swap(rhs);
  return lhs == std::string_view("right")
    && rhs == std::string_view("left");
}() };
static_assert(kConstexprMemberSwapWorks);

constexpr bool kConstexprFreeSwapWorks{ []() constexpr {
  // Free swap should route through the same observable behavior.
  std::string_view lhs{"one"};
  std::string_view rhs{"two"};
  using Constexpr::swap;
  swap(lhs, rhs);
  return lhs == std::string_view("two")
    && rhs == std::string_view("one");
}() };
static_assert(kConstexprFreeSwapWorks);

// Runtime coverage
TEST(CStrConstexpr, SupportsCompileTimeEvaluation)
{
  // Each named constexpr proof should also hold in the runtime test binary.
  EXPECT_TRUE(kConstexprDefaultConstructionWorks);
  EXPECT_TRUE(kConstexprLiteralConstructionWorks);
  EXPECT_TRUE(kConstexprCompareWorks);
  EXPECT_TRUE(kConstexprAtWorks);
  EXPECT_TRUE(kConstexprMemberSwapWorks);
  EXPECT_TRUE(kConstexprFreeSwapWorks);
}

TEST(CStrRuntime, PreservesStringAndStorageSemantics)
{
  // Start from a normal runtime string literal.
  std::string_view str{"hello"};

  EXPECT_EQ(5u, str.size());
  EXPECT_EQ(5u, str.length());
  EXPECT_EQ(std::string_view("hello"), str);
}

TEST(CStrRuntime, AtRejectsEndIndexAndThrowsPastIt)
{
  // at() should reject the end index just like std::string_view::at().
  std::string_view str{"hello"};
  std::size_t const end_index{ str.size() };

  // The logical end is already out of range.
  EXPECT_THROW(
    static_cast<void>(str.at(end_index)),
    std::out_of_range
  );

  // Any index beyond that should fail the same way.
  EXPECT_THROW(
    static_cast<void>(str.at(end_index + 1u)),
    std::out_of_range
  );
}
