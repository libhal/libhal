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

#include <cstddef>
#include <exception>
#include <memory>
#include <memory_resource>
#include <new>
#include <system_error>
#include <type_traits>
#include <utility>

// Set to 1 to enable the debug-only runtime lifetime checks in `hal::ref`.
// This changes `hal::ref`'s ABI, so every part of an image must agree.
#if !defined(HAL_LIFETIME_CHECKS)
#define HAL_LIFETIME_CHECKS 0
#endif

export module hal:lifetime;

import :error;

/**
 * References with compile-time lifetime checks, and one owner type for
 * objects whose lifetime is controlled at runtime.
 *
 * Borrowing:
 *  static_ref<T>      proven static storage (consteval), or
 * unsafe_assume_static() ref<T>             borrow of a static, local, or owned
 * object
 *
 * Owning:
 *  owner<T>           RAII owner; destroying it ends the object
 *  reusable<T>        static storage for one T; emplace() returns its owner
 *  make_owner<T>()    allocates a T from a memory resource; returns its owner
 *
 *  no_bare_new_mixin  opt-in mixin that blocks `new T` / make_unique
 */

#if HAL_LIFETIME_CHECKS
/**
 * Debug-only runtime backstop for what the compiler can't see: an object in
 * static storage holding a ref to an object on the stack (e.g. `static sensor
 * s(local_bus);`). Checked whenever a ref is constructed or copied into place,
 * so it fires at the line that creates the bad binding, not when it later
 * crashes.
 *
 * The platform provides the three hooks. On a Cortex-M these are a few
 * compares against linker-script symbols (_sdata/_ebss, _sstack/_estack).
 * They are attached to the global module (extern "C++") so a platform can
 * define them in an ordinary source file.
 *
 * NOTE: enabling this makes ref<T> non-trivially-copyable, which changes how
 * it is passed (by invisible reference instead of in a register). Everything
 * in one image must be built with the same setting, so make it a package
 * option that affects the package id.
 */
export extern "C++"
{
  namespace hal::inline v5::lifetime_checks {
  /// @return true if p_address is in static storage (.data/.bss)
  bool in_static_memory(void const* p_address) noexcept;
  /// @return true if p_address is on the current stack
  bool in_stack_memory(void const* p_address) noexcept;
  /// Called when a ref in static storage is bound to a stack object
  [[noreturn]] void violation(void const* p_holder,
                              void const* p_target) noexcept;
  }  // namespace hal::inline v5::lifetime_checks
}

namespace hal::inline v5::lifetime_checks {
inline void check(void const* p_holder, void const* p_target) noexcept
{
  if (in_static_memory(p_holder) && in_stack_memory(p_target)) {
    violation(p_holder, p_target);
  }
}
}  // namespace hal::inline v5::lifetime_checks

#define HAL_LIFETIME_CHECK(holder, target)                                     \
  do {                                                                         \
    if (!std::is_constant_evaluated()) {                                       \
      ::hal::lifetime_checks::check(holder, target);                           \
    }                                                                          \
  } while (0)
#else
#define HAL_LIFETIME_CHECK(holder, target) ((void)0)
#endif

namespace hal::inline v5 {

/**
 * @brief Satisfied when a `From*` converts implicitly to a `To*`
 *
 * @tparam From - the source pointee type
 * @tparam To - the destination pointee type
 */
export template<class From, class To>
concept pointer_convertible_to = std::is_convertible_v<From*, To*>;

// ---------------------------------------------------------------------------
// no_bare_new_mixin
// ---------------------------------------------------------------------------

/**
 * @brief Mixin that blocks `new T` and `std::make_unique<T>()`
 *
 * Opt-in: inherit from this in drivers that should never live on the heap,
 * such as platform drivers that own a fixed peripheral and live in static
 * storage or a reusable. Allocator and placement paths (std::construct_at,
 * pmr, std::optional, reusable, `new (buffer) T`) still work.
 *
 * `delete p` is also rejected, so an object created with an explicit `::new`
 * can't be released through the class (which would otherwise destroy it and
 * silently leak its memory). Because of that, a class using this mixin can't
 * have a virtual destructor.
 */
export struct no_bare_new_mixin
{
  /// Deleted: interface objects are not created with a bare `new`.
  static void* operator new(std::size_t) = delete;
  /// Deleted: interface objects are not created with a bare `new[]`.
  static void* operator new[](std::size_t) = delete;
  /**
   * @brief Placement new, restored for `new (buffer) T(...)`
   *
   * Declaring any class-scope operator new hides the global placement form.
   *
   * @param p_ptr - storage to construct the object in
   * @return void* - p_ptr
   */
  static void* operator new(std::size_t, void* p_ptr) noexcept
  {
    return p_ptr;
  }
  /// Deleted: these objects are never allocated with a bare `new`.
  static void operator delete(void*) = delete;

protected:
  ~no_bare_new_mixin() = default;
};

// ---------------------------------------------------------------------------
// static_ref
// ---------------------------------------------------------------------------

/**
 * @brief Non-owning reference to an object with static storage duration
 *
 * The constructor is consteval: the result must be a constant expression, and
 * only addresses of static-storage objects are constant expressions. Locals,
 * heap objects, thread_locals, and runtime references are rejected at the
 * construction site.
 *
 * unsafe_assume_static() is the unchecked escape hatch for memory that is never
 * freed in practice (e.g. a monotonic arena filled at startup).
 *
 * @tparam T - the referenced type
 */
export template<class T>
class static_ref
{
public:
  /**
   * @brief Reference a static-storage object
   *
   * @param p_ref - object with static storage duration
   */
  consteval static_ref(T& p_ref) noexcept
    : m_ptr(&p_ref)
  {
  }

  static_ref(T&&) = delete;

  /**
   * @brief Convert from a static_ref of a derived type
   *
   * @param p_other - static_ref to a type convertible to T
   */
  template<pointer_convertible_to<T> U>
  constexpr static_ref(static_ref<U> const& p_other) noexcept
    : m_ptr(&p_other.get())
  {
  }

  /**
   * @brief Unchecked escape hatch, named so it is easy to search for
   *
   * @param p_ref - object the caller guarantees is never destroyed or freed
   * @return static_ref - reference to p_ref
   */
  [[nodiscard]] static constexpr static_ref unsafe_assume_static(
    T& p_ref) noexcept
  {
    return static_ref(unchecked{}, p_ref);
  }

  /// @return T& - the referenced object
  [[nodiscard]] constexpr T& get() const noexcept
  {
    return *m_ptr;
  }
  /// @return T& - the referenced object
  [[nodiscard]] constexpr T& operator*() const noexcept
  {
    return *m_ptr;
  }
  /// @return T* - the referenced object
  [[nodiscard]] constexpr T* operator->() const noexcept
  {
    return m_ptr;
  }

private:
  struct unchecked
  {};
  constexpr static_ref(unchecked, T& p_ref) noexcept
    : m_ptr(&p_ref)
  {
  }

  T* m_ptr;
};

// ---------------------------------------------------------------------------
// owner, reusable, make_owner
// ---------------------------------------------------------------------------

export template<class T>
class owner;

/**
 * @brief Records how to end the object an owner<T> holds
 *
 * Internal. Each place an owner's object can live supplies one: a reusable<T>
 * (destroy the object and mark the storage empty) or an allocation made by
 * make_owner() (destroy the object and free the memory). Because the record,
 * not the owner's type, knows the complete object, owner<Base> ends a Derived
 * correctly without a virtual destructor.
 */
class owner_control
{
public:
  owner_control(owner_control const&) = delete;
  owner_control& operator=(owner_control const&) = delete;

protected:
  /// Ends the object and empties or frees its storage. Each derived class
  /// passes its own static release() function to the constructor, and
  /// owner<T> calls it through m_release when it is destroyed.
  using release_fn = void (*)(owner_control&) noexcept;

  constexpr explicit owner_control(release_fn p_release) noexcept
    : m_release(p_release)
  {
  }
  ~owner_control() = default;

  /**
   * @brief Create the owner of an object this record ends
   *
   * owner<T>'s constructor is private; only the places an owned object can
   * live (classes derived from this one) can create an owner.
   *
   * @param p_ptr - the object, as constructed
   * @param p_control - the record that ends it
   * @return owner<T> - owner of p_ptr
   */
  template<class T>
  static constexpr owner<T> adopt(T* p_ptr, owner_control& p_control) noexcept
  {
    return owner<T>(p_ptr, p_control);
  }

  release_fn m_release;

  template<class>
  friend class owner;
};

/**
 * @brief Unique owner of an object
 *
 * Create one by filling a `hal::reusable<T>` with emplace(), or by allocating
 * with `hal::make_owner<T>()`. Destroying the owner runs T's destructor, which
 * is where a driver resets its peripheral, and then empties the reusable or
 * frees the allocation.
 *
 * Move-only, two pointers, never empty except after being moved from.
 * owner<Derived> converts to owner<Base>; the complete Derived object is still
 * ended correctly, so Base needs no virtual destructor. An owner ends only when
 * it goes out of scope: there is no reset(), because ending it early is the one
 * operation the lifetime analysis cannot see while a composite still borrows
 * it. To end one early anyway, hold it in a std::optional.
 *
 * @tparam T - the owned type, or a base of it
 */
export template<class T>
class [[gsl::Owner(T)]] owner
{
public:
  /**
   * @brief Take ownership from another owner
   *
   * @param p_other - owner that is left empty
   */
  owner(owner&& p_other) noexcept
    : m_ptr(std::exchange(p_other.m_ptr, nullptr))
    , m_control(std::exchange(p_other.m_control, nullptr))
  {
  }

  /**
   * @brief Take ownership from an owner of a derived type
   *
   * @param p_other - owner that is left empty
   */
  template<pointer_convertible_to<T> U>
  owner(owner<U>&& p_other) noexcept
    : m_ptr(std::exchange(p_other.m_ptr, nullptr))
    , m_control(std::exchange(p_other.m_control, nullptr))
  {
  }

  owner& operator=(owner&&) = delete;

  /// End the complete object: destroy it, then empty or free its storage
  ~owner()
  {
    if (auto* control = std::exchange(m_control, nullptr)) {
      m_ptr = nullptr;
      control->m_release(*control);
    }
  }

  /// @return true - if this owner holds an object (it was not moved from)
  [[nodiscard]] explicit operator bool() const noexcept
  {
    return m_ptr != nullptr;
  }

  // Dereferencing a temporary owner would hand out a reference to an object
  // destroyed at the end of the full-expression, so rvalue access is deleted.

  /// @return T& - the owned object
  [[nodiscard]] T& operator*() const& noexcept [[clang::lifetimebound]]
  {
    return *m_ptr;
  }
  /// @return T* - the owned object
  [[nodiscard]] T* operator->() const& noexcept [[clang::lifetimebound]]
  {
    return m_ptr;
  }
  T& operator*() const&& = delete;
  T* operator->() const&& = delete;

private:
  constexpr owner(T* p_ptr, owner_control& p_control) noexcept
    : m_ptr(p_ptr)
    , m_control(&p_control)
  {
  }

  T* m_ptr;
  owner_control* m_control;

  template<class>
  friend class owner;
  friend class owner_control;
};

/// @brief Thrown by emplace() when a reusable<T> already holds an object
export struct reusable_busy : public exception
{
  /**
   * @brief Construct a new reusable_busy exception
   *
   * @param p_instance - the reusable storage that is already in use
   */
  explicit reusable_busy(void const* p_instance)
    : exception(std::errc::device_or_resource_busy, p_instance)
  {
  }

  /// @return char const* - "hal::reusable_busy"
  [[nodiscard]] char const* what() const noexcept final
  {
    return "hal::reusable_busy";
  }
};

/**
 * @brief Storage for one T that can be created, destroyed, and created again
 *
 * Holds no object or one object. Fill it with emplace(), which constructs T in
 * place and returns its owner. When the owner is destroyed, T's destructor
 * runs, which is where a driver resets its peripheral, and the storage is
 * empty again.
 *
 * A reusable<T> must have static storage duration, and emplace() enforces it:
 * it takes the storage as a static_ref, so passing a local fails to compile.
 * Declare it `static constinit`, usually inside the factory for that
 * peripheral:
 *
 *   hal::owner<stm32::i2c> acquire_i2c1() {
 *     static constinit hal::reusable<stm32::i2c> storage;
 *     return emplace(storage, stm32::peripheral::i2c1);
 *   }
 *
 * emplace() is found through the argument, so call it unqualified, as shown.
 * Its other arguments are ordinary runtime values; only the storage must be
 * static.
 *
 * @tparam T - the type it can hold
 */
export template<class T>
class reusable : public owner_control
{
public:
  /// Construct empty storage (constinit-friendly)
  constexpr reusable() noexcept
    : owner_control(&release)
  {
  }

  reusable(reusable const&) = delete;
  reusable& operator=(reusable const&) = delete;

  /// Trivial when T is, so a static reusable needs no exit-time destructor
  ~reusable()
    requires std::is_trivially_destructible_v<T>
  = default;
  /// Does not end a held object: the owner does that
  ~reusable()
  {
  }

  /// @return true - if it currently holds an object
  [[nodiscard]] constexpr bool has_value() const noexcept
  {
    return m_has_value;
  }

  /**
   * @brief Construct a T in static storage and return its owner
   *
   * @param p_storage - the storage, which must have static storage duration.
   * Passing a local or a runtime reference fails to compile.
   * @param p_args - arguments forwarded to T's constructor
   * @return owner<T> - owner of the new object
   * @throws hal::reusable_busy - if the storage already holds an object
   */
  template<class... Args>
  [[nodiscard]] friend owner<T> emplace(static_ref<reusable> p_storage,
                                        Args&&... p_args)
  {
    auto& self = *p_storage;
    if (self.m_has_value) {
      throw reusable_busy(&self);
    }
    T* object =
      std::construct_at(&self.m_object, std::forward<Args>(p_args)...);
    self.m_has_value = true;  // only after construction succeeded
    return adopt(object, self);
  }

private:
  /// Stored in m_release by the constructor; called when the owner ends.
  /// Destroys the object and marks the storage empty.
  static void release(owner_control& p_control) noexcept
  {
    auto& self = static_cast<reusable&>(p_control);
    std::destroy_at(&self.m_object);
    self.m_has_value = false;
  }

  // A union member holds no object until construct_at() creates one, like
  // std::optional's storage. Unlike a byte array, it needs no
  // reinterpret_cast or std::launder to reach the object.
  union
  {
    T m_object;
  };
  bool m_has_value = false;
};

/// Internal: an allocation made by make_owner(), header first, then the object
template<class T>
class owner_block final : public owner_control
{
public:
  /**
   * @brief Allocate a block, construct T in it, and return T's owner
   *
   * @param p_allocator - allocator to obtain the storage from
   * @param p_args - arguments forwarded to T's constructor
   * @return owner<T> - owner of the new object
   */
  template<class... Args>
  static owner<T> make(std::pmr::polymorphic_allocator<> p_allocator,
                       Args&&... p_args)
  {
    auto* storage = p_allocator.allocate_object<owner_block>();
    try {
      auto* block = std::construct_at(
        storage, *p_allocator.resource(), std::forward<Args>(p_args)...);
      return adopt(&block->m_object, *block);
    } catch (...) {
      p_allocator.deallocate_object(storage);
      throw;
    }
  }

  template<class... Args>
  explicit owner_block(std::pmr::memory_resource& p_resource, Args&&... p_args)
    : owner_control(&release)
    , m_object(std::forward<Args>(p_args)...)
    , m_resource(&p_resource)
  {
  }

private:
  /// Stored in m_release by the constructor; called when the owner ends.
  /// Destroys the object and returns the block to its memory resource.
  static void release(owner_control& p_control) noexcept
  {
    auto* self = static_cast<owner_block*>(&p_control);
    std::pmr::polymorphic_allocator<> allocator(self->m_resource);
    allocator.delete_object(self);
  }

  T m_object;
  std::pmr::memory_resource* m_resource;
};

/**
 * @brief Allocate and construct a T, returning its owner
 *
 * Use this when the object's concrete type should stay private, for example a
 * manager returning `hal::owner<hal::serial>` while the type that implements
 * it is only defined in the manager's `.cpp`. The caller picks where the
 * memory comes from by choosing the allocator.
 *
 * The object and a small header are allocated together from p_allocator's
 * memory resource. If T's constructor throws, the storage is freed and the
 * exception propagates.
 *
 * @tparam T - the type to construct
 * @param p_allocator - allocator to obtain the storage from
 * @param p_args - arguments forwarded to T's constructor
 * @return owner<T> - owner of the new object
 */
export template<class T, class... Args>
[[nodiscard]] owner<T> make_owner(std::pmr::polymorphic_allocator<> p_allocator,
                                  Args&&... p_args)
{
  return owner_block<T>::make(p_allocator, std::forward<Args>(p_args)...);
}

// ---------------------------------------------------------------------------
// ref
// ---------------------------------------------------------------------------

/**
 * @brief Non-owning reference for composites to store
 *
 * For example, a sensor holding its bus:
 *
 *  - from static_ref<U>: proven static, no lifetime tie
 *  - from T&:            tied to the referent via lifetimebound (Clang)
 *  - from owner<U>&:     tied to the owner via lifetimebound (Clang)
 *
 * Rejected outright: temporaries, temporary owners, unique_ptr, raw pointers.
 *
 * Take it by value with [[clang::lifetimebound]] in constructors that capture
 * it:
 *
 *   explicit sensor(hal::ref<hal::i2c> p_bus [[clang::lifetimebound]]);
 *
 * gsl::Pointer makes Clang treat ref as a view, so lifetimebound on a by-value
 * ref parameter ties to what it points at rather than to the ref itself.
 *
 * @tparam T - the referenced type
 */
export template<class T>
class [[gsl::Pointer(T)]] ref
{
public:
  /**
   * @brief Reference a proven-static object
   *
   * @param p_static - static_ref to a type convertible to T
   */
  template<pointer_convertible_to<T> U>
  constexpr ref(static_ref<U> p_static) noexcept
    : m_ptr(&*p_static)
  {
  }

  /**
   * @brief Borrow an object; the ref must not outlive it
   *
   * @param p_ref - object to reference
   */
  constexpr ref(T& p_ref [[clang::lifetimebound]]) noexcept
    : m_ptr(&p_ref)
  {
    HAL_LIFETIME_CHECK(this, m_ptr);
  }

  /**
   * @brief Borrow an owned object; the ref must not outlive the owner
   *
   * @param p_owner - owner of a type convertible to T
   */
  template<pointer_convertible_to<T> U>
  constexpr ref(owner<U> const& p_owner [[clang::lifetimebound]]) noexcept
    : m_ptr(&*p_owner)
  {
    HAL_LIFETIME_CHECK(this, m_ptr);
  }

  /**
   * @brief Convert from a ref of a derived type
   *
   * @param p_other - ref to a type convertible to T
   */
  template<pointer_convertible_to<T> U>
  constexpr ref(ref<U> p_other) noexcept
    : m_ptr(&*p_other)
  {
    HAL_LIFETIME_CHECK(this, m_ptr);
  }

#if HAL_LIFETIME_CHECKS
  // A composite's member is copy-constructed from its constructor parameter,
  // so this is where `static sensor s(local_bus)` gets caught.
  constexpr ref(ref const& p_other) noexcept
    : m_ptr(p_other.m_ptr)
  {
    HAL_LIFETIME_CHECK(this, m_ptr);
  }
  constexpr ref& operator=(ref const& p_other) noexcept
  {
    if (this != &p_other) {
      m_ptr = p_other.m_ptr;
      HAL_LIFETIME_CHECK(this, m_ptr);
    }
    return *this;
  }
#endif

  ref(T&&) = delete;
  template<class U>
  ref(owner<U>&&) = delete;
  template<class D>
  ref(std::unique_ptr<T, D> const&) = delete;
  ref(T*) = delete;

  /// @return T& - the referenced object
  [[nodiscard]] constexpr T& operator*() const noexcept
  {
    return *m_ptr;
  }
  /// @return T* - the referenced object
  [[nodiscard]] constexpr T* operator->() const noexcept
  {
    return m_ptr;
  }

private:
  T* m_ptr;
};

}  // namespace hal::inline v5
