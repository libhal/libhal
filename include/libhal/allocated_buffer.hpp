// Copyright 2024 - 2025 Khalil Estell and the libhal contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <iterator>
#include <memory_resource>
#include <type_traits>

#include "error.hpp"
#include "units.hpp"

namespace hal::v5 {

/**
 * @brief A dynamically allocated buffer with runtime size.
 *
 * allocated_buffer<T> provides a non-resizable buffer container with
 * runtime-determined size. This type is meant for use as buffer for serial data
 * or sampled data. The expectation is that the memory will be used with DMA, so
 * its access operators like operator[] are bounds checked. It's similar to
 * std::array<T,N>, but with the size determined at runtime rather than compile
 * time.
 *
 * Unlike std::vector<T>, allocated_buffer<T> does not support dynamic resizing
 * operations such as push_back() or resize().
 *
 * Example usage:
 * ```
 * std::pmr::polymorphic_allocator<hal::byte> allocator;
 * hal::v5::allocated_buffer<int> buffer(allocator, 10); // Create buffer of 10
 * ints
 *
 * // Fill with values
 * for (size_t i = 0; i < buffer.size(); ++i) {
 *   buffer[i] = i * 2;
 * }
 *
 * // Range-based for loop
 * for (auto& value : buffer) {
 *   hal::print<16>(*serial, "%d ", value);
 * }
 * ```
 *
 * @tparam T The type of elements in the buffer
 */
template<typename T>
class allocated_buffer
{
public:
  static_assert(std::is_default_constructible_v<T>,
                "allocated_buffer requires T to be default constructible");

  // Standard container type definitions
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = value_type*;
  using const_pointer = value_type const*;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  /**
   * @brief Default constructor is deleted - size must be provided
   */
  allocated_buffer() = delete;

  /**
   * @brief Move constructor is deleted - allocated_buffer should not be moved
   */
  allocated_buffer(allocated_buffer&&) = delete;

  /**
   * @brief Move assignment is deleted - allocated_buffer should not be moved
   */
  allocated_buffer& operator=(allocated_buffer&&) = delete;

  /**
   * @brief Copy constructor is deleted - allocated_buffer should not be copied
   */
  allocated_buffer(allocated_buffer const& p_other) = delete;

  /**
   * @brief Copy assignment is deleted - allocated_buffer should not be copied
   */
  allocated_buffer& operator=(allocated_buffer const&) = delete;

  /**
   * @brief Create a allocated_buffer with specified size
   *
   * All elements are default-constructed.
   * If p_size is 0, an array of size 1 will be allocated.
   *
   * @param p_allocator The allocator to use for memory allocation
   * @param p_size The number of elements in the array
   * @throws std::bad_alloc if memory allocation fails
   */
  explicit allocated_buffer(std::pmr::polymorphic_allocator<> p_allocator,
                            size_type p_size)
    : m_size(std::max<size_type>(1, p_size))
    , m_allocator(p_allocator)
  {
    // Allocate memory for the array
    m_data = static_cast<pointer>(
      m_allocator.allocate_bytes(sizeof(T) * m_size, alignof(T)));

    // Default construct each element
    if constexpr (std::is_trivially_default_constructible_v<T>) {
      // For trivial types, no need to call constructors explicitly
    } else {
      for (size_type i = 0; i < m_size; ++i) {
        new (&m_data[i]) T();
      }
    }
  }

  /**
   * @brief Create a allocated_buffer with specified size and initial value
   *
   * All elements are copy-constructed from the provided value.
   * If p_size is 0, an array of size 1 will be allocated.
   *
   * @param p_allocator The allocator to use for memory allocation
   * @param p_size The number of elements in the array
   * @param p_value The initial value for all elements
   * @throws std::bad_alloc if memory allocation fails
   */
  allocated_buffer(std::pmr::polymorphic_allocator<> p_allocator,
                   size_type p_size,
                   T const& p_value)
    : m_size(std::max<size_type>(1, p_size))
    , m_allocator(p_allocator)
  {
    // Allocate memory for the array
    m_data = static_cast<pointer>(
      m_allocator.allocate_bytes(sizeof(T) * m_size, alignof(T)));

    // Copy construct each element
    for (size_type i = 0; i < m_size; ++i) {
      new (&m_data[i]) T(p_value);
    }
  }

  /**
   * @brief Create a allocated_buffer from an initializer list
   *
   * @param p_allocator The allocator to use for memory allocation
   * @param p_init The initializer list with the values
   * @throws std::bad_alloc if memory allocation fails
   */
  allocated_buffer(std::pmr::polymorphic_allocator<> p_allocator,
                   std::initializer_list<T> p_init)
    : m_size(std::max<size_type>(1, p_init.size()))
    , m_allocator(p_allocator)
  {
    // Allocate memory for the array
    m_data = static_cast<pointer>(
      m_allocator.allocate_bytes(sizeof(T) * m_size, alignof(T)));

    // Copy construct each element from the initializer list
    if (p_init.size() > 0) {
      size_type i = 0;
      for (auto const& item : p_init) {
        new (&m_data[i++]) T(item);
      }
    } else {
      // Initialize the single element with default construction
      if constexpr (not std::is_trivially_default_constructible_v<T>) {
        new (&m_data[0]) T();
      }
    }
  }

  /**
   * @brief Destructor
   *
   * Destroys all elements and deallocates memory.
   */
  ~allocated_buffer()
  {
    destroy_elements();
  }

  /**
   * @brief Access element at specified index with bounds checking
   *
   * @param p_index Position of the element to access
   * @return Reference to the element
   * @throws hal::out_of_range if p_index >= size()
   */
  [[nodiscard]] reference at(size_type p_index)
  {
    if (p_index >= m_size) {
      hal::safe_throw(hal::out_of_range{
        this, { .m_index = p_index, .m_capacity = m_size } });
    }
    return m_data[p_index];
  }

  /**
   * @brief Access element at specified index with bounds checking (const
   * version)
   *
   * @param p_index Position of the element to access
   * @return Const reference to the element
   * @throws hal::out_of_range if p_index >= size()
   */
  [[nodiscard]] const_reference at(size_type p_index) const
  {
    if (p_index >= m_size) {
      hal::safe_throw(hal::out_of_range{
        this, { .m_index = p_index, .m_capacity = m_size } });
    }
    return m_data[p_index];
  }

  /**
   * @brief Access element at specified index with bounds checking
   *
   * @param p_index Position of the element to access
   * @return Reference to the element
   * @throws hal::out_of_range if p_index >= size()
   */
  [[nodiscard]] reference operator[](size_type p_index)
  {
    return at(p_index);
  }

  /**
   * @brief Access element at specified index with bounds checking (const
   * version)
   *
   * @param p_index Position of the element to access
   * @return Const reference to the element
   * @throws hal::out_of_range if p_index >= size()
   */
  [[nodiscard]] const_reference operator[](size_type p_index) const
  {
    return at(p_index);
  }

  /**
   * @brief Access the first element
   *
   * @return Reference to the first element
   */
  [[nodiscard]] reference front()
  {
    return m_data[0];
  }

  /**
   * @brief Access the first element (const version)
   *
   * @return Const reference to the first element
   */
  [[nodiscard]] const_reference front() const
  {
    return m_data[0];
  }

  /**
   * @brief Access the last element
   *
   * @return Reference to the last element
   */
  [[nodiscard]] reference back()
  {
    return m_data[m_size - 1];
  }

  /**
   * @brief Access the last element (const version)
   *
   * @return Const reference to the last element
   */
  [[nodiscard]] const_reference back() const
  {
    return m_data[m_size - 1];
  }

  /**
   * @brief Direct access to the underlying array
   *
   * @return Pointer to the underlying array
   */
  [[nodiscard]] pointer data() noexcept
  {
    return m_data;
  }

  /**
   * @brief Direct access to the underlying array (const version)
   *
   * @return Const pointer to the underlying array
   */
  [[nodiscard]] const_pointer data() const noexcept
  {
    return m_data;
  }

  /**
   * @brief Returns an iterator to the beginning of the allocated_buffer
   *
   * @return Iterator to the first element
   */
  [[nodiscard]] iterator begin() noexcept
  {
    return m_data;
  }

  /**
   * @brief Returns a const iterator to the beginning of the allocated_buffer
   *
   * @return Const iterator to the first element
   */
  [[nodiscard]] const_iterator begin() const noexcept
  {
    return m_data;
  }

  /**
   * @brief Returns a const iterator to the beginning of the allocated_buffer
   *
   * @return Const iterator to the first element
   */
  [[nodiscard]] const_iterator cbegin() const noexcept
  {
    return m_data;
  }

  /**
   * @brief Returns an iterator to the end of the allocated_buffer
   *
   * @return Iterator one past the last element
   */
  [[nodiscard]] iterator end() noexcept
  {
    return m_data + m_size;
  }

  /**
   * @brief Returns a const iterator to the end of the allocated_buffer
   *
   * @return Const iterator one past the last element
   */
  [[nodiscard]] const_iterator end() const noexcept
  {
    return m_data + m_size;
  }

  /**
   * @brief Returns a const iterator to the end of the allocated_buffer
   *
   * @return Const iterator one past the last element
   */
  [[nodiscard]] const_iterator cend() const noexcept
  {
    return m_data + m_size;
  }

  /**
   * @brief Returns a reverse iterator to the beginning of the reversed
   * allocated_buffer
   *
   * @return Reverse iterator to the first element of the reversed
   * allocated_buffer
   */
  [[nodiscard]] reverse_iterator rbegin() noexcept
  {
    return reverse_iterator(end());
  }

  /**
   * @brief Returns a const reverse iterator to the beginning of the reversed
   * allocated_buffer
   *
   * @return Const reverse iterator to the first element of the reversed
   * allocated_buffer
   */
  [[nodiscard]] const_reverse_iterator rbegin() const noexcept
  {
    return const_reverse_iterator(end());
  }

  /**
   * @brief Returns a const reverse iterator to the beginning of the reversed
   * allocated_buffer
   *
   * @return Const reverse iterator to the first element of the reversed
   * allocated_buffer
   */
  [[nodiscard]] const_reverse_iterator crbegin() const noexcept
  {
    return const_reverse_iterator(end());
  }

  /**
   * @brief Returns a reverse iterator to the end of the reversed
   * allocated_buffer
   *
   * @return Reverse iterator one past the last element of the reversed
   * allocated_buffer
   */
  [[nodiscard]] reverse_iterator rend() noexcept
  {
    return reverse_iterator(begin());
  }

  /**
   * @brief Returns a const reverse iterator to the end of the reversed
   * allocated_buffer
   *
   * @return Const reverse iterator one past the last element of the reversed
   * allocated_buffer
   */
  [[nodiscard]] const_reverse_iterator rend() const noexcept
  {
    return const_reverse_iterator(begin());
  }

  /**
   * @brief Returns a const reverse iterator to the end of the reversed
   * allocated_buffer
   *
   * @return Const reverse iterator one past the last element of the reversed
   * allocated_buffer
   */
  [[nodiscard]] const_reverse_iterator crend() const noexcept
  {
    return const_reverse_iterator(begin());
  }

  /**
   * @brief Checks if the allocated_buffer is empty (always false since minimum
   * size is 1)
   *
   * @return Always false since allocated_buffer always allocates at least one
   * element
   */
  [[nodiscard]] bool empty() const noexcept
  {
    return false;
  }

  /**
   * @brief Returns the number of elements in the allocated_buffer
   *
   * @return The number of elements
   */
  [[nodiscard]] size_type size() const noexcept
  {
    return m_size;
  }

  /**
   * @brief Returns the size of the allocated_buffer in bytes
   *
   * @return The size in bytes
   */
  [[nodiscard]] usize size_bytes() const noexcept
  {
    return m_size * sizeof(T);
  }

  /**
   * @brief Fills the allocated_buffer with the specified value
   *
   * @param p_value The value to fill the allocated_buffer with
   */
  void fill(T const& p_value)
  {
    std::fill(begin(), end(), p_value);
  }

private:
  /**
   * @brief Helper method to destroy all elements and deallocate memory
   */
  void destroy_elements() noexcept
  {
    if (m_data) {
      // Call destructor for each element if not trivially destructible
      if constexpr (not std::is_trivially_destructible_v<T>) {
        for (size_type i = 0; i < m_size; ++i) {
          m_data[i].~T();
        }
      }

      // Deallocate memory
      m_allocator.deallocate_bytes(m_data, sizeof(T) * m_size, alignof(T));
      m_data = nullptr;
    }
  }

  size_type m_size = 0;      ///< Number of elements
  pointer m_data = nullptr;  ///< Pointer to allocated memory
  std::pmr::polymorphic_allocator<> m_allocator;  ///< Allocator
};

/**
 * @brief Equality operator for allocated_buffer
 *
 * Compares if two allocated_buffer instances have the same size and elements.
 *
 * @tparam T The type of elements in the allocated_buffer
 * @param p_lhs First allocated_buffer to compare
 * @param p_rhs Second allocated_buffer to compare
 * @return true if both have the same size and elements, false otherwise
 */
template<typename T>
[[nodiscard]] bool operator==(allocated_buffer<T> const& p_lhs,
                              allocated_buffer<T> const& p_rhs)
{
  if (p_lhs.size() != p_rhs.size()) {
    return false;
  }

  return std::equal(p_lhs.begin(), p_lhs.end(), p_rhs.begin());
}

/**
 * @brief Inequality operator for allocated_buffer
 *
 * @tparam T The type of elements in the allocated_buffer
 * @param p_lhs First allocated_buffer to compare
 * @param p_rhs Second allocated_buffer to compare
 * @return true if they have different sizes or elements, false otherwise
 */
template<typename T>
[[nodiscard]] bool operator!=(allocated_buffer<T> const& p_lhs,
                              allocated_buffer<T> const& p_rhs)
{
  return !(p_lhs == p_rhs);
}

/**
 * @brief Creates a allocated_buffer with the given size and fills it with
 * value-constructed elements
 *
 * @tparam T The type of elements in the allocated_buffer
 * @param p_allocator The allocator to use
 * @param p_size The size of the allocated_buffer
 * @return A new allocated_buffer of the specified size with value-constructed
 * elements
 */
template<typename T>
[[nodiscard]] allocated_buffer<T> make_allocated_buffer(
  std::pmr::polymorphic_allocator<> p_allocator,
  typename allocated_buffer<T>::size_type p_size)
{
  return allocated_buffer<T>(p_allocator, p_size);
}

/**
 * @brief Creates a allocated_buffer with the given size and fills it with
 * copies of the provided value
 *
 * @tparam T The type of elements in the allocated_buffer
 * @param p_allocator The allocator to use
 * @param p_size The size of the allocated_buffer
 * @param p_value The value to fill the allocated_buffer with
 * @return A new allocated_buffer of the specified size filled with the given
 * value
 */
template<typename T>
[[nodiscard]] allocated_buffer<T> make_allocated_buffer(
  std::pmr::polymorphic_allocator<> p_allocator,
  typename allocated_buffer<T>::size_type p_size,
  T const& p_value)
{
  return allocated_buffer<T>(p_allocator, p_size, p_value);
}
}  // namespace hal::v5
