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
#include <stdexcept>
#include <utility>

#include <libhal/units.hpp>

namespace hal::v5 {

/**
 * @brief A dynamically allocated array with runtime size.
 *
 * dynarray<T> provides a non-resizable array container with runtime-determined
 * size. This type is meant for use as buffer for serial data or sampled data.
 * The expectation is that the memory will be used with DMA, so its access
 * operators like operator[] are bounds checked. It's similar to
 * std::array<T,N>, but with the size determined at runtime rather than compile
 * time.
 *
 * Unlike std::vector<T>, dynarray<T> does not support dynamic resizing
 * operations such as push_back() or resize().
 *
 * Example usage:
 * ```
 * std::pmr::polymorphic_allocator<hal::byte> allocator;
 * hal::v5::dynarray<int> arr(10, allocator); // Create array of 10 ints
 *
 * // Fill with values
 * for (size_t i = 0; i < arr.size(); ++i) {
 *   arr[i] = i * 2;
 * }
 *
 * // Range-based for loop
 * for (auto& value : arr) {
 *   hal::print<16>(*serial, "%d ", value);
 * }
 * ```
 *
 * @tparam T The type of elements in the array
 */
template<typename T>
class dynarray
{
public:
  static_assert(std::is_default_constructible_v<T>,
                "dynarray requires T to be default constructible");

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
  dynarray() = delete;

  /**
   * @brief Move constructor is deleted - dynarray should not be moved
   */
  dynarray(dynarray&&) = delete;

  /**
   * @brief Move assignment is deleted - dynarray should not be moved
   */
  dynarray& operator=(dynarray&&) = delete;

  /**
   * @brief Create a dynarray with specified size
   *
   * All elements are default-constructed.
   * If p_size is 0, an array of size 1 will be allocated.
   *
   * @param p_allocator The allocator to use for memory allocation
   * @param p_size The number of elements in the array
   * @throws std::bad_alloc if memory allocation fails
   */
  explicit dynarray(std::pmr::polymorphic_allocator<byte> p_allocator,
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
   * @brief Create a dynarray with specified size and initial value
   *
   * All elements are copy-constructed from the provided value.
   * If p_size is 0, an array of size 1 will be allocated.
   *
   * @param p_allocator The allocator to use for memory allocation
   * @param p_size The number of elements in the array
   * @param p_value The initial value for all elements
   * @throws std::bad_alloc if memory allocation fails
   */
  dynarray(std::pmr::polymorphic_allocator<byte> p_allocator,
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
   * @brief Create a dynarray from an initializer list
   *
   * @param p_allocator The allocator to use for memory allocation
   * @param p_init The initializer list with the values
   * @throws std::bad_alloc if memory allocation fails
   */
  dynarray(std::pmr::polymorphic_allocator<byte> p_allocator,
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
   * @brief Copy constructor is deleted - dynarray should not be copied
   */
  dynarray(dynarray const& p_other) = delete;

  /**
   * @brief Copy assignment is deleted - dynarray should not be copied
   */
  dynarray& operator=(dynarray const&) = delete;

  /**
   * @brief Destructor
   *
   * Destroys all elements and deallocates memory.
   */
  ~dynarray()
  {
    destroy_elements();
  }

  /**
   * @brief Access element at specified index with bounds checking
   *
   * @param p_index Position of the element to access
   * @return Reference to the element
   * @throws std::out_of_range if p_index >= size()
   */
  reference at(size_type p_index)
  {
    if (p_index >= m_size) {
      throw std::out_of_range("dynarray: index out of range");
    }
    return m_data[p_index];
  }

  /**
   * @brief Access element at specified index with bounds checking (const
   * version)
   *
   * @param p_index Position of the element to access
   * @return Const reference to the element
   * @throws std::out_of_range if p_index >= size()
   */
  const_reference at(size_type p_index) const
  {
    if (p_index >= m_size) {
      throw std::out_of_range("dynarray: index out of range");
    }
    return m_data[p_index];
  }

  /**
   * @brief Access element at specified index with bounds checking
   *
   * @param p_index Position of the element to access
   * @return Reference to the element
   * @throws std::out_of_range if p_index >= size()
   */
  reference operator[](size_type p_index)
  {
    return at(p_index);
  }

  /**
   * @brief Access element at specified index with bounds checking (const
   * version)
   *
   * @param p_index Position of the element to access
   * @return Const reference to the element
   * @throws std::out_of_range if p_index >= size()
   */
  const_reference operator[](size_type p_index) const
  {
    return at(p_index);
  }

  /**
   * @brief Access the first element
   *
   * @return Reference to the first element
   */
  reference front()
  {
    return m_data[0];
  }

  /**
   * @brief Access the first element (const version)
   *
   * @return Const reference to the first element
   */
  const_reference front() const
  {
    return m_data[0];
  }

  /**
   * @brief Access the last element
   *
   * @return Reference to the last element
   */
  reference back()
  {
    return m_data[m_size - 1];
  }

  /**
   * @brief Access the last element (const version)
   *
   * @return Const reference to the last element
   */
  const_reference back() const
  {
    return m_data[m_size - 1];
  }

  /**
   * @brief Direct access to the underlying array
   *
   * @return Pointer to the underlying array
   */
  pointer data() noexcept
  {
    return m_data;
  }

  /**
   * @brief Direct access to the underlying array (const version)
   *
   * @return Const pointer to the underlying array
   */
  const_pointer data() const noexcept
  {
    return m_data;
  }

  /**
   * @brief Returns an iterator to the beginning of the dynarray
   *
   * @return Iterator to the first element
   */
  iterator begin() noexcept
  {
    return m_data;
  }

  /**
   * @brief Returns a const iterator to the beginning of the dynarray
   *
   * @return Const iterator to the first element
   */
  const_iterator begin() const noexcept
  {
    return m_data;
  }

  /**
   * @brief Returns a const iterator to the beginning of the dynarray
   *
   * @return Const iterator to the first element
   */
  const_iterator cbegin() const noexcept
  {
    return m_data;
  }

  /**
   * @brief Returns an iterator to the end of the dynarray
   *
   * @return Iterator one past the last element
   */
  iterator end() noexcept
  {
    return m_data + m_size;
  }

  /**
   * @brief Returns a const iterator to the end of the dynarray
   *
   * @return Const iterator one past the last element
   */
  const_iterator end() const noexcept
  {
    return m_data + m_size;
  }

  /**
   * @brief Returns a const iterator to the end of the dynarray
   *
   * @return Const iterator one past the last element
   */
  const_iterator cend() const noexcept
  {
    return m_data + m_size;
  }

  /**
   * @brief Returns a reverse iterator to the beginning of the reversed dynarray
   *
   * @return Reverse iterator to the first element of the reversed dynarray
   */
  reverse_iterator rbegin() noexcept
  {
    return reverse_iterator(end());
  }

  /**
   * @brief Returns a const reverse iterator to the beginning of the reversed
   * dynarray
   *
   * @return Const reverse iterator to the first element of the reversed
   * dynarray
   */
  const_reverse_iterator rbegin() const noexcept
  {
    return const_reverse_iterator(end());
  }

  /**
   * @brief Returns a const reverse iterator to the beginning of the reversed
   * dynarray
   *
   * @return Const reverse iterator to the first element of the reversed
   * dynarray
   */
  const_reverse_iterator crbegin() const noexcept
  {
    return const_reverse_iterator(end());
  }

  /**
   * @brief Returns a reverse iterator to the end of the reversed dynarray
   *
   * @return Reverse iterator one past the last element of the reversed dynarray
   */
  reverse_iterator rend() noexcept
  {
    return reverse_iterator(begin());
  }

  /**
   * @brief Returns a const reverse iterator to the end of the reversed dynarray
   *
   * @return Const reverse iterator one past the last element of the reversed
   * dynarray
   */
  const_reverse_iterator rend() const noexcept
  {
    return const_reverse_iterator(begin());
  }

  /**
   * @brief Returns a const reverse iterator to the end of the reversed dynarray
   *
   * @return Const reverse iterator one past the last element of the reversed
   * dynarray
   */
  const_reverse_iterator crend() const noexcept
  {
    return const_reverse_iterator(begin());
  }

  /**
   * @brief Checks if the dynarray is empty (always false since minimum size is
   * 1)
   *
   * @return Always false since dynarray always allocates at least one element
   */
  [[nodiscard]] bool empty() const noexcept
  {
    return false;
  }

  /**
   * @brief Returns the number of elements in the dynarray
   *
   * @return The number of elements
   */
  [[nodiscard]] size_type size() const noexcept
  {
    return m_size;
  }

  /**
   * @brief Returns the size of the dynarray in bytes
   *
   * @return The size in bytes
   */
  [[nodiscard]] usize size_bytes() const noexcept
  {
    return m_size * sizeof(T);
  }

  /**
   * @brief Fills the dynarray with the specified value
   *
   * @param p_value The value to fill the dynarray with
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
      if constexpr (!std::is_trivially_destructible_v<T>) {
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
  std::pmr::polymorphic_allocator<byte> m_allocator;  ///< Allocator
};

/**
 * @brief Equality operator for dynarray
 *
 * Compares if two dynarray instances have the same size and elements.
 *
 * @tparam T The type of elements in the dynarray
 * @param p_lhs First dynarray to compare
 * @param p_rhs Second dynarray to compare
 * @return true if both have the same size and elements, false otherwise
 */
template<typename T>
bool operator==(dynarray<T> const& p_lhs, dynarray<T> const& p_rhs)
{
  if (p_lhs.size() != p_rhs.size()) {
    return false;
  }

  return std::equal(p_lhs.begin(), p_lhs.end(), p_rhs.begin());
}

/**
 * @brief Inequality operator for dynarray
 *
 * @tparam T The type of elements in the dynarray
 * @param p_lhs First dynarray to compare
 * @param p_rhs Second dynarray to compare
 * @return true if they have different sizes or elements, false otherwise
 */
template<typename T>
bool operator!=(dynarray<T> const& p_lhs, dynarray<T> const& p_rhs)
{
  return !(p_lhs == p_rhs);
}

/**
 * @brief Creates a dynarray with the given size and fills it with
 * value-constructed elements
 *
 * @tparam T The type of elements in the dynarray
 * @param p_allocator The allocator to use
 * @param p_size The size of the dynarray
 * @return A new dynarray of the specified size with value-constructed elements
 */
template<typename T>
dynarray<T> make_dynarray(std::pmr::polymorphic_allocator<byte> p_allocator,
                          typename dynarray<T>::size_type p_size)
{
  return dynarray<T>(p_allocator, p_size);
}

/**
 * @brief Creates a dynarray with the given size and fills it with copies of the
 * provided value
 *
 * @tparam T The type of elements in the dynarray
 * @param p_allocator The allocator to use
 * @param p_size The size of the dynarray
 * @param p_value The value to fill the dynarray with
 * @return A new dynarray of the specified size filled with the given value
 */
template<typename T>
dynarray<T> make_dynarray(std::pmr::polymorphic_allocator<byte> p_allocator,
                          typename dynarray<T>::size_type p_size,
                          T const& p_value)
{
  return dynarray<T>(p_allocator, p_size, p_value);
}
}  // namespace hal::v5
