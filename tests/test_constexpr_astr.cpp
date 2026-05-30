#include "constexpr/AStr.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string_view>

using Constexpr::AStr;

constexpr bool kConstexprDefaultConstructionWorks{ []() constexpr {
  AStr<8> str{};
  return str.empty()
    && str.size() == 0u
    && str.length() == 0u
    && str.storage_size() == 1u
    && str.capacity() == 7u
    && str.max_size() == 7u
    && str.storage_capacity() == 8u
    && str.begin() == str.end()
    && str.cbegin() == str.cend()
    && str.data()[0] == '\0'
    && str.c_str()[0] == '\0'
    && str.view().empty();
}() };
static_assert(kConstexprDefaultConstructionWorks);

constexpr bool kConstexprLiteralConstructionWorks{ []() constexpr {
  AStr<8> str{"alpha"};
  return !str.empty()
    && str.size() == 5u
    && str.length() == 5u
    && str.storage_size() == 6u
    && str.capacity() == 7u
    && str.storage_capacity() == 8u
    && str.front() == 'a'
    && str.back() == 'a'
    && str[1] == 'l'
    && str[5] == '\0'
    && str.begin()[2] == 'p'
    && *(str.end() - 1) == 'a'
    && *(str.rbegin()) == 'a'
    && *(str.crbegin()) == 'a'
    && str.view() == std::string_view("alpha")
    && static_cast<std::string_view>(str) == std::string_view("alpha")
    && str.c_str()[5] == '\0';
}() };
static_assert(kConstexprLiteralConstructionWorks);

constexpr bool kConstexprCtadWorks{ []() constexpr {
  AStr str{"hi"};
  return str.size() == 2u
    && str.length() == 2u
    && str.storage_size() == 3u
    && str.capacity() == 2u
    && str.storage_capacity() == 3u;
}() };
static_assert(kConstexprCtadWorks);

constexpr bool kConstexprCompareWorks{ []() constexpr {
  AStr<8> alpha{"alpha"};
  AStr<4> bet{"bet"};
  AStr<6> also_alpha{"alpha"};
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
  AStr<8> str{"alpha"};
  str.at(1) = 'o';
  AStr<8> const& const_str{ str };
  return str.view() == std::string_view("aopha")
    && const_str.at(0) == 'a'
    && const_str.at(5) == '\0';
}() };
static_assert(kConstexprAtWorks);

constexpr bool kConstexprMutationWorks{ []() constexpr {
  AStr<8> str{"abc"};
  str[1] = 'x';
  str[2] = '\0';
  return str.view() == std::string_view("ax")
    && str.size() == 2u
    && str.storage_size() == 3u
    && str.back() == 'x';
}() };
static_assert(kConstexprMutationWorks);

constexpr bool kConstexprClearWorks{ []() constexpr {
  AStr<8> str{"clear"};
  str.clear();
  return str.empty()
    && str.size() == 0u
    && str.storage_size() == 1u
    && str.c_str()[0] == '\0';
}() };
static_assert(kConstexprClearWorks);

constexpr bool kConstexprMemberSwapWorks{ []() constexpr {
  AStr<8> lhs{"left"};
  AStr<8> rhs{"right"};
  lhs.swap(rhs);
  return lhs.view() == std::string_view("right")
    && rhs.view() == std::string_view("left");
}() };
static_assert(kConstexprMemberSwapWorks);

constexpr bool kConstexprFreeSwapWorks{ []() constexpr {
  AStr<8> lhs{"one"};
  AStr<8> rhs{"two"};
  swap(lhs, rhs);
  return lhs.view() == std::string_view("two")
    && rhs.view() == std::string_view("one");
}() };
static_assert(kConstexprFreeSwapWorks);

TEST(AStrConstexpr, SupportsCompileTimeEvaluation)
{
  EXPECT_TRUE(kConstexprDefaultConstructionWorks);
  EXPECT_TRUE(kConstexprLiteralConstructionWorks);
  EXPECT_TRUE(kConstexprCtadWorks);
  EXPECT_TRUE(kConstexprCompareWorks);
  EXPECT_TRUE(kConstexprAtWorks);
  EXPECT_TRUE(kConstexprMutationWorks);
  EXPECT_TRUE(kConstexprClearWorks);
  EXPECT_TRUE(kConstexprMemberSwapWorks);
  EXPECT_TRUE(kConstexprFreeSwapWorks);
}

TEST(AStrRuntime, PreservesStringAndStorageSemantics)
{
  AStr<8> str{"hello"};

  EXPECT_EQ(5u, str.size());
  EXPECT_EQ(5u, str.length());
  EXPECT_EQ(6u, str.storage_size());
  EXPECT_EQ(7u, str.capacity());
  EXPECT_EQ(8u, str.storage_capacity());
  EXPECT_EQ(std::string_view("hello"), str.view());
  EXPECT_STREQ("hello", str.c_str());
}

TEST(AStrRuntime, AtAllowsTerminatorAndThrowsPastIt)
{
  AStr<8> str{"hello"};

  EXPECT_EQ('\0', str.at(5));
  EXPECT_THROW(
    static_cast<void>(str.at(6)),
    std::out_of_range
  );
}
