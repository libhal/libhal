#include <array>
#include <coroutine>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <memory_resource>
#include <numeric>
#include <span>
#include <stdexcept>
#include <utility>

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
    std::println("bytes = %zu, alignment = %zu\n", p_bytes, p_alignment);
    return m_resource.allocate(p_bytes, p_alignment);
  }
  void do_deallocate(void* p_address,
                     std::size_t p_bytes,
                     std::size_t p_alignment) override
  {
    std::println("address = %p, bytes = %zu, alignment = %zu\n",
                 p_address,
                 p_bytes,
                 p_alignment);
    return m_resource.deallocate(p_address, p_bytes, p_alignment);
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

class coroutine_context
{
public:
  explicit coroutine_context(std::pmr::memory_resource* p_resource)
    : m_resource(p_resource)
  {
  }

  coroutine_context() = default;

  [[nodiscard]] auto* resource() const
  {
    return m_resource;
  }

private:
  std::pmr::memory_resource* m_resource = nullptr;
};

class task_promise_base
{
public:
  task_promise_base() = default;

  std::suspend_always initial_suspend() noexcept
  {
    return {};
  }

  struct final_awaiter
  {
    constexpr bool await_ready() noexcept
    {
      return false;
    }

    template<typename Promise>
    std::coroutine_handle<> await_suspend(
      std::coroutine_handle<Promise> p_handle) noexcept
    {
      // Perform symmetric transfer to continuation if it exists
      task_promise_base& promise = p_handle.promise();
      if (promise.m_continuation) {
        return promise.m_continuation;
      }
      // Otherwise just return to caller
      return std::noop_coroutine();
    }

    void await_resume() noexcept
    {
    }
  };

  final_awaiter final_suspend() noexcept
  {
    return {};
  }

  void unhandled_exception() noexcept
  {
    m_error = std::current_exception();
  }

  auto& context()
  {
    return m_context;
  }

  void context(coroutine_context& p_context)
  {
    m_context = p_context;
  }

  // Storage for the coroutine result/error
  std::coroutine_handle<> m_continuation{};
  // NOLINTNEXTLINE(bugprone-throw-keyword-missing)
  std::exception_ptr m_error{};
  coroutine_context m_context{};
};

template<typename T>
class task;

template<typename T>
class task_promise_type : public task_promise_base
{
public:
  // For regular functions
  template<typename... Args>
  static void* operator new(std::size_t size,
                            coroutine_context& p_context,
                            Args&&...)
  {
    return std::pmr::polymorphic_allocator<>(p_context.resource())
      .allocate(size);
  }

  // For member functions - handles the implicit 'this' parameter
  template<typename Class, typename... Args>
  static void* operator new(std::size_t size,
                            Class&,  // The 'this' object
                            coroutine_context& p_context,
                            Args&&...)
  {
    return std::pmr::polymorphic_allocator<>(p_context.resource())
      .allocate(size);
  }

  // Custom deletion - no-op as specified by the requirements
  static void operator delete(void*)
  {
    // Deallocation is handled by task
  }

  task_promise_type() = default;

  task<T> get_return_object() noexcept;

  // For non-void return type
  template<typename U = T>
  void return_value(U&& p_value) noexcept
    requires(not std::is_void_v<T>)
  {
    m_value = std::forward<U>(p_value);
  }

  decltype(auto) result()
  {
    return m_value;
  }

  T m_value{};
};

template<>
class task_promise_type<void> : public task_promise_base
{
public:
  // For regular functions
  template<typename... Args>
  static void* operator new(std::size_t size,
                            coroutine_context& p_context,
                            Args&&...)
  {
    return std::pmr::polymorphic_allocator<>(p_context.resource())
      .allocate(size);
  }

  // For member functions - handles the implicit 'this' parameter
  template<typename Class, typename... Args>
  static void* operator new(std::size_t size,
                            Class&,  // The 'this' object
                            coroutine_context& p_context,
                            Args&&...)
  {
    return std::pmr::polymorphic_allocator<>(p_context.resource())
      .allocate(size);
  }

  // Custom deletion - no-op as specified by the requirements
  static void operator delete(void*)
  {
    // Deallocation is handled by task
  }

  task_promise_type();

  void return_void() noexcept
  {
  }
};

template<typename T = void>
class task
{
public:
  using promise_type = task_promise_type<T>;
  friend promise_type;

  // Run synchronously and return result
  T result()
  {
    if (!m_handle) {
      throw std::runtime_error("Task has no associated coroutine");
    }

    while (!m_handle.done()) {
      m_handle.resume();
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

    [[nodiscard]] bool await_ready() const noexcept
    {
      return !m_handle || m_handle.done();
    }

    // Generic await_suspend for any promise type
    template<typename Promise>
    std::coroutine_handle<> await_suspend(
      std::coroutine_handle<Promise> p_continuation) noexcept
    {
      m_handle.promise().set_continuation(p_continuation);
      return m_handle;
    }

    // Specialized await_suspend for our promise type - propagates allocator
    std::coroutine_handle<> await_suspend(
      std::coroutine_handle<promise_type> p_continuation) noexcept
    {
      // Get allocator from parent coroutine
      auto* parent_allocator = p_continuation.promise().get_allocator();

      // Propagate allocator to child coroutine
      if (parent_allocator) {
        m_handle.promise().set_allocator(parent_allocator);
      }

      // Store continuation for resumption
      m_handle.promise().set_continuation(p_continuation);

      return m_handle;
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

  task() noexcept = default;
  task(task&& p_other) noexcept
    : m_handle(std::exchange(p_other.m_handle, {}))
  {
  }
  ~task()
  {
    if (m_handle) {
      // auto& context = m_handle.promise().m_context;
      m_handle.destroy();
      // context.resource()->deallocate(m_handle.address(), 1);
    }
  }

  task& operator=(task&& p_other) noexcept
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
  explicit task(std::coroutine_handle<promise_type> p_handle)
    : m_handle(p_handle)
  {
  }

  std::coroutine_handle<promise_type> m_handle{};
};
template<typename T>
task<T> task_promise_type<T>::get_return_object() noexcept
{
  return task<T>{ std::coroutine_handle<task_promise_type<T>>::from_promise(
    *this) };
}
}  // namespace hal

// Generic type-erased captured call with no dynamic memory allocation
template<class Class, typename Ret, typename... Args>
class forward_context
{
private:
  std::coroutine_handle<hal::task_promise_type<Ret>> m_handle;

  Class* m_instance;
  // Function pointer to invoke the member function with type erasure
  hal::task<Ret> (*m_invoke_fn)(Class*, hal::coroutine_context&, Args...);

  // Stored arguments
  std::tuple<Args...> m_args;

public:
  // Constructor that captures instance, method, and arguments
  forward_context(Class* instance,
                  hal::task<Ret> (*mem_fn)(Class*,
                                           hal::coroutine_context&,
                                           Args...),
                  Args... args)
    : m_instance(instance)
    , m_invoke_fn(mem_fn)
    , m_args(std::forward<Args>(args)...)
  {
  }

  // Execute the captured call
  hal::task<Ret> invoke(hal::coroutine_context& p_resource) const
  {
    auto args = std::tuple_cat(std::make_tuple(m_instance, p_resource), m_args);
    return std::apply(m_invoke_fn, args);
  }

  [[nodiscard]] constexpr bool await_ready() const noexcept
  {
    return false;  // always suspend!
  }

  // Specialized await_suspend for our promise type - propagates allocator
  template<typename T>
  std::coroutine_handle<> await_suspend(
    std::coroutine_handle<hal::task_promise_type<T>> p_continuation) noexcept
  {
    // Get allocator from parent coroutine
    auto context = p_continuation.promise().context();
    auto new_task = invoke(context);
    m_handle = new_task.handle();
    m_handle.promise().m_continuation = p_continuation;
    return m_handle;
  }

  Ret await_resume()
  {
    if (!m_handle) {
      throw std::runtime_error("Awaiting a null task");
    }

    if constexpr (std::is_void_v<Ret>) {
      m_handle.promise().result();
    } else {
      return m_handle.promise().result();
    }
  }
};

// Example interface class
class interface
{
public:
  // Example method that returns a captured call
  auto evaluate(std::span<hal::byte const> p_data)
  {
    return forward_context(this, &interface::deferred_evaluate, p_data);
  }

  // Example method that returns a captured call
  auto sum(std::span<hal::byte const> p_data)
  {
    return forward_context(this, &interface::deferred_hash, p_data);
  }

  virtual ~interface() = default;

private:
  static hal::task<hal::usize> deferred_evaluate(
    interface* p_instance,
    hal::coroutine_context& p_resource,
    std::span<hal::byte const> p_data)
  {
    return p_instance->driver_evaluate(p_resource, p_data);
  }

  virtual hal::task<hal::usize> driver_evaluate(
    hal::coroutine_context& p_resource,
    std::span<hal::byte const> p_data) = 0;

  static hal::task<hal::usize> deferred_hash(interface* p_instance,
                                             hal::coroutine_context& p_resource,
                                             std::span<hal::byte const> p_data)
  {
    return p_instance->driver_hash(p_resource, p_data);
  }
  virtual hal::task<hal::usize> driver_hash(
    hal::coroutine_context& p_resource,
    std::span<hal::byte const> p_data) = 0;
};

hal::debug_monotonic_buffer_resource<1024> buff;
hal::coroutine_context context(&buff);

class my_implementation : public interface
{
private:
  hal::task<hal::usize> driver_evaluate(
    hal::coroutine_context&,
    std::span<hal::byte const> p_data) override
  {
    // Implementation here
    co_return p_data.size();
  }

  hal::task<hal::usize> driver_hash(hal::coroutine_context& p_resource,
                                    std::span<hal::byte const> p_data) override
  {
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
  hal::task<hal::usize> driver_evaluate(
    hal::coroutine_context&,
    std::span<hal::byte const> p_data) override
  {
    auto const sum = co_await m_impl->evaluate(p_data);
    // Implementation here
    co_return sum* p_data.size();
  }

  hal::task<hal::usize> driver_hash(hal::coroutine_context& p_resource,
                                    std::span<hal::byte const> p_data) override
  {
    unsigned const sum = std::accumulate(p_data.begin(), p_data.end(), 0);
    co_return sum* p_data.size();
  }

  interface* m_impl = nullptr;
};

// Example usage
auto example(interface& p_impl, std::span<hal::byte> p_buff)
{
  auto call = p_impl.evaluate(p_buff);
  return call.invoke(context);
}

hal::task<hal::usize> my_task(hal::coroutine_context&,
                              interface& p_impl,
                              std::span<hal::byte> p_buff)
{
  auto value = co_await p_impl.evaluate(p_buff);
  auto value1 = co_await p_impl.evaluate(p_buff);
  auto value2 = co_await p_impl.evaluate(p_buff);

  co_return value + value1 + value2;
}

boost::ut::suite<"coro_exper"> coro_expr = []() {
  using namespace boost::ut;
  "angular velocity sensor interface test"_test = []() {
    my_implementation impl;
    my_implementation2 impl2(impl);

    std::array<hal::byte, 5> special_buffer{};

    auto handle = example(impl2, special_buffer);
    auto value = handle.result();
    std::println("value = %ld\n", value);

    auto handle2 = my_task(context, impl2, special_buffer);
    auto value2 = handle.result();
    std::println("value2 = %ld\n", value2);
  };
};
