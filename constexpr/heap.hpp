/**
 * @file heap.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Small constexpr storage helpers for strings and variant-like items.
 * @version 0.1
 * @date 2026-06-02
 * 
 * @copyright Copyright (c) 2026
 * 
 */
 #ifndef CONSTEXPR_HEAP_HPP
 #define CONSTEXPR_HEAP_HPP

#include <cstdlib>
#include <cstdint>
#include <limits>
#include <string_view>
#include <array>
#include "constexpr/string.hpp"

namespace Constexpr {

  using string_id_t = std::uint16_t;

  /**
   * @brief Poor-man's constexpr heap for strings in c++17.
   *
   * Strings never get deleted.
   *
   * @tparam N - Maximum number of char characters to hold.  Each string is
   *   terminated by a NUL. Storing a string with an embedded NUL is possible,
   *   but to retrieve it, the 2 parameter get_string() function must be used.
   *   The second parameter can be requested calling the end_id() value after
   *   add_string() call.
   *
   *   There is \b ALWAYS an implicit NUL character stored after each string.
   */
  template <std::uint16_t N>
  class Strings {
    static_assert(N < std::numeric_limits<std::uint16_t>::max(),
      "Exceeded the maximum number of string space allowed.");

    string<N+1> memory;

  public:
    /**
      * @brief Add a string to the memory store.
      *
      * @param str_to_register - String to register in memory.
      * @return string_id_t - Returns the begin_id that can be used with
      *   get_string().  Value will never be 0.
      */
    constexpr string_id_t add_string(std::string_view str_to_register) {
      auto length { memory.length() };
      memory.append(str_to_register);
      memory.push_back('\0');
      return static_cast<string_id_t>(length + 1);
    }

    /**
      * @brief Get the end id for the string just registered before.
      * 
      * @return string_id_t - End id.
      */
    constexpr string_id_t end_id() const {
      auto length{ memory.size() };
      if (length) {
        --length;
      }
      return static_cast<string_id_t>(length);
    }

    /**
      * @brief Get a string view beginning at a previously returned string id.
      *
      * This overload expects the stored string to be terminated by the implicit
      * trailing NUL inserted by add_string().
      *
      * @param begin_id - Begin id previously returned by add_string().
      * @return std::string_view - View of the stored string.
      */
    constexpr std::string_view get_string(string_id_t begin_id) const {
      assert(begin_id);
      assert(begin_id < memory.size());
      return std::string_view(memory.begin() + begin_id - 1);
    }

    /**
      * @brief Get a string view over an explicit stored range.
      *
      * @param begin_id - Begin id for the requested range.
      * @param end_id - End id for the requested range.
      * @return std::string_view - View spanning the requested range.
      */
    constexpr std::string_view get_string(string_id_t begin_id, string_id_t end_id) const {
      assert(begin_id);
      assert(begin_id < end_id && end_id <= memory.size());
      return std::string_view(memory.begin() + begin_id - 1, end_id - begin_id);
    }
  };
  

  using item_id_t = std::uint16_t;

  /**
    * @brief Poor-man's constexpr heap for objects that are about the same size
    *   in c++17.
    *
    * Objects never get deleted.
    *
    * TODO: If delete/reuse is needed later, add a separate ItemsDeletable
    *       type rather than changing Items semantics.
    *
    *       - Items remains an append-only slot store with stable ids.
    *       - ItemsDeletable can use an Items internally plus a free-id list.
    *       - delete_item(): record the freed slot id for later reuse.
    *       - add_item(): reuse a freed slot if one is available; otherwise call
    *         Items::add_item() to claim the next fresh slot.
    *       - If id reuse is allowed, consider a generation counter or debug
    *         handle validation so stale ids can be detected.
    *
    * TODO: If useful, factor out a small constexpr LIFO helper for internal
    *       free-id management and reuse it here. Items itself still models slot
    *       storage, not stack semantics.
    *
    * @tparam N - Maximum number of objects that can be stored.
    * @tparam Variant - Common storage type used for every stored object.
    */
  template <std::uint16_t N, typename Variant>
  class Items {
    static_assert(N < std::numeric_limits<std::uint16_t>::max(),
      "Exceeded the maximum number of allocatable items allowed.");

    std::array<Variant, N> memory{};
    std::uint16_t next_id{0};

  public:
    /**
      * @brief Store an item in the backing array and return its id.
      *
      * @tparam T - Type assignable into \p Variant.
      * @param item - Item to store.
      * @return item_id_t - Id that can later be used with get_item().  Value
      *   will never be 0.
      */
    template <typename T>
    constexpr item_id_t add_item(T item) {
      memory.at(next_id) = item;
      return ++next_id;
    }

    /**
      * @brief Retrieves a previously stored item by id.
      *
      * @param id - Id returned by add_item().
      * @return auto& - Reference to the stored item slot.
      */
    constexpr auto& get_item(item_id_t id) {
      assert(id && id <= next_id || !"Out of bounds");
      return memory.at(id-1);
    }

    /**
      * @brief Retrieves a previously stored item as const by id.
      *
      * @param id - Id returned by add_item().
      * @return auto& - Reference to the stored item slot.
      */
    constexpr auto& get_item(item_id_t id) const {
      assert(id && id <= next_id || !"Out of bounds");
      return memory.at(id-1);
    }

    /**
      * @brief Retrieves a previously stored item as Item type by id.
      *
      * @param id - Id returned by add_item().
      * @return auto& - Reference to the stored item slot.
      */
    template <typename Item>
    constexpr auto& get_item(item_id_t id) {
      assert(id && id <= next_id || !"Out of bounds");
      return std::get<Item>(memory.at(id-1));
    }

    /**
      * @brief Retrieves a previously stored item as const Item type by id.
      *
      * @param id - Id returned by add_item().
      * @return auto& - Reference to the stored item slot.
      */
    template <typename Item>
    constexpr auto& get_item(item_id_t id) const {
      assert(id && id <= next_id || !"Out of bounds");
      return std::get<Item>(memory.at(id-1));
    }

    /**
      * @brief Retrieves a previously stored item as Item type pointer by id if
      *   correct type, otherwise return nullptr.
      *
      * @param id - Id returned by add_item().
      * @return auto* - Pointer to the stored item slot or nullptr.
      */
    template <typename Item>
    constexpr auto* get_item_if(item_id_t id) {
      assert(id && id <= next_id || !"Out of bounds");
      return std::get_if<Item>(&memory.at(id-1));
    }

    /**
      * @brief Retrieves a previously stored item as Item type pointer by id if
      *   correct type, otherwise return nullptr.
      *
      * @param id - Id returned by add_item().
      * @return auto* - Pointer to the stored item slot or nullptr.
      */
    template <typename Item>
    constexpr auto const* get_item_if(item_id_t id) const {
      assert(id && id <= next_id || !"Out of bounds");
      return std::get_if<Item>(&memory.at(id-1));
    }
  };

} // namespace Constexpr
#endif // CONSTEXPR_HEAP_HPP
