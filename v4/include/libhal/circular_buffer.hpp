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

#include <memory_resource>
#include <type_traits>
#include <utility>

#include <libhal/units.hpp>

namespace hal::v5 {

/**
 * @brief A circular buffer with runtime size.
 *
 * circular_buffer<T> provides a non-resizable circular buffer container with
 * runtime-determined size. The buffer wraps around when reaching the end,
 * making it useful for streaming data or implementing producer-consumer
 * patterns. This container is designed to be observed by multiple read-only
 * clients, each with their own read index.
 *
 * Unlike std::vector<T>, circular_buffer<T> does not support dynamic resizing
 * operations. Instead, it provides a push() method to add elements, which
 * will overwrite the oldest element when the buffer is full.
 *
 * Example usage:
 * ```
 * std::pmr::polymorphic_allocator<hal::byte> allocator;
 * hal::v5::circular_buffer<int> buffer(allocator, 10); // Create buffer of 10
 * ints
 *
 * // Push elements into the buffer
 * for (int i = 0; i < 15; ++i) {
 *   buffer.push(i * 2);
 * }
 *
 * // Access elements (which will be values 10, 12, 14, 16, 18, 20, 22, 24, 26,
 * 28) for (size_t i = 0; i < buffer.capacity(); ++i) { hal::print<16>(*serial,
 * "%d ", buffer[i]);
 * }
 * ```
 *
 * @tparam T The type of elements in the buffer
 */
template<typename T>
class circular_buffer
{
public:
  static_assert(std::is_default_constructible_v<T>,
                "circular_buffer requires T to be default constructible");

  // Standard container type definitions
  using value_type = T;
  using size_type = usize;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = value_type*;
  using const_pointer = value_type const*;

  /**
   * @brief Default constructor is deleted - size must be provided
   */
  circular_buffer() = delete;

  /**
   * @brief Move constructor is deleted - circular_buffer should not be moved
   */
  circular_buffer(circular_buffer&&) = delete;

  /**
   * @brief Move assignment is deleted - circular_buffer should not be moved
   */
  circular_buffer& operator=(circular_buffer&&) = delete;

  /**
   * @brief Copy constructor is deleted - circular_buffer should not be copied
   */
  circular_buffer(circular_buffer const& p_other) = delete;

  /**
   * @brief Copy assignment is deleted - circular_buffer should not be copied
   */
  circular_buffer& operator=(circular_buffer const&) = delete;

  /**
   * @brief Create a circular_buffer with specified capacity
   *
   * All elements are default-constructed.
   * If p_capacity is 0, a buffer of capacity 1 will be allocated.
   *
   * @param p_allocator The allocator to use for memory allocation
   * @param p_capacity The capacity of the buffer
   * @throws std::bad_alloc if memory allocation fails
   */
  explicit circular_buffer(std::pmr::polymorphic_allocator<byte> p_allocator,
                           size_type p_capacity)
    : m_allocator(p_allocator)
    , m_capacity(std::max<size_type>(1, p_capacity))
  {
    // Allocate memory for the array using allocate_object
    m_data = m_allocator.allocate_object<T>(m_capacity);

    for (size_type i = 0; i < m_capacity; ++i) {
      new (&m_data[i]) T();
    }
  }

  /**
   * @brief Create a circular_buffer with specified capacity and initial value
   *
   * All elements are copy-constructed from the provided value.
   * If p_capacity is 0, a buffer of capacity 1 will be allocated.
   *
   * @param p_allocator The allocator to use for memory allocation
   * @param p_capacity The capacity of the buffer
   * @param p_value The initial value for all elements
   * @throws std::bad_alloc if memory allocation fails
   */
  circular_buffer(std::pmr::polymorphic_allocator<byte> p_allocator,
                  size_type p_capacity,
                  T const& p_value)
    : m_allocator(p_allocator)
    , m_capacity(std::max<size_type>(1, p_capacity))
  {
    // Allocate memory for the array using allocate_object
    m_data = m_allocator.allocate_object<T>(m_capacity);

    // Copy construct each element
    for (size_type i = 0; i < m_capacity; ++i) {
      new (&m_data[i]) T(p_value);
    }
  }

  /**
   * @brief Create a circular_buffer from an initializer list
   *
   * @param p_allocator The allocator to use for memory allocation
   * @param p_init The initializer list with the values
   * @throws std::bad_alloc if memory allocation fails
   */
  circular_buffer(std::pmr::polymorphic_allocator<byte> p_allocator,
                  std::initializer_list<T> p_init)
    : m_allocator(p_allocator)
    , m_capacity(std::max<size_type>(1, p_init.size()))
  {
    // Allocate memory for the array using allocate_object
    m_data = m_allocator.allocate_object<T>(m_capacity);

    // Copy construct each element from the initializer list
    if (p_init.size() > 0) {
      size_type i = 0;
      for (auto const& item : p_init) {
        new (&m_data[i++]) T(item);
      }
      m_write_index = p_init.size();
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
  ~circular_buffer()
  {
    if (m_data) {
      // Call destructor for each element if not trivially destructible
      if constexpr (!std::is_trivially_destructible_v<T>) {
        for (size_type i = 0; i < m_capacity; ++i) {
          m_data[i].~T();
        }
      }

      // Deallocate memory using deallocate_object
      m_allocator.deallocate_object(m_data, m_capacity);
    }
  }

  /**
   * @brief Adds a new element to the circular buffer
   *
   * This overwrites the oldest element if the buffer is full.
   *
   * @param p_value The value to add to the buffer
   */
  void push(T const& p_value)
  {
    // Replace the element at the current write index
    m_data[m_write_index] = p_value;

    // Advance the write index, wrapping around if necessary
    m_write_index = (m_write_index + 1) % m_capacity;
  }

  /**
   * @brief Push a new element to the circular buffer with a temporary
   *
   * This overwrites the oldest element if the buffer is full.
   *
   * @param p_value The value to move into the buffer
   */
  void push(T&& p_value)
  {
    // Replace the element at the current write index
    m_data[m_write_index] = std::move(p_value);

    // Advance the write index, wrapping around if necessary
    m_write_index = (m_write_index + 1) % m_capacity;
  }

  /**
   * @brief Emplace a new element in the circular buffer
   *
   * Constructs an element in-place at the current write position and advances
   * the write position.
   *
   * @tparam Args Types of the arguments to forward to the constructor
   * @param args Arguments to forward to the constructor
   * @return Reference to the newly constructed element
   */
  template<typename... Args>
  reference emplace(Args&&... args)
  {
    // Destroy the object at the current write index
    m_data[m_write_index].~T();

    // Construct a new object in its place
    new (&m_data[m_write_index]) T(std::forward<Args>(args)...);

    // Get a reference to the newly constructed object
    reference result = m_data[m_write_index];

    // Advance the write index, wrapping around if necessary
    m_write_index = (m_write_index + 1) % m_capacity;

    return result;
  }

  /**
   * @brief Access element at the specified relative index with circular
   * wrapping
   *
   * This method provides access to elements relative to the current write
   * position. The index will be wrapped using modulo operation to ensure it's
   * always valid.
   *
   * @param p_index Position of the element to access (will be wrapped using
   * modulo)
   * @return Reference to the element
   */
  [[nodiscard]] reference operator[](size_type p_index)
  {
    size_type const actual_index = p_index % m_capacity;
    return m_data[actual_index];
  }

  /**
   * @brief Access element at the specified relative index with circular
   * wrapping (const version)
   *
   * @param p_index Position of the element to access (will be wrapped using
   * modulo)
   * @return Const reference to the element
   */
  [[nodiscard]] const_reference operator[](size_type p_index) const
  {
    size_type const actual_index = p_index % m_capacity;
    return m_data[actual_index];
  }

  /**
   * @brief Direct access to the underlying array
   *
   * Note that this provides access to the raw array, which may not reflect
   * the logical ordering of elements in the circular buffer.
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
   * @brief Returns the capacity of the circular_buffer
   *
   * @return The capacity of the buffer
   */
  [[nodiscard]] size_type capacity() const noexcept
  {
    return m_capacity;
  }

  /**
   * @brief Returns the size of the circular_buffer in bytes
   *
   * @return The size in bytes
   */
  [[nodiscard]] usize size_bytes() const noexcept
  {
    return m_capacity * sizeof(T);
  }

  /**
   * @brief Get the current write position in the buffer
   *
   * This can be useful for clients that need to track which elements are new.
   *
   * @return The current write index
   */
  [[nodiscard]] size_type write_index() const noexcept
  {
    return m_write_index;
  }

private:
  std::pmr::polymorphic_allocator<byte> m_allocator;  ///< Allocator
  pointer m_data = nullptr;     ///< Pointer to allocated memory
  size_type m_capacity = 0;     ///< Capacity of the buffer
  size_type m_write_index = 0;  ///< Current write position
};

/**
 * @brief Creates a circular_buffer with the given capacity and fills it with
 * value-constructed elements
 *
 * @tparam T The type of elements in the circular_buffer
 * @param p_allocator The allocator to use
 * @param p_capacity The capacity of the circular_buffer
 * @return A new circular_buffer of the specified capacity with
 * value-constructed elements
 */
template<typename T>
[[nodiscard]] circular_buffer<T> make_circular_buffer(
  std::pmr::polymorphic_allocator<byte> p_allocator,
  typename circular_buffer<T>::size_type p_capacity)
{
  return circular_buffer<T>(p_allocator, p_capacity);
}

/**
 * @brief Creates a circular_buffer with the given capacity and fills it with
 * copies of the provided value
 *
 * @tparam T The type of elements in the circular_buffer
 * @param p_allocator The allocator to use
 * @param p_capacity The capacity of the circular_buffer
 * @param p_value The value to fill the circular_buffer with
 * @return A new circular_buffer of the specified capacity filled with the given
 * value
 */
template<typename T>
[[nodiscard]] circular_buffer<T> make_circular_buffer(
  std::pmr::polymorphic_allocator<byte> p_allocator,
  typename circular_buffer<T>::size_type p_capacity,
  T const& p_value)
{
  return circular_buffer<T>(p_allocator, p_capacity, p_value);
}
}  // namespace hal::v5

// Bring hal::v5 symbols into hal namespace
namespace hal {
using hal::v5::circular_buffer;
using hal::v5::make_circular_buffer;
}  // namespace hal
