#pragma once

#include <array>
#include <span>
#include <utility>

#include "error.hpp"
#include "units.hpp"

namespace hal {

template<typename T>
class tracked_ptr;

template<typename T>
concept has_fatal_fallback = requires(T a) {
  { T::fatal_observer };
};

template<class T, usize N>
class observer;

template<class T>
class observer_container
{
private:
  template<class U, usize N>
  friend class observer;

  template<typename U>
  friend class tracked_ptr;

  template<typename... Args>
  explicit observer_container(usize p_length, Args&&... args)
    : m_length(p_length)
    , m_object(args...)
  {
    // TODO(kammce): static assert if T doesn't have a fatal type
  }

  std::span<void*> get_registry()
  {
    // Calculate offset to the observer instance containing this container
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
class observer
{
public:
  template<typename U>
  friend class tracked_ptr;

  template<class U>
  friend class observer_container;

  template<typename... Args>
  explicit observer(Args&&... args)
    : m_container(N, args...)
  {
    // TODO(kammce): static assert if T doesn't have a fatal type
  }

  ~observer()
  {
    // Redirect all tracked pointers to the fatal observer
    for (auto& pointer : m_registry) {
      if (pointer) {
        // Update the pointer to point to the fatal observer
        pointer->reset();
      }
    }
  }

  auto get_tracked_pointer()
  {
    return tracked_ptr<T>(m_container);
  }

  observer_container<T>* get_observer_container()
  {
    return &m_container;
  }

private:
  std::array<tracked_ptr<T>*, N> m_registry = {};
  observer_container<T> m_container;
  // Pointers to the tracked_ptr's. We use a void* because the tracked pointers
  // could be either T or some base class of T
};

// Helper type for single-observer case
template<typename T>
using unique_observer = observer<T, 1>;

template<typename T>
class tracked_ptr
{
public:
  template<typename U, usize N>
  friend class observer;

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
      m_observer = p_other.m_observer;

      // Find the entry within the m_registry belonging to the p_other and
      // replace it with ourselves.
      for (auto& pointer : m_observer->get_registry()) {
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
#if 0
  template<class Other>
  tracked_ptr operator=(tracked_ptr<Other>&& p_other)
  {
    if (this != &p_other) {
      // Take ownership of the other pointer's data
      m_observer = p_other.m_observer;

      // Find the entry within the m_registry belonging to the p_other and
      // replace it with ourselves.
      for (auto const& pointer : m_observer->get_registry()) {
        if (pointer == &p_other) {
          pointer = this;
          break;
        }
      }

      // Set the other pointer's data to the fatal type
      p_other.m_observer = T::fatal_observer().get_observer_container();
    }
    return *this;
  }
#endif

  ~tracked_ptr()
  {
    // Find an empty slot in the registry
    for (auto& pointer : m_observer->get_registry()) {
      if (pointer == this) {
        pointer = nullptr;
        return;
      }
    }
  }

  // Access the object using the observer's m_object
  [[nodiscard]] T* operator->() const
  {
    return &m_observer->m_object;
  }

  [[nodiscard]] T& operator*() const
  {
    return m_observer->m_object;
  }

  // Check if pointer is valid
  operator bool() const noexcept
  {
    // TODO(kammce): check if the m_observer pointes to the fatal observer.
    return m_observer == T::fatal_observer().get_observer_container();
  }

  void reset()
  {
    // Set the other pointer's data to the fatal type
    // TODO(kammce): Make this work polymorphically
    m_observer = reinterpret_cast<observer_container<T>*>(
      T::fatal_observer().get_observer_container());
    // TODO(kammce): This should also remove itself from the registry
  }

  // Get the raw pointer
  [[nodiscard]] T* get() const noexcept
  {
    return &m_observer->m_object;
  }

private:
  tracked_ptr(observer_container<T>& p_observer)
    : m_observer(&p_observer)
  {
    // Find an empty slot in the registry
    for (auto& pointer : m_observer->get_registry()) {
      if (pointer == nullptr) {
        pointer = this;
        return;
      }
    }
    // If we get here, the registry is full
    hal::safe_throw(hal::device_or_resource_busy(this));
  }

  /// Assume that this pointer is never null and never invalid
  observer_container<T>* m_observer;
};
}  // namespace hal
