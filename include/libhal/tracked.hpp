#pragma once

#include <array>
#include <concepts>
#include <span>
#include <utility>

#include "error.hpp"
#include "units.hpp"

namespace hal {

template<typename T>
class tracked_ptr;

template<typename T>
concept has_fatal_fallback = requires(T a) {
  { T::fatal_tracked };
};

template<class T, usize N>
class tracked;

template<class T>
class tracked_container
{
private:
  template<class U, usize N>
  friend class tracked;

  template<typename U>
  friend class tracked_ptr;

  template<typename... Args>
  explicit tracked_container(usize p_length, Args&&... args)
    : m_length(p_length)
    , m_object(args...)
  {
    // TODO(kammce): static assert if T doesn't have a fatal type
  }

  std::span<void*> get_registry()
  {
    // Calculate offset to the tracked instance containing this container
    byte* container_ptr = reinterpret_cast<byte*>(this);
    // The registry is located right before the container in memory layout
    void** registry_ptr =
      reinterpret_cast<void**>(container_ptr - (m_length * sizeof(void*)));

    return std::span<void*>{ registry_ptr, m_length };
  }

  usize m_length;
  T m_object;
};

template<class T, usize N>
class tracked
{
public:
  template<typename U>
  friend class tracked_ptr;

  template<class U>
  friend class tracked_container;

  template<typename... Args>
  explicit tracked(Args&&... args)
    : m_container(N, args...)
  {
    // TODO(kammce): static assert if T doesn't have a fatal type
  }

  ~tracked()
  {
    // Redirect all tracked pointers to the fatal tracked
    for (auto& pointer : m_registry) {
      if (pointer) {
        // Update the pointer to point to the fatal tracked
        pointer->reset();
      }
    }
  }

  auto get_tracked_pointer()
  {
    return tracked_ptr<T>(m_container);
  }

  tracked_container<T>* get_tracked_container()
  {
    return &m_container;
  }

private:
  std::array<tracked_ptr<T>*, N> m_registry = {};
  tracked_container<T> m_container;
  // Pointers to the tracked_ptr's. We use a void* because the tracked pointers
  // could be either T or some base class of T
};

// Helper type for single-tracked case
template<typename T>
using unique_tracked = tracked<T, 1>;

template<typename T>
class tracked_ptr
{
public:
  template<typename U, usize N>
  friend class tracked;

  using invalidated = typename T::invalidated;

  template<typename U, usize N>
  tracked_ptr(tracked<U, N>& p_other) noexcept
  {
    static_assert(std::is_convertible_v<U, T>,
                  "The tracked type is not converted to this type.");
    *this = std::move(p_other.get_tracked_pointer());
  }
  template<typename U>
  tracked_ptr(tracked_ptr<U>&& p_other) noexcept
  {
    static_assert(std::is_convertible_v<U, T>,
                  "The tracked type is not converted to this type.");
    *this = std::move(p_other);
  }
  tracked_ptr(tracked_ptr const&) = delete;
  tracked_ptr& operator=(tracked_ptr const&) = delete;
  tracked_ptr(tracked_ptr&& p_other) noexcept
  {
    *this = std::move(p_other);
  }

  tracked_ptr& operator=(tracked_ptr&& p_other) noexcept
  {
    if (this != &p_other) {
      // Take ownership of the other pointer's data
      m_tracked = p_other.m_tracked;

      // Find the entry within the m_registry belonging to the p_other and
      // replace it with ourselves.
      for (auto& pointer : m_tracked->get_registry()) {
        if (pointer == &p_other) {
          pointer = this;
          break;
        }
      }

      // Set the other pointer's data to the fatal type
      // TODO(kammce): Make this work polymorphically
      p_other.reset();
    }

    return *this;
  }

  ~tracked_ptr()
  {
    // Find an empty slot in the registry
    for (auto& pointer : m_tracked->get_registry()) {
      if (pointer == this) {
        pointer = nullptr;
        return;
      }
    }
  }

  // Access the object using the tracked's m_object
  [[nodiscard]] T* operator->() const
  {
    return &m_tracked->m_object;
  }

  [[nodiscard]] T& operator*() const
  {
    return m_tracked->m_object;
  }

  // Check if pointer is valid
  operator bool() const noexcept
  {
    // TODO(kammce): check if the m_tracked pointes to the fatal tracked.
    return m_tracked != invalidated_type_container_pointer();
  }

  void reset()
  {
    // Set the other pointer's data to the fatal type
    m_tracked = invalidated_type_container_pointer();
    // TODO(kammce): Make this work polymorphically
    // TODO(kammce): This should also remove itself from the registry
  }

  // Get the raw pointer
  [[nodiscard]] T* get() const noexcept
  {
    return &m_tracked->m_object;
  }

private:
  static tracked_container<invalidated>* invalidated_type_container_pointer()
  {
    static tracked<invalidated, 1> invalidated_object_replacement;
    return &invalidated_object_replacement.m_container;
  }

  tracked_ptr(tracked_container<T>& p_tracked)
    : m_tracked(&p_tracked)
  {
    // Find an empty slot in the registry
    for (auto& pointer : m_tracked->get_registry()) {
      if (pointer == nullptr) {
        pointer = this;
        return;
      }
    }
    // If we get here, the registry is full
    hal::safe_throw(hal::device_or_resource_busy(this));
  }

  /// Assume that this pointer is never null and never invalid
  tracked_container<T>* m_tracked;
};
}  // namespace hal
