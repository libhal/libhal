#pragma once

#include <coroutine>
#include <exception>
#include <memory_resource>
#include <type_traits>
#include <utility>

namespace hal::v5 {
namespace detail {
// Helper type for void
struct void_placeholder
{};

// Type selection for void vs non-void
template<typename T>
using void_to_placeholder_t =
  std::conditional_t<std::is_void_v<T>, void_placeholder, T>;
}  // namespace detail

// Forward declarations
template<typename T>
class task;

class coroutine_context
{
public:
  explicit coroutine_context(std::pmr::memory_resource* p_resource)
    : m_resource(p_resource)
  {
  }

  [[nodiscard]] auto* resource() const
  {
    return m_resource;
  }

private:
  std::pmr::memory_resource* m_resource;
};

// Primary promise type
template<typename T>
class hal_promise_type
{
private:
  using value_type = detail::void_to_placeholder_t<T>;

  std::pmr::memory_resource* m_allocator{};
  std::exception_ptr m_exception = nullptr;
  std::coroutine_handle<> m_continuation = nullptr;
  value_type m_result{};

public:
  // Default constructor - allocator will be set via operator new
  hal_promise_type() = default;

  // Create the task object
  task<T> get_return_object() noexcept;

  // Start executing immediately
  std::suspend_never initial_suspend() noexcept
  {
    return {};
  }

  // Suspend at end
  std::suspend_always final_suspend() noexcept
  {
    return {};
  }

  // Store non-void return value
  template<typename U>
  void return_value(U&& value)
    requires(!std::is_void_v<T>)
  {
    m_result = std::forward<U>(value);
  }

#if 0
  // For void coroutines
  void return_void()
    requires(std::is_void_v<T>)
  {
    // Nothing to do
  }
#endif

  // Handle exceptions
  void unhandled_exception() noexcept
  {
    m_exception = std::current_exception();
  }

  // Get result value or propagate exception
  T& result()
    requires(!std::is_void_v<T>)
  {
    if (m_exception) {
      std::rethrow_exception(m_exception);
    }
    return m_result;
  }

  // Void specialization
  void result()
    requires(std::is_void_v<T>)
  {
    if (m_exception) {
      std::rethrow_exception(m_exception);
    }
  }

  // Store continuation for resumption
  void set_continuation(std::coroutine_handle<> p_continuation) noexcept
  {
    m_continuation = p_continuation;
  }

  // Resume stored continuation
  void resume_continuation() noexcept
  {
    if (m_continuation) {
      auto handle = m_continuation;
      m_continuation = nullptr;
      handle.resume();
    }
  }

  // Get allocator for child coroutines
  [[nodiscard]] std::pmr::memory_resource* get_allocator() const noexcept
  {
    return m_allocator;
  }

  // Set allocator for child coroutines (used during operator new and
  // await_suspend)
  void set_allocator(std::pmr::memory_resource* p_allocator) noexcept
  {
    m_allocator = p_allocator;
  }

#if 0
  // Custom allocation using our allocator - for top-level coroutines with
  // explicit context
  static void* operator new(std::size_t size, coro_context& ctx)
  {
    return std::pmr::polymorphic_allocator<>(ctx.resource()).allocate(size);
  }
#else
  // Custom allocation using our allocator - for top-level coroutines with
  // explicit context
  // This is specifically for the first parameter being coro_context&
  static void* operator new(std::size_t size, hal::v5::coro_context& ctx)
  {
    return std::pmr::polymorphic_allocator<>(ctx.resource()).allocate(size);
  }
#endif

  // For nested coroutines that don't have an explicit context
  static void* operator new(std::size_t size,
                            std::pmr::memory_resource* p_allocator)
  {
    return std::pmr::polymorphic_allocator<>(p_allocator).allocate(size);
  }

#if 0
  // Default operator new - will only be used if no allocator is available
  static void* operator new(std::size_t)
  {
    // This should never happen in our design
    throw std::bad_alloc();
  }
#endif

  // Custom deletion - no-op as specified by the requirements
  static void operator delete(void*)
  {
    // Deallocation is handled elsewhere
  }
};

// Task class template
template<typename T>
class task
{
private:
  std::coroutine_handle<hal_promise_type<T>> m_handle;

  explicit task(std::coroutine_handle<hal_promise_type<T>> p_handle) noexcept
    : m_handle(p_handle)
  {
  }

  template<typename U>
  friend class hal_promise_type;

public:
  // Coroutine promise type
  using promise_type = hal_promise_type<T>;

  // Default constructor for empty task
  task() noexcept = default;

  // Move constructor
  task(task&& other) noexcept
    : m_handle(std::exchange(other.m_handle, {}))
  {
  }

  // Move assignment
  task& operator=(task&& other) noexcept
  {
    if (this != &other) {
      if (m_handle) {
        m_handle.destroy();
      }
      m_handle = std::exchange(other.m_handle, {});
    }
    return *this;
  }

  // Destructor - clean up coroutine
  ~task()
  {
    if (m_handle) {
      m_handle.destroy();
    }
  }

  // Awaiter for when this task is awaited
  struct awaiter
  {
    std::coroutine_handle<hal_promise_type<T>> m_handle;

    explicit awaiter(
      std::coroutine_handle<hal_promise_type<T>> p_handle) noexcept
      : m_handle(p_handle)
    {
    }

    [[nodiscard]] bool await_ready() const noexcept
    {
      return !m_handle || m_handle.done();
    }

    // Generic await_suspend for any promise type
    template<typename Promise>
    void await_suspend(std::coroutine_handle<Promise> continuation) noexcept
    {
      m_handle.promise().set_continuation(continuation);
    }

    // Specialized await_suspend for our promise type - propagates allocator
    template<typename U>
    void await_suspend(
      std::coroutine_handle<hal_promise_type<U>> continuation) noexcept
    {
      // Get allocator from parent coroutine
      auto* parent_allocator = continuation.promise().get_allocator();

      // Propagate allocator to child coroutine
      if (parent_allocator) {
        m_handle.promise().set_allocator(parent_allocator);
      }

      // Store continuation for resumption
      m_handle.promise().set_continuation(continuation);
    }

    T await_resume()
    {
      if (!m_handle) {
        throw std::runtime_error("Awaiting a null task");
      }

      if constexpr (std::is_void_v<T>) {
        m_handle.promise().result();
      } else {
        return m_handle.promise().result();
      }
    }
  };

  [[nodiscard]] awaiter operator co_await() const noexcept
  {
    return awaiter{ m_handle };
  }

  // Run synchronously and return result
  T get_result()
  {
    if (!m_handle) {
      throw std::runtime_error("Task has no associated coroutine");
    }

    while (!m_handle.done()) {
      m_handle.resume();
    }

    if constexpr (std::is_void_v<T>) {
      m_handle.promise().result();
      return;
    } else {
      return m_handle.promise().result();
    }
  }
};

// Implement get_return_object now that task is defined
template<typename T>
task<T> hal_promise_type<T>::get_return_object() noexcept
{
  return task<T>{ std::coroutine_handle<hal_promise_type<T>>::from_promise(
    *this) };
}

// Utility function to run a task synchronously
template<typename T>
T sync_wait(task<T> p_task)
{
  return p_task.get_result();
}

// Example usage of creating top-level tasks
template<typename F>
auto make_task(coro_context& ctx, F&& func) -> decltype(func())
{
  return func();  // Context will be passed to operator new automatically
}

}  // namespace hal::v5
