#include <array>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <type_traits>
#include <utility>
#include <variant>

#include <boost/ut.hpp>

namespace hal {

// Forward declarations
template<typename T>
class async;
template<>
class task<void>;  // Explicit specialization declaration
class async_context;

// Type aliases
using byte = std::uint8_t;

// Async context for PMR allocator management
class async_context
{
public:
  explicit async_context(std::pmr::memory_resource* p_resource)
    : m_resource(p_resource)
  {
  }

  std::pmr::memory_resource* resource() const
  {
    return m_resource;
  }

private:
  std::pmr::memory_resource* m_resource;
};

// Base promise type for common functionality
template<typename T>
class task_promise_base
{
public:
  // For regular functions - context is first parameter
  template<typename... Args>
  static void* operator new(std::size_t size,
                            async_context& p_context,
                            Args&&...)
  {
    return p_context.resource()->allocate(size);
  }

  // For member functions - handles the implicit 'this' parameter
  template<typename Class, typename... Args>
  static void* operator new(std::size_t size,
                            Class&,  // The 'this' object
                            async_context& p_context,
                            Args&&...)
  {
    return p_context.resource()->allocate(size);
  }

  // Add regular delete operators for normal coroutine destruction
  static void operator delete(void* p_ptr) noexcept
  {
    auto* promise = reinterpret_cast<task_promise_base*>(p_ptr);
    auto* resource = promise->m_context->resource();
    std::pmr::polymorphic_allocator<>(resource).deallocate_object(promise, 1);
  }

  static void operator delete(void* p_ptr, std::size_t size) noexcept
  {
    auto* promise = reinterpret_cast<task_promise_base*>(p_ptr);
    auto* resource = promise->m_context->resource();
    std::pmr::polymorphic_allocator<>(resource).deallocate_bytes(p_ptr, size);
  }

  std::suspend_always initial_suspend() noexcept
  {
    return {};
  }
  std::suspend_always final_suspend() noexcept
  {
    return {};
  }

  void unhandled_exception()
  {
    m_exception = std::current_exception();
  }

  void set_continuation(std::coroutine_handle<> continuation)
  {
    m_continuation = continuation;
  }

  std::coroutine_handle<> continuation() const
  {
    return m_continuation;
  }

  constexpr void set_context(async_context& p_context)
  {
    m_context = &p_context;
  }

  async_context* context() const
  {
    return m_context;
  }

protected:
  std::coroutine_handle<> m_continuation;
  std::exception_ptr m_exception;
  async_context* m_context;
};

// Promise type for non-void T
template<typename T>
class task_promise : public task_promise_base<T>
{
public:
  async<T> get_return_object()
  {
    return async<T>{ std::coroutine_handle<task_promise>::from_promise(*this) };
  }

  void return_value(T value)
  {
    m_value = std::move(value);
  }

  T& result()
  {
    if (this->m_exception) {
      std::rethrow_exception(this->m_exception);
    }
    return m_value;
  }

private:
  T m_value{};
};

// Specialization for void
template<>
class task_promise<void> : public task_promise_base<void>
{
public:
  task<void> get_return_object();  // Defined after task<void> is complete

  void return_void()
  {
  }

  void result()
  {
    if (m_exception) {
      std::rethrow_exception(m_exception);
    }
  }
};

// Task type - the main coroutine type
template<typename T>
class async
{
public:
  using promise_type = task_promise<T>;
  using handle_type = std::coroutine_handle<promise_type>;

  explicit async(handle_type handle)
    : m_handle(handle)
  {
  }

  async(async&& other) noexcept
    : m_handle(std::exchange(other.m_handle, {}))
  {
  }

  async& operator=(async&& other) noexcept
  {
    if (this != &other) {
      if (m_handle) {
        m_handle.destroy();
      }
      m_handle = std::exchange(other.m_handle, {});
    }
    return *this;
  }

  ~async()
  {
    if (m_handle) {
      m_handle.destroy();
    }
  }

  // Awaitable interface
  constexpr bool await_ready() const noexcept
  {
    return false;
  }

  std::coroutine_handle<> await_suspend(
    std::coroutine_handle<promise_type> continuation)
  {
    m_handle.promise().set_continuation(continuation);
    m_handle.promise().set_allocator(continuation.promise().allocator());
    return m_handle;
  }

  T await_resume()
  {
    return m_handle.promise().result();
  }

  // For manual execution (non-coroutine contexts)
  T get()
  {
    if (!m_handle.done()) {
      m_handle.resume();
    }
    return m_handle.promise().result();
  }

private:
  handle_type m_handle;
};

// Specialization for void
template<>
class task<void>
{
public:
  using promise_type = task_promise<void>;
  using handle_type = std::coroutine_handle<promise_type>;

  explicit task(handle_type handle)
    : m_handle(handle)
  {
  }

  task(task&& other) noexcept
    : m_handle(std::exchange(other.m_handle, {}))
  {
  }

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

  ~task()
  {
    if (m_handle) {
      m_handle.destroy();
    }
  }

  // Awaitable interface
  constexpr bool await_ready() const noexcept
  {
    return false;
  }

  void await_suspend(std::coroutine_handle<> continuation)
  {
    m_handle.promise().set_continuation(continuation);
    m_handle.resume();
  }

  void await_resume()
  {
    m_handle.promise().result();
  }

  // For manual execution (non-coroutine contexts)
  void get()
  {
    if (!m_handle.done()) {
      m_handle.resume();
    }
    m_handle.promise().result();
  }

private:
  handle_type m_handle;
};

// Now we can define the get_return_object for void promise
inline task<void> task_promise<void>::get_return_object()
{
  return task<void>{ std::coroutine_handle<task_promise<void>>::from_promise(
    *this) };
}

// Hybrid io<T> type - using monostate for void
template<typename T>
using io = std::conditional_t<std::is_void_v<T>,
                              std::variant<std::monostate, task<void>>,
                              std::variant<T, async<T>>>;

// Awaitable wrapper for io<T> - non-void version
template<typename T>
class io_awaitable
{
public:
  explicit io_awaitable(io<T>&& value)
    : m_value(std::move(value))
  {
  }

  bool await_ready() const noexcept
  {
    return std::holds_alternative<T>(m_value);
  }

  void await_suspend(std::coroutine_handle<> continuation)
  {
    auto& task_val = std::get<async<T>>(m_value);
    task_val.await_suspend(continuation);
  }

  T await_resume()
  {
    if (std::holds_alternative<T>(m_value)) {
      return std::get<T>(m_value);
    } else {
      return std::get<async<T>>(m_value).await_resume();
    }
  }

private:
  io<T> m_value;
};

// Specialization for void
template<>
class io_awaitable<void>
{
public:
  explicit io_awaitable(io<void>&& value)
    : m_value(std::move(value))
  {
  }

  bool await_ready() const noexcept
  {
    return std::holds_alternative<std::monostate>(m_value);
  }

  void await_suspend(std::coroutine_handle<> continuation)
  {
    auto& task_val = std::get<task<void>>(m_value);
    task_val.await_suspend(continuation);
  }

  void await_resume()
  {
    if (std::holds_alternative<task<void>>(m_value)) {
      std::get<task<void>>(m_value).await_resume();
    }
  }

private:
  io<void> m_value;
};

// Helper to make io<T> awaitable
template<typename T>
io_awaitable<T> make_awaitable(io<T>&& value)
{
  return io_awaitable<T>(std::move(value));
}

// Interfaces for benchmarking

// 1. Baseline synchronous interface
class output_pin_sync
{
public:
  virtual ~output_pin_sync() = default;
  virtual void level(bool p_state) = 0;
};

// 2. Always-coroutine interface
class output_pin_always_coro
{
public:
  virtual ~output_pin_always_coro() = default;
  virtual task<void> level(async_context& p_ctx, bool p_state) = 0;
};

// 3. Hybrid interface
class output_pin_hybrid
{
public:
  virtual ~output_pin_hybrid() = default;
  virtual io<void> level(async_context& p_ctx, bool p_state) = 0;
};

// Concrete implementations

// 1. Synchronous implementation
class concrete_pin_sync : public output_pin_sync
{
public:
  void level(bool p_state) override
  {
    m_state = p_state;
    // Simulate some work
    int volatile dummy = 0;
    for (int i = 0; i < 10; ++i) {
      dummy += i;
    }
  }

  bool state() const
  {
    return m_state;
  }

private:
  bool m_state = false;
};

// 2. Always-coroutine implementation
class concrete_pin_always_coro : public output_pin_always_coro
{
public:
  task<void> level(async_context& p_ctx, bool p_state) override
  {
    m_state = p_state;
    // Simulate some work
    int volatile dummy = 0;
    for (int i = 0; i < 10; ++i) {
      dummy += i;
    }
    co_return;
  }

  bool state() const
  {
    return m_state;
  }

private:
  bool m_state = false;
};

// 3. Hybrid implementation - synchronous version
class concrete_pin_hybrid_sync : public output_pin_hybrid
{
public:
  io<void> level(async_context&, bool p_state) override
  {
    m_state = p_state;
    // Simulate some work
    int volatile dummy = 0;
    for (int i = 0; i < 10; ++i) {
      dummy += i;
    }
    return std::monostate{};  // Return monostate for void
  }

  [[nodiscard]] bool state() const
  {
    return m_state;
  }

private:
  bool m_state = false;
};

// 3. Hybrid implementation - asynchronous version
class concrete_pin_hybrid_async : public output_pin_hybrid
{
public:
  io<void> level(async_context& p_ctx, bool p_state) override
  {
    return do_level_async(p_ctx, p_state);
  }

  [[nodiscard]] bool state() const
  {
    return m_state;
  }

private:
  task<void> do_level_async(async_context& p_ctx, bool p_state)
  {
    m_state = p_state;
    // Simulate some async work
    int volatile dummy = 0;
    for (int i = 0; i < 10; ++i) {
      dummy += i;
    }
    co_return;
  }

  bool m_state = false;
};

}  // namespace hal

// Benchmark harness
namespace benchmark {

using namespace std::chrono;

template<typename F>
double measure_time(F&& func, int iterations)
{
  auto start = high_resolution_clock::now();

  for (int i = 0; i < iterations; ++i) {
    func();
  }

  auto end = high_resolution_clock::now();
  auto duration = duration_cast<nanoseconds>(end - start);
  return static_cast<double>(duration.count()) / iterations;
}

// Test functions for each approach

void test_sync(hal::output_pin_sync& pin, int toggles)
{
  bool state = false;
  for (int i = 0; i < toggles; ++i) {
    pin.level(state);
    state = !state;
  }
}

hal::task<void> test_always_coro(hal::async_context& ctx,
                                 hal::output_pin_always_coro& pin,
                                 int toggles)
{
  bool state = false;
  for (int i = 0; i < toggles; ++i) {
    co_await pin.level(ctx, state);
    state = !state;
  }
}

hal::task<void> test_hybrid(hal::async_context& ctx,
                            hal::output_pin_hybrid& pin,
                            int toggles)
{
  bool state = false;
  for (int i = 0; i < toggles; ++i) {
    co_await hal::make_awaitable<void>(pin.level(ctx, state));
    state = !state;
  }
}

void run_benchmarks()
{
  // Setup PMR allocator
  std::array<std::byte, 1024 * 1024> buffer;
  std::pmr::monotonic_buffer_resource resource(buffer.data(), buffer.size());
  hal::async_context ctx(&resource);

  // Create instances
  hal::concrete_pin_sync sync_pin;
  hal::concrete_pin_always_coro coro_pin;
  hal::concrete_pin_hybrid_sync hybrid_sync_pin;
  hal::concrete_pin_hybrid_async hybrid_async_pin;

  int const iterations = 1000;
  int const toggles_per_iteration = 100;

  // Benchmark synchronous
  double sync_time = measure_time(
    [&] { test_sync(sync_pin, toggles_per_iteration); }, iterations);

  // Benchmark always-coroutine
  double coro_time = measure_time(
    [&] {
      auto task = test_always_coro(ctx, coro_pin, toggles_per_iteration);
      task.get();
    },
    iterations);

  // Benchmark hybrid synchronous
  double hybrid_sync_time = measure_time(
    [&] {
      auto task = test_hybrid(ctx, hybrid_sync_pin, toggles_per_iteration);
      task.get();
    },
    iterations);

  // Benchmark hybrid asynchronous
  double hybrid_async_time = measure_time(
    [&] {
      auto task = test_hybrid(ctx, hybrid_async_pin, toggles_per_iteration);
      task.get();
    },
    iterations);

  std::cout << "Benchmark Results (nanoseconds per iteration):\n";
  std::cout << "Synchronous:        " << sync_time << " ns\n";
  std::cout << "Always Coroutine:   " << coro_time << " ns\n";
  std::cout << "Hybrid (sync impl): " << hybrid_sync_time << " ns\n";
  std::cout << "Hybrid (async impl):" << hybrid_async_time << " ns\n";
  std::cout << "\nOverhead vs Synchronous:\n";
  std::cout << "Always Coroutine:   " << (coro_time / sync_time - 1) * 100
            << "%\n";
  std::cout << "Hybrid (sync impl): "
            << (hybrid_sync_time / sync_time - 1) * 100 << "%\n";
  std::cout << "Hybrid (async impl):"
            << (hybrid_async_time / sync_time - 1) * 100 << "%\n";
}

}  // namespace benchmark

boost::ut::suite<"coro_triple_test"> coro_triple_test = []() {
  using namespace boost::ut;
  "coro_triple_test"_test = []() { benchmark::run_benchmarks(); };
};
