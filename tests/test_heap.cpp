#include <cstdint>
#include <variant>

#include "constexpr/heap.hpp"

using TestStringStore = Constexpr::Strings<16>;
using TestItemStore = Constexpr::Items<4, std::variant<std::uint16_t, std::uint32_t>>;

// Compile-time heap scenarios pin the storage layer directly instead of reaching it only through EnumBuilder.

// Prove that the string heap can append and read back multiple strings during constant evaluation.
constexpr bool strings_add_and_read_back_during_constant_evaluation{
  [] {
    TestStringStore strings{};
    auto const first_id{ strings.add_string("one") };
    auto const second_id{ strings.add_string("two") };

    return first_id == 1u
      && second_id == 5u
      && strings.get_string(first_id) == "one"
      && strings.get_string(second_id) == "two"
      && strings.used_space() == 8u;
  }()
};
static_assert(strings_add_and_read_back_during_constant_evaluation);

// Prove that the item heap can store and recover variant alternatives during constant evaluation.
constexpr bool items_add_and_read_back_during_constant_evaluation{
  [] {
    TestItemStore items{};
    auto const first_id{ items.add_item(std::uint16_t{ 7u }) };
    auto const second_id{ items.add_item(std::uint32_t{ 9u }) };
    auto const* first_item{ items.get_item_if<std::uint16_t>(first_id) };
    auto const* second_item{ items.get_item_if<std::uint32_t>(second_id) };

    return first_id == 1u
      && second_id == 2u
      && first_item
      && *first_item == 7u
      && second_item
      && *second_item == 9u
      && items.used_space() == 2u;
  }()
};
static_assert(items_add_and_read_back_during_constant_evaluation);
