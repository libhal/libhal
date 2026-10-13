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

#include <array>
#include <cstddef>
#include <exception>
#include <memory>
#include <memory_resource>
#include <new>
#include <type_traits>
#include <utility>

// Set to 1 to enable the debug-only runtime lifetime checks in `hal::ref`.
// This changes `hal::ref`'s ABI, so every part of an image must agree.
#if !defined(HAL_LIFETIME_CHECKS)
#define HAL_LIFETIME_CHECKS 0
#endif

export module hal:lifetime;

/**
 * Non-owning references with compile-time lifetime checks, plus static
 * storage for reusable peripherals whose lifetime is controlled at runtime.
 *
 *  static_ref<T>      proven static storage (consteval), or assume_static()
 *  ref<T>             borrow of static, automatic, or leased objects
 *  static_optional<T> std::optional-like static storage; emplace() leases
 *  lease<T>           RAII owner of a static_optional's object; destroying it
 *                     empties the static_optional
 *  owner<T>           RAII owner of an object allocated from a
 *                     std::pmr::memory_resource; may hold a derived type
 *  no_bare_new_mixin  mixin that blocks `new T` / make_unique but not
 *                     allocators
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
 * storage or a static_optional. Allocator and placement paths
 * (std::construct_at, pmr, std::optional, static_optional, `new (buffer) T`)
 * still work.
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
 * assume_static() is the unchecked escape hatch for memory that is never
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
  [[nodiscard]] static constexpr static_ref assume_static(T& p_ref) noexcept
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
// static_optional / lease
// ---------------------------------------------------------------------------

/// @brief Thrown by static_optional::emplace() when it already holds a value.
export struct already_leased : std::exception
{
  /// @return char const* - description of the error
  [[nodiscard]] char const* what() const noexcept override
  {
    return "hal::static_optional is already leased";
  }
};

export template<class T>
class lease;

/**
 * @brief Type-erased base of static_optional<T>, used by lease<T>
 */
export class static_optional_base
{
public:
  static_optional_base(static_optional_base const&) = delete;
  static_optional_base& operator=(static_optional_base const&) = delete;

  /// @return true - if it currently holds a leased object
  [[nodiscard]] constexpr bool has_value() const noexcept
  {
    return m_has_value;
  }

protected:
  using destroy_fn = void (*)(static_optional_base&) noexcept;

  constexpr explicit static_optional_base(destroy_fn p_destroy) noexcept
    : m_destroy(p_destroy)
  {
  }
  ~static_optional_base() = default;

  [[noreturn]] static void busy()
  {
#if defined(__cpp_exceptions)
    throw already_leased{};
#else
    std::terminate();
#endif
  }

  destroy_fn m_destroy;
  bool m_has_value = false;

  template<class>
  friend class lease;
};

/**
 * @brief A std::optional<T> in static memory that hands out leases
 *
 * Like std::optional, it is either empty or holds one T. emplace() constructs
 * T in place, but instead of a reference it returns a lease. When the lease is
 * destroyed, T's destructor runs, which is where a driver resets its
 * peripheral, and the static_optional is empty and can be emplaced again.
 * Declare it static, usually function-local inside the factory for that
 * peripheral:
 *
 *   hal::lease<stm32::i2c> acquire_i2c1() {
 *     static constinit hal::static_optional<stm32::i2c> storage;
 *     return storage.emplace(stm32::peripheral::i2c1);
 *   }
 *
 * @tparam T - the type it can hold
 */
export template<class T>
class static_optional : public static_optional_base
{
public:
  /// Construct an empty static_optional (constinit-friendly)
  constexpr static_optional() noexcept
    : static_optional_base(&destroy)
  {
  }

  /**
   * @brief Construct T in place and lease it
   *
   * @param p_args - arguments forwarded to T's constructor
   * @return lease<T> - owner of the new object
   * @throws hal::already_leased - if it already holds a leased object
   */
  template<class... Args>
  [[nodiscard]] lease<T> emplace(Args&&... p_args)
  {
    if (m_has_value) {
      busy();
    }
    T* obj = std::construct_at(reinterpret_cast<T*>(m_storage.data()),
                               std::forward<Args>(p_args)...);
    m_has_value = true;  // only after construction succeeded
    return lease<T>(obj, *this);
  }

private:
  static void destroy(static_optional_base& p_base) noexcept
  {
    auto& self = static_cast<static_optional&>(p_base);
    std::destroy_at(std::launder(reinterpret_cast<T*>(self.m_storage.data())));
    self.m_has_value = false;
  }

  alignas(T) std::array<std::byte, sizeof(T)> m_storage{};
};

/**
 * @brief Unique owner of an object living in a static_optional
 *
 * Move-only, two pointers. lease<Derived> converts to lease<Base>; the
 * static_optional still destroys the complete Derived object, so Base needs no
 * virtual destructor. A lease ends only when it goes out of scope: there is no
 * reset(), because releasing a lease early is the one operation the lifetime
 * analysis cannot see while a composite still borrows it.
 *
 * @tparam T - the leased type
 */
export template<class T>
class lease
{
public:
  /**
   * @brief Take ownership from another lease
   *
   * @param p_other - lease that is left empty
   */
  lease(lease&& p_other) noexcept
    : m_ptr(std::exchange(p_other.m_ptr, nullptr))
    , m_origin(std::exchange(p_other.m_origin, nullptr))
  {
  }

  /**
   * @brief Take ownership from a lease of a derived type
   *
   * @param p_other - lease that is left empty
   */
  template<pointer_convertible_to<T> U>
  lease(lease<U>&& p_other) noexcept
    : m_ptr(std::exchange(p_other.m_ptr, nullptr))
    , m_origin(std::exchange(p_other.m_origin, nullptr))
  {
  }

  lease& operator=(lease&&) = delete;

  /// Destroy the object (resetting the peripheral) and empty the
  /// static_optional
  ~lease()
  {
    if (auto* origin = std::exchange(m_origin, nullptr)) {
      m_ptr = nullptr;
      origin->m_destroy(*origin);
    }
  }

  /// @return true - if this lease owns an object (it was not moved from)
  [[nodiscard]] explicit operator bool() const noexcept
  {
    return m_ptr != nullptr;
  }

  // Dereferencing a temporary lease would hand out a reference to an object
  // destroyed at the end of the full-expression, so rvalue access is deleted.

  /// @return T& - the leased object
  [[nodiscard]] T& operator*() const& noexcept [[clang::lifetimebound]]
  {
    return *m_ptr;
  }
  /// @return T* - the leased object
  [[nodiscard]] T* operator->() const& noexcept [[clang::lifetimebound]]
  {
    return m_ptr;
  }
  T& operator*() const&& = delete;
  T* operator->() const&& = delete;

private:
  constexpr lease(T* p_ptr, static_optional_base& p_origin) noexcept
    : m_ptr(p_ptr)
    , m_origin(&p_origin)
  {
  }

  T* m_ptr;
  static_optional_base* m_origin;

  template<class>
  friend class lease;
  template<class>
  friend class static_optional;
};

// ---------------------------------------------------------------------------
// owner
// ---------------------------------------------------------------------------

export template<class T>
class owner;

/**
 * @brief Header placed in front of every object an owner<T> holds
 *
 * Records how to destroy the complete object and which memory resource to
 * return its storage to, so owner<Base> can destroy a Derived without a
 * virtual destructor.
 */
export class owner_control
{
public:
  owner_control(owner_control const&) = delete;
  owner_control& operator=(owner_control const&) = delete;

protected:
  using destroy_fn = void (*)(owner_control&) noexcept;

  constexpr owner_control(destroy_fn p_destroy,
                          std::pmr::memory_resource& p_resource) noexcept
    : m_destroy(p_destroy)
    , m_resource(&p_resource)
  {
  }
  ~owner_control() = default;

  destroy_fn m_destroy;
  std::pmr::memory_resource* m_resource;

  template<class>
  friend class owner;
};

/**
 * @brief Unique owner of an object allocated from a memory resource
 *
 * Like lease, but for objects that live in memory obtained from a
 * std::pmr::memory_resource rather than in a static_optional. Use it when a
 * factory must hand out an object whose concrete type stays private, for
 * example a manager returning `hal::owner<hal::serial>` while the type that
 * implements it is only defined in the manager's `.cpp`. The caller picks
 * where the memory comes from by choosing the allocator.
 *
 * Move-only, two pointers, never empty except after being moved from.
 * owner<Derived> converts to owner<Base>; the complete Derived object is still
 * destroyed and its storage returned to the same resource, so Base needs no
 * virtual destructor. Like lease, an owner ends only when it goes out of
 * scope. To end one early, hold it in a std::optional.
 *
 * Create one with hal::make_owner<T>().
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

  /// Destroy the complete object and return its storage to its resource
  ~owner()
  {
    if (auto* control = std::exchange(m_control, nullptr)) {
      m_ptr = nullptr;
      control->m_destroy(*control);
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
  template<class U, class... Args>
  friend owner<U> make_owner(std::pmr::polymorphic_allocator<> p_allocator,
                             Args&&... p_args);
};

/// Storage for an owner's object, placed right after its owner_control header
template<class T>
class owner_block final : public owner_control
{
public:
  template<class... Args>
  explicit owner_block(std::pmr::memory_resource& p_resource, Args&&... p_args)
    : owner_control(&destroy, p_resource)
    , object(std::forward<Args>(p_args)...)
  {
  }

  static void destroy(owner_control& p_control) noexcept
  {
    auto* self = static_cast<owner_block*>(&p_control);
    std::pmr::polymorphic_allocator<> allocator(self->m_resource);
    allocator.delete_object(self);
  }

  T object;
};

/**
 * @brief Allocate and construct a T, returning its owner
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
  auto* resource = p_allocator.resource();
  auto* storage = p_allocator.allocate_object<owner_block<T>>();
  try {
    auto* created =
      std::construct_at(storage, *resource, std::forward<Args>(p_args)...);
    return owner<T>(&created->object, *created);
  } catch (...) {
    p_allocator.deallocate_object(storage);
    throw;
  }
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
 *  - from lease<U>&:     tied to the lease via lifetimebound (Clang)
 *  - from owner<U>&:     tied to the owner via lifetimebound (Clang)
 *
 * Rejected outright: temporaries, temporary leases and owners, unique_ptr, raw
 * pointers.
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
   * @brief Borrow a leased object; the ref must not outlive the lease
   *
   * @param p_lease - lease of a type convertible to T
   */
  template<pointer_convertible_to<T> U>
  constexpr ref(lease<U> const& p_lease [[clang::lifetimebound]]) noexcept
    : m_ptr(&*p_lease)
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
  ref(lease<U>&&) = delete;
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
