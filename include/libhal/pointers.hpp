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

#include <atomic>
#include <memory_resource>
#include <type_traits>
#include <utility>

#include <libhal/error.hpp>
#include <libhal/units.hpp>

namespace hal::v5 {
// Forward declarations
template<typename T>
class strong_ptr;

template<typename T>
class weak_ptr;

namespace detail {
/**
 * @brief Control block for reference counting - type erased.
 *
 * This structure manages the lifetime of reference-counted objects by tracking
 * strong and weak references. It also stores the memory allocator and destroy
 * function used to clean up the object when no more references exist.
 */
struct ref_info
{
  /**
   * @brief Destroy function for ref counted object
   *
   * Always returns the total size of the object wrapped in a ref count object.
   * Thus the size should normally be greater than sizeof(T). Expect sizeof(T) +
   * sizeof(ref_info) and anything else the ref count object may contain.
   *
   * If a nullptr is passed to the destroy function, it returns the object size
   * but does not destroy the object.
   */
  using destroy_fn_t = usize(void const*);

  /// Initialize to 1 since creation implies a reference
  std::pmr::polymorphic_allocator<> allocator;
  destroy_fn_t* destroy;
  std::atomic<i32> strong_count = 1;
  std::atomic<i32> weak_count = 0;
};

/**
 * @brief Add strong reference to control block
 *
 * @param p_info Pointer to the control block
 */
inline void ptr_add_ref(ref_info* p_info)
{
  p_info->strong_count.fetch_add(1, std::memory_order_relaxed);
}

/**
 * @brief Release strong reference from control block
 *
 * If this was the last strong reference, the pointed-to object will be
 * destroyed. If there are no remaining weak references, the memory
 * will also be deallocated.
 *
 * @param p_info Pointer to the control block
 */
inline void ptr_release(ref_info* p_info)
{
  if (p_info->strong_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    // No more strong references, destroy the object but keep control block
    // if there are weak references

    // Call the destroy function which will:
    // 1. Call the destructor of the object
    // 2. Return the size of the rc for deallocation when needed
    usize const object_size = p_info->destroy(p_info);

    // If there are no weak references, deallocate memory
    if (p_info->weak_count.load(std::memory_order_acquire) == 0) {
      // Save allocator for deallocating
      auto alloc = p_info->allocator;

      // Deallocate memory
      alloc.deallocate_bytes(p_info, object_size);
    }
  }
}

/**
 * @brief Add weak reference to control block
 *
 * @param p_info Pointer to the control block
 */
inline void ptr_add_weak(ref_info* p_info)
{
  p_info->weak_count.fetch_add(1, std::memory_order_relaxed);
}

/**
 * @brief Release weak reference from control block
 *
 * If this was the last weak reference and there are no remaining
 * strong references, the memory will be deallocated.
 *
 * @param p_info Pointer to the control block
 */
inline void ptr_release_weak(ref_info* p_info)
{
  if (p_info->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 0) {
    // No more weak references, check if we can deallocate
    if (p_info->strong_count.load(std::memory_order_acquire) == 0) {
      // No strong references either, get the size from the destroy function
      auto const object_size = p_info->destroy(nullptr);

      // Save allocator for deallocating
      auto alloc = p_info->allocator;

      // Deallocate memory
      alloc.deallocate_bytes(p_info, object_size);
    }
  }
}

/**
 * @brief A wrapper that contains both the ref_info and the actual object
 *
 * This structure keeps the control block and managed object together in memory.
 *
 * @tparam T The type of the managed object
 */
template<typename T>
struct rc
{
  ref_info m_info;
  T m_object;

  // Constructor that forwards arguments to the object
  template<typename... Args>
  rc(std::pmr::polymorphic_allocator<> p_alloc, Args&&... args)
    : m_info{ .allocator = p_alloc, .destroy = &destroy_function }
    , m_object(std::forward<Args>(args)...)
  {
  }

  // Static function to destroy an instance and return its size
  static usize destroy_function(void const* p_object)
  {
    if (p_object != nullptr) {
      auto const* obj = static_cast<rc<T> const*>(p_object);
      // Call destructor for the object only
      obj->m_object.~T();
    }
    // Return size for future deallocation
    return sizeof(rc<T>);
  }
};
// Check if a type is an array or std::array
template<typename T>
struct is_array_like : std::false_type
{};

// NOLINTBEGIN(modernize-avoid-c-arrays)
// Specialization for C-style arrays
template<typename T, std::size_t N>
struct is_array_like<T[N]> : std::true_type
{};
// NOLINTEND(modernize-avoid-c-arrays)

// Specialization for std::array
template<typename T, std::size_t N>
struct is_array_like<std::array<T, N>> : std::true_type
{};

// Helper variable template
template<typename T>
inline constexpr bool is_array_like_v = is_array_like<T>::value;

// Concept for array-like types
template<typename T>
concept array_like = is_array_like_v<T>;

// Concept for non-array-like types
template<typename T>
concept non_array_like = !array_like<T>;
}  // namespace detail

/**
 * @brief A non-nullable strong reference counted pointer
 *
 * strong_ptr is a smart pointer that maintains shared ownership of an object
 * through a reference count. It is similar to std::shared_ptr but with these
 * key differences:
 *
 * 1. Cannot be null - must always point to a valid object
 * 2. Can only be created via make_strong_ptr, not from raw pointers
 * 3. More memory efficient implementation
 *
 * Use strong_ptr when you need shared ownership semantics and can guarantee
 * the pointer will never be null. For nullable references, use optional_ptr.
 *
 * Example usage:
 *
 * ```C++
 * // Create a strong_ptr to an object
 * auto ptr = hal::make_strong_ptr<my_i2c_driver>(allocator, arg1, arg2);
 *
 * // Use the object using dereference (*) operator
 * (*ptr).configure({ .clock_rate = 250_kHz });
 *
 * // OR use the object using arrow (->) operator
 * ptr->configure({ .clock_rate = 250_kHz });
 *
 * // Share ownership with another driver or object
 * auto my_imu = hal::make_strong_ptr<my_driver>(allocator, ptr, 0x13);
 * ```
 *
 * @tparam T The type of the managed object
 */
template<typename T>
class strong_ptr
{
public:
  using element_type = T;

  /// Delete default constructor - strong_ptr must always be valid
  strong_ptr() = delete;

  /// Delete nullptr constructor - strong_ptr must always be valid
  strong_ptr(std::nullptr_t) = delete;

  /**
   * @brief Copy constructor
   *
   * Creates a new strong reference to the same object.
   *
   * @param p_other The strong_ptr to copy from
   */
  strong_ptr(strong_ptr const& p_other) noexcept
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(p_other.m_ptr)
  {
    ptr_add_ref(m_ctrl);
  }

  /**
   * @brief Converting copy constructor
   *
   * Creates a new strong reference to the same object, converting from
   * a derived type U to base type T.
   *
   * @tparam U A type convertible to T
   * @param p_other The strong_ptr to copy from
   */
  template<typename U>
  strong_ptr(strong_ptr<U> const& p_other) noexcept
    requires(std::is_convertible_v<U*, T*>)
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(p_other.m_ptr)
  {
    ptr_add_ref(m_ctrl);
  }

  /**
   * @brief Move constructor that intentionally behaves like a copy constructor
   * for safety
   *
   * This move constructor deliberately performs a full copy operation rather
   * than transferring ownership. This is a safety feature to prevent potential
   * undefined behavior that could occur if code accidentally accessed a
   * moved-from strong_ptr.
   *
   * After this operation, both the source and destination objects remain in
   * valid states, and the reference count is incremented by 1. This ensures
   * that even if code incorrectly continues to use the source object after a
   * move, no undefined behavior will occur.
   *
   * @param p_other The strong_ptr to "move" from (actually copied for safety)
   */
  strong_ptr(strong_ptr&& p_other) noexcept
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(p_other.m_ptr)
  {
    ptr_add_ref(m_ctrl);
  }

  /**
   * @brief Move assignment operator that behaves like a copy assignment for
   * safety
   *
   * This move assignment operator deliberately performs a full copy operation
   * rather than transferring ownership. This is a safety feature to prevent
   * potential undefined behavior that could occur if code accidentally accessed
   * a moved-from strong_ptr.
   *
   * After this operation, both the source and destination objects remain in
   * valid states, and the reference count is incremented by 1. This ensures
   * that even if code incorrectly continues to use the source object after a
   * move, no undefined behavior will occur.
   *
   * @param p_other The strong_ptr to "move" from (actually copied for safety)
   * @return Reference to *this
   */
  strong_ptr& operator=(strong_ptr&& p_other) noexcept
  {
    if (this != &p_other) {
      release();
      m_ctrl = p_other.m_ctrl;
      m_ptr = p_other.m_ptr;
      ptr_add_ref(m_ctrl);
    }
    return *this;
  }

  /**
   * @brief Compile time error message for bad alias value
   *
   * `std::shared_ptr` provides an alias constructor that accepts any `void*`
   * which is UB if that `void*` doesn't have the same lifetime as the object
   * referenced by the `std::shared_ptr`. Users attempting to do this will get a
   * list of constructors that failed to fit. This is not a good error message
   * for users. Instead, we provide a static_assert message in plain english
   * that explains why this overload fails at compile time.
   *
   * @tparam U - some type for the strong_ptr.
   */
  template<typename U>
  strong_ptr(strong_ptr<U> const&, void const*) noexcept
  {
    // NOTE: The conditional used here is to prevent the compiler from
    // jumping-the-gun and emitting the static assert error during template
    // instantiation of the class. With this conditional, the error only appears
    // when this constructor is used.
    static_assert(
      std::is_same_v<U, void> && !std::is_same_v<U, void>,
      "Aliasing constructor only works with pointers-to-members "
      "and does not work with arbitrary pointers like std::shared_ptr allows.");
  }

  /**
   * @brief Safe aliasing constructor for object members
   *
   * This constructor creates a strong_ptr that points to a member of an object
   * managed by another strong_ptr. The resulting strong_ptr shares ownership
   * with the original strong_ptr, keeping the entire parent object alive.
   *
   * This version is only enabled for non-array members to prevent potential
   * undefined behavior when accessing array elements directly. Use the
   * array-specific versions instead.
   *
   * Example usage:
   * ```
   * struct container {
   *   component part;
   * };
   *
   * // Create a strong_ptr to the container
   * auto container_ptr = make_strong_ptr<container>(allocator);
   *
   * // Create a strong_ptr to just the component
   * auto component_ptr = strong_ptr<component>(container_ptr,
   * &container::part);
   * ```
   *
   * @tparam U Type of the parent object
   * @tparam M Type of the member
   * @param p_other The strong_ptr to the parent object
   * @param p_member_ptr Pointer-to-member identifying which member to reference
   */
  template<typename U, detail::non_array_like M>
  strong_ptr(strong_ptr<U> const& p_other,
             // clang-format off
             M U::* p_member_ptr
             // clang-format on
             ) noexcept

    : m_ctrl(p_other.m_ctrl)
    , m_ptr(&((*p_other).*p_member_ptr))
  {
    ptr_add_ref(m_ctrl);
  }

  /**
   * @brief Safe aliasing constructor for std::array members
   *
   * This constructor creates a strong_ptr that points to an element of an array
   * member in an object managed by another strong_ptr. It performs bounds
   * checking to ensure the index is valid.
   *
   * Example usage:
   * ```
   * struct array_container {
   *   std::array<element, 5> elements;
   * };
   *
   * auto container_ptr = make_strong_ptr<array_container>(allocator);
   *
   * // Get strong_ptr to the 2nd element
   * auto element_ptr = strong_ptr<element>(
   *   container_ptr,
   *   &array_container::elements,
   *   2 // Index to access
   * );
   * ```
   *
   * @tparam U Type of the parent object
   * @tparam E Type of the array element
   * @tparam N Size of the array
   * @param p_other The strong_ptr to the parent object
   * @param p_array_ptr Pointer-to-member identifying the array member
   * @param p_index Index of the element to reference
   * @throws hal::out_of_range if index is out of bounds
   */
  template<typename U, typename E, std::size_t N>
  strong_ptr(strong_ptr<U> const& p_other,
             // clang-format off
             std::array<E, N> U::* p_array_ptr,
             // clang-format on
             std::size_t p_index)
  {
    static_assert(std::is_convertible_v<E*, T*>,
                  "Array element type must be convertible to T");
    throw_if_out_of_bounds(N, p_index);
    m_ctrl = p_other.m_ctrl;
    m_ptr = &((*p_other).*p_array_ptr)[p_index];
    ptr_add_ref(m_ctrl);
  }

  // NOLINTBEGIN(modernize-avoid-c-arrays)
  /**
   * @brief Safe aliasing constructor for C-array members
   *
   * This constructor creates a strong_ptr that points to an element of a
   * C-style array member in an object managed by another strong_ptr. It
   * performs bounds checking to ensure the index is valid.
   *
   * Example usage:
   * ```
   * struct c_array_container {
   *   element elements[5];
   * };
   *
   * auto container_ptr = make_strong_ptr<c_array_container>(allocator);
   *
   * // Get strong_ptr to the 2nd element
   * auto element_ptr = strong_ptr<element>(
   *   container_ptr,
   *   &c_array_container::elements,
   *   2 // Index to access
   * );
   * ```
   *
   * @tparam U Type of the parent object
   * @tparam E Type of the array element
   * @tparam N Size of the array
   * @param p_other The strong_ptr to the parent object
   * @param p_array_ptr Pointer-to-member identifying the array member
   * @param p_index Index of the element to reference
   * @throws hal::out_of_range if index is out of bounds
   */
  template<typename U, typename E, std::size_t N>
  strong_ptr(strong_ptr<U> const& p_other,
             E (U::*p_array_ptr)[N],
             std::size_t p_index)
  {
    static_assert(std::is_convertible_v<E*, T*>,
                  "Array element type must be convertible to T");
    throw_if_out_of_bounds(N, p_index);
    m_ctrl = p_other.m_ctrl;
    m_ptr = &((*p_other).*p_array_ptr)[p_index];
    ptr_add_ref(m_ctrl);
  }
  // NOLINTEND(modernize-avoid-c-arrays)

  /**
   * @brief Destructor
   *
   * Decrements the reference count and destroys the managed object
   * if this was the last strong reference.
   */
  ~strong_ptr()
  {
    release();
  }

  /**
   * @brief Copy assignment operator
   *
   * Replaces the managed object with the one managed by p_other.
   *
   * @param p_other The strong_ptr to copy from
   * @return Reference to *this
   */
  strong_ptr& operator=(strong_ptr const& p_other) noexcept
  {
    if (this != &p_other) {
      release();
      m_ctrl = p_other.m_ctrl;
      m_ptr = p_other.m_ptr;
      ptr_add_ref(m_ctrl);
    }
    return *this;
  }

  /**
   * @brief Converting copy assignment operator
   *
   * Replaces the managed object with the one managed by p_other,
   * converting from type U to type T.
   *
   * @tparam U A type convertible to T
   * @param p_other The strong_ptr to copy from
   * @return Reference to *this
   */
  template<typename U>
  strong_ptr& operator=(strong_ptr<U> const& p_other) noexcept
    requires(std::is_convertible_v<U*, T*>)
  {
    release();
    m_ctrl = p_other.m_ctrl;
    m_ptr = p_other.m_ptr;
    ptr_add_ref(m_ctrl);
    return *this;
  }

  /**
   * @brief Swap the contents of this strong_ptr with another
   *
   * @param p_other The strong_ptr to swap with
   */
  void swap(strong_ptr& p_other) noexcept
  {
    std::swap(m_ctrl, p_other.m_ctrl);
    std::swap(m_ptr, p_other.m_ptr);
  }

  /**
   * @brief Disable dereferencing for r-values (temporaries)
   */
  T& operator*() && = delete;

  /**
   * @brief Disable member access for r-values (temporaries)
   */
  T* operator->() && = delete;

  /**
   * @brief Dereference operator to access the managed object
   *
   * @return Reference to the managed object
   */
  [[nodiscard]] T& operator*() const& noexcept
  {
    return *m_ptr;
  }

  /**
   * @brief Member access operator to access the managed object
   *
   * @return Pointer to the managed object
   */
  [[nodiscard]] T* operator->() const& noexcept
  {
    return m_ptr;
  }

  /**
   * @brief Get the current reference count
   *
   * This is primarily for testing purposes.
   *
   * @return The number of strong references to the managed object
   */
  [[nodiscard]] auto use_count() const noexcept
  {
    return m_ctrl ? m_ctrl->strong_count.load(std::memory_order_relaxed) : 0;
  }

private:
  template<class U, typename... Args>
  friend strong_ptr<U> make_strong_ptr(std::pmr::polymorphic_allocator<>,
                                       Args&&...);

  template<typename U>
  friend class strong_ptr;

  template<typename U>
  friend class weak_ptr;

  template<typename U>
  friend class optional_ptr;

  inline void throw_if_out_of_bounds(usize p_size, usize p_index)
  {
    if (p_index >= p_size) {
      hal::safe_throw(
        hal::out_of_range(this, { .m_index = p_index, .m_capacity = p_size }));
    }
  }

  // Internal constructor with control block and pointer - used by make() and
  // aliasing
  strong_ptr(detail::ref_info* p_ctrl, T* p_ptr) noexcept
    : m_ctrl(p_ctrl)
    , m_ptr(p_ptr)
  {
  }

  void release()
  {
    if (m_ctrl) {
      ptr_release(m_ctrl);
    }
  }

  detail::ref_info* m_ctrl = nullptr;
  T* m_ptr = nullptr;
};

/**
 * @brief CRTP mixin to enable objects to create strong_ptr instances to
 * themselves
 *
 * Similar to `std::enable_shared_from_this`, this class allows an object to
 * safely obtain a strong_ptr to itself. The object must inherit from this class
 * and be managed by a strong_ptr created via make_strong_ptr.
 *
 * Example usage:
 * ```cpp
 * class my_driver : public enable_strong_from_this<my_driver> {
 * public:
 *   void register_callback() {
 *     // Get a strong_ptr to ourselves
 *     auto self = strong_from_this();
 *     some_async_system.register_callback([self](){
 *       self->handle_callback();
 *     });
 *   }
 * };
 *
 * auto obj = make_strong_ptr<my_driver>(allocator);
 * obj->register_callback(); // Safe to get strong_ptr to self
 * ```
 *
 * @tparam T The derived class type
 */
template<typename T>
class enable_strong_from_this
{
public:
  /**
   * @brief Get a strong_ptr to this object
   *
   * @return strong_ptr<T> pointing to this object
   * @throws hal::bad_weak_ptr if this object is not managed by a strong_ptr
   */
  [[nodiscard]] strong_ptr<T> strong_from_this()
  {
    auto locked = m_weak_this.lock();
    if (!locked) {
      hal::safe_throw(hal::bad_weak_ptr(&m_weak_this));
    }
    return locked.value();
  }

  /**
   * @brief Get a strong_ptr to this object (const version)
   *
   * @return strong_ptr<T const> pointing to this object
   * @throws hal::bad_weak_ptr if this object is not managed by a strong_ptr
   */
  [[nodiscard]] strong_ptr<T const> strong_from_this() const
  {
    auto locked = m_weak_this.lock();
    if (!locked) {
      hal::safe_throw(hal::bad_weak_ptr(&m_weak_this));
    }
    // Cast the strong_ptr<T> to strong_ptr<T const>
    return strong_ptr<T const>(locked.value());
  }

  /**
   * @brief Get a weak_ptr to this object
   *
   * @return weak_ptr<T> pointing to this object
   */
  [[nodiscard]] weak_ptr<T> weak_from_this() noexcept
  {
    return m_weak_this;
  }

  /**
   * @brief Get a weak_ptr to this object (const version)
   *
   * @return weak_ptr<T const> pointing to this object
   */
  [[nodiscard]] weak_ptr<T const> weak_from_this() const noexcept
  {
    return weak_ptr<T const>(m_weak_this);
  }

protected:
  /**
   * @brief Protected constructor to prevent direct instantiation
   */
  enable_strong_from_this() = default;

  /**
   * @brief Protected copy constructor
   *
   * Note: The weak_ptr is not copied - each object gets its own weak reference
   */
  enable_strong_from_this(enable_strong_from_this const&) noexcept
  {
    // Intentionally don't copy m_weak_this
  }

  /**
   * @brief Protected assignment operator
   *
   * Note: The weak_ptr is not assigned - each object keeps its own weak
   * reference
   */
  enable_strong_from_this& operator=(enable_strong_from_this const&) noexcept
  {
    // Intentionally don't assign m_weak_this
    return *this;
  }

  /**
   * @brief Protected destructor
   */
  ~enable_strong_from_this() = default;

private:
  template<class U, typename... Args>
  friend strong_ptr<U> make_strong_ptr(std::pmr::polymorphic_allocator<>,
                                       Args&&...);

  /**
   * @brief Initialize the weak reference (called by make_strong_ptr)
   *
   * @param p_self The strong_ptr that manages this object
   */
  void init_weak_this(strong_ptr<T> const& p_self) noexcept
  {
    m_weak_this = p_self;
  }

  mutable weak_ptr<T> m_weak_this;
};

template<typename T>
class optional_ptr;

/**
 * @brief A weak reference to a strong_ptr
 *
 * weak_ptr provides a non-owning reference to an object managed by strong_ptr.
 * It can be used to break reference cycles or to create an optional_ptr.
 *
 * A weak_ptr doesn't increase the strong reference count, so it doesn't
 * prevent the object from being destroyed when the last strong_ptr goes away.
 *
 * Example usage:
 * ```
 * // Create a strong_ptr to an object
 * auto ptr = hal::make_strong_ptr<my_driver>(allocator, args...);
 *
 * // Create a weak reference
 * weak_ptr<my_driver> weak = ptr;
 *
 * // Later, try to get a strong reference
 * if (auto locked = weak.lock()) {
 *   // Use the object via locked
 *   locked->doSomething();
 * } else {
 *   // Object has been destroyed
 * }
 * ```
 *
 * @tparam T The type of the referenced object
 */
template<typename T>
class weak_ptr
{
public:
  template<typename U>
  friend class strong_ptr;

  template<typename U>
  friend class weak_ptr;

  using element_type = T;

  /**
   * @brief Default constructor creates empty weak_ptr
   */
  weak_ptr() noexcept = default;

  /**
   * @brief Create weak_ptr from strong_ptr
   *
   * @param p_strong The strong_ptr to create a weak reference to
   */
  weak_ptr(strong_ptr<T> const& p_strong) noexcept
    : m_ctrl(p_strong.m_ctrl)
    , m_ptr(p_strong.m_ptr)
  {
    if (m_ctrl) {
      ptr_add_weak(m_ctrl);
    }
  }

  /**
   * @brief Copy constructor
   *
   * @param p_other The weak_ptr to copy from
   */
  weak_ptr(weak_ptr const& p_other) noexcept
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(p_other.m_ptr)
  {
    if (m_ctrl) {
      ptr_add_weak(m_ctrl);
    }
  }

  /**
   * @brief Move constructor
   *
   * @param p_other The weak_ptr to move from
   */
  weak_ptr(weak_ptr&& p_other) noexcept
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(p_other.m_ptr)
  {
    p_other.m_ctrl = nullptr;
    p_other.m_ptr = nullptr;
  }

  /**
   * @brief Converting copy constructor
   *
   * Creates a weak_ptr of T from a weak_ptr of U where U is convertible to T.
   *
   * @tparam U A type convertible to T
   * @param p_other The weak_ptr to copy from
   */
  template<typename U>
  weak_ptr(weak_ptr<U> const& p_other) noexcept
    requires(std::is_convertible_v<U*, T*>)
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(static_cast<T*>(p_other.m_ptr))
  {
    if (m_ctrl) {
      ptr_add_weak(m_ctrl);
    }
  }

  /**
   * @brief Converting move constructor
   *
   * Moves a weak_ptr of U to a weak_ptr T where U is convertible to T.
   *
   * @tparam U A type convertible to T
   * @param p_other The weak_ptr to move from
   */
  template<typename U>
  weak_ptr(weak_ptr<U>&& p_other) noexcept
    requires(std::is_convertible_v<U*, T*>)
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(static_cast<T*>(p_other.m_ptr))
  {
    p_other.m_ctrl = nullptr;
    p_other.m_ptr = nullptr;
  }

  /**
   * @brief Converting constructor from strong_ptr
   *
   * Creates a weak_ptr<T> from a strong_ptr where U is convertible to T.
   *
   * @tparam U A type convertible to T
   * @param p_other The strong_ptr to create a weak reference to
   */
  template<typename U>
  weak_ptr(strong_ptr<U> const& p_other) noexcept
    requires(std::is_convertible_v<U*, T*>)
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(static_cast<T*>(p_other.m_ptr))
  {
    if (m_ctrl) {
      ptr_add_weak(m_ctrl);
    }
  }

  /**
   * @brief Destructor
   *
   * Decrements the weak reference count and potentially deallocates
   * memory if this was the last reference.
   */
  ~weak_ptr()
  {
    if (m_ctrl) {
      ptr_release_weak(m_ctrl);
    }
  }

  /**
   * @brief Copy assignment operator
   *
   * @param p_other The weak_ptr to copy from
   * @return Reference to *this
   */
  weak_ptr& operator=(weak_ptr const& p_other) noexcept
  {
    weak_ptr(p_other).swap(*this);
    return *this;
  }

  /**
   * @brief Move assignment operator
   *
   * @param p_other The weak_ptr to move from
   * @return Reference to *this
   */
  weak_ptr& operator=(weak_ptr&& p_other) noexcept
  {
    weak_ptr(std::move(p_other)).swap(*this);
    return *this;
  }

  /**
   * @brief Assignment from strong_ptr
   *
   * @param p_strong The strong_ptr to create a weak reference to
   * @return Reference to *this
   */
  weak_ptr& operator=(strong_ptr<T> const& p_strong) noexcept
  {
    weak_ptr(p_strong).swap(*this);
    return *this;
  }

  /**
   * @brief Swap the contents of this weak_ptr with another
   *
   * @param p_other The weak_ptr to swap with
   */
  void swap(weak_ptr& p_other) noexcept
  {
    std::swap(m_ctrl, p_other.m_ctrl);
    std::swap(m_ptr, p_other.m_ptr);
  }

  /**
   * @brief Check if the referenced object has been destroyed
   *
   * @return true if the object has been destroyed, false otherwise
   */
  [[nodiscard]] bool expired() const noexcept
  {
    return !m_ctrl || m_ctrl->strong_count.load(std::memory_order_relaxed) == 0;
  }

  /**
   * @brief Attempt to obtain a strong_ptr to the referenced object
   *
   * If the object still exists, returns an optional_ptr containing
   * a strong_ptr to it. Otherwise, returns an empty optional_ptr.
   *
   * @return An optional_ptr that is either empty or contains a strong_ptr
   */
  [[nodiscard]] optional_ptr<T> lock() const noexcept;

  /**
   * @brief Get the current strong reference count
   *
   * This is primarily for testing purposes.
   *
   * @return The number of strong references to the managed object
   */
  [[nodiscard]] auto use_count() const noexcept
  {
    return m_ctrl ? m_ctrl->strong_count.load(std::memory_order_relaxed) : 0;
  }

private:
  detail::ref_info* m_ctrl = nullptr;
  T* m_ptr = nullptr;
};

/**
 * @brief Optional, nullable, smart pointer that works with `hal::strong_ptr`.
 *
 * optional_ptr provides a way to represent a strong_ptr that may or may not
 * be present. Unlike strong_ptr, which is always valid, optional_ptr can be
 * in a "disengaged" state where it doesn't reference any object.
 *
 * Use optional_ptr when you need a nullable reference to a reference-counted
 * object, such as:
 * - Representing the absence of a value
 * - Return values from functions that may fail
 * - Results of locking a weak_ptr
 *
 * Example usage:
 * ```
 * // Create an empty optional_ptr
 * optional_ptr<my_driver> opt1;
 *
 * // Create an optional_ptr from a strong_ptr
 * auto ptr = make_strong_ptr<my_driver>(allocator, args...);
 * optional_ptr<my_driver> opt2 = ptr;
 *
 * // Check if the optional_ptr is engaged
 * if (opt2) {
 *   // Use the contained object
 *   opt2->doSomething();
 * }
 *
 * // Reset to disengage
 * opt2.reset();
 * ```
 *
 * @tparam T - The type pointed to by strong_ptr
 */
template<typename T>
class optional_ptr
{
public:
  /**
   * @brief Default constructor creates a disengaged optional
   */
  constexpr optional_ptr() noexcept
  {
  }

  /**
   * @brief Constructor for nullptr (creates a disengaged optional)
   */
  constexpr optional_ptr(std::nullptr_t) noexcept
  {
  }

  /**
   * @brief Move constructor is deleted
   */
  constexpr optional_ptr(optional_ptr&& p_other) noexcept = delete;

  /**
   * @brief Construct from a strong_ptr lvalue
   *
   * @param value The strong_ptr to wrap
   */
  constexpr optional_ptr(strong_ptr<T> const& value) noexcept
    : m_value(value)
  {
  }

  /**
   * @brief Copy constructor
   *
   * @param p_other The optional_ptr to copy from
   */
  constexpr optional_ptr(optional_ptr const& p_other)
  {
    *this = p_other;
  }

  /**
   * @brief Converting constructor from a strong_ptr
   *
   * @tparam U A type convertible to T
   * @param p_value The strong_ptr to wrap
   */
  template<typename U>
  constexpr optional_ptr(strong_ptr<U> const& p_value)
    requires(std::is_convertible_v<U*, T*>)
  {
    *this = p_value;
  }

  /**
   * @brief Move assignment operator is deleted
   */
  constexpr optional_ptr& operator=(optional_ptr&& other) noexcept = delete;

  /**
   * @brief Copy assignment operator
   *
   * @param other The optional_ptr to copy from
   * @return Reference to *this
   */
  constexpr optional_ptr& operator=(optional_ptr const& other)
  {
    if (this != &other) {
      if (is_engaged() && other.is_engaged()) {
        m_value = other.m_value;
      } else if (is_engaged() && not other.is_engaged()) {
        reset();
      } else if (not is_engaged() && other.is_engaged()) {
        new (&m_value) strong_ptr<T>(other.m_value);
      }
    }
    return *this;
  }

  /**
   * @brief Assignment from a strong_ptr
   *
   * @param value The strong_ptr to wrap
   * @return Reference to *this
   */
  constexpr optional_ptr& operator=(strong_ptr<T> const& value) noexcept
  {
    if (is_engaged()) {
      m_value = value;
    } else {
      new (&m_value) strong_ptr<T>(value);
    }
    return *this;
  }

  /**
   * @brief Converting assignment from a strong_ptr
   *
   * @tparam U A type convertible to T
   * @param p_value The strong_ptr to wrap
   * @return Reference to *this
   */
  template<typename U>
  constexpr optional_ptr& operator=(strong_ptr<U> const& p_value) noexcept
    requires(std::is_convertible_v<U*, T*>)
  {
    if (is_engaged()) {
      m_value = p_value;
    } else {
      new (&m_value) strong_ptr<U>(p_value);
    }
    return *this;
  }

  /**
   * @brief Assignment from nullptr (resets to disengaged state)
   *
   * @return Reference to *this
   */
  constexpr optional_ptr& operator=(std::nullptr_t) noexcept
  {
    reset();
    return *this;
  }

  /**
   * @brief Destructor
   *
   * Properly destroys the contained strong_ptr if engaged.
   */
  ~optional_ptr()
  {
    if (is_engaged()) {
      m_value.~strong_ptr<T>();
    }
  }

  /**
   * @brief Check if the optional_ptr is engaged
   *
   * @return true if the optional_ptr contains a value, false otherwise
   */
  [[nodiscard]] constexpr bool has_value() const noexcept
  {
    return is_engaged();
  }

  /**
   * @brief Check if the optional_ptr is engaged
   *
   * @return true if the optional_ptr contains a value, false otherwise
   */
  constexpr explicit operator bool() const noexcept
  {
    return is_engaged();
  }

  /**
   * @brief Access the contained value, throw if not engaged
   *
   * @return A copy of the contained strong_ptr
   * @throws hal::bad_optional_ptr_access if *this is disengaged
   */
  [[nodiscard]] constexpr strong_ptr<T>& value()
  {
    if (!is_engaged()) {
      hal::safe_throw(hal::bad_optional_ptr_access(this));
    }
    return m_value;
  }

  /**
   * @brief Access the contained value, throw if not engaged (const version)
   *
   * @return A copy of the contained strong_ptr
   * @throws hal::bad_optional_ptr_access if *this is disengaged
   */
  [[nodiscard]] constexpr strong_ptr<T> const& value() const
  {
    if (!is_engaged()) {
      hal::safe_throw(hal::bad_optional_ptr_access(this));
    }
    return m_value;
  }

  /**
   * @brief Implicitly convert to a strong_ptr<T>
   *
   * This allows optional_ptr to be used anywhere a strong_ptr is expected
   * when the optional_ptr is engaged.
   *
   * @return A copy of the contained strong_ptr
   * @throws hal::bad_optional_ptr_access if *this is disengaged
   */
  [[nodiscard]] constexpr operator strong_ptr<T>()
  {
    return value();
  }

  /**
   * @brief Implicitly convert to a strong_ptr<T> (const version)
   *
   * @return A copy of the contained strong_ptr
   * @throws hal::bad_optional_ptr_access if *this is disengaged
   */
  [[nodiscard]] constexpr operator strong_ptr<T>() const
  {
    return value();
  }

  /**
   * @brief Implicitly convert to a strong_ptr for polymorphic types
   *
   * This allows optional_ptr<Derived> to be converted to strong_ptr<Base>
   * when Derived is convertible to Base.
   *
   * @tparam U The target type (must be convertible from T)
   * @return A copy of the contained strong_ptr, converted to the target type
   * @throws hal::bad_optional_ptr_access if *this is disengaged
   */
  template<typename U>
  [[nodiscard]] constexpr operator strong_ptr<U>()
    requires(std::is_convertible_v<T*, U*> && !std::is_same_v<T, U>)
  {
    if (!is_engaged()) {
      hal::safe_throw(hal::bad_optional_ptr_access(this));
    }
    // strong_ptr handles the polymorphic conversion
    return strong_ptr<U>(m_value);
  }

  /**
   * @brief Implicitly convert to a strong_ptr for polymorphic types (const
   * version)
   *
   * @tparam U The target type (must be convertible from T)
   * @return A copy of the contained strong_ptr, converted to the target type
   * @throws hal::bad_optional_ptr_access if *this is disengaged
   */
  template<typename U>
  [[nodiscard]] constexpr operator strong_ptr<U>() const
    requires(std::is_convertible_v<T*, U*> && !std::is_same_v<T, U>)
  {
    if (!is_engaged()) {
      hal::safe_throw(hal::bad_optional_ptr_access(this));
    }
    // strong_ptr handles the polymorphic conversion
    return strong_ptr<U>(m_value);
  }

  /**
   * @brief Arrow operator for accessing members of the contained object
   *
   * @return Pointer to the object managed by the contained strong_ptr
   */
  [[nodiscard]] constexpr auto* operator->()
  {
    auto& ref = *(this->value());
    return &ref;
  }

  /**
   * @brief Arrow operator for accessing members of the contained object (const
   * version)
   *
   * @return Pointer to the object managed by the contained strong_ptr
   */
  [[nodiscard]] constexpr auto* operator->() const
  {
    auto& ref = *(this->value());
    return &ref;
  }

  /**
   * @brief Dereference operator for accessing the contained object
   *
   * @return Reference to the object managed by the contained strong_ptr
   */
  [[nodiscard]] constexpr auto& operator*()
  {
    auto& ref = *(this->value());
    return ref;
  }

  /**
   * @brief Dereference operator for accessing the contained object (const
   * version)
   *
   * @return Reference to the object managed by the contained strong_ptr
   */
  [[nodiscard]] constexpr auto& operator*() const
  {
    auto& ref = *(this->value());
    return ref;
  }

  /**
   * @brief Reset the optional to a disengaged state
   */
  constexpr void reset() noexcept
  {
    if (is_engaged()) {
      m_value.~strong_ptr<T>();
      m_raw_ptrs[0] = nullptr;
      m_raw_ptrs[1] = nullptr;
    }
  }

  /**
   * @brief Emplace a new value
   *
   * Reset the optional and construct a new strong_ptr in-place.
   *
   * @tparam Args Types of arguments to forward to the constructor
   * @param args Arguments to forward to the constructor
   * @return Reference to the newly constructed strong_ptr
   */
  template<typename... Args>
  constexpr strong_ptr<T>& emplace(Args&&... args)
  {
    reset();
    new (&m_value) strong_ptr<T>(std::forward<Args>(args)...);
    return m_value;
  }

  /**
   * @brief Swap the contents of this optional_ptr with another
   *
   * @param other The optional_ptr to swap with
   */
  constexpr void swap(optional_ptr& other) noexcept
  {
    if (is_engaged() && other.is_engaged()) {
      std::swap(m_value, other.m_value);
    } else if (is_engaged() && !other.is_engaged()) {
      new (&other.m_value) strong_ptr<T>(std::move(m_value));
      reset();
    } else if (!is_engaged() && other.is_engaged()) {
      new (&m_value) strong_ptr<T>(std::move(other.m_value));
      other.reset();
    }
  }

private:
  /**
   * @brief Use the strong_ptr's memory directly through a union
   *
   * This allows us to detect whether the optional_ptr is engaged
   * by checking if the internal pointers are non-null.
   */
  union
  {
    std::array<void*, 2> m_raw_ptrs = { nullptr, nullptr };
    strong_ptr<T> m_value;
  };

  // Ensure the strong_ptr layout matches our expectations
  static_assert(sizeof(m_value) == sizeof(m_raw_ptrs),
                "strong_ptr must be exactly the size of two pointers");

  /**
   * @brief Helper to check if the optional is engaged
   *
   * @return true if the optional_ptr contains a value, false otherwise
   */
  [[nodiscard]] constexpr bool is_engaged() const noexcept
  {
    return m_raw_ptrs[0] != nullptr || m_raw_ptrs[1] != nullptr;
  }
};

/**
 * @brief Implement weak_ptr::lock() now that optional_ptr is defined
 *
 * This function attempts to obtain a strong_ptr from a weak_ptr.
 * If the referenced object still exists, it returns an optional_ptr
 * containing a strong_ptr to it. Otherwise, it returns an empty optional_ptr.
 *
 * @tparam T The type of the referenced object
 * @return An optional_ptr that is either empty or contains a strong_ptr
 */
template<typename T>
[[nodiscard]] inline optional_ptr<T> weak_ptr<T>::lock() const noexcept
{
  if (expired()) {
    return nullptr;
  }

  // Try to increment the strong count
  auto current_count = m_ctrl->strong_count.load(std::memory_order_relaxed);
  while (current_count > 0) {
    // TODO(kammce): Consider if this is dangerous
    if (m_ctrl->strong_count.compare_exchange_weak(current_count,
                                                   current_count + 1,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
      // Successfully incremented
      auto obj = strong_ptr<T>(m_ctrl, m_ptr);
      return obj;
    }
  }

  // Strong count is now 0
  return nullptr;
}

/**
 * @brief Non-member swap for strong_ptr
 *
 * @tparam T The type of the managed object
 * @param p_lhs First strong_ptr to swap
 * @param p_rhs Second strong_ptr to swap
 */
template<typename T>
void swap(strong_ptr<T>& p_lhs, strong_ptr<T>& p_rhs) noexcept
{
  p_lhs.swap(p_rhs);
}

/**
 * @brief Non-member swap for weak_ptr
 *
 * @tparam T The type of the referenced object
 * @param p_lhs First weak_ptr to swap
 * @param p_rhs Second weak_ptr to swap
 */
template<typename T>
void swap(weak_ptr<T>& p_lhs, weak_ptr<T>& p_rhs) noexcept
{
  p_lhs.swap(p_rhs);
}

/**
 * @brief Equality operator for strong_ptr
 *
 * Compares if two strong_ptr instances point to the same object.
 *
 * @tparam T The type of the first strong_ptr
 * @tparam U The type of the second strong_ptr
 * @param p_lhs First strong_ptr to compare
 * @param p_rhs Second strong_ptr to compare
 * @return true if both point to the same object, false otherwise
 */
template<typename T, typename U>
bool operator==(strong_ptr<T> const& p_lhs, strong_ptr<U> const& p_rhs) noexcept
{
  return p_lhs.operator->() == p_rhs.operator->();
}

/**
 * @brief Inequality operator for strong_ptr
 *
 * Compares if two strong_ptr instances point to different objects.
 *
 * @tparam T The type of the first strong_ptr
 * @tparam U The type of the second strong_ptr
 * @param p_lhs First strong_ptr to compare
 * @param p_rhs Second strong_ptr to compare
 * @return true if they point to different objects, false otherwise
 */
template<typename T, typename U>
bool operator!=(strong_ptr<T> const& p_lhs, strong_ptr<U> const& p_rhs) noexcept
{
  return !(p_lhs == p_rhs);
}
/**
 * @brief Equality operator for optional_ptr
 *
 * Compares if two optional_ptr instances are equal - they are equal if:
 * 1. Both are disengaged (both empty)
 * 2. Both are engaged and point to the same object
 *
 * @tparam T The type of the first optional_ptr
 * @tparam U The type of the second optional_ptr
 * @param p_lhs First optional_ptr to compare
 * @param p_rhs Second optional_ptr to compare
 * @return true if both are equal according to the rules above
 */
template<typename T, typename U>
[[nodiscard]] bool operator==(optional_ptr<T> const& p_lhs,
                              optional_ptr<U> const& p_rhs) noexcept
{
  // If both are disengaged, they're equal
  if (!p_lhs.has_value() && !p_rhs.has_value()) {
    return true;
  }

  // If one is engaged and the other isn't, they're not equal
  if (p_lhs.has_value() != p_rhs.has_value()) {
    return false;
  }

  // Both are engaged, compare the underlying pointers
  return p_lhs.value().operator->() == p_rhs.value().operator->();
}

/**
 * @brief Inequality operator for optional_ptr
 *
 * Returns the opposite of the equality operator.
 *
 * @tparam T The type of the first optional_ptr
 * @tparam U The type of the second optional_ptr
 * @param p_lhs First optional_ptr to compare
 * @param p_rhs Second optional_ptr to compare
 * @return true if they are not equal
 */
template<typename T, typename U>
[[nodiscard]] bool operator!=(optional_ptr<T> const& p_lhs,
                              optional_ptr<U> const& p_rhs) noexcept
{
  return !(p_lhs == p_rhs);
}

/**
 * @brief Equality operator between optional_ptr and nullptr
 *
 * An optional_ptr equals nullptr if it's disengaged.
 *
 * @tparam T The type of the optional_ptr
 * @param p_lhs The optional_ptr to compare
 * @return true if the optional_ptr is disengaged
 */
template<typename T>
[[nodiscard]] bool operator==(optional_ptr<T> const& p_lhs,
                              std::nullptr_t) noexcept
{
  return !p_lhs.has_value();
}

/**
 * @brief Equality operator between nullptr and optional_ptr
 *
 * nullptr equals an optional_ptr if it's disengaged.
 *
 * @tparam T The type of the optional_ptr
 * @param p_rhs The optional_ptr to compare
 * @return true if the optional_ptr is disengaged
 */
template<typename T>
[[nodiscard]] bool operator==(std::nullptr_t,
                              optional_ptr<T> const& p_rhs) noexcept
{
  return !p_rhs.has_value();
}

/**
 * @brief Inequality operator between optional_ptr and nullptr
 *
 * An optional_ptr does not equal nullptr if it's engaged.
 *
 * @tparam T The type of the optional_ptr
 * @param p_lhs The optional_ptr to compare
 * @return true if the optional_ptr is engaged
 */
template<typename T>
[[nodiscard]] bool operator!=(optional_ptr<T> const& p_lhs,
                              std::nullptr_t) noexcept
{
  return p_lhs.has_value();
}

/**
 * @brief Inequality operator between nullptr and optional_ptr
 *
 * nullptr does not equal an optional_ptr if it's engaged.
 *
 * @tparam T The type of the optional_ptr
 * @param p_rhs The optional_ptr to compare
 * @return true if the optional_ptr is engaged
 */
template<typename T>
[[nodiscard]] bool operator!=(std::nullptr_t,
                              optional_ptr<T> const& p_rhs) noexcept
{
  return p_rhs.has_value();
}

/**
 * @brief A construction token that can only be created by make_strong_ptr
 *
 * Make the first parameter of your class's constructor(s) in order to limit
 * that constructor to only be used via `make_strong_ptr`.
 */
class strong_ptr_only_token
{
private:
  strong_ptr_only_token() = default;

  template<class U, typename... Args>
  friend strong_ptr<U> make_strong_ptr(std::pmr::polymorphic_allocator<>,
                                       Args&&...);
};

/**
 * @brief Factory function to create a strong_ptr with automatic construction
 * detection
 *
 * This is the primary way to create a new strong_ptr. It automatically detects
 * whether the target type requires token-based construction (for classes that
 * should only be managed by strong_ptr) or supports normal construction.
 *
 * The function performs the following operations:
 * 1. Allocates memory for both the object and its control block together
 * 2. Detects at compile time if the type expects a strong_ptr_only_token
 * 3. Constructs the object with appropriate parameters
 * 4. Initializes enable_strong_from_this support if the type inherits from it
 * 5. Returns a strong_ptr managing the newly created object
 *
 * **Token-based construction**: If a class constructor takes
 * `strong_ptr_only_token` as its first parameter, this function automatically
 * provides that token, ensuring the class can only be constructed via
 * make_strong_ptr.
 *
 * **Normal construction**: For regular classes, construction proceeds normally
 * with the provided arguments.
 *
 * **enable_strong_from_this support**: If the constructed type inherits from
 * `enable_strong_from_this<T>`, the weak reference is automatically initialized
 * to enable `strong_from_this()` and `weak_from_this()` functionality.
 *
 * Example usage:
 *
 * ```cpp
 * // Token-protected class (can only be created via make_strong_ptr)
 * class protected_driver {
 * public:
 *   protected_driver(strong_ptr_only_token, driver_config config);
 * };
 * auto driver = make_strong_ptr<protected_driver>(allocator, config{});
 *
 * // Regular class
 * class regular_class {
 * public:
 *   regular_class(int value);
 * };
 * auto obj = make_strong_ptr<regular_class>(allocator, 42);
 *
 * // Class with enable_strong_from_this support
 * class self_aware : public enable_strong_from_this<self_aware> {
 * public:
 *   self_aware(std::string name);
 *   void register_callback() {
 *     auto self = strong_from_this(); // This works automatically
 *   }
 * };
 * auto obj = make_strong_ptr<self_aware>(allocator, "example");
 * ```
 *
 * @tparam T The type of object to create
 * @tparam Args Types of arguments to forward to the constructor
 * @param p_alloc Allocator to use for memory allocation
 * @param p_args Arguments to forward to the constructor
 * @return A strong_ptr managing the newly created object
 * @throws Any exception thrown by the object's constructor
 * @throws std::bad_alloc if memory allocation fails
 */
template<class T, typename... Args>
[[nodiscard]] inline strong_ptr<T> make_strong_ptr(
  std::pmr::polymorphic_allocator<> p_alloc,
  Args&&... p_args)
{
  using rc_t = detail::rc<T>;

  rc_t* obj = nullptr;

  if constexpr (std::is_constructible_v<T, strong_ptr_only_token, Args...>) {
    // Type expects token as first parameter
    obj = p_alloc.new_object<rc_t>(
      p_alloc, strong_ptr_only_token{}, std::forward<Args>(p_args)...);
  } else {
    // Normal type, construct without token
    obj = p_alloc.new_object<rc_t>(p_alloc, std::forward<Args>(p_args)...);
  }

  strong_ptr<T> result(&obj->m_info, &obj->m_object);

  // Initialize enable_strong_from_this if the type inherits from it
  if constexpr (std::is_base_of_v<enable_strong_from_this<T>, T>) {
    result->init_weak_this(result);
  }

  return result;
}
}  // namespace hal::v5
