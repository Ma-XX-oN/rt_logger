#ifndef CONSTEXPR_CSTR_HPP
#define CONSTEXPR_CSTR_HPP
/**
 * @file constexpr_CStr.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Allows a c-string to be passed around more easily as a constexpr
 *   object.
 * @version 0.1
 * @date 2026-05-24
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <cassert>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace Constexpr
{
  /**
   * @brief An immutable contiguous container that stores a null-terminated
   *   string view with constexpr-friendly construction.
   *
   * The container models the character sequence without the trailing null
   * terminator. `c_str()` and `storage_size()` expose the underlying
   * null-terminated storage when that distinction matters.
   */
  struct CStr {
    using value_type = char;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type const*;
    using const_pointer = value_type const*;
    using reference = value_type const&;
    using const_reference = value_type const&;
    using iterator = const_pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static constexpr size_type npos = static_cast<size_type>(-1);

    /**
     * @brief Constructs an empty string container.
     */
    constexpr CStr() noexcept
    : m_data("")
    , m_size(0)
    {
    }

    /**
     * @brief Constructs a container from a string literal or char array.
     *
     * @tparam N - The storage size of the source array, including the trailing
     *   null terminator.
     * @param s - The source string storage.
     */
    template<size_type N>
    constexpr CStr(char const (&s)[N]) noexcept
    : m_data(s)
    , m_size(N - 1)
    {
    }

    /**
     * @brief Gets an iterator to the first character.
     *
     * @return const_iterator - Iterator to the first character.
     */
    constexpr const_iterator begin() const noexcept {
      return m_data;
    }

    /**
     * @brief Gets an iterator to the first character.
     *
     * @return const_iterator - Iterator to the first character.
     */
    constexpr const_iterator cbegin() const noexcept {
      return begin();
    }

    /**
     * @brief Gets an iterator one past the last character.
     *
     * @return const_iterator - Iterator one past the last character.
     */
    constexpr const_iterator end() const noexcept {
      return m_data + m_size;
    }

    /**
     * @brief Gets an iterator one past the last character.
     *
     * @return const_iterator - Iterator one past the last character.
     */
    constexpr const_iterator cend() const noexcept {
      return end();
    }

    /**
     * @brief Gets a reverse iterator to the last character.
     *
     * @return const_reverse_iterator - Reverse iterator to the last character.
     */
    constexpr const_reverse_iterator rbegin() const noexcept {
      return const_reverse_iterator(end());
    }

    /**
     * @brief Gets a reverse iterator to the last character.
     *
     * @return const_reverse_iterator - Reverse iterator to the last character.
     */
    constexpr const_reverse_iterator crbegin() const noexcept {
      return rbegin();
    }

    /**
     * @brief Gets a reverse iterator one before the first character.
     *
     * @return const_reverse_iterator - Reverse end iterator.
     */
    constexpr const_reverse_iterator rend() const noexcept {
      return const_reverse_iterator(begin());
    }

    /**
     * @brief Gets a reverse iterator one before the first character.
     *
     * @return const_reverse_iterator - Reverse end iterator.
     */
    constexpr const_reverse_iterator crend() const noexcept {
      return rend();
    }

    /**
     * @brief Gets the number of characters, excluding the trailing null
     *   terminator.
     *
     * @return size_type - Number of characters.
     */
    constexpr size_type size() const noexcept {
      return m_size;
    }

    /**
     * @brief Gets the number of characters, excluding the trailing null
     *   terminator.
     *
     * @return size_type - Number of characters.
     */
    constexpr size_type length() const noexcept {
      return size();
    }

    /**
     * @brief Gets the number of stored bytes, including the trailing null
     *   terminator.
     *
     * @return size_type - Number of stored bytes.
     */
    constexpr size_type storage_size() const noexcept {
      return m_size + 1;
    }

    /**
     * @brief Checks whether the character sequence is empty.
     *
     * @return bool - `true` if there are no characters before the terminator.
     */
    constexpr bool empty() const noexcept {
      return m_size == 0;
    }

    /**
     * @brief Gets the theoretical maximum number of characters.
     *
     * @return size_type - Maximum supported character count.
     */
    constexpr size_type max_size() const noexcept {
      return npos;
    }

    /**
     * @brief Gets a pointer to the underlying character sequence.
     *
     * @return const_pointer - Pointer to the first character.
     */
    constexpr const_pointer data() const noexcept {
      return m_data;
    }

    /**
     * @brief Gets a pointer to the underlying null-terminated C string.
     *
     * @return const_pointer - Pointer to the null-terminated string.
     */
    constexpr const_pointer c_str() const noexcept {
      return m_data;
    }

    /**
     * @brief Converts the container to a `std::string_view`.
     *
     * @return std::string_view - View of the character sequence.
     */
    constexpr std::string_view view() const noexcept {
      return std::string_view(m_data, m_size);
    }

    /**
     * @brief Converts the container to a `std::string_view`.
     *
     * @return std::string_view - View of the character sequence.
     */
    constexpr operator std::string_view() const noexcept {
      return view();
    }

    /**
     * @brief Gets the character at an index without bounds checking.
     *
     * @param i - Index of the character to access.
     * @return const_reference - The character at `i`.
     */
    constexpr const_reference operator[](size_type i) const noexcept {
      assert(i < m_size+1);
      return m_data[i];
    }

    /**
     * @brief Gets the character at an index with bounds checking.
     *
     * @param i - Index of the character to access.
     * @return const_reference - The character at `i`.
     * @throws std::out_of_range if `i` is out of bounds.
     */
    constexpr const_reference at(size_type i) const {
      if (i >= m_size+1) {
        throw std::out_of_range("CStr::at");
      }
      return m_data[i];
    }

    /**
     * @brief Gets the first character.
     *
     * @return const_reference - The first character.
     */
    constexpr const_reference front() const noexcept {
      assert(!empty());
      return m_data[0];
    }

    /**
     * @brief Gets the last character before the null terminator.
     *
     * @return const_reference - The last character.
     */
    constexpr const_reference back() const noexcept {
      assert(!empty());
      return m_data[m_size - 1];
    }

    /**
     * @brief Compares two string containers lexicographically.
     *
     * @param rhs - String container to compare against.
     * @return int - Negative if `*this < rhs`, positive if `rhs < *this`, or
     *   zero if equal.
     */
    constexpr int compare(CStr rhs) const noexcept {
      size_type i = 0;
      const size_type common = m_size < rhs.m_size ? m_size : rhs.m_size;

      for (; i < common; ++i) {
        if (m_data[i] < rhs.m_data[i]) {
          return -1;
        }
        if (rhs.m_data[i] < m_data[i]) {
          return 1;
        }
      }

      if (m_size < rhs.m_size) {
        return -1;
      }
      if (rhs.m_size < m_size) {
        return 1;
      }
      return 0;
    }

    /**
     * @brief Exchanges the contents of two string containers.
     *
     * @param rhs - The container to swap with.
     */
    constexpr void swap(CStr& rhs) noexcept {
      const auto old_data = m_data;
      const auto old_size = m_size;
      m_data = rhs.m_data;
      m_size = rhs.m_size;
      rhs.m_data = old_data;
      rhs.m_size = old_size;
    }

  private:
    char const* m_data;
    size_type m_size;
  };

  /**
   * @brief Checks whether two string containers are equal.
   *
   * @param lhs - Left-hand operand.
   * @param rhs - Right-hand operand.
   * @return bool - `true` if the containers contain the same character sequence.
   */
  inline constexpr bool operator==(CStr lhs, CStr rhs) noexcept {
    return lhs.compare(rhs) == 0;
  }

  /**
   * @brief Checks whether two string containers are not equal.
   *
   * @param lhs - Left-hand operand.
   * @param rhs - Right-hand operand.
   * @return bool - `true` if the containers differ.
   */
  inline constexpr bool operator!=(CStr lhs, CStr rhs) noexcept {
    return !(lhs == rhs);
  }

  /**
   * @brief Checks whether one string container is lexicographically less than
   *   another.
   *
   * @param lhs - Left-hand operand.
   * @param rhs - Right-hand operand.
   * @return bool - `true` if `lhs < rhs`.
   */
  inline constexpr bool operator<(CStr lhs, CStr rhs) noexcept {
    return lhs.compare(rhs) < 0;
  }

  /**
   * @brief Checks whether one string container is lexicographically less than or
   *   equal to another.
   *
   * @param lhs - Left-hand operand.
   * @param rhs - Right-hand operand.
   * @return bool - `true` if `lhs <= rhs`.
   */
  inline constexpr bool operator<=(CStr lhs, CStr rhs) noexcept {
    return lhs.compare(rhs) <= 0;
  }

  /**
   * @brief Checks whether one string container is lexicographically greater than
   *   another.
   *
   * @param lhs - Left-hand operand.
   * @param rhs - Right-hand operand.
   * @return bool - `true` if `lhs > rhs`.
   */
  inline constexpr bool operator>(CStr lhs, CStr rhs) noexcept {
    return lhs.compare(rhs) > 0;
  }

  /**
   * @brief Checks whether one string container is lexicographically greater than
   *   or equal to another.
   *
   * @param lhs - Left-hand operand.
   * @param rhs - Right-hand operand.
   * @return bool - `true` if `lhs >= rhs`.
   */
  inline constexpr bool operator>=(CStr lhs, CStr rhs) noexcept {
    return lhs.compare(rhs) >= 0;
  }

  /**
   * @brief Exchanges the contents of two string containers.
   *
   * @param lhs - Left-hand operand.
   * @param rhs - Right-hand operand.
   */
  inline constexpr void swap(CStr& lhs, CStr& rhs) noexcept {
    lhs.swap(rhs);
  }
} // namespace Constexpr
#endif // CONSTEXPR_CSTR_HPP
