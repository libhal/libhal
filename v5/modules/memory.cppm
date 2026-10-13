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

namespace hal::inline v5 {

/// Polymorphic allocator for dynamic memory allocation
///
/// A `std::pmr::polymorphic_allocator<>` that enables flexible memory resource
/// management. Can be configured to use custom memory resources for different
/// allocation strategies (arena, monotonic, unsynchronize, etc).
export using allocator = std::pmr::polymorphic_allocator<>;

/// Enable Pimpl (Private Implementation) pattern for hiding implementation
///
/// Base class for implementing the Pimpl idiom, commonly used by manager
/// objects that own hardware or resources. Hides platform-specific or
/// complex implementation details in a private `impl` struct. The derived
/// class must define a nested `impl` type.
/// @tparam Derived The derived manager class implementing Pimpl
/// @note Define a nested `struct impl` in the .cpp file only. Call
///       initialize_pimpl(allocator, ...) in the constructor to allocate
///       and initialize the impl object. Use impl() to access it.
export template<typename Derived>
class pimpl
{
protected:
  using destroy_fn_t = void(void*, allocator) noexcept;

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
    // The impl memory must always be returned to the allocator, even when the
    // impl is trivially destructible. This check exists in the event that the
    // impl was never allocated.
    if (m_impl != nullptr) {
      using impl_type = typename Derived::impl;
      allocator(m_allocator).delete_object(static_cast<impl_type*>(m_impl));
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
