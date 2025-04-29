#pragma once

#include <cstddef>

#include <atomic>
#include <memory_resource>
#include <type_traits>
#include <utility>

#include <libhal/error.hpp>
#include <libhal/units.hpp>

namespace hal::inline v1 {
// Forward declarations
template<typename T>
class strong_ptr;

template<typename T>
class weak_ptr;

// Control block for reference counting - type erased
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
  std::pmr::polymorphic_allocator<hal::byte> allocator;
  destroy_fn_t* destroy;
  std::atomic<int> strong_count = 1;
  std::atomic<int> weak_count = 0;
};

// Add strong reference to control block
inline void ptr_add_ref(ref_info* p_info)
{
  p_info->strong_count.fetch_add(1, std::memory_order_relaxed);
}

// Release strong reference from control block
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
      std::pmr::polymorphic_allocator<byte> alloc = p_info->allocator;

      // Deallocate memory
      alloc.deallocate_bytes(p_info, object_size);
    }
  }
}

// Add weak reference to control block
inline void ptr_add_weak(ref_info* p_info)
{
  p_info->weak_count.fetch_add(1, std::memory_order_relaxed);
}

// Release weak reference from control block
inline void ptr_release_weak(ref_info* p_info)
{
  if (p_info->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 0) {
    // No more weak references, check if we can deallocate
    if (p_info->strong_count.load(std::memory_order_acquire) == 0) {
      // No strong references either, get the size from the destroy function
      usize const object_size = p_info->destroy(nullptr);

      // Save allocator for deallocating
      std::pmr::polymorphic_allocator<byte> alloc = p_info->allocator;

      // Deallocate memory
      alloc.deallocate_bytes(p_info, object_size);
    }
  }
}

namespace detail {
// A wrapper that contains both the ref_info and the actual object
template<typename T>
struct rc
{
  ref_info m_info;
  T m_object;

  // Constructor that forwards arguments to the object
  template<typename... Args>
  rc(std::pmr::polymorphic_allocator<byte> p_alloc, Args&&... args)
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
}  // namespace detail

/**
 * @brief A non-nullable strong reference counted pointer
 *
 * strong_ptr is similar to std::shared_ptr but with these key differences:
 * 1. Cannot be null - must always point to a valid object
 * 2. Can only be created via make_strong_ptr, not from raw pointers
 * 3. More memory efficient implementation
 *
 * @tparam T The type of the managed object
 */
template<typename T>
class strong_ptr
{
public:
  template<class U, typename... Args>
  friend strong_ptr<U> make_strong_ptr(
    std::pmr::polymorphic_allocator<byte> p_alloc,
    Args&&... args);

  template<typename U>
  friend class strong_ptr;

  template<typename U>
  friend class weak_ptr;

  template<typename U>
  friend class optional_ptr;

  using element_type = T;

  /// Delete default constructor - strong_ptr must always be valid
  strong_ptr() = delete;

  /// Delete nullptr constructor - strong_ptr must always be valid
  strong_ptr(std::nullptr_t) = delete;

  // Copy constructor
  strong_ptr(strong_ptr const& p_other) noexcept
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(p_other.m_ptr)
  {
    ptr_add_ref(m_ctrl);
  }

  // Converting copy constructor
  template<typename U>
  strong_ptr(strong_ptr<U> const& p_other) noexcept
    requires(std::is_convertible_v<U*, T*>)
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(p_other.m_ptr)
  {
    ptr_add_ref(m_ctrl);
  }

  strong_ptr(strong_ptr&& p_other) noexcept = delete;

  // Aliasing constructor - create a strong_ptr that points to an object within
  // the managed object
  template<typename U>
  strong_ptr(strong_ptr<U> const& p_other, T* p_ptr) noexcept
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(p_ptr)
  {
    ptr_add_ref(m_ctrl);
  }

  // Destructor
  ~strong_ptr()
  {
    release();
  }

  // Copy assignment operator
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

  // Copy assignment operator
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

  // Copy assignment operator
  strong_ptr& operator=(strong_ptr&& p_other) noexcept = delete;

  // Copy assignment operator
  template<typename U>
  strong_ptr& operator=(strong_ptr<U>&& p_other) noexcept = delete;

  // Swap function
  void swap(strong_ptr& p_other) noexcept
  {
    std::swap(m_ctrl, p_other.m_ctrl);
    std::swap(m_ptr, p_other.m_ptr);
  }

  // Disable dereferencing for r-values (temporaries)
  T& operator*() && = delete;
  T* operator->() && = delete;

  // Dereference operators
  T& operator*() const& noexcept
  {
    return *m_ptr;
  }

  T* operator->() const& noexcept
  {
    return m_ptr;
  }

  // Get reference count (for testing)
  auto use_count() const noexcept
  {
    return m_ctrl ? m_ctrl->strong_count.load(std::memory_order_relaxed) : 0;
  }

private:
  // Internal constructor with control block and pointer - used by make() and
  // aliasing
  strong_ptr(ref_info* p_ctrl, T* p_ptr) noexcept
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

  ref_info* m_ctrl = nullptr;
  T* m_ptr = nullptr;
};

template<typename T>
class optional_ptr;

/**
 * @brief A weak reference to a strong_ptr
 *
 * weak_ptr provides a non-owning reference to an object managed by strong_ptr.
 * It can be used to break reference cycles or to create an optional_ptr.
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

  /// Default constructor creates empty weak_ptr
  weak_ptr() noexcept = default;

  /// Create weak_ptr from strong_ptr
  weak_ptr(strong_ptr<T> const& p_strong) noexcept
    : m_ctrl(p_strong.m_ctrl)
    , m_ptr(p_strong.m_ptr)
  {
    if (m_ctrl) {
      ptr_add_weak(m_ctrl);
    }
  }

  // Copy constructor
  weak_ptr(weak_ptr const& p_other) noexcept
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(p_other.m_ptr)
  {
    if (m_ctrl) {
      ptr_add_weak(m_ctrl);
    }
  }

  // Move constructor
  weak_ptr(weak_ptr&& p_other) noexcept
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(p_other.m_ptr)
  {
    p_other.m_ctrl = nullptr;
    p_other.m_ptr = nullptr;
  }

  // Converting copy constructor
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

  // Converting move constructor
  template<typename U>
  weak_ptr(weak_ptr<U>&& p_other) noexcept
    requires(std::is_convertible_v<U*, T*>)
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(static_cast<T*>(p_other.m_ptr))
  {
    p_other.m_ctrl = nullptr;
    p_other.m_ptr = nullptr;
  }

  // Converting copy constructor
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

  // Destructor
  ~weak_ptr()
  {
    if (m_ctrl) {
      ptr_release_weak(m_ctrl);
    }
  }

  // Copy assignment operator
  weak_ptr& operator=(weak_ptr const& p_other) noexcept
  {
    weak_ptr(p_other).swap(*this);
    return *this;
  }

  // Move assignment operator
  weak_ptr& operator=(weak_ptr&& p_other) noexcept
  {
    weak_ptr(std::move(p_other)).swap(*this);
    return *this;
  }

  // Assignment from strong_ptr
  weak_ptr& operator=(strong_ptr<T> const& p_strong) noexcept
  {
    weak_ptr(p_strong).swap(*this);
    return *this;
  }

  // Swap function
  void swap(weak_ptr& p_other) noexcept
  {
    std::swap(m_ctrl, p_other.m_ctrl);
    std::swap(m_ptr, p_other.m_ptr);
  }

  // Check if expired
  [[nodiscard]] bool expired() const noexcept
  {
    return !m_ctrl || m_ctrl->strong_count.load(std::memory_order_relaxed) == 0;
  }

  // Lock to get a strong_ptr
  optional_ptr<T> lock() const noexcept;

  // Get use count for testing
  auto use_count() const noexcept
  {
    return m_ctrl ? m_ctrl->strong_count.load(std::memory_order_relaxed) : 0;
  }

private:
  ref_info* m_ctrl = nullptr;
  T* m_ptr = nullptr;
};

/**
 * @brief Optional, nullable, smart pointer that works with `hal::strong_ptr`.
 *
 * [Claude fill this out please, maybe add some usage examples]
 *
 * @tparam T - The type pointed to by strong_ptr
 */
template<typename T>
class optional_ptr
{
public:
  // Default constructor creates a disengaged optional
  constexpr optional_ptr() noexcept
  {
  }

  // Constructor for nullptr
  constexpr optional_ptr(nullptr_t) noexcept
  {
  }

  // Deleted! Move constructor
  constexpr optional_ptr(optional_ptr&& p_other) noexcept = delete;

  // Construct from a strong_ptr lvalue
  constexpr optional_ptr(strong_ptr<T> const& value) noexcept
    : m_value(value)
  {
  }

  // Copy constructor
  constexpr optional_ptr(optional_ptr const& p_other)
  {
    *this = p_other;
  }

  // Assignment from a strong_ptr<U>
  template<typename U>
  constexpr optional_ptr(strong_ptr<U> const& p_value)
    requires(std::is_convertible_v<U*, T*>)
  {
    *this = p_value;
  }

  // Deleted! Move assignment operator
  constexpr optional_ptr& operator=(optional_ptr&& other) noexcept = delete;

  // Assignment operator
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

  // Assignment from a strong_ptr
  constexpr optional_ptr& operator=(strong_ptr<T> const& value) noexcept
  {
    if (is_engaged()) {
      m_value = value;
    } else {
      new (&m_value) strong_ptr<T>(value);
    }
    return *this;
  }

  // Assignment from a strong_ptr
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

  // Constructor for nullptr
  constexpr optional_ptr& operator=(nullptr_t) noexcept
  {
    reset();
    return *this;
  }

  // Destructor
  ~optional_ptr()
  {
    if (is_engaged()) {
      m_value.~strong_ptr<T>();
    }
  }

  // Check if the optional_ptr is engaged
  [[nodiscard]] constexpr bool has_value() const noexcept
  {
    return is_engaged();
  }

  // Operator bool for checking engagement
  constexpr explicit operator bool() const noexcept
  {
    return is_engaged();
  }

  // Access the contained value, throw if not engaged
  constexpr strong_ptr<T>& value()
  {
    if (!is_engaged()) {
      throw std::bad_optional_access();
    }
    return m_value;
  }

  // Access the contained value, throw if not engaged (const version)
  constexpr strong_ptr<T> const& value() const
  {
    if (!is_engaged()) {
      throw std::bad_optional_access();
    }
    return m_value;
  }

  // Arrow operator (compatible with the base optional implementation)
  constexpr auto* operator->()
  {
    auto& ref = *(this->value());
    return &ref;
  }
  constexpr auto* operator->() const
  {
    auto& ref = *(this->value());
    return &ref;
  }

  // Dereference operator (compatible with the base optional implementation)
  constexpr auto& operator*()
  {
    auto& ref = *(this->value());
    return ref;
  }

  constexpr auto& operator*() const
  {
    auto& ref = *(this->value());
    return ref;
  }

  // Reset the optional to a disengaged state
  constexpr void reset() noexcept
  {
    if (is_engaged()) {
      m_value.~strong_ptr<T>();
      m_raw_ptrs[0] = nullptr;
      m_raw_ptrs[1] = nullptr;
    }
  }

  // Emplace a new value
  template<typename... Args>
  constexpr strong_ptr<T>& emplace(Args&&... args)
  {
    reset();
    new (&m_value) strong_ptr<T>(std::forward<Args>(args)...);
    return m_value;
  }

  // Swap with another optional
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
  // Use the strong_ptr's memory directly
  union
  {
    std::array<void*, 2> m_raw_ptrs = { nullptr, nullptr };
    strong_ptr<T> m_value;
  };

  // Ensure the strong_ptr layout matches our expectations
  static_assert(sizeof(m_value) == sizeof(m_raw_ptrs),
                "strong_ptr must be exactly the size of two pointers");

  // Helper to check if the optional is engaged
  [[nodiscard]] constexpr bool is_engaged() const noexcept
  {
    return m_raw_ptrs[0] != nullptr || m_raw_ptrs[1] != nullptr;
  }
};

// Implement weak_ptr::lock() now that optional_ptr is defined
template<typename T>
inline optional_ptr<T> weak_ptr<T>::lock() const noexcept
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

// Non-member swap for strong_ptr
template<typename T>
void swap(strong_ptr<T>& p_lhs, strong_ptr<T>& p_rhs) noexcept
{
  p_lhs.swap(p_rhs);
}

// Non-member swap for weak_ptr
template<typename T>
void swap(weak_ptr<T>& p_lhs, weak_ptr<T>& p_rhs) noexcept
{
  p_lhs.swap(p_rhs);
}

// Equality operators for strong_ptr
template<typename T, typename U>
bool operator==(strong_ptr<T> const& p_lhs, strong_ptr<U> const& p_rhs) noexcept
{
  return p_lhs.operator->() == p_rhs.operator->();
}

template<typename T, typename U>
bool operator!=(strong_ptr<T> const& p_lhs, strong_ptr<U> const& p_rhs) noexcept
{
  return !(p_lhs == p_rhs);
}

// Factory function to create a strong_ptr with its control block
template<class T, typename... Args>
inline strong_ptr<T> make_strong_ptr(
  std::pmr::polymorphic_allocator<byte> p_alloc,
  Args&&... args)
{
  using rc_t = detail::rc<T>;
  auto* obj = p_alloc.new_object<rc_t>(p_alloc, std::forward<Args>(args)...);
  return strong_ptr<T>(&obj->m_info, &obj->m_object);
}
}  // namespace hal::inline v1
