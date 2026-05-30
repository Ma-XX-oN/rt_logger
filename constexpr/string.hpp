#ifndef CONSTEXPR_STRING_HPP
#define CONSTEXPR_STRING_HPP
/**
 * @file string.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Owns a fixed-capacity null-terminated counted string in
 *   constexpr-friendly storage, with an API modelled after `std::string`.
 * @version 0.1
 * @date 2026-05-24
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <array>
#include <cassert>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace Constexpr
{
  /**
   * @brief An owning fixed-capacity contiguous container for counted strings.
   *
   * `string<N>` stores up to `N - 1` logical characters plus a trailing null
   * terminator. The logical character sequence is the first `size()` bytes and
   * may include embedded `'\0'` values. The owned storage remains
   * null-terminated for C-string interop.
   *
   * @tparam N - Storage capacity including the trailing null terminator.
   */
  template <std::size_t N>
  class string {
  public:
    static_assert(N > 0, "string requires storage for at least a null terminator.");

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
    constexpr string() noexcept
    : m_data{}
    , m_size(0)
    {
    }

    /**
     * @brief Constructs a string container from a string literal or char array.
     *
     * The logical string becomes the source array contents excluding the
     * trailing null terminator.
     *
     * @tparam M - Source storage size including the trailing null terminator.
     * @param s - Source string storage.
     */
    template <std::size_t M>
    constexpr string(char const (&s)[M]) noexcept
    : m_data{}
    , m_size(0)
    {
      static_assert(M <= N,
        "string source string must fully fit in destination storage.");

      assign_range(s, M - 1);
    }

    /**
     * @brief Constructs a string container from a null-terminated C string.
     *
     * The logical string becomes the source characters before the first null
     * terminator.
     *
     * @param s - Null-terminated source string.
     * @throws std::length_error if the source string is too long for this
     *   container.
     */
    template <typename Ptr,
      typename std::enable_if_t<
        !std::is_array<typename std::remove_reference<Ptr>::type>::value
        && std::is_convertible<Ptr, char const*>::value,
        int> = 0>
    constexpr string(Ptr&& s)
    : m_data{}
    , m_size(0)
    {
      assign(static_cast<char const*>(s));
    }

    /**
     * @brief Constructs a string container from a counted string view.
     *
     * @param sv - Source logical character sequence.
     * @throws std::length_error if the source view is too large for this
     *   container.
     */
    constexpr string(std::string_view sv)
    : m_data{}
    , m_size(0)
    {
      assign(sv);
    }

    /**
     * @brief Constructs a string container by copying another container with
     *   the same storage size.
     *
     * Only the source logical characters and trailing null terminator are
     * copied. Bytes beyond the logical end are left unspecified.
     *
     * @param rhs - String container to copy.
     */
    constexpr string(string const& rhs) noexcept
    : m_data{}
    , m_size(0)
    {
      assign_range(rhs.c_str(), rhs.size());
    }

    /**
     * @brief Constructs a string container by copying another container with a
     *   different storage size.
     *
     * @tparam M - Storage size of the source container.
     * @param rhs - String container to copy.
     * @throws std::length_error if the source string is too large for this
     *   container.
     */
    template <std::size_t M>
    constexpr string(string<M> const& rhs)
    : m_data{}
    , m_size(0)
    {
      assign(rhs);
    }

    /**
     * @brief Constructs a string container by moving another container with the
     *   same storage size.
     *
     * @param rhs - String container to move from.
     */
    constexpr string(string&& rhs) noexcept
    : m_data{}
    , m_size(0)
    {
      assign_range(rhs.c_str(), rhs.size());
      rhs.clear();
    }

    /**
     * @brief Assigns the logical contents of another container with the same
     *   storage size.
     *
     * Only the source logical characters and trailing null terminator are
     * copied. Bytes beyond the logical end are left unspecified.
     *
     * @param rhs - String container to copy from.
     * @return string& - Reference to this container.
     */
    constexpr string& operator=(string const& rhs) noexcept {
      assign_range(rhs.c_str(), rhs.size());
      return *this;
    }

    /**
     * @brief Assigns the logical contents of another container with a
     *   different storage size.
     *
     * @tparam M - Storage size of the source container.
     * @param rhs - String container to copy from.
     * @return string& - Reference to this container.
     * @throws std::length_error if the source string is too large for this
     *   container.
     */
    template <std::size_t M>
    constexpr string& operator=(string<M> const& rhs) {
      assign(rhs);
      return *this;
    }

    /**
     * @brief Assigns the logical contents of another container by move.
     *
     * @param rhs - String container to move from.
     * @return string& - Reference to this container.
     */
    constexpr string& operator=(string&& rhs) noexcept {
      if (this != &rhs) {
        assign_range(rhs.c_str(), rhs.size());
        rhs.clear();
      }
      return *this;
    }

    /**
     * @brief Assigns the logical contents of a counted string view.
     *
     * @param sv - New logical character sequence.
     * @return string& - Reference to this container.
     * @throws std::length_error if the source view is too large for this
     *   container.
     */
    constexpr string& operator=(std::string_view sv) {
      assign(sv);
      return *this;
    }

    /**
     * @brief Assigns the logical contents of a null-terminated C string.
     *
     * @param s - Null-terminated source string.
     * @return string& - Reference to this container.
     * @throws std::length_error if the source string is too long for this
     *   container.
     */
    constexpr string& operator=(char const* s) {
      assign(s);
      return *this;
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
     * @brief Gets an iterator one past the last logical character.
     *
     * @return iterator - Iterator one past the last character.
     */
    constexpr iterator end() noexcept {
      return m_data.data() + size();
    }

    /**
     * @brief Gets an iterator one past the last logical character.
     *
     * @return const_iterator - Iterator one past the last character.
     */
    constexpr const_iterator end() const noexcept {
      return m_data.data() + size();
    }

    /**
     * @brief Gets an iterator one past the last logical character.
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
     * @brief Gets the number of logical characters.
     *
     * @return size_type - Number of characters in the logical string.
     */
    constexpr size_type size() const noexcept {
      return m_size;
    }

    /**
     * @brief Gets the number of logical characters.
     *
     * @return size_type - Number of characters in the logical string.
     */
    constexpr size_type length() const noexcept {
      return size();
    }

    /**
     * @brief Gets the maximum number of logical characters.
     *
     * @return size_type - Maximum supported character count.
     */
    constexpr size_type capacity() const noexcept {
      return N - 1;
    }

    /**
     * @brief Gets the maximum number of logical characters.
     *
     * @return size_type - Maximum supported character count.
     */
    constexpr size_type max_size() const noexcept {
      return capacity();
    }

    /**
     * @brief Checks whether the character sequence is empty.
     *
     * @return bool - `true` if there are no logical characters.
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
     * Access is limited to the current string contents.
     *
     * @param i - Zero-based index into the logical storage.
     * @return reference - Reference to the character at `i`.
     */
    constexpr reference operator[](size_type i) noexcept {
      assert(i < size());
      return m_data[i];
    }

    /**
     * @brief Gets the character at an index without bounds checking.
     *
     * Access is limited to the current string contents.
     *
     * @param i - Zero-based index into the logical storage.
     * @return const_reference - Reference to the character at `i`.
     */
    constexpr const_reference operator[](size_type i) const noexcept {
      assert(i < size());
      return m_data[i];
    }

    /**
     * @brief Gets the character at an index with bounds checking.
     *
     * Access is limited to the current string contents.
     *
     * @param i - Zero-based index into the logical storage.
     * @return reference - Reference to the character at `i`.
     * @throws std::out_of_range if the index is past the end of the current
     *   string.
     */
    constexpr reference at(size_type i) {
      if (i >= size()) {
        throw std::out_of_range("string::at: index out of range");
      }
      return m_data[i];
    }

    /**
     * @brief Gets the character at an index with bounds checking.
     *
     * Access is limited to the current string contents.
     *
     * @param i - Zero-based index into the logical storage.
     * @return const_reference - Reference to the character at `i`.
     * @throws std::out_of_range if the index is past the end of the current
     *   string.
     */
    constexpr const_reference at(size_type i) const {
      if (i >= size()) {
        throw std::out_of_range("string::at: index out of range");
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
      m_size = 0;
      set_terminator();
    }

    /**
     * @brief Changes the logical string length.
     *
     * If the string grows, new logical characters are initialized to `fill`.
     * If the string shrinks, bytes past the new logical end are left
     * unspecified.
     *
     * @param new_size - Requested logical character count.
     * @param fill - Fill character used when growing.
     * @throws std::length_error if the requested size exceeds the container
     *   capacity.
     */
    constexpr void resize(size_type new_size, char fill = '\0') {
      if (new_size > capacity()) {
        throw std::length_error("string::resize: new_size exceeds capacity");
      }

      if (new_size > m_size) {
        for (size_type i{ m_size }; i < new_size; ++i) {
          m_data[i] = fill;
        }
      }

      m_size = new_size;
      set_terminator();
    }

    /**
     * @brief Appends one character to the logical string.
     *
     * @param ch - Character to append.
     * @throws std::length_error if the string is already at capacity.
     */
    constexpr void push_back(char ch) {
      if (m_size == capacity()) {
        throw std::length_error("string::push_back: string is at capacity");
      }

      m_data[m_size] = ch;
      ++m_size;
      set_terminator();
    }

    /**
     * @brief Removes the last logical character.
     *
     * @pre The string is not empty.
     */
    constexpr void pop_back() noexcept {
      assert(!empty());
      --m_size;
      set_terminator();
    }

    /**
     * @brief Appends a counted string view.
     *
     * @param sv - String view to append.
     * @throws std::length_error if the appended view would exceed the container
     *   capacity.
     */
    constexpr void append(std::string_view sv) {
      append_range(sv.data(), sv.size());
    }

    /**
     * @brief Appends a null-terminated C string.
     *
     * @param s - Null-terminated source string.
     * @throws std::length_error if the appended string would exceed the
     *   container capacity.
     */
    constexpr void append(char const* s) {
      append_range(s, cstring_length(s));
    }

    /**
     * @brief Appends a counted character range.
     *
     * @param ptr - Pointer to the characters to append.
     * @param count - Number of characters to append.
     * @throws std::length_error if the appended character range would exceed
     *   the container capacity.
     */
    constexpr void append(char const* ptr, size_type count) {
      append_range(ptr, count);
    }

    /**
     * @brief Appends a repeated character sequence.
     *
     * @param count - Number of characters to append.
     * @param ch - Character to append repeatedly.
     * @throws std::length_error if the appended repeated characters would
     *   exceed the container capacity.
     */
    constexpr void append(size_type count, char ch) {
      check_append_fits(count, "string::append: append would exceed capacity");

      for (size_type i{ 0 }; i < count; ++i) {
        m_data[m_size + i] = ch;
      }

      m_size += count;
      set_terminator();
    }

    /**
     * @brief Appends another string container.
     *
     * @tparam M - Storage capacity of the source container.
     * @param rhs - String container to append.
     * @throws std::length_error if the appended string would exceed the
     *   container capacity.
     */
    template <std::size_t M>
    constexpr void append(string<M> const& rhs) {
      append_range(rhs.data(), rhs.size());
    }

    /**
     * @brief Replaces the logical string with a counted string view.
     *
     * @param sv - New logical character sequence.
     * @throws std::length_error if the replacement view is too large for this
     *   container.
     */
    constexpr void assign(std::string_view sv) {
      check_size_fits(sv.size(), "string::assign: source size exceeds capacity");
      assign_range(sv.data(), sv.size());
    }

    /**
     * @brief Replaces the logical string with a null-terminated C string.
     *
     * @param s - Null-terminated source string.
     * @throws std::length_error if the source string is too long for this
     *   container.
     */
    constexpr void assign(char const* s) {
      size_type const count{ cstring_length(s) };
      check_size_fits(count, "string::assign: source size exceeds capacity");
      assign_range(s, count);
    }

    /**
     * @brief Replaces the logical string with a counted character range.
     *
     * @param ptr - Pointer to the new character sequence.
     * @param count - Number of characters to assign.
     * @throws std::length_error if the replacement range is too large for this
     *   container.
     */
    constexpr void assign(char const* ptr, size_type count) {
      check_size_fits(count, "string::assign: source size exceeds capacity");
      assign_range(ptr, count);
    }

    /**
     * @brief Replaces the logical string with a repeated character sequence.
     *
     * @param count - Number of characters to assign.
     * @param ch - Character to assign repeatedly.
     * @throws std::length_error if the requested repeated string is too large
     *   for this container.
     */
    constexpr void assign(size_type count, char ch) {
      check_size_fits(count, "string::assign: source size exceeds capacity");

      for (size_type i{ 0 }; i < count; ++i) {
        m_data[i] = ch;
      }

      m_size = count;
      set_terminator();
    }

    /**
     * @brief Replaces the logical string with another string container.
     *
     * @tparam M - Storage capacity of the source container.
     * @param rhs - String container to copy from.
     * @throws std::length_error if the replacement string is too large for this
     *   container.
     */
    template <std::size_t M>
    constexpr void assign(string<M> const& rhs) {
      check_size_fits(rhs.size(), "string::assign: source size exceeds capacity");
      assign_range(rhs.data(), rhs.size());
    }

    /**
     * @brief Appends one character sequence source in-place.
     *
     * @param ch - Character to append.
     * @return string& - Reference to this container.
     */
    constexpr string& operator+=(char ch) {
      push_back(ch);
      return *this;
    }

    /**
     * @brief Appends a counted string view in-place.
     *
     * @param sv - Character sequence to append.
     * @return string& - Reference to this container.
     * @throws std::length_error if the appended view would exceed the container
     *   capacity.
     */
    constexpr string& operator+=(std::string_view sv) {
      append(sv);
      return *this;
    }

    /**
     * @brief Appends a null-terminated C string in-place.
     *
     * @param s - Null-terminated character sequence to append.
     * @return string& - Reference to this container.
     * @throws std::length_error if the appended string would exceed the
     *   container capacity.
     */
    constexpr string& operator+=(char const* s) {
      append(s);
      return *this;
    }

    /**
     * @brief Appends another string container in-place.
     *
     * @tparam M - Storage size of the source container.
     * @param rhs - String container to append.
     * @return string& - Reference to this container.
     * @throws std::length_error if the appended string would exceed the
     *   container capacity.
     */
    template <std::size_t M>
    constexpr string& operator+=(string<M> const& rhs) {
      append(rhs);
      return *this;
    }

    /**
     * @brief Lexicographically compares this string container with a counted
     *   character sequence.
     *
     * @param rhs - Character sequence to compare against.
     * @return int - Negative if `*this < rhs`, positive if `rhs < *this`, or
     *   `0` if both strings are equal.
     */
    constexpr int compare(std::string_view rhs) const noexcept {
      return view().compare(rhs);
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
    constexpr int compare(string<M> const& rhs) const noexcept {
      return compare(rhs.view());
    }

    /**
     * @brief Creates a substring copy of the logical character sequence.
     *
     * @param pos - Starting position of the substring.
     * @param count - Maximum number of characters to copy.
     * @return string - Substring copy with the same storage capacity.
     * @throws std::out_of_range if the starting position is past the logical
     *   end of the string.
     */
    constexpr string substr(size_type pos = 0u, size_type count = npos) const {
      if (pos > size()) {
        throw std::out_of_range("string::substr: pos > size()");
      }

      size_type const actual_count{ clamp_count(pos, count) };
      string result{};
      result.assign_range(m_data.data() + pos, actual_count);
      return result;
    }

    /**
     * @brief Copies logical characters into a caller-provided buffer.
     *
     * @param dest - Destination character buffer.
     * @param count - Maximum number of characters to copy.
     * @param pos - Starting position in the logical string.
     * @return size_type - Number of characters copied.
     * @throws std::out_of_range if the starting position is past the logical
     *   end of the string.
     */
    constexpr size_type copy(char* dest, size_type count, size_type pos = 0u) const {
      if (pos > size()) {
        throw std::out_of_range("string::copy: pos > size()");
      }

      size_type const actual_count{ clamp_count(pos, count) };
      for (size_type i{ 0 }; i < actual_count; ++i) {
        dest[i] = m_data[pos + i];
      }
      return actual_count;
    }

    /**
     * @brief Finds the first occurrence of a character sequence.
     *
     * @param sv - Character sequence to find.
     * @param pos - Starting position for the search.
     * @return size_type - Index of the first match, or `npos`.
     */
    constexpr size_type find(std::string_view sv, size_type pos = 0u) const noexcept {
      return view().find(sv, pos);
    }

    /**
     * @brief Finds the first occurrence of a character.
     *
     * @param ch - Character to find.
     * @param pos - Starting position for the search.
     * @return size_type - Index of the first match, or `npos`.
     */
    constexpr size_type find(char ch, size_type pos = 0u) const noexcept {
      return view().find(ch, pos);
    }

    /**
     * @brief Finds the last occurrence of a character sequence.
     *
     * @param sv - Character sequence to find.
     * @param pos - Starting position bias for the reverse search.
     * @return size_type - Index of the last match, or `npos`.
     */
    constexpr size_type rfind(std::string_view sv, size_type pos = npos) const noexcept {
      return view().rfind(sv, pos);
    }

    /**
     * @brief Finds the last occurrence of a character.
     *
     * @param ch - Character to find.
     * @param pos - Starting position bias for the reverse search.
     * @return size_type - Index of the last match, or `npos`.
     */
    constexpr size_type rfind(char ch, size_type pos = npos) const noexcept {
      return view().rfind(ch, pos);
    }

    /**
     * @brief Finds the first occurrence of any character from a set.
     *
     * @param sv - Character set to search for.
     * @param pos - Starting position for the search.
     * @return size_type - Index of the first match, or `npos`.
     */
    constexpr size_type find_first_of(std::string_view sv,
      size_type pos = 0u) const noexcept
    {
      return view().find_first_of(sv, pos);
    }

    /**
     * @brief Finds the first occurrence of a character.
     *
     * @param ch - Character to search for.
     * @param pos - Starting position for the search.
     * @return size_type - Index of the first match, or `npos`.
     */
    constexpr size_type find_first_of(char ch, size_type pos = 0u) const noexcept {
      return view().find_first_of(ch, pos);
    }

    /**
     * @brief Finds the last occurrence of any character from a set.
     *
     * @param sv - Character set to search for.
     * @param pos - Starting position bias for the reverse search.
     * @return size_type - Index of the last match, or `npos`.
     */
    constexpr size_type find_last_of(std::string_view sv,
      size_type pos = npos) const noexcept
    {
      return view().find_last_of(sv, pos);
    }

    /**
     * @brief Finds the last occurrence of a character.
     *
     * @param ch - Character to search for.
     * @param pos - Starting position bias for the reverse search.
     * @return size_type - Index of the last match, or `npos`.
     */
    constexpr size_type find_last_of(char ch, size_type pos = npos) const noexcept {
      return view().find_last_of(ch, pos);
    }

    /**
     * @brief Finds the first character not belonging to a set.
     *
     * @param sv - Character set to exclude.
     * @param pos - Starting position for the search.
     * @return size_type - Index of the first non-matching character, or `npos`.
     */
    constexpr size_type find_first_not_of(std::string_view sv,
      size_type pos = 0u) const noexcept
    {
      return view().find_first_not_of(sv, pos);
    }

    /**
     * @brief Finds the first character not equal to a specific character.
     *
     * @param ch - Character to exclude.
     * @param pos - Starting position for the search.
     * @return size_type - Index of the first non-matching character, or `npos`.
     */
    constexpr size_type find_first_not_of(char ch,
      size_type pos = 0u) const noexcept
    {
      return view().find_first_not_of(ch, pos);
    }

    /**
     * @brief Finds the last character not belonging to a set.
     *
     * @param sv - Character set to exclude.
     * @param pos - Starting position bias for the reverse search.
     * @return size_type - Index of the last non-matching character, or `npos`.
     */
    constexpr size_type find_last_not_of(std::string_view sv,
      size_type pos = npos) const noexcept
    {
      return view().find_last_not_of(sv, pos);
    }

    /**
     * @brief Finds the last character not equal to a specific character.
     *
     * @param ch - Character to exclude.
     * @param pos - Starting position bias for the reverse search.
     * @return size_type - Index of the last non-matching character, or `npos`.
     */
    constexpr size_type find_last_not_of(char ch,
      size_type pos = npos) const noexcept
    {
      return view().find_last_not_of(ch, pos);
    }

    /**
     * @brief Checks whether the logical string begins with a sequence.
     *
     * @param sv - Prefix to test.
     * @return bool - `true` if the logical string begins with `sv`.
     */
    constexpr bool starts_with(std::string_view sv) const noexcept {
      return sv.size() <= size()
        && view().substr(0u, sv.size()) == sv;
    }

    /**
     * @brief Checks whether the logical string begins with a character.
     *
     * @param ch - Prefix character to test.
     * @return bool - `true` if the logical string begins with `ch`.
     */
    constexpr bool starts_with(char ch) const noexcept {
      return !empty() && front() == ch;
    }

    /**
     * @brief Checks whether the logical string ends with a sequence.
     *
     * @param sv - Suffix to test.
     * @return bool - `true` if the logical string ends with `sv`.
     */
    constexpr bool ends_with(std::string_view sv) const noexcept {
      return sv.size() <= size()
        && view().substr(size() - sv.size(), sv.size()) == sv;
    }

    /**
     * @brief Checks whether the logical string ends with a character.
     *
     * @param ch - Suffix character to test.
     * @return bool - `true` if the logical string ends with `ch`.
     */
    constexpr bool ends_with(char ch) const noexcept {
      return !empty() && back() == ch;
    }

    /**
     * @brief Swaps the contents of this container with another of the same
     *   storage size.
     *
     * @param rhs - The container to swap with.
     */
    constexpr void swap(string& rhs) noexcept {
      size_type const old_size{ m_size };
      size_type const rhs_size{ rhs.m_size };
      size_type const limit{ old_size < rhs_size ? rhs_size : old_size };

      for (size_type i{ 0 }; i <= limit; ++i) {
        value_type const old_value{ m_data[i] };
        m_data[i] = rhs.m_data[i];
        rhs.m_data[i] = old_value;
      }

      m_size = rhs_size;
      rhs.m_size = old_size;
    }

  private:
    /**
     * @brief Gets the number of characters before the first null terminator in
     *   a C string.
     *
     * @param s - Null-terminated source string.
     * @return size_type - Number of characters before the first terminator.
     */
    static constexpr size_type cstring_length(char const* s) noexcept {
      size_type count{ 0 };

      while (s[count] != '\0') {
        ++count;
      }

      return count;
    }

    /**
     * @brief Validates that a requested logical size fits within capacity.
     *
     * @param count - Requested logical character count.
     * @param message - Exception message for overflow.
     * @throws std::length_error if the requested size is too large for this
     *   container.
     */
    constexpr void check_size_fits(size_type count, char const* message) const {
      if (count > capacity()) {
        throw std::length_error(message);
      }
    }

    /**
     * @brief Validates that an append count fits within the remaining capacity.
     *
     * @param count - Requested appended character count.
     * @param message - Exception message for overflow.
     * @throws std::length_error if the requested append would exceed the
     *   remaining capacity.
     */
    constexpr void check_append_fits(size_type count, char const* message) const {
      if (count > capacity() - m_size) {
        throw std::length_error(message);
      }
    }

    /**
     * @brief Clamps a requested substring length to the remaining logical size.
     *
     * @param pos - Starting logical position.
     * @param count - Requested character count.
     * @return size_type - Number of logical characters available from `pos`.
     */
    constexpr size_type clamp_count(size_type pos, size_type count) const noexcept {
      size_type const remaining{ size() - pos };
      return count < remaining ? count : remaining;
    }

    /**
     * @brief Replaces the logical string with a counted character range.
     *
     * The trailing null terminator is restored at the new logical end. Bytes
     * beyond the new logical end are left unspecified.
     *
     * @param src - Pointer to the replacement characters.
     * @param count - Number of logical characters to copy.
     */
    constexpr void assign_range(char const* src, size_type count) noexcept {
      for (size_type i{ 0 }; i < count; ++i) {
        m_data[i] = src[i];
      }

      m_size = count;
      set_terminator();
    }

    /**
     * @brief Appends a counted character range to the logical string.
     *
     * @param src - Pointer to the characters to append.
     * @param count - Number of logical characters to append.
     * @throws std::length_error if the appended character range would exceed
     *   the container capacity.
     */
    constexpr void append_range(char const* src, size_type count) {
      check_append_fits(count, "string::append: append would exceed capacity");
      for (size_type i{ 0 }; i < count; ++i) {
        m_data[m_size + i] = src[i];
      }

      m_size += count;
      set_terminator();
    }

    /**
     * @brief Writes the trailing null terminator at the logical end.
     */
    constexpr void set_terminator() noexcept {
      m_data[m_size] = '\0';
    }

    std::array<char, N> m_data;
    size_type m_size;
  };

  template <std::size_t N>
  string(std::array<char, N>) -> string<N>;

  template <std::size_t N>
  string(char const (&)[N]) -> string<N>;

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
  inline constexpr bool operator==(string<LhsN> const& lhs,
    string<RhsN> const& rhs) noexcept
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
  inline constexpr bool operator!=(string<LhsN> const& lhs,
    string<RhsN> const& rhs) noexcept
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
  inline constexpr bool operator<(string<LhsN> const& lhs,
    string<RhsN> const& rhs) noexcept
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
  inline constexpr bool operator<=(string<LhsN> const& lhs,
    string<RhsN> const& rhs) noexcept
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
  inline constexpr bool operator>(string<LhsN> const& lhs,
    string<RhsN> const& rhs) noexcept
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
  inline constexpr bool operator>=(string<LhsN> const& lhs,
    string<RhsN> const& rhs) noexcept
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
  inline constexpr void swap(string<N>& lhs, string<N>& rhs) noexcept {
    lhs.swap(rhs);
  }
} // namespace Constexpr

#endif // CONSTEXPR_STRING_HPP
