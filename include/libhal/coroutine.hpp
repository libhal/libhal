#pragma once

#include <chrono>
#include <coroutine>
#include <exception>
#include <memory_resource>
#include <type_traits>
#include <utility>
#include <variant>

#include "units.hpp"

namespace hal::v5 {

enum class async_state : u8
{
  // This state represents a coroutine that can be executed immediately.
  // Coroutines start off in this state and can become blocked if an operation
  // may take time in which more computation could be done.
  ready = 0,
  time_blocked = 1,
  io_blocked = 2,
  sync_blocked = 3,
  message_blocked = 4,
  cancelled = 5
};

struct ready_state
{};

class time_blocked_state
{
public:
  explicit time_blocked_state(hal::time_duration p_time_duration)
    : m_time_duration(p_time_duration)
  {
  }

  auto get()
  {
    return m_time_duration;
  }

private:
  hal::time_duration m_time_duration{};
};

struct io_blocked_state
{};

class async_context
{
public:
  explicit async_context(std::pmr::memory_resource& p_resource,
                         hal::usize p_coroutine_stack_size)
    : m_resource(&p_resource)
    , m_coroutine_stack_size(p_coroutine_stack_size)
  {
    m_buffer = std::pmr::polymorphic_allocator<hal::byte>(m_resource)
                 .allocate(p_coroutine_stack_size);
  }

  async_context() = default;

  constexpr auto last_allocation_size()
  {
    return m_last_allocation_size;
  }

  constexpr std::coroutine_handle<> active_handle()
  {
    return m_active_handle;
  }

  constexpr void active_handle(std::coroutine_handle<> p_active_handle)
  {
    m_active_handle = p_active_handle;
  }

  auto state()
  {
    return m_state;
  }

  auto delay_time()
  {
    return m_delay_request_time;
  }

  ~async_context()
  {
    std::pmr::polymorphic_allocator<hal::byte>(m_resource)
      .deallocate(m_buffer, m_coroutine_stack_size);
  }

private:
  friend class async_promise_base;

  template<typename>
  friend class async;

  [[nodiscard]] constexpr void* allocate(std::size_t p_bytes)
  {
    m_last_allocation_size = p_bytes;
    auto* const new_stack_pointer = &m_buffer[m_stack_pointer];
    m_stack_pointer += p_bytes;
    return new_stack_pointer;
  }

  constexpr void deallocate(std::size_t p_bytes)
  {
    m_stack_pointer -= p_bytes;
  }

  void state(async_state p_state)
  {
    m_state = p_state;
  }

  void delay_time(hal::time_duration p_delay_request_time)
  {
    m_delay_request_time = p_delay_request_time;
  }

  std::coroutine_handle<> m_active_handle = std::noop_coroutine();
  std::pmr::memory_resource* m_resource = nullptr;
  hal::byte* m_buffer = nullptr;
  usize m_coroutine_stack_size = 0;
  usize m_stack_pointer = 0;
  usize m_last_allocation_size = 0;
  hal::time_duration m_delay_request_time = std::chrono::nanoseconds(0);
  async_state m_state = async_state::ready;
};

auto constexpr async_context_size = sizeof(async_context);

class async_promise_base
{
public:
  // For regular functions
  template<typename... Args>
  static constexpr void* operator new(std::size_t p_size,
                                      async_context& p_context,
                                      Args&&...)
  {
    return p_context.allocate(p_size);
  }

  // For member functions - handles the implicit 'this' parameter
  template<typename Class, typename... Args>
  static constexpr void* operator new(std::size_t p_size,
                                      Class&,  // The 'this' object
                                      async_context& p_context,
                                      Args&&...)
  {
    return p_context.allocate(p_size);
  }

  // Add regular delete operators for normal coroutine destruction
  static constexpr void operator delete(void*) noexcept
  {
  }

  static constexpr void operator delete(void*, std::size_t) noexcept
  {
  }

  // Constructor for functions accepting no arguments
  async_promise_base(async_context& p_context)
    : m_context(&p_context)
  {
  }

  // Constructor for functions accepting arguments
  template<typename... Args>
  async_promise_base(async_context& p_context, Args&&...)
    : m_context(&p_context)
  {
  }

  // Constructor for member functions (handles 'this' parameter)
  template<typename Class>
  async_promise_base(Class&, async_context& p_context)
    : m_context(&p_context)
  {
  }

  // Constructor for member functions with additional parameters
  template<typename Class, typename... Args>
  async_promise_base(Class&, async_context& p_context, Args&&...)
    : m_context(&p_context)
  {
  }

  constexpr std::suspend_always initial_suspend() noexcept
  {
    return {};
  }

  constexpr auto await_transform(ready_state&&) noexcept
  {
    m_context->state(async_state::ready);
    return std::suspend_never{};
  }

  constexpr auto await_transform(time_blocked_state&& p_state) noexcept
  {
    m_context->state(async_state::time_blocked);
    m_context->delay_time(p_state.get());
    return std::suspend_always{};
  }

  constexpr auto await_transform(io_blocked_state&&) noexcept
  {
    m_context->state(async_state::io_blocked);
    return std::suspend_always{};
  }

  template<typename U>
  constexpr U&& await_transform(U&& p_awaitable) noexcept
  {
    return static_cast<U&&>(p_awaitable);
  }
  constexpr auto& context()
  {
    return *m_context;
  }
  constexpr auto continuation()
  {
    return m_continuation;
  }
  constexpr void continuation(std::coroutine_handle<> p_continuation)
  {
    m_continuation = p_continuation;
  }

  constexpr std::coroutine_handle<> pop_active_coroutine()
  {
    m_context->active_handle(m_continuation);
    return m_continuation;
  }

protected:
  // Storage for the coroutine result/error
  std::coroutine_handle<> m_continuation = std::noop_coroutine();
  async_context* m_context;
};

template<typename T>
class async;

// Type selection for void vs non-void
template<typename T>
using type_or_monostate =
  std::conditional_t<std::is_void_v<T>, std::monostate, T>;

template<typename T>
class async_promise_type : public async_promise_base
{
public:
  using async_promise_base::async_promise_base;  // Inherit constructors
  using async_promise_base::operator new;

  // Add regular delete operators for normal coroutine destruction
  static constexpr void operator delete(void*) noexcept
  {
  }

  static constexpr void operator delete(void*, std::size_t) noexcept
  {
  }

  void unhandled_exception() noexcept
  {
    m_value = std::current_exception();
  }

  struct final_awaiter
  {
    constexpr bool await_ready() noexcept
    {
      return false;
    }

    template<typename U>
    std::coroutine_handle<> await_suspend(
      std::coroutine_handle<async_promise_type<U>> p_self) noexcept
    {
      // The coroutine is now suspended at the final-suspend point.
      // Lookup its continuation in the promise and resume it symmetrically.
      //
      // Rather than return control back to the application, we continue the
      // caller function allowing it to yield when it reaches another suspend
      // point. The idea is that prior to this being called, we were executing
      // code and thus, when we resume the caller, we are still running code.
      // Lets continue to run as much code until we reach an actual suspend
      // point.
      return p_self.promise().pop_active_coroutine();
    }

    void await_resume() noexcept
    {
    }
  };

  constexpr final_awaiter final_suspend() noexcept
  {
    return {};
  }

  constexpr async<T> get_return_object() noexcept;

  // For non-void return type
  template<typename U = T>
  void return_value(U&& p_value) noexcept
    requires(not std::is_void_v<T>)
  {
    m_value = std::forward<U>(p_value);
  }

  /**
   * @brief We should only call this within await_resume
   *
   */
  T get_result_or_rethrow()
  {
    if (std::holds_alternative<type_or_monostate<T>>(m_value)) [[likely]] {
      return std::get<type_or_monostate<T>>(m_value);
    } else {
      std::rethrow_exception(std::get<std::exception_ptr>(m_value));
    }
  }

  std::variant<type_or_monostate<T>, std::exception_ptr> m_value{};
};

template<>
class async_promise_type<void> : public async_promise_base
{
public:
  // Inherit constructors & operator new
  using async_promise_base::async_promise_base;
  using async_promise_base::operator new;

  async_promise_type();

  constexpr void return_void() noexcept
  {
  }

  constexpr async<void> get_return_object() noexcept;

  // Delete operators are defined as no-ops to ensure that these calls get
  // removed from the binary if inlined.
  static constexpr void operator delete(void*) noexcept
  {
  }
  static constexpr void operator delete(void*, std::size_t) noexcept
  {
  }

  struct final_awaiter
  {
    constexpr bool await_ready() noexcept
    {
      return false;
    }

    template<typename U>
    std::coroutine_handle<> await_suspend(
      std::coroutine_handle<async_promise_type<U>> p_self) noexcept
    {
      // The coroutine is now suspended at the final-suspend point.
      // Lookup its continuation in the promise and resume it symmetrically.
      //
      // Rather than return control back to the application, we continue the
      // caller function allowing it to yield when it reaches another suspend
      // point. The idea is that prior to this being called, we were executing
      // code and thus, when we resume the caller, we are still running code.
      // Lets continue to run as much code until we reach an actual suspend
      // point.
      return p_self.promise().pop_active_coroutine();
    }

    void await_resume() noexcept
    {
    }
  };

  constexpr final_awaiter final_suspend() noexcept
  {
    return {};
  }

  void unhandled_exception() noexcept
  {
    m_exception_ptr = std::current_exception();
  }

  /**
   * @brief We should only call this within await_resume
   *
   */
  void get_result_or_rethrow()
  {
    if (m_exception_ptr) [[unlikely]] {
      std::rethrow_exception(m_exception_ptr);
    }
  }

  std::exception_ptr m_exception_ptr;
};

template<typename T = void>
class async
{
public:
  using promise_type = async_promise_type<T>;
  friend promise_type;

  void resume()
  {
    auto active = m_handle.promise().context().active_handle();
    active.resume();
  }

  // Run synchronously and return result
  T sync_result()
  {
    if (not m_handle) {
      return T{};
    }
    auto& context = m_handle.promise().context();

    while (not m_handle.done()) {
      auto active = context.active_handle();
      active.resume();
    }

    if constexpr (not std::is_void_v<T>) {
      return m_handle.promise().get_result_or_rethrow();
    }
  }

  // Awaiter for when this task is awaited
  struct awaiter
  {
    std::coroutine_handle<promise_type> m_handle;

    explicit awaiter(std::coroutine_handle<promise_type> p_handle) noexcept
      : m_handle(p_handle)
    {
    }

    [[nodiscard]] constexpr bool await_ready() const noexcept
    {
      return not m_handle;
    }

    // Generic await_suspend for any promise type
    template<typename Promise>
    std::coroutine_handle<> await_suspend(
      std::coroutine_handle<Promise> p_continuation) noexcept
    {
      m_handle.promise().continuation(p_continuation);
      return m_handle;
    }

    T await_resume()
    {
      if (m_handle) {
        if constexpr (std::is_void_v<T>) {
          m_handle.promise().get_result_or_rethrow();
        } else {
          return m_handle.promise().get_result_or_rethrow();
        }
      }
    }
  };

  [[nodiscard]] constexpr awaiter operator co_await() const noexcept
  {
    return awaiter{ m_handle };
  }

  async() noexcept = default;
  async(async const&) = delete;
  async& operator=(async const&) = delete;
  async(async&& p_other) noexcept
    : m_handle(std::exchange(p_other.m_handle, {}))
  {
  }
  async& operator=(async&& p_other) noexcept
  {
    if (this != &p_other) {
      if (m_handle) {
        m_handle.destroy();
      }
      m_handle = std::exchange(p_other.m_handle, {});
    }
    return *this;
  }

  ~async()
  {
    if (m_handle) {
      auto& context = m_handle.promise().context();
      m_handle.destroy();
      context.deallocate(m_frame_size);
    }
  }

  auto handle()
  {
    return m_handle;
  }

private:
  explicit constexpr async(std::coroutine_handle<promise_type> p_handle,
                           hal::usize p_frame_size)
    : m_handle(p_handle)
    , m_frame_size(p_frame_size)
  {
  }

  std::coroutine_handle<promise_type> m_handle{};
  hal::usize m_frame_size;
};

template<typename T>
constexpr async<T> async_promise_type<T>::get_return_object() noexcept
{
  auto handle =
    std::coroutine_handle<async_promise_type<T>>::from_promise(*this);
  m_context->active_handle(handle);
  return { handle, m_context->last_allocation_size() };
}

constexpr async<void> async_promise_type<void>::get_return_object() noexcept
{
  auto handle =
    std::coroutine_handle<async_promise_type<void>>::from_promise(*this);
  m_context->active_handle(handle);
  return async<void>{ handle, m_context->last_allocation_size() };
}
}  // namespace hal::v5
