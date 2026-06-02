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
#include <string_view>
#include <array>
#include "constexpr/string.hpp"

namespace Constexpr {

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
  template <std::size_t N>
  class Strings {
    string<N+1> memory;

  public:
    /**
      * @brief Add a string to the memory store.
      *
      * @param str_to_register - String to register in memory.
      * @return std::size_t - Returns the begin_id that can be used with
      *   get_string().
      */
    constexpr std::size_t add_string(std::string_view str_to_register) {
      auto length { memory.length() };
      memory.append(str_to_register);
      memory.push_back('\0');
      return length;
    }

    /**
      * @brief Get the end id for the string just registered before.
      * 
      * @return std::size_t - end id
      */
    constexpr std::size_t end_id() const {
      auto length{ memory.size() };
      if (length) {
        --length;
      }
      return length;
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
    constexpr std::string_view get_string(std::size_t begin_id) const {
      assert(begin_id <= memory.size());
      return std::string_view(memory.begin() + begin_id);
    }

    /**
      * @brief Get a string view over an explicit subrange of the backing store.
      *
      * @param begin_id - First character index in the backing store.
      * @param end_id - One-past-the-end character index in the backing store.
      * @return std::string_view - View spanning the requested range.
      */
    constexpr std::string_view get_string(std::size_t begin_id, std::size_t end_id) const {
      assert(begin_id <= end_id && end_id <= memory.size());
      return std::string_view(memory.begin() + begin_id, end_id - begin_id);
    }
  };
  

  /**
    * @brief Poor-man's constexpr heap for objects that are about the same size
    *   in c++17.
    *
    * Objects never get deleted.
    *
    * @tparam N - Maximum number of objects that can be stored.
    * @tparam Variant - Common storage type used for every stored object.
    */
  template <std::size_t N, typename Variant>
  class Items {
    std::array<Variant, N> memory{};
    std::size_t next_id{0};

  public:
    /**
      * @brief Store an item in the backing array and return its id.
      *
      * @tparam T - Type assignable into \p Variant.
      * @param item - Item to store.
      * @return std::size_t - Id that can later be used with get_item().
      */
    template <typename T>
    constexpr std::size_t add_item(T item) {
      memory.at(next_id) = item;
      return next_id++;
    }

    /**
      * @brief Access a previously stored item by id.
      *
      * @param id - Id returned by add_item().
      * @return auto& - Reference to the stored item slot.
      */
    constexpr auto& get_item(std::size_t id) {
      return memory.at(id);
    }

    /**
      * @brief Access a previously stored item by id.
      *
      * @param id - Id returned by add_item().
      * @return auto& - Reference to the stored item slot.
      */
    constexpr auto& get_item(std::size_t id) const {
      return memory.at(id);
    }
  };

} // namespace Constexpr
#endif // CONSTEXPR_HEAP_HPP
