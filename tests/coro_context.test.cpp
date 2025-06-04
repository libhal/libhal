#include <array>
#include <coroutine>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <memory_resource>
#include <numeric>
#include <print>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>

#include <boost/ut.hpp>

namespace hal {
using byte = std::uint8_t;
using usize = std::size_t;

template<usize Size>
class debug_monotonic_buffer_resource : public std::pmr::memory_resource
{
private:
  void* do_allocate(std::size_t p_bytes, std::size_t p_alignment) override
  {
    auto ptr = m_resource.allocate(p_bytes, p_alignment);
    std::println(
      "🟩 do_allocate(): {}, {}, alignment = {}", ptr, p_bytes, p_alignment);
    return ptr;
  }
  void do_deallocate(void* p_address,
                     std::size_t p_bytes,
                     std::size_t p_alignment) override
  {
    std::println("🟥 do_deallocate(): {}, {}, alignment = {}",
                 p_address,
                 p_bytes,
                 p_alignment);
    // return m_resource.deallocate(p_address, p_bytes, p_alignment);
  }

  [[nodiscard]] bool do_is_equal(
    std::pmr::memory_resource const& other) const noexcept override
  {
    return this == &other;
  }

  std::array<hal::byte, Size> m_buffer{};
  std::pmr::monotonic_buffer_resource m_resource{ m_buffer.data(),
                                                  m_buffer.size() };
};

class async_context
{
public:
  explicit async_context(std::pmr::memory_resource* p_resource)
    : m_resource(p_resource)
  {
  }

  async_context() = default;

  [[nodiscard]] auto* resource()
  {
    return m_resource;
  }

  auto last_allocation_size()
  {
    return m_last_allocation_size;
  }

  void last_allocation_size(usize p_last_allocation_size)
  {
    m_last_allocation_size = p_last_allocation_size;
  }

  std::coroutine_handle<> active_handle()
  {
    return m_active_handle;
  }

  void active_handle(std::coroutine_handle<> p_active_handle)
  {
    m_active_handle = p_active_handle;
  }

private:
  std::pmr::memory_resource* m_resource = nullptr;
  usize m_last_allocation_size = 0;
  std::coroutine_handle<> m_active_handle = std::noop_coroutine();
};

class task_promise_base
{
public:
  // For regular functions
  template<typename... Args>
  static void* operator new(std::size_t p_size,
                            async_context& p_context,
                            Args&&...)
  {
    p_context.last_allocation_size(p_size);
    std::println("🟩 new(p_size={}, async_context& p_context, Args&&...)",
                 p_context.last_allocation_size());
    return std::pmr::polymorphic_allocator<>(p_context.resource())
      .allocate(p_size);
  }

  // For member functions - handles the implicit 'this' parameter
  template<typename Class, typename... Args>
  static void* operator new(std::size_t p_size,
                            Class&,  // The 'this' object
                            async_context& p_context,
                            Args&&...)
  {
    p_context.last_allocation_size(p_size);
    std::println(
      "🟩 new(p_size={}, Class&, async_context& p_context, Args&&...)",
      p_context.last_allocation_size());
    return std::pmr::polymorphic_allocator<>(p_context.resource())
      .allocate(p_size);
  }
#if 1
  // Add regular delete operators for normal coroutine destruction
  static void operator delete(void* p_ptr) noexcept
  {
    std::println("🟥 delete<base>(void*={})", p_ptr);
  }

  static void operator delete(void* p_ptr, std::size_t p_size) noexcept
  {

    std::println("🟥 delete<base>(void*={}, usize={}) ", p_ptr, p_size);
  }
#endif

  // Constructor for regular functions
  task_promise_base(async_context& context)
  {
    m_context = &context;
    m_frame_size = context.last_allocation_size();
    std::println("🚜 task_promise_base(async_context& context) {}",
                 m_frame_size);
  }

  // Constructor for member functions (handles 'this' parameter)
  template<typename Class>
  task_promise_base(Class&, async_context& context)
  {
    m_context = &context;
    m_frame_size = context.last_allocation_size();
    std::println("🚜 task_promise_base(Class&, async_context& context) {}",
                 m_frame_size);
  }

  // Generic constructor for additional parameters
  template<typename... Args>
  task_promise_base(async_context& context, Args&&...)
  {
    m_context = &context;
    m_frame_size = context.last_allocation_size();
    std::println("🚜 task_promise_base(async_context& context, Args&&...) {}",
                 m_frame_size);
  }

  // Constructor for member functions with additional parameters
  template<typename Class, typename... Args>
  task_promise_base(Class&, async_context& context, Args&&...)
  {
    m_context = &context;
    m_frame_size = context.last_allocation_size();
    std::println(
      "🚜 task_promise_base(Class&, async_context& context, Args&&...) {}",
      m_frame_size);
  }

  constexpr std::suspend_always initial_suspend() noexcept
  {
    return {};
  }

  // Handle the rest...
  template<typename U>
  U&& await_transform(U&& awaitable) noexcept
  {
    return static_cast<U&&>(awaitable);
  }

  void unhandled_exception() noexcept
  {
    m_error = std::current_exception();
  }

  auto& context()
  {
    return *m_context;
  }

  void context(async_context& p_context)
  {
    m_context = &p_context;
  }

  auto continuation()
  {
    return m_continuation;
  }

  void continuation(std::coroutine_handle<> p_continuation)
  {
    m_continuation = p_continuation;
  }

  [[nodiscard]] auto frame_size() const
  {
    return m_frame_size;
  }

private:
  // Storage for the coroutine result/error
  std::coroutine_handle<> m_continuation{};
  async_context* m_context{};
  // NOLINTNEXTLINE(bugprone-throw-keyword-missing)
  std::exception_ptr m_error{};
  usize m_frame_size = 0;
};

template<typename T>
class async;

template<typename T>
class task_promise_type : public task_promise_base
{
public:
  using task_promise_base::task_promise_base;  // Inherit constructors
  using task_promise_base::operator new;

#if 1
  // Add regular delete operators for normal coroutine destruction
  static void operator delete(void* p_ptr) noexcept
  {
    std::println("🟥 delete<T>(void*={}) ", p_ptr);
  }

  static void operator delete(void* p_ptr, std::size_t p_size) noexcept
  {
    std::println("🟥 delete<T>(void*={}, usize={}) ", p_ptr, p_size);
  }
#endif

  struct final_awaiter
  {
    constexpr bool await_ready() noexcept
    {
      std::println("⏳ final_awaiter()::is_ready (ALWAYS FALSE)");
      return false;
    }

    std::coroutine_handle<> await_suspend(
      std::coroutine_handle<task_promise_type<T>> p_self) noexcept
    {
      // The coroutine is now suspended at the final-suspend point.
      // Lookup its continuation in the promise and resume it symmetrically.
      auto cont = p_self.promise().continuation();
      p_self.promise().context().active_handle(cont);
      std::println("⏳ final_awaiter::on_suspend @ {} --> {}",
                   p_self.address(),
                   cont.address());
      return cont;
    }

    void await_resume() noexcept
    {
      std::println("⏳ final_awaiter::get_result");
    }
  };

  final_awaiter final_suspend() noexcept
  {
    return {};
  }

  async<T> get_return_object() noexcept;

  // For non-void return type
  template<typename U = T>
  void return_value(U&& p_value) noexcept
    requires(not std::is_void_v<T>)
  {
    m_value = std::forward<U>(p_value);
  }

  auto result()
  {
    return m_value;
  }

  T m_value{};
};

template<>
class task_promise_type<void> : public task_promise_base
{
public:
  using task_promise_base::task_promise_base;  // Inherit constructors
  using task_promise_base::operator new;
  // using task_promise_base::operator delete;

  task_promise_type();

  constexpr void return_void() noexcept
  {
  }
};

template<typename T = void>
class async
{
public:
  using promise_type = task_promise_type<T>;
  friend promise_type;

  // Run synchronously and return result
  T sync_result()
  {
    if (not m_handle) {
      throw std::runtime_error("Task has no associated coroutine");
    }

    while (not m_handle.done()) {
      auto active = m_handle.promise().context().active_handle();
      std::println("🔄 resuming active: {}", active.address());
      active.resume();
    }

    if constexpr (not std::is_void_v<T>) {
      return m_handle.promise().result();
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
      std::println("⏱️ async::awaiter::is_ready @ {} ret [{}]",
                   m_handle.address(),
                   m_handle && m_handle.done());
      return m_handle && m_handle.done();
    }

    // Generic await_suspend for any promise type
    template<typename Promise>
    std::coroutine_handle<> await_suspend(
      std::coroutine_handle<Promise> p_continuation) noexcept
    {
      std::println("⏱️ async::awaiter::on_suspend {} active = {}",
                   p_continuation.address(),
                   m_handle.address());
      m_handle.promise().continuation(p_continuation);
      return std::noop_coroutine();
    }

    T await_resume()
    {
      std::println("⏱️ async::awaiter::get_result {}", m_handle.address());

      if (not m_handle) {
        throw std::runtime_error("Awaiting a null task");
      }

      if constexpr (not std::is_void_v<T>) {
        return m_handle.promise().result();
      }
    }
  };

  [[nodiscard]] awaiter operator co_await() const noexcept
  {
    return awaiter{ m_handle };
  }

  async() noexcept = default;
  async(async&& p_other) noexcept
    : m_handle(std::exchange(p_other.m_handle, {}))
  {
  }

  ~async()
  {
    std::println("~async()");
    void* address = m_handle.address();
    if (address) {
      auto* dealloc = m_handle.promise().context().resource();
      auto frame_size = m_handle.promise().frame_size();
      std::println("🗑️ ~async() destroy frame");
      m_handle.destroy();
      m_handle = nullptr;
      std::println("🟥 ~async(): {}, {}", address, frame_size);
      std::pmr::polymorphic_allocator<>(dealloc).deallocate_bytes(address,
                                                                  frame_size);
    }
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

  auto handle()
  {
    return m_handle;
  }

private:
  explicit async(std::coroutine_handle<promise_type> p_handle)
    : m_handle(p_handle)
  {
    m_handle.promise().continuation(std::noop_coroutine());
    m_handle.promise().context().active_handle(m_handle);
    std::println("🚜 async({})", p_handle.address());
  }

  std::coroutine_handle<promise_type> m_handle;
};

template<typename T>
async<T> task_promise_type<T>::get_return_object() noexcept
{
  return async<T>{ std::coroutine_handle<task_promise_type<T>>::from_promise(
    *this) };
}

struct yield_awaitable
{
  std::coroutine_handle<> m_handle;
  usize counter = 0;
  [[nodiscard]] bool await_ready() noexcept
  {
    counter++;
    std::println("🟡 yield::await_read()...");
    if (counter > 3) {
      std::println("🟡 yield::await_read() DONE!");
      return true;
    }
    return false;
  }

  template<typename Promise>
  void await_suspend(std::coroutine_handle<Promise>) noexcept
  {
  }

  void await_resume() const noexcept
  {
  }
};

auto yield()
{
  return yield_awaitable{};
}
}  // namespace hal

// Example interface class
class interface
{
public:
  // Example method that returns a captured call
  auto evaluate(hal::async_context& p_resource,
                std::span<hal::byte const> p_data)
  {
    return driver_evaluate(p_resource, p_data);
  }

  // Example method that returns a captured call
  auto hash(hal::async_context& p_resource, std::span<hal::byte const> p_data)
  {
    return driver_hash(p_resource, p_data);
  }

  virtual ~interface() = default;

private:
  virtual hal::async<hal::usize> driver_evaluate(
    hal::async_context&,
    std::span<hal::byte const> p_data) = 0;

  virtual hal::async<hal::usize> driver_hash(
    hal::async_context&,
    std::span<hal::byte const> p_data) = 0;
};

class my_implementation : public interface
{
private:
  hal::async<hal::usize> driver_evaluate(
    hal::async_context& p_context,
    std::span<hal::byte const> p_data) override
  {
    // Implementation here
    std::println("__impl__::coro_evaluate!");
    co_await hal::yield();

    std::println("__impl__::coro_evaluate post yield!");
    co_return p_data.size();
  }

  hal::async<hal::usize> driver_hash(hal::async_context&,
                                     std::span<hal::byte const> p_data) override
  {
    std::println("__impl__::driver_hash sync!");
    unsigned const sum = std::accumulate(p_data.begin(), p_data.end(), 0);
    co_return sum;
  }
};

class my_implementation2 : public interface
{
public:
  my_implementation2(interface& p_impl)
    : m_impl(&p_impl)
  {
  }

private:
  hal::async<hal::usize> driver_evaluate(
    hal::async_context& p_context,
    std::span<hal::byte const> p_data) override
  {
    std::println("impl2::drive_evaluate CORO!");
    auto const sum = co_await m_impl->evaluate(p_context, p_data);
    std::println("impl2::drive_evaluate CORO! 2");
    auto const sum2 = sum + co_await m_impl->evaluate(p_context, p_data);
    std::println("impl2::drive_evaluate CORO! 3");
    co_await std::suspend_always();
    // Implementation here
    co_return sum2 + p_data.size();
  }

  hal::async<hal::usize> coro_hash(hal::async_context& p_resource,
                                   std::span<hal::byte const> p_data)
  {
    std::println("impl2::coro_hash!");
    unsigned const sum = std::accumulate(p_data.begin(), p_data.end(), 0);
    auto const eval = co_await driver_evaluate(p_resource, p_data);
    co_return sum + eval + p_data.size();
  }

  hal::async<hal::usize> driver_hash(hal::async_context& p_resource,
                                     std::span<hal::byte const> p_data) override
  {
    co_return co_await coro_hash(p_resource, p_data);
  }

  interface* m_impl = nullptr;
};

hal::async<hal::usize> my_task(hal::async_context& p_context,
                               interface& p_impl,
                               std::span<hal::byte> p_buff)
{
  std::println("my task! 1");
  auto value1 = co_await p_impl.evaluate(p_context, p_buff);
  std::println("my task! 2");
  auto value2 = co_await p_impl.evaluate(p_context, p_buff);
  co_return value1 + value2;
}

hal::debug_monotonic_buffer_resource<1024UZ * 10> buff;
hal::async_context ctx(&buff);

boost::ut::suite<"coro_exper"> coro_expr = []() {
  using namespace boost::ut;
  "angular velocity sensor interface test"_test = []() {
    my_implementation impl;
    my_implementation2 impl2(impl);

    std::array<hal::byte, 5> special_buffer{};

    auto coro = my_task(ctx, impl2, special_buffer);
    std::println("coro initialized! now calling result()!");
    auto const value = coro.sync_result();

    // auto value2 = handle2.result();
    std::println("value = {}", value);
  };
};
