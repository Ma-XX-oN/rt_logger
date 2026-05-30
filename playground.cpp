#include "constexpr/string.hpp"

template <std::size_t V>
void test_constexpr_value() {}

/**
 * @brief Entry point for ad-hoc experiments against the header-only library.
 *
 * @return Zero when the playground executable exits successfully.
 */
int main()
{
  // can string::size() be used as a template parameter?
  using namespace Constexpr;
  constexpr string x{ "hi" };
  test_constexpr_value<x.size()>();
  return 0;
}
