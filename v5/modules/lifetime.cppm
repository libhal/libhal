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
 * Non-owning references with compile-time lifetime checks, plus
 * static-storage / scoped-lifetime slots for reusable peripherals.
 *
 *  static_ref<T>      proven static storage (consteval), or assume_static()
 *  ref<T>             borrow of static, automatic, or leased objects
 *  static_slot<T>     static storage constructed/destroyed repeatedly
 *  lease<T>           RAII owner of a slot's object; destroying it frees the
 *                     slot
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
 * storage or a static_slot. Allocator and placement paths (std::construct_at,
 * pmr, std::optional, static_slot, `new (buffer) T`) still work.
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
// static_slot / lease
// ---------------------------------------------------------------------------

/// @brief Thrown by static_slot::emplace() when the slot is already leased.
export struct slot_busy : std::exception
{
  /// @return char const* - description of the error
  [[nodiscard]] char const* what() const noexcept override
  {
    return "hal::static_slot is already in use";
  }
};

export template<class T>
class lease;

/**
 * @brief Type-erased base of static_slot<T>, used by lease<T>
 */
export class slot_base
{
public:
  slot_base(slot_base const&) = delete;
  slot_base& operator=(slot_base const&) = delete;

  /// @return true - if the slot currently holds a leased object
  [[nodiscard]] constexpr bool in_use() const noexcept
  {
    return m_in_use;
  }

protected:
  using destroy_fn = void (*)(slot_base&) noexcept;

  constexpr explicit slot_base(destroy_fn p_destroy) noexcept
    : m_destroy(p_destroy)
  {
  }
  ~slot_base() = default;

  [[noreturn]] static void busy()
  {
#if defined(__cpp_exceptions)
    throw slot_busy{};
#else
    std::terminate();
#endif
  }

  destroy_fn m_destroy;
  bool m_in_use = false;

  template<class>
  friend class lease;
};

/**
 * @brief Static storage for one T whose lifetime is controlled at runtime
 *
 * emplace() constructs T in place and returns a lease. When the lease is
 * destroyed, T's destructor runs, which is where a driver resets its
 * peripheral, and the slot can be emplaced again. Declare it static, usually
 * function-local inside the factory for that peripheral:
 *
 *   hal::lease<stm32::i2c> acquire_i2c1() {
 *     static constinit hal::static_slot<stm32::i2c> slot;
 *     return slot.emplace(stm32::peripheral::i2c1);
 *   }
 *
 * @tparam T - the type stored in the slot
 */
export template<class T>
class static_slot : public slot_base
{
public:
  /// Construct an empty slot (constinit-friendly)
  constexpr static_slot() noexcept
    : slot_base(&destroy)
  {
  }

  /**
   * @brief Construct T in the slot and lease it
   *
   * @param p_args - arguments forwarded to T's constructor
   * @return lease<T> - owner of the new object
   * @throws hal::slot_busy - if the slot is already leased
   */
  template<class... Args>
  [[nodiscard]] lease<T> emplace(Args&&... p_args)
  {
    if (m_in_use) {
      busy();
    }
    T* obj = std::construct_at(reinterpret_cast<T*>(m_storage),
                               std::forward<Args>(p_args)...);
    m_in_use = true;  // only after construction succeeded
    return lease<T>(obj, *this);
  }

private:
  static void destroy(slot_base& p_base) noexcept
  {
    auto& self = static_cast<static_slot&>(p_base);
    std::destroy_at(std::launder(reinterpret_cast<T*>(self.m_storage)));
    self.m_in_use = false;
  }

  alignas(T) std::byte m_storage[sizeof(T)]{};
};

/**
 * @brief Unique owner of an object living in a static_slot
 *
 * Move-only, two pointers. lease<Derived> converts to lease<Base>; the slot
 * still destroys the complete Derived object, so Base needs no virtual
 * destructor. A lease ends only when it goes out of scope: there is no
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
    , m_slot(std::exchange(p_other.m_slot, nullptr))
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
    , m_slot(std::exchange(p_other.m_slot, nullptr))
  {
  }

  lease& operator=(lease&&) = delete;

  /// Destroy the object (resetting the peripheral) and free the slot
  ~lease()
  {
    if (auto* slot = std::exchange(m_slot, nullptr)) {
      m_ptr = nullptr;
      slot->m_destroy(*slot);
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
  constexpr lease(T* p_ptr, slot_base& p_slot) noexcept
    : m_ptr(p_ptr)
    , m_slot(&p_slot)
  {
  }

  T* m_ptr;
  slot_base* m_slot;

  template<class>
  friend class lease;
  template<class>
  friend class static_slot;
};

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
 *
 * Rejected outright: temporaries, temporary leases, unique_ptr, raw pointers.
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
    m_ptr = p_other.m_ptr;
    HAL_LIFETIME_CHECK(this, m_ptr);
    return *this;
  }
#endif

  ref(T&&) = delete;
  template<class U>
  ref(lease<U>&&) = delete;
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
