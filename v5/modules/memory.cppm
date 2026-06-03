// Copyright 2026 Khalil Estell and the libhal contributors
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

module;

#include <memory_resource>
#include <type_traits>

export module hal:memory;

import strong_ptr;
import async_context;

namespace hal::inline v5 {

/// Polymorphic allocator for dynamic memory allocation
///
/// A `std::pmr::polymorphic_allocator<>` that enables flexible memory resource
/// management. Can be configured to use custom memory resources for different
/// allocation strategies (arena, monotonic, unsynchronize, etc).
export using allocator = std::pmr::polymorphic_allocator<>;

/// Strong pointer with shared ownership and automatic cleanup
///
/// A reference-counted smart pointer that automatically deletes the managed
/// object when the last pointer to it is destroyed. Thread-safe and moveable.
/// @tparam T The type of object being managed
export template<class T>
using ptr = mem::strong_ptr<T>;

/// Optional strong pointer that may be null
///
/// A nullable reference-counted smart pointer combining the benefits of
/// optional and strong_ptr. Useful for optional owning references.
/// @tparam T The type of object being managed
export template<class T>
using opt_ptr = mem::optional_ptr<T>;

export template<class T = void>
using future = async::future<T>;

/// Async factory result for a managed object
///
/// A future that resolves to a strong_ptr. Used for factory functions that
/// perform async initialization. The caller co_awaits the factory and receives
/// a strong_ptr to the fully initialized object.
/// @tparam T The type of object being created asynchronously
export template<class T>
using future_ptr = future<hal::ptr<T>>;

/// Create a managed object with a strong pointer
///
/// Allocates and constructs a new object with the given allocator,
/// returning a strong_ptr that manages its lifetime.
/// @tparam T The type of object to create
/// @tparam Args Parameter types forwarded to T's constructor
/// @param p_allocator The polymorphic allocator to use for allocation
/// @param p_args Arguments forwarded to T's constructor
/// @return A strong_ptr managing the newly created object
export template<class T, class... Args>
ptr<T> allocate(allocator p_allocator, Args&&... p_args)
{
  return mem::make_strong_ptr<T>(p_allocator, p_args...);
}

/// Create a static-duration managed object with a strong pointer
///
/// Creates a static object and returns a strong_ptr wrapping it.
/// The object persists for the entire program duration and is never deleted.
/// The unique_key template parameter enables multiple static instances of
/// the same type by providing different lambda keys.
/// @tparam T The type of object to create
/// @tparam Args Parameter types forwarded to T's constructor
/// @tparam unique_key A unique lambda to allow multiple static instances
/// @param p_args Arguments forwarded to T's constructor
/// @return A strong_ptr managing the static object
export template<class T, class... Args, auto unique_key = []() {}>
mem::strong_ptr<T> static_allocate(Args... p_args)
{
  static T object(p_args...);
  return mem::strong_ptr<T>(mem::unsafe_assume_static_tag{}, object);
}

/// Enable Pimpl (Private Implementation) pattern for hiding implementation
///
/// Base class for implementing the Pimpl idiom, commonly used by manager
/// objects that own hardware or resources. Hides platform-specific or
/// complex implementation details in a private `impl` struct. Derives from
/// enable_strong_from_this to allow the impl to obtain pointers back to
/// the manager. The derived class must define a nested `impl` type.
/// @tparam Derived The derived manager class implementing Pimpl
/// @note Define a nested `struct impl` in the .cpp file only. Call
///       initialize_pimpl(allocator, ...) in the constructor to allocate
///       and initialize the impl object. Use impl() to access it.
export template<typename Derived>
class pimpl
{
protected:
  using destroy_fn_t = void(void*, allocator) noexcept;

  static consteval bool needs_destruction()
  {
    return not std::is_trivially_destructible_v<typename Derived::impl>;
  }

  /// Access the mutable implementation object
  ///
  /// Returns a reference to the implementation struct allocated by
  /// initialize_pimpl(). Used in public methods of the derived class
  /// to access hardware state and configuration.
  [[nodiscard]] auto& inner() noexcept
  {
    return *static_cast<typename Derived::impl*>(m_impl);
  }

  /// Access the immutable implementation object
  ///
  /// Const version of inner(). Used in const methods of the derived class.
  [[nodiscard]] auto const& inner() const noexcept
  {
    return *static_cast<typename Derived::impl const*>(m_impl);
  }

  /// Initialize the implementation object with custom allocator
  ///
  /// Allocates and constructs the impl struct using the provided allocator
  /// and constructor arguments. Must be called exactly once in the derived
  /// class constructor. The impl object is automatically destroyed when
  /// the manager is destroyed.
  /// @param p_resource The allocator to use for impl allocation
  /// @param p_args Arguments forwarded to the impl struct constructor
  /// @note Call this from the derived class constructor before any
  ///       other operations that access impl()
  template<typename... Args>
  pimpl(allocator p_allocator, Args&&... p_args)
    : m_allocator(p_allocator.resource())
  {
    using impl_type = typename Derived::impl;
    m_impl = p_allocator.new_object<impl_type>(std::forward<Args>(p_args)...);
  }

  ~pimpl() noexcept
  {
    // This check exists in the event that `initialize_pimpl` was never called
    if constexpr (needs_destruction()) {
      if (m_impl != nullptr) {
        using impl_type = typename Derived::impl;
        allocator(m_allocator).delete_object(static_cast<impl_type*>(m_impl));
      }
    }
  }

private:
  friend Derived;
  struct private_key
  {
    friend Derived;

  private:
    private_key() = default;
  };

  pimpl() = default;

  std::pmr::memory_resource* m_allocator;
  void* m_impl = nullptr;
};
}  // namespace hal::inline v5
