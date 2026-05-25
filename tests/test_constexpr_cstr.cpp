#include "constexpr_CStr.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string_view>

constexpr bool kConstexprDefaultConstructionWorks = []() constexpr {
  Constexpr::CStr str{};
  return str.empty()
    && str.size() == 0u
    && str.length() == 0u
    && str.storage_size() == 1u
    && str.begin() == str.end()
    && str.cbegin() == str.cend()
    && str.data()[0] == '\0'
    && str.c_str()[0] == '\0'
    && str.view().empty();
}();
static_assert(kConstexprDefaultConstructionWorks);

constexpr bool kConstexprLiteralConstructionWorks = []() constexpr {
  Constexpr::CStr str{"alpha"};
  return !str.empty()
    && str.size() == 5u
    && str.length() == 5u
    && str.storage_size() == 6u
    && str.front() == 'a'
    && str.back() == 'a'
    && str[1] == 'l'
    && str.begin()[2] == 'p'
    && *(str.end() - 1) == 'a'
    && *(str.rbegin()) == 'a'
    && *(str.crbegin()) == 'a'
    && str.max_size() == Constexpr::CStr::npos
    && str.view() == std::string_view("alpha")
    && static_cast<std::string_view>(str) == std::string_view("alpha")
    && str.c_str()[5] == '\0';
}();
static_assert(kConstexprLiteralConstructionWorks);

constexpr bool kConstexprCompareWorks = []() constexpr {
  Constexpr::CStr alpha{"alpha"};
  Constexpr::CStr beta{"beta"};
  Constexpr::CStr also_alpha{"alpha"};
  return alpha.compare(beta) < 0
    && beta.compare(alpha) > 0
    && alpha.compare(also_alpha) == 0
    && alpha == also_alpha
    && alpha != beta
    && alpha < beta
    && alpha <= also_alpha
    && beta > alpha
    && beta >= alpha;
}();
static_assert(kConstexprCompareWorks);

constexpr bool kConstexprAtWorks = []() constexpr {
  Constexpr::CStr str{"alpha"};
  return str.at(0) == 'a'
    && str.at(4) == 'a'
    && str.at(5) == '\0';
}();
static_assert(kConstexprAtWorks);

constexpr bool kConstexprMemberSwapWorks = []() constexpr {
  Constexpr::CStr lhs{"left"};
  Constexpr::CStr rhs{"right"};
  lhs.swap(rhs);
  return lhs.view() == std::string_view("right")
    && rhs.view() == std::string_view("left");
}();
static_assert(kConstexprMemberSwapWorks);

constexpr bool kConstexprFreeSwapWorks = []() constexpr {
  Constexpr::CStr lhs{"one"};
  Constexpr::CStr rhs{"two"};
  swap(lhs, rhs);
  return lhs.view() == std::string_view("two")
    && rhs.view() == std::string_view("one");
}();
static_assert(kConstexprFreeSwapWorks);

TEST(CStrConstexpr, SupportsCompileTimeEvaluation)
{
  EXPECT_TRUE(kConstexprDefaultConstructionWorks);
  EXPECT_TRUE(kConstexprLiteralConstructionWorks);
  EXPECT_TRUE(kConstexprCompareWorks);
  EXPECT_TRUE(kConstexprAtWorks);
  EXPECT_TRUE(kConstexprMemberSwapWorks);
  EXPECT_TRUE(kConstexprFreeSwapWorks);
}

TEST(CStrRuntime, PreservesStringAndStorageSemantics)
{
  Constexpr::CStr str{"hello"};

  EXPECT_EQ(5u, str.size());
  EXPECT_EQ(5u, str.length());
  EXPECT_EQ(6u, str.storage_size());
  EXPECT_EQ(std::string_view("hello"), str.view());
  EXPECT_STREQ("hello", str.c_str());
}

TEST(CStrRuntime, AtAllowsTerminatorAndThrowsPastIt)
{
  Constexpr::CStr str{"hello"};

  EXPECT_EQ('\0', str.at(5));
  EXPECT_THROW(
    static_cast<void>(str.at(6)),
    std::out_of_range
  );
}
