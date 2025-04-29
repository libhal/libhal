#pragma once

#include <cstddef>

#include <atomic>
#include <memory_resource>
#include <type_traits>
#include <utility>

#include <libhal/units.hpp>

namespace hal {

// Forward declaration
template<typename T>
class smart_ref;

// Control block for reference counting - type erased
struct ref_info
{
  using destroy_fn_t = usize(void const*);

  /// Initialize to 1 since creation implies a reference
  std::pmr::polymorphic_allocator<hal::byte> allocator;
  destroy_fn_t* destroy;
  std::atomic<int> count = 1;
};

// Add reference to control block
inline void intrusive_ptr_add_ref(ref_info* p_info)
{
  p_info->count.fetch_add(1, std::memory_order_relaxed);
}

// Release reference from control block
inline void intrusive_ptr_release(ref_info* p_info)
{
  if (p_info->count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    // Last reference, we know p_info is actually a pointer to the
    // rc So we can just cast it back to get the whole wrapper
    // object We use a void* here because the actual type is unknown at this
    // point

    // Save allocator for deallocating after destructor
    std::pmr::polymorphic_allocator<byte> alloc = p_info->allocator;

    // Call the destroy function which will:
    // 1. Call the destructor of the rc (including the object)
    // 2. Return the size of the rc for deallocation
    usize const object_size = p_info->destroy(p_info);

    // Deallocate memory
    alloc.deallocate_bytes(p_info, object_size);
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
    static_cast<rc<T> const*>(p_object)->~rc<T>();
    return sizeof(rc<T>);
  }
};

// static_assert(offsetof(rc<int>, ))
}  // namespace detail

// The smart_ref implementation
template<typename T>
class smart_ref
{
public:
  template<class U, typename... Args>
  friend smart_ref<U> make_shared_ref(
    std::pmr::polymorphic_allocator<byte> p_alloc,
    Args&&... args);

  using element_type = T;

  /// Delete default constructor - smart_ref must always be valid
  smart_ref() = delete;

  /// Delete nullptr constructor - smart_ref must always be valid
  smart_ref(std::nullptr_t) = delete;

  // Copy constructor
  smart_ref(smart_ref const& p_other) noexcept
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(p_other.m_ptr)
  {
    intrusive_ptr_add_ref(m_ctrl);
  }

  // Converting copy constructor
  template<typename U>
  smart_ref(smart_ref<U> const& p_other) noexcept
    requires(std::is_convertible_v<U*, T*>)
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(static_cast<T*>(p_other.m_ptr))
  {
    intrusive_ptr_add_ref(m_ctrl);
  }

  // Converting move constructor
  template<typename U>
  smart_ref(smart_ref<U>&& p_other) noexcept
    requires(std::is_convertible_v<U*, T*>)
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(static_cast<T*>(p_other.m_ptr))
  {
    // This should never happen but prevents double-free
    p_other.m_ctrl = nullptr;
    p_other.m_ptr = nullptr;
  }

  // Aliasing constructor - create a smart_ref that points to an object within
  // the managed object
  template<typename U>
  smart_ref(smart_ref<U> const& p_other, T* p_ptr) noexcept
    : m_ctrl(p_other.m_ctrl)
    , m_ptr(p_ptr)
  {
    intrusive_ptr_add_ref(m_ctrl);
  }

  // Destructor
  ~smart_ref()
  {
    if (m_ctrl && m_ptr) {
      intrusive_ptr_release(m_ctrl);
    }
  }

  // Copy assignment operator
  smart_ref& operator=(smart_ref const& p_other) noexcept
  {
    if (this != &p_other) {
      intrusive_ptr_release(m_ctrl);
      m_ctrl = p_other.m_ctrl;
      m_ptr = p_other.m_ptr;
      intrusive_ptr_add_ref(m_ctrl);
    }
    return *this;
  }

  // Delete move assignment operator because we do not want to allow invalid
  // moved from objects to exist. A new smart_ref is always a copy.
  smart_ref& operator=(smart_ref&& p_other) noexcept = delete;
  // Delete move assignment operator because we do not want to allow invalid
  // moved from objects to exist. A new smart_ref is always a copy.
  smart_ref(smart_ref&& p_other) noexcept = delete;

  // Swap function
  void swap(smart_ref& p_other) noexcept
  {
    std::swap(m_ctrl, p_other.m_ctrl);
    std::swap(m_ptr, p_other.m_ptr);
  }

  // Dereference operators
  T& operator*() const noexcept
  {
    return *m_ptr;
  }

  T* operator->() const noexcept
  {
    return m_ptr;
  }

  // Get reference count (for testing)
  auto use_count() const noexcept
  {
    return m_ctrl->count.load(std::memory_order_relaxed);
  }

private:
  // Internal constructor with control block and pointer - used by make() and
  // aliasing
  smart_ref(ref_info* p_ctrl, T* p_ptr) noexcept
    : m_ctrl(p_ctrl)
    , m_ptr(p_ptr)
  {
  }

  ref_info* m_ctrl;
  T* m_ptr;

  // Friend declarations for cross-type access
  template<typename U>
  friend class smart_ref;
};

// Non-member swap
template<typename T>
void swap(smart_ref<T>& p_lhs, smart_ref<T>& p_rhs) noexcept
{
  p_lhs.swap(p_rhs);
}

// Equality operators
template<typename T, typename U>
bool operator==(smart_ref<T> const& p_lhs, smart_ref<U> const& p_rhs) noexcept
{
  return p_lhs.operator->() == p_rhs.operator->();
}

template<typename T, typename U>
bool operator!=(smart_ref<T> const& p_lhs, smart_ref<U> const& p_rhs) noexcept
{
  return !(p_lhs == p_rhs);
}

// Factory function to create a smart_ref with its control block
template<class T, typename... Args>
inline smart_ref<T> make_shared_ref(
  std::pmr::polymorphic_allocator<byte> p_alloc,
  Args&&... args)
{
  using rc_t = detail::rc<T>;
  auto* obj = p_alloc.new_object<rc_t>(p_alloc, std::forward<Args>(args)...);
  return smart_ref(&obj->m_info, &obj->m_object);
}
}  // namespace hal
