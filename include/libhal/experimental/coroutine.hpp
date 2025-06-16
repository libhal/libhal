#pragma once

#include <bit>
#include <chrono>
#include <coroutine>
#include <exception>
#include <memory_resource>
#include <type_traits>
#include <utility>
#include <variant>

#include "../functional.hpp"
#include "../initializers.hpp"
#include "../units.hpp"

namespace hal::v5 {

enum class blocked_by : u8
{
  /// Not blocked by anything, ready to run!
  nothing = 0,
  /// Blocked by an amount of time that must be waited before the task resumes.
  /// It is the responsibility of the scheduler to defer calling the active
  /// coroutine of an `async_context` until the amount of time requested has
  /// elapsed.
  time = 1,
  /// Blocked by an I/O operation such as a DMA transfer or a bus. It is the
  /// responsibility of an interrupt (or thread performing I/O operations) to
  /// change the state to 'nothing' when the operation has completed.
  io = 2,
  /// Blocked by a synchronization primitive of some kind. It is the
  /// responsibility of - to change the state to 'nothing' when new work is
  /// available.
  sync = 3,
  /// Blocked by a lack of work to perform. It is the responsibility of - to
  /// change the state to 'nothing' when new work is available.
  inbox_empty = 4,
  /// Blocked by congestion to a mailbox of work. For example attempting to
  /// write a message to one of 3 outgoing mailboxes over CAN but all are
  /// currently busy waiting to emit their message. It is the
  /// responsibility of - to change the state to 'nothing' when new work is
  /// available.
  outbox_full = 5,
};

struct nothing_info
{
  std::array<hal::u32, 2> reserved{};
};

struct time_info
{
  hal::time_duration duration;
  time_info(hal::time_duration p_duration)
    : duration(p_duration)
  {
  }
  time_info& operator=(hal::time_duration p_duration)
  {
    duration = p_duration;
    return *this;
  }
};

struct io_info
{
  std::array<hal::u32, 2> reserved{};
};

struct sync_info
{
  std::array<hal::u32, 2> reserved{};
};

struct inbox_info
{
  std::array<hal::u32, 2> reserved{};
};

struct outbox_info
{
  std::array<hal::u32, 2> reserved{};
};

struct info_slot_6
{
  std::array<hal::u32, 2> data{};
};

struct info_slot_7
{
  std::array<hal::u32, 2> reserved{};
};

struct info_slot_8
{
  std::array<hal::u32, 2> data{};
};

struct info_slot_9
{
  std::array<hal::u32, 2> data{};
};

struct info_slot_10
{
  std::array<hal::u32, 2> reserved{};
};

struct info_slot_11
{
  std::array<hal::u32, 2> data{};
};

struct info_slot_12
{
  std::array<hal::u32, 2> data{};
};

struct info_slot_13
{
  std::array<hal::u32, 2> reserved{};
};

struct info_slot_14
{
  std::array<hal::u32, 2> data{};
};

struct info_slot_15
{
  std::array<hal::u32, 2> data{};
};

// Pre-allocate variant with all possible future types
using block_info = std::variant<nothing_info,  // 0
                                time_info,     // 1
                                io_info,       // 2
                                sync_info,     // 3
                                inbox_info,    // 4
                                outbox_info,   // 5
                                // For future use
                                info_slot_6,   // 6
                                info_slot_7,   // 7
                                info_slot_8,   // 8
                                info_slot_9,   // 9
                                info_slot_10,  // 10
                                info_slot_11,  // 11
                                info_slot_12,  // 12
                                info_slot_13,  // 13
                                info_slot_14,  // 14
                                info_slot_15   // 15
                                >;

class async_context
{
public:
  // Fixed ABI from day one - can use all slots without breaking compatibility
  using transition_handler =
    hal::callback<void(async_context&, blocked_by, block_info)>;

  explicit async_context(std::pmr::memory_resource& p_resource,
                         hal::usize p_coroutine_stack_size,
                         transition_handler p_handler)
    : m_resource(&p_resource)
    , m_coroutine_stack_size(p_coroutine_stack_size)
    , m_handler(std::move(p_handler))
  {
    m_buffer = std::pmr::polymorphic_allocator<hal::byte>(m_resource)
                 .allocate(p_coroutine_stack_size);
  }

  async_context() = default;

  void unblock()
  {
    transition_to(blocked_by::nothing, nothing_info{});
  }

  void unblock_without_notification()
  {
    m_state = blocked_by::nothing;
  }

  void block_by_time(hal::time_duration p_duration)
  {
    transition_to(blocked_by::time, time_info{ p_duration });
  }

  void block_by_io()
  {
    transition_to(blocked_by::io, io_info{});
  }

  void block_by_sync()
  {
    transition_to(blocked_by::sync, sync_info{});
  }

  void block_by_inbox_empty()
  {
    transition_to(blocked_by::inbox_empty, inbox_info{});
  }

  void block_by_outbox_full()
  {
    transition_to(blocked_by::outbox_full, outbox_info{});
  }

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
    return m_state.load();
  }

  void busy_loop_until_unblocked()
  {
    while (m_state != blocked_by::nothing) {
      continue;
    }
  }

  ~async_context()
  {
    if (m_resource) {
      std::pmr::polymorphic_allocator<hal::byte>(m_resource)
        .deallocate(m_buffer, m_coroutine_stack_size);
    }
  }

private:
  friend class async_promise_base;

  template<typename>
  friend class async_promise_type;

  template<typename>
  friend class async;

  /**
   * @brief Construct a new async context object from a parent async context
   *
   * This constructor is currently not in use until we determine how the
   * transition handler should be written for child async context objects.
   *
   * @param p_buffer A pointer to this context's portion of a parent
   * async_context's coroutine stack
   * @param p_coroutine_stack_size - the size of the portion of the parent
   * async_context's coroutine stack
   * @param p_handler - handler for this async context
   */
  explicit async_context(hal::byte* p_buffer,
                         hal::usize p_coroutine_stack_size,
                         transition_handler p_handler)
    : m_buffer(p_buffer)
    , m_coroutine_stack_size(p_coroutine_stack_size)
    , m_handler(std::move(p_handler))
  {
  }

  static void noop(async_context&, blocked_by, block_info) noexcept
  {
    return;
  }

  void transition_to(blocked_by p_new_state, block_info p_info)
  {
    if (m_state == p_new_state) {
      return;
    }
    m_state = p_new_state;
    m_handler(*this, p_new_state, p_info);
  }

  [[nodiscard]] constexpr void* allocate(std::size_t p_bytes)
  {
    // TODO(): consider making this memory safe by performing a check for
    // stack exhaustion.
    m_last_allocation_size = p_bytes;
    auto* const new_stack_pointer = &m_buffer[m_stack_pointer];
    m_stack_pointer += p_bytes;
    return new_stack_pointer;
  }

  constexpr void deallocate(std::size_t p_bytes)
  {
    m_stack_pointer -= p_bytes;
  }

  std::exception_ptr& exception_ptr()
  {
    return m_exception;
  }

  void clear_exception()
  {
    m_exception = {};
  }

  void rethrow_if_exception_caught()
  {
    if (m_exception) [[unlikely]] {
      auto const copy = m_exception;
      clear_exception();
      std::rethrow_exception(copy);
    }
  }

  std::coroutine_handle<> m_active_handle = std::noop_coroutine();  // word 1
  std::pmr::memory_resource* m_resource = nullptr;                  // word 2
  hal::byte* m_buffer = nullptr;                                    // word 3
  usize m_coroutine_stack_size = 0;                                 // word 4
  usize m_stack_pointer = 0;                                        // word 5
  usize m_last_allocation_size = 0;                                 // word 6
  transition_handler m_handler = noop;                              // word 7-10
  std::atomic<blocked_by> m_state = blocked_by::nothing;            // word 11
  std::exception_ptr m_exception = {};                              // word 12
};

auto constexpr transition_handler = sizeof(async_context::transition_handler);
auto constexpr async_context_size = sizeof(async_context);

struct sleep
{
  hal::time_duration time;
};

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

  template<typename Rep, typename Ratio>
  constexpr auto await_transform(
    std::chrono::duration<Rep, Ratio> p_time_duration) noexcept
  {
    m_context->block_by_time(p_time_duration);
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
    m_context->exception_ptr() = std::current_exception();
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
    new (m_value_address) T{ std::forward<U>(p_value) };
  }

  /**
   * @brief We should only call this within await_resume
   *
   */
  T get_result_or_rethrow()
  {
    m_context->rethrow_if_exception_caught();
    return *m_value_address;
  }

  void set_object_address(T* p_value_address)
  {
    m_value_address = p_value_address;
  }

private:
  T* m_value_address;
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

    std::coroutine_handle<> await_suspend(
      std::coroutine_handle<async_promise_type<void>> p_self) noexcept
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

    constexpr void await_resume() noexcept
    {
    }
  };

  constexpr final_awaiter final_suspend() noexcept
  {
    return {};
  }

  void unhandled_exception() noexcept
  {
    m_context->exception_ptr() = std::current_exception();
  }

  /**
   * @brief We should only call this within await_resume
   *
   */
  void get_result_or_rethrow()
  {
    m_context->rethrow_if_exception_caught();
  }
};

template<typename T>
class async
{
public:
  using promise_type = async_promise_type<T>;
  friend promise_type;

  void resume() const
  {
    auto active = m_handle.promise().context().active_handle();
    active.resume();
  }

  [[nodiscard]] bool done() const
  {
    // True if the handle isn't valid
    // OR
    // If the coroutine is valid, then check if it has suspended at its final
    // suspension point.
    return not m_handle || m_handle.done();
  }

  /**
   * @brief Extract result value from async operation.
   *
   * The result is undefined if `done()` does not return `true`.
   *
   * @return T - value from this async operation.
   */
  [[nodiscard]] T result(hal::unsafe)
  {
    // Rethrow exception caught by top level coroutine
    if constexpr (not std::is_void_v<T>) {
      return *reinterpret_cast<T*>(m_result.data());
    } else {
      return;
    }
  }

  // Run synchronously and return result
  T wait()
  {
    // If the handle is not set then this async object was completed
    // synchronously.
    if (not m_handle) {
      if constexpr (std::is_void_v<T>) {
        return;
      } else {
        return result(hal::unsafe{});
      }
    }

    auto& context = m_handle.promise().context();

    while (not m_handle.done()) {
      auto active = context.active_handle();
      active.resume();
    }

    // Rethrow exception caught by top level coroutine
    context.rethrow_if_exception_caught();

    if constexpr (not std::is_void_v<T>) {
      // It is safe to return this value here because the coroutine has
      // completed.
      return result(hal::unsafe{});
    }
  }

  // Awaiter for when this task is awaited
  struct awaiter
  {
    async<T>* m_async_operation;

    explicit awaiter(async<T>* p_async_operation) noexcept
      : m_async_operation(p_async_operation)
    {
    }

    [[nodiscard]] constexpr bool await_ready() const noexcept
    {
      return m_async_operation->done();
    }

    // Generic await_suspend for any promise type
    template<typename Promise>
    std::coroutine_handle<> await_suspend(
      std::coroutine_handle<Promise> p_continuation) noexcept
    {
      m_async_operation->handle().promise().continuation(p_continuation);
      return m_async_operation->handle();
    }

    T await_resume() const
    {
      // The wait() operation, on a completed coroutine will return the result
      // or rethrow a caught exception.
      return m_async_operation->wait();
    }
  };

  [[nodiscard]] constexpr awaiter operator co_await() noexcept
  {
    return awaiter{ this };
  }

  async() noexcept
    requires(std::is_void_v<T>)
  = default;

  template<typename U>
  async(U&& p_value) noexcept
    requires(not std::is_void_v<T>)
  {
    static_assert(sizeof(Type) == sizeof(m_result));
    new (m_result.data()) T{ std::forward<U>(p_value) };
  };

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
    if constexpr (not std::is_void_v<T>) {
      m_handle.promise().set_object_address(std::bit_cast<T*>(m_result.data()));
    }
  }

  // Replace void type with a byte
  using Type = std::conditional_t<std::is_void_v<T>, hal::byte, T>;

  std::coroutine_handle<promise_type> m_handle = nullptr;
  hal::usize m_frame_size;
  // Keep this member uninitialized to safe on cycles.
  alignas(Type) std::array<hal::byte, sizeof(Type)> m_result;
};

template<typename T>
constexpr async<T> async_promise_type<T>::get_return_object() noexcept
{
  auto handle =
    std::coroutine_handle<async_promise_type<T>>::from_promise(*this);
  m_context->active_handle(handle);
  return async<T>{ handle, m_context->last_allocation_size() };
}

constexpr async<void> async_promise_type<void>::get_return_object() noexcept
{
  auto handle =
    std::coroutine_handle<async_promise_type<void>>::from_promise(*this);
  m_context->active_handle(handle);
  return async<void>{ handle, m_context->last_allocation_size() };
}
}  // namespace hal::v5
