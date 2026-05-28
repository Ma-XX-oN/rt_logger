#ifndef CONSTEXPR_STRARRAY_HPP
#define CONSTEXPR_STRARRAY_HPP
/**
 * @file constexpr_StrArray.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Owns a fixed-capacity null-terminated string in constexpr-friendly
 *   storage.
 * @version 0.1
 * @date 2026-05-24
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <array>
#include <cassert>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace Constexpr
{
  /**
   * @brief An owning fixed-capacity contiguous container for null-terminated
   *   strings.
   *
   * `StrArray<N>` stores up to `N - 1` characters plus a trailing null
   * terminator. The logical character sequence ends at the first `'\0'`, while
   * the underlying storage always remains `N` bytes wide.
   *
   * @tparam N - Storage capacity including the trailing null terminator.
   */
  template <std::size_t N>
  class StrArray {
  public:
    static_assert(N > 0, "StrArray requires storage for at least a null terminator.");

    using Base = std::array<char, N>;
    using value_type = char;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using const_pointer = value_type const*;
    using reference = value_type&;
    using const_reference = value_type const&;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static constexpr size_type npos{ std::numeric_limits<size_type>::max() };

    /**
     * @brief Constructs an empty string container.
     */
    constexpr StrArray() noexcept
    : m_data{}
    {
    }

    /**
     * @brief Constructs a string container from an existing null-terminated
     *   storage buffer.
     *
     * @param storage - Full storage buffer including the terminating null.
     */
    constexpr StrArray(Base const& storage) noexcept
    : m_data(storage)
    {
      assert(find_terminator() != npos);
    }

    /**
     * @brief Constructs a string container from a string literal or char array.
     *
     * The entire source array, including its trailing null terminator, must
     * fit in the destination storage.
     *
     * @tparam M - Source storage size including the trailing null terminator.
     * @param s - Source string storage.
     */
    template <std::size_t M>
    constexpr StrArray(char const (&s)[M]) noexcept
    : m_data{}
    {
      static_assert(M <= N,
        "StrArray source string must fully fit in destination storage.");

      for (size_type i{ 0 }; i < M; ++i) {
        m_data[i] = s[i];
      }
    }

    /**
     * @brief Gets an iterator to the first character.
     *
     * @return iterator - Iterator to the first character.
     */
    constexpr iterator begin() noexcept {
      return m_data.data();
    }

    /**
     * @brief Gets an iterator to the first character.
     *
     * @return const_iterator - Iterator to the first character.
     */
    constexpr const_iterator begin() const noexcept {
      return m_data.data();
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
     * @brief Gets an iterator one past the last character before the
     *   terminator.
     *
     * @return iterator - Iterator one past the last character.
     */
    constexpr iterator end() noexcept {
      return m_data.data() + size();
    }

    /**
     * @brief Gets an iterator one past the last character before the
     *   terminator.
     *
     * @return const_iterator - Iterator one past the last character.
     */
    constexpr const_iterator end() const noexcept {
      return m_data.data() + size();
    }

    /**
     * @brief Gets an iterator one past the last character before the
     *   terminator.
     *
     * @return const_iterator - Iterator one past the last character.
     */
    constexpr const_iterator cend() const noexcept {
      return end();
    }

    /**
     * @brief Gets a reverse iterator to the last character.
     *
     * @return reverse_iterator - Reverse iterator to the last character.
     */
    constexpr reverse_iterator rbegin() noexcept {
      return reverse_iterator(end());
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
     * @return reverse_iterator - Reverse end iterator.
     */
    constexpr reverse_iterator rend() noexcept {
      return reverse_iterator(begin());
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
     * @brief Gets the number of characters before the terminating null.
     *
     * @return size_type - Number of characters in the logical string.
     */
    constexpr size_type size() const noexcept {
      size_type const terminator{ find_terminator() };
      return terminator == npos ? N : terminator;
    }

    /**
     * @brief Gets the number of characters before the terminating null.
     *
     * @return size_type - Number of characters in the logical string.
     */
    constexpr size_type length() const noexcept {
      return size();
    }

    /**
     * @brief Gets the number of bytes used by the string including the
     *   terminating null.
     *
     * @return size_type - Number of used bytes including the terminator.
     */
    constexpr size_type storage_size() const noexcept {
      size_type const terminator{ find_terminator() };
      assert(terminator != npos);
      return terminator == npos ? N : terminator + 1;
    }

    /**
     * @brief Gets the maximum number of characters before the terminating null.
     *
     * @return size_type - Maximum supported character count.
     */
    constexpr size_type capacity() const noexcept {
      return N - 1;
    }

    /**
     * @brief Gets the maximum number of characters before the terminating null.
     *
     * @return size_type - Maximum supported character count.
     */
    constexpr size_type max_size() const noexcept {
      return capacity();
    }

    /**
     * @brief Gets the total storage capacity including the terminator slot.
     *
     * @return size_type - Number of bytes in the underlying storage buffer.
     */
    constexpr size_type storage_capacity() const noexcept {
      return N;
    }

    /**
     * @brief Checks whether the character sequence is empty.
     *
     * @return bool - `true` if there are no characters before the terminator.
     */
    constexpr bool empty() const noexcept {
      return size() == 0;
    }

    /**
     * @brief Gets a pointer to the underlying storage buffer.
     *
     * @return pointer - Pointer to the owned storage.
     */
    constexpr pointer data() noexcept {
      return m_data.data();
    }

    /**
     * @brief Gets a pointer to the underlying storage buffer.
     *
     * @return const_pointer - Pointer to the owned storage.
     */
    constexpr const_pointer data() const noexcept {
      return m_data.data();
    }

    /**
     * @brief Gets a pointer to the underlying null-terminated string.
     *
     * @return const_pointer - Pointer to the null-terminated string.
     */
    constexpr const_pointer c_str() const noexcept {
      return m_data.data();
    }

    /**
     * @brief Converts the container to a `std::string_view`.
     *
     * @return std::string_view - View of the logical character sequence.
     */
    constexpr std::string_view view() const noexcept {
      return std::string_view(m_data.data(), size());
    }

    /**
     * @brief Converts the container to a `std::string_view`.
     *
     * @return std::string_view - View of the logical character sequence.
     */
    constexpr operator std::string_view() const noexcept {
      return view();
    }

    /**
     * @brief Gets the character at an index without bounds checking.
     *
     * Access is allowed through the terminating null.
     *
     * @param i - Zero-based index into the logical storage.
     * @return reference - Reference to the character at `i`.
     */
    constexpr reference operator[](size_type i) noexcept {
      assert(i < storage_size());
      return m_data[i];
    }

    /**
     * @brief Gets the character at an index without bounds checking.
     *
     * Access is allowed through the terminating null.
     *
     * @param i - Zero-based index into the logical storage.
     * @return const_reference - Reference to the character at `i`.
     */
    constexpr const_reference operator[](size_type i) const noexcept {
      assert(i < storage_size());
      return m_data[i];
    }

    /**
     * @brief Gets the character at an index with bounds checking.
     *
     * Access is allowed through the terminating null.
     *
     * @param i - Zero-based index into the logical storage.
     * @return reference - Reference to the character at `i`.
     */
    constexpr reference at(size_type i) {
      if (i >= storage_size()) {
        throw std::out_of_range("StrArray::at");
      }
      return m_data[i];
    }

    /**
     * @brief Gets the character at an index with bounds checking.
     *
     * Access is allowed through the terminating null.
     *
     * @param i - Zero-based index into the logical storage.
     * @return const_reference - Reference to the character at `i`.
     */
    constexpr const_reference at(size_type i) const {
      if (i >= storage_size()) {
        throw std::out_of_range("StrArray::at");
      }
      return m_data[i];
    }

    /**
     * @brief Gets the first character.
     *
     * @return reference - Reference to the first character.
     */
    constexpr reference front() noexcept {
      assert(!empty());
      return m_data[0];
    }

    /**
     * @brief Gets the first character.
     *
     * @return const_reference - Reference to the first character.
     */
    constexpr const_reference front() const noexcept {
      assert(!empty());
      return m_data[0];
    }

    /**
     * @brief Gets the last character before the terminating null.
     *
     * @return reference - Reference to the last logical character.
     */
    constexpr reference back() noexcept {
      assert(!empty());
      return m_data[size() - 1];
    }

    /**
     * @brief Gets the last character before the terminating null.
     *
     * @return const_reference - Reference to the last logical character.
     */
    constexpr const_reference back() const noexcept {
      assert(!empty());
      return m_data[size() - 1];
    }

    /**
     * @brief Clears the string while preserving its storage capacity.
     */
    constexpr void clear() noexcept {
      for (auto& ch : m_data) {
        ch = '\0';
      }
    }

    /**
     * @brief Lexicographically compares this string container with another.
     *
     * @tparam M - Storage capacity of the other string container.
     * @param rhs - String container to compare against.
     * @return int - Negative if `*this < rhs`, positive if `rhs < *this`, or
     *   `0` if both strings are equal.
     */
    template <size_type M>
    constexpr int compare(StrArray<M> const& rhs) const noexcept {
      size_type i{ 0 };
      size_type const lhs_size{ size() };
      size_type const rhs_size{ rhs.size() };
      size_type const common{ lhs_size < rhs_size ? lhs_size : rhs_size };

      while (i < common) {
        if (m_data[i] < rhs.data()[i]) {
          return -1;
        }
        if (rhs.data()[i] < m_data[i]) {
          return 1;
        }
        ++i;
      }

      if (lhs_size < rhs_size) {
        return -1;
      }
      if (rhs_size < lhs_size) {
        return 1;
      }
      return 0;
    }

    /**
     * @brief Swaps the contents of this container with another of the same
     *   storage size.
     *
     * @param rhs - The container to swap with.
     */
    constexpr void swap(StrArray& rhs) noexcept {
      for (size_type i{ 0 }; i < N; ++i) {
        value_type const old_value{ m_data[i] };
        m_data[i] = rhs.m_data[i];
        rhs.m_data[i] = old_value;
      }
    }

  private:
    /**
     * @brief Finds the first null terminator in the owned storage.
     *
     * @return size_type - Index of the first `'\0'`, or `npos` if no
     *   terminator exists.
     */
    constexpr size_type find_terminator() const noexcept {
      for (size_type i{ 0 }; i < N; ++i) {
        if (m_data[i] == '\0') {
          return i;
        }
      }
      return npos;
    }

    Base m_data;
  };

  template <std::size_t N>
  StrArray(std::array<char, N>) -> StrArray<N>;

  template <std::size_t N>
  StrArray(char const (&)[N]) -> StrArray<N>;

  /**
   * @brief Checks whether two string containers are equal.
   *
   * @tparam LhsN - Storage capacity of the left-hand string container.
   * @tparam RhsN - Storage capacity of the right-hand string container.
   * @param lhs - Left-hand string container.
   * @param rhs - Right-hand string container.
   * @return bool - `true` if both strings are equal.
   */
  template <std::size_t LhsN, std::size_t RhsN>
  inline constexpr bool operator==(StrArray<LhsN> const& lhs,
    StrArray<RhsN> const& rhs) noexcept
  {
    return lhs.compare(rhs) == 0;
  }

  /**
   * @brief Checks whether two string containers differ.
   *
   * @tparam LhsN - Storage capacity of the left-hand string container.
   * @tparam RhsN - Storage capacity of the right-hand string container.
   * @param lhs - Left-hand string container.
   * @param rhs - Right-hand string container.
   * @return bool - `true` if the strings differ.
   */
  template <std::size_t LhsN, std::size_t RhsN>
  inline constexpr bool operator!=(StrArray<LhsN> const& lhs,
    StrArray<RhsN> const& rhs) noexcept
  {
    return !(lhs == rhs);
  }

  /**
   * @brief Checks whether one string container is lexicographically less than
   *   another.
   *
   * @tparam LhsN - Storage capacity of the left-hand string container.
   * @tparam RhsN - Storage capacity of the right-hand string container.
   * @param lhs - Left-hand string container.
   * @param rhs - Right-hand string container.
   * @return bool - `true` if `lhs < rhs`.
   */
  template <std::size_t LhsN, std::size_t RhsN>
  inline constexpr bool operator<(StrArray<LhsN> const& lhs,
    StrArray<RhsN> const& rhs) noexcept
  {
    return lhs.compare(rhs) < 0;
  }

  /**
   * @brief Checks whether one string container is lexicographically less than
   *   or equal to another.
   *
   * @tparam LhsN - Storage capacity of the left-hand string container.
   * @tparam RhsN - Storage capacity of the right-hand string container.
   * @param lhs - Left-hand string container.
   * @param rhs - Right-hand string container.
   * @return bool - `true` if `lhs <= rhs`.
   */
  template <std::size_t LhsN, std::size_t RhsN>
  inline constexpr bool operator<=(StrArray<LhsN> const& lhs,
    StrArray<RhsN> const& rhs) noexcept
  {
    return lhs.compare(rhs) <= 0;
  }

  /**
   * @brief Checks whether one string container is lexicographically greater
   *   than another.
   *
   * @tparam LhsN - Storage capacity of the left-hand string container.
   * @tparam RhsN - Storage capacity of the right-hand string container.
   * @param lhs - Left-hand string container.
   * @param rhs - Right-hand string container.
   * @return bool - `true` if `lhs > rhs`.
   */
  template <std::size_t LhsN, std::size_t RhsN>
  inline constexpr bool operator>(StrArray<LhsN> const& lhs,
    StrArray<RhsN> const& rhs) noexcept
  {
    return lhs.compare(rhs) > 0;
  }

  /**
   * @brief Checks whether one string container is lexicographically greater
   *   than or equal to another.
   *
   * @tparam LhsN - Storage capacity of the left-hand string container.
   * @tparam RhsN - Storage capacity of the right-hand string container.
   * @param lhs - Left-hand string container.
   * @param rhs - Right-hand string container.
   * @return bool - `true` if `lhs >= rhs`.
   */
  template <std::size_t LhsN, std::size_t RhsN>
  inline constexpr bool operator>=(StrArray<LhsN> const& lhs,
    StrArray<RhsN> const& rhs) noexcept
  {
    return lhs.compare(rhs) >= 0;
  }

  /**
   * @brief Swaps two string containers with the same storage size.
   *
   * @tparam N - Storage capacity of both containers.
   * @param lhs - Left-hand string container.
   * @param rhs - Right-hand string container.
   */
  template <std::size_t N>
  inline constexpr void swap(StrArray<N>& lhs, StrArray<N>& rhs) noexcept {
    lhs.swap(rhs);
  }
} // namespace Constexpr

#endif // CONSTEXPR_STRARRAY_HPP
