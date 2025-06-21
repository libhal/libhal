#pragma once

#include <bitset>
#include <chrono>
#include <coroutine>
#include <exception>
#include <memory_resource>
#include <numeric>
#include <type_traits>
#include <utility>
#include <variant>

#include "../functional.hpp"
#include "../initializers.hpp"
#include "../units.hpp"
#include "libhal/error.hpp"

namespace hal::v5 {

enum class blocked_by : u8
{
  /// Not blocked by anything, ready to run!
  nothing = 0,
  /// Blocked by an amount of time that must be waited before the task resumes.
  /// It is the responsibility of the scheduler to defer calling the active
  /// coroutine of an `async_context` until the amount of time requested has
  /// elapsed.
  ///
  /// The time duration passed to the transition handler function represents the
  /// amount of time that the coroutine must suspend for until before scheduling
  /// the task again. Timed delays in this fashion are not real time and only
  /// represent the shortest duration of time necessary to fulfil the
  /// coroutine's delay needs. To schedule the coroutine before its delay time
  /// has been awaited, is considered to be undefined behavior. It is the
  /// responsibility of the develop of the transition handler to ensure that
  /// tasks are not executed until their delay time has elapsed.
  ///
  /// A value of 0 means do not wait and suspend but set the blocked by state to
  /// `time`. This will suspend the coroutine and it later. This is equivalent
  /// to just performing `std::suspend_always` but with additional steps, thus
  /// it is not advised to perform `co_await 0ns`.
  time = 1,
  /// Blocked by an I/O operation such as a DMA transfer or a bus. It is the
  /// responsibility of an interrupt (or thread performing I/O operations) to
  /// change the state to 'nothing' when the operation has completed.
  ///
  /// The time duration passed to the transition handler represents the amount
  /// of time the task must wait until it may resume the task even before its
  /// block status has transitioned to "nothing". This would represent the
  /// coroutine providing a hint to the scheduler about polling the coroutine. A
  /// value of 0 means wait indefinitely.
  io = 2,
  /// Blocked by a synchronization primitive of some kind. It is the
  /// responsibility of - to change the state to 'nothing' when new work is
  /// available.
  ///
  /// The time duration passed to the transition handler represents... TBD
  sync = 3,
  /// Blocked by a lack of work to perform. It is the responsibility of - to
  /// change the state to 'nothing' when new work is available.
  ///
  /// The time duration passed to the transition handler represents the amount
  /// of time the task must wait until it may resume the task even before its
  /// block status has transitioned to "nothing". This would represent the
  /// coroutine providing a hint to the scheduler about polling the coroutine. A
  /// value of 0 means wait indefinitely.
  inbox_empty = 4,
  /// Blocked by congestion to a mailbox of work. For example attempting to
  /// write a message to one of 3 outgoing mailboxes over CAN but all are
  /// currently busy waiting to emit their message. It is the
  /// responsibility of - to change the state to 'nothing' when new work is
  /// available.
  ///
  /// The time duration passed to the transition handler represents the amount
  /// of time the task must wait until it may resume the task even before its
  /// block status has transitioned to "nothing". This would represent the
  /// coroutine providing a hint to the scheduler about polling the coroutine. A
  /// value of 0 means wait indefinitely.
  outbox_full = 5,
};

class async_context;

template<typename T>
using type_or_byte = std::conditional_t<std::is_void_v<T>, u8, T>;

using async_transition_handler =
  hal::callback<void(async_context&, blocked_by, hal::time_duration)>;

class async_runtime_base
{
public:
  ~async_runtime_base()
  {
    if (m_resource) {
      std::pmr::polymorphic_allocator<hal::byte>(m_resource)
        .deallocate(m_buffer.data(), m_buffer.size());
    }
  }

  async_runtime_base(async_runtime_base const&) = delete;
  async_runtime_base& operator=(async_runtime_base const&) = delete;
  async_runtime_base(async_runtime_base&&) = default;
  async_runtime_base& operator=(async_runtime_base&&) = default;

protected:
  explicit async_runtime_base(std::pmr::memory_resource& p_resource,
                              hal::usize p_coroutine_stack_size,
                              async_transition_handler p_handler)
    : m_resource(&p_resource)
    , m_handler(std::move(p_handler))
  {
    m_buffer = std::span{
      std::pmr::polymorphic_allocator<hal::byte>(m_resource)
        .allocate(p_coroutine_stack_size),
      p_coroutine_stack_size,
    };
  }

  friend class async_context;

  std::pmr::memory_resource* m_resource = nullptr;  // word 1
  std::span<hal::byte> m_buffer{};
  async_transition_handler m_handler;  // word 4-7
};

class async_context
{
public:
  static auto constexpr default_timeout = hal::time_duration(0);

  /**
   * @brief Default construct a new async context object
   *
   * Using this async context on a coroutine will result in the allocation
   * throwing an exception.
   */
  async_context() = default;
  void unblock()
  {
    transition_to(blocked_by::nothing, default_timeout);
  }
  void unblock_without_notification()
  {
    m_state = blocked_by::nothing;
  }
  void block_by_time(hal::time_duration p_duration)
  {
    transition_to(blocked_by::time, p_duration);
  }
  void block_by_io(hal::time_duration p_duration = default_timeout)
  {
    transition_to(blocked_by::io, p_duration);
  }
  void block_by_sync(hal::time_duration p_duration = default_timeout)
  {
    transition_to(blocked_by::sync, p_duration);
  }
  void block_by_inbox_empty(hal::time_duration p_duration = default_timeout)
  {
    transition_to(blocked_by::inbox_empty, p_duration);
  }
  void block_by_outbox_full(hal::time_duration p_duration = default_timeout)
  {
    transition_to(blocked_by::outbox_full, p_duration);
  }

  [[nodiscard]] constexpr std::coroutine_handle<> active_handle() const
  {
    return m_active_handle;
  }

  [[nodiscard]] auto state() const
  {
    return std::get<1>(m_state);
  }

private:
  friend class async_promise_base;

  template<typename>
  friend class async_promise_type;

  template<typename>
  friend class async;

  friend class async_runtime_base;

  template<usize N>
  friend class async_runtime;

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
  explicit async_context(async_runtime_base& p_manager,
                         std::span<hal::byte> p_buffer)
    : m_manager(&p_manager)
    , m_buffer(p_buffer)
  {
  }

  constexpr void active_handle(std::coroutine_handle<> p_active_handle)
  {
    m_active_handle = p_active_handle;
  }

  constexpr auto last_allocation_size()
  {
    return std::get<usize>(m_state);
  }

  void transition_to(blocked_by p_new_state, hal::time_duration p_info)
  {
    m_state = p_new_state;
    m_manager->m_handler(*this, p_new_state, p_info);
  }

  [[nodiscard]] constexpr void* allocate(std::size_t p_bytes)
  {
    auto const new_stack_pointer = m_stack_pointer + p_bytes;
    if (new_stack_pointer > m_buffer.size()) [[unlikely]] {
      hal::safe_throw(hal::bad_coroutine_alloc(this));
    }
    m_state = p_bytes;
    auto* const stack_address = &m_buffer[m_stack_pointer];
    m_stack_pointer = new_stack_pointer;
    return stack_address;
  }

  constexpr void deallocate(std::size_t p_bytes)
  {
    m_stack_pointer -= p_bytes;
  }

  void rethrow_if_exception_caught()
  {
    if (std::holds_alternative<std::exception_ptr>(m_state)) [[unlikely]] {
      auto const exception_ptr_copy = std::get<std::exception_ptr>(m_state);
      // TODO(kammce): spurious "bad variant access" errors have occurred.
      m_state = 0uz;  // destroy exception_ptr and set state to `usize`
      std::rethrow_exception(exception_ptr_copy);
    }
  }

  std::coroutine_handle<> m_active_handle = std::noop_coroutine();  // word 1
  async_runtime_base* m_manager = nullptr;                          // word 2
  std::span<hal::byte> m_buffer{};                                  // word 3-4
  usize m_stack_pointer = 0;                                        // word 5
  std::variant<usize, blocked_by, std::exception_ptr> m_state;      // word 6-8
};

template<usize context_count = 1>
class async_runtime : public async_runtime_base
{
public:
  static_assert(context_count >= 1);

  // Constructor with individual sizes
  async_runtime(std::pmr::memory_resource& p_resource,
                std::array<usize, context_count> const& p_stack_sizes,
                async_transition_handler p_handler)
    : async_runtime_base(
        p_resource,
        std::accumulate(p_stack_sizes.begin(), p_stack_sizes.end(), 0uz),
        p_handler)
  {
    // Partition buffer and construct contexts
    usize offset = 0;
    for (usize i = 0; i < context_count; ++i) {
      m_contexts[i] =
        async_context(*this, m_buffer.subspan(offset, p_stack_sizes[i]));
      offset += p_stack_sizes[i];
    }
  }

  static constexpr auto make_array(usize p_stack_size_per_context)
  {
    std::array<usize, context_count> result{};
    result.fill(p_stack_size_per_context);
    return result;
  }

  // Constructor with equal sizes
  async_runtime(std::pmr::memory_resource& resource,
                usize p_stack_size_per_context,
                async_transition_handler handler)
    : async_runtime(resource, make_array(p_stack_size_per_context), handler)
  {
  }

  async_runtime(async_runtime const&) = delete;
  async_runtime& operator=(async_runtime const&) = delete;
  // Move constructor is deleted to prevent invalidation of async_context. Each
  // has a pointer to this object which is being relocated.
  async_runtime(async_runtime&&) = delete;
  async_runtime& operator=(async_runtime&&) = delete;

  /**
   * @brief Lease access to an async_context
   *
   * @param p_index - which async context to use.
   * @return async_context&
   */
  async_context& operator[](usize p_index)
  {
    if (p_index >= context_count) {
      hal::safe_throw(hal::out_of_range(this,
                                        {
                                          .m_index = p_index,
                                          .m_capacity = context_count,
                                        }));
    }
    if (m_busy[p_index]) {
      hal::safe_throw(hal::device_or_resource_busy(this));
    }

    m_busy[p_index] = true;

    return m_contexts[p_index];
  }

  constexpr void release(usize idx)
  {
    if (idx < context_count) {
      m_busy[idx] = false;
    }
  }

private:
  std::bitset<context_count> m_busy;
  std::array<async_context, context_count> m_contexts;
};

auto constexpr async_transition_handler_size = sizeof(async_transition_handler);
auto constexpr async_manager = sizeof(async_runtime_base);
auto constexpr async_context_size = sizeof(async_context);
auto constexpr sizeof_std_exception_ptr = sizeof(std::exception_ptr);

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
    // After this point accessing the state of the coroutine is UB.
    m_context->m_state = std::current_exception();
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

  template<typename U>
  void return_value(U&& p_value) noexcept
    requires std::is_constructible_v<T, U&&>
  {
    m_value_address->emplace(std::forward<U>(p_value));
  }

  /**
   * @brief We should only call this within await_resume
   *
   */
  T& get_result_or_rethrow()
  {
    m_context->rethrow_if_exception_caught();
    return *m_value_address;
  }

  void set_object_address(std::optional<type_or_byte<T>>* p_value_address)
  {
    m_value_address = p_value_address;
  }

private:
  std::optional<type_or_byte<T>>* m_value_address;
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
    m_context->m_state = std::current_exception();
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
   * @return Type - reference to the value from this async operation.
   */
  [[nodiscard]] type_or_byte<T>& result(hal::unsafe)
  {
    return *m_result;
  }

  // Run synchronously and return result
  type_or_byte<T>& wait()
  {
    // If the handle is not set then this async object was completed
    // synchronously.
    if (not m_handle) {
      return result(hal::unsafe{});
    }

    auto& context = m_handle.promise().context();

    while (not m_handle.done()) {
      auto active = context.active_handle();
      active.resume();
    }

    // Rethrow exception caught by top level coroutine
    context.rethrow_if_exception_caught();
    return result(hal::unsafe{});
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

    type_or_byte<T>& await_resume() const
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
    m_result.emplace(std::forward<U>(p_value));
  };

  async(async const&) = delete;
  async& operator=(async const&) = delete;
  // async(async&& p_other) noexcept
  //   : m_handle(std::exchange(p_other.m_handle, {}))
  // {
  // }
  // async& operator=(async&& p_other) noexcept
  // {
  //   if (this != &p_other) {
  //     if (m_handle) {
  //       m_handle.destroy();
  //     }
  //     m_handle = std::exchange(p_other.m_handle, {});
  //   }
  //   return *this;
  // }
  async(async&& p_other) noexcept
    : m_handle(std::exchange(p_other.m_handle, {}))
    , m_frame_size(p_other.m_frame_size)
    , m_result(std::move(p_other.m_result))
  {
    // Update promise to point to our new location
    if (m_handle && !std::is_void_v<T>) {
      m_handle.promise().set_object_address(&m_result);
    }
  }

  async& operator=(async&& p_other) noexcept
  {
    if (this != &p_other) {
      if (m_handle) {
        m_handle.destroy();
      }
      m_handle = std::exchange(p_other.m_handle, {});
      m_frame_size = p_other.m_frame_size;
      m_result = std::move(p_other.m_result);

      // Update promise to point to our new location
      if (m_handle && !std::is_void_v<T>) {
        m_handle.promise().set_object_address(&m_result);
      }
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
  friend promise_type;

  explicit constexpr async(std::coroutine_handle<promise_type> p_handle,
                           hal::usize p_frame_size)
    : m_handle(p_handle)
    , m_frame_size(p_frame_size)
  {
    auto& promise = m_handle.promise();
    if constexpr (not std::is_void_v<T>) {
      promise.set_object_address(&m_result);
    }
  }

  std::coroutine_handle<promise_type> m_handle = nullptr;
  hal::usize m_frame_size;
  // Keep this member uninitialized to save on cycles.
  std::optional<type_or_byte<T>> m_result;
};

template<typename T>
constexpr async<T> async_promise_type<T>::get_return_object() noexcept
{
  auto handle =
    std::coroutine_handle<async_promise_type<T>>::from_promise(*this);
  m_context->active_handle(handle);
  // Copy the last allocation size before changing the representation of
  // m_state to 'blocked_by::nothing'.
  auto const last_allocation_size = m_context->last_allocation_size();
  // Now stomp the union out and set it to the blocked_by::nothing state.
  m_context->m_state = blocked_by(blocked_by::nothing);
  return async<T>{ handle, last_allocation_size };
}

inline constexpr async<void>
async_promise_type<void>::get_return_object() noexcept
{
  auto handle =
    std::coroutine_handle<async_promise_type<void>>::from_promise(*this);
  m_context->active_handle(handle);
  // Copy the last allocation size before changing the representation of
  // m_state to 'blocked_by::nothing'.
  auto const last_allocation_size = m_context->last_allocation_size();
  // Now stomp the union out and set it to the blocked_by::nothing state.
  m_context->m_state = blocked_by::nothing;
  return async<void>{ handle, last_allocation_size };
}
}  // namespace hal::v5
