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
    std::println("bytes = {}, alignment = {}", p_bytes, p_alignment);
    return m_resource.allocate(p_bytes, p_alignment);
  }
  void do_deallocate(void* p_address,
                     std::size_t p_bytes,
                     std::size_t p_alignment) override
  {
    std::println("address = {}, bytes = {}, alignment = {}",
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

template<class Class, typename Ret, typename... Args>
class deferred_coroutine
{
private:
  Class* m_instance;
  Ret (Class::*m_invoke_fn)(hal::coroutine_context&, Args...);
  std::tuple<Args...> m_args;

public:
  deferred_coroutine(Class* p_instance,
                     Ret (Class::*p_mem_fn)(hal::coroutine_context&, Args...),
                     Args... p_args)
    : m_instance(p_instance)
    , m_invoke_fn(p_mem_fn)
    , m_args(std::forward<Args>(p_args)...)
  {
  }

  auto invoke(hal::coroutine_context& p_resource) const
  {
    auto args = std::tuple_cat(std::make_tuple(m_instance, p_resource), m_args);
    return std::apply(m_invoke_fn, args);
  }
};

class task_promise_base
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

  task_promise_base() = default;

  // Constructor that captures the coroutine parameters
  // This gets called automatically by the coroutine machinery
  // task_promise_type() = default;

  // Constructor for regular functions
  task_promise_base(coroutine_context& context)
  {
    m_context = context;  // Store the context
  }

  // Constructor for member functions (handles 'this' parameter)
  template<typename Class>
  task_promise_base(Class&, coroutine_context& context)
  {
    m_context = context;  // Store the context
  }

  // Generic constructor for additional parameters
  template<typename... Args>
  task_promise_base(coroutine_context& context, Args&&...)
  {
    m_context = context;  // Store the context
  }

  // Constructor for member functions with additional parameters
  template<typename Class, typename... Args>
  task_promise_base(Class&, coroutine_context& context, Args&&...)
  {
    m_context = context;  // Store the context
  }

  constexpr std::suspend_always initial_suspend() noexcept
  {
    return {};
  }

  template<class Class, typename Ret, typename... Args>
  auto await_transform(deferred_coroutine<Class, Ret, Args...>&& p_deferred)
  {
    return p_deferred.invoke(m_context);
  }

  // Handle the rest...
  template<typename U>
  U&& await_transform(U&& awaitable) noexcept
  {
    return static_cast<U&&>(awaitable);
  }

  struct final_awaiter
  {
    bool await_ready() noexcept
    {
      return false;
    }

    template<typename Promise>
    std::coroutine_handle<> await_suspend(
      std::coroutine_handle<Promise> p_handle) noexcept
    {
      // The coroutine is now suspended at the final-suspend point.
      // Lookup its continuation in the promise and resume it symmetrically.
      return p_handle.promise().m_continuation;
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
  coroutine_context m_context{};
  // NOLINTNEXTLINE(bugprone-throw-keyword-missing)
  std::exception_ptr m_error{};
};

template<typename T>
class task;

template<typename T>
class task_promise_type : public task_promise_base
{
public:
  using task_promise_base::task_promise_base;  // Inherit constructors
  using task_promise_base::operator new;
  using task_promise_base::operator delete;

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
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    return m_value.value();
  }

  auto* optional_result_address()
  {
    return &m_value;
  }

  std::optional<T> m_value{};
};

template<>
class task_promise_type<void> : public task_promise_base
{
public:
  using task_promise_base::task_promise_base;  // Inherit constructors
  using task_promise_base::operator new;
  using task_promise_base::operator delete;

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
    if (not m_handle) {
      throw std::runtime_error("Task has no associated coroutine");
    }

    while (not m_handle.done()) {
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
      return not m_handle || m_handle.done();
    }

    // Generic await_suspend for any promise type
    template<typename Promise>
    std::coroutine_handle<> await_suspend(
      std::coroutine_handle<Promise> p_continuation) noexcept
    {
      m_handle.promise().m_continuation = p_continuation;
      return m_handle;
    }

    T await_resume()
    {
      if (not m_handle) {
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
      // m_handle.destroy();
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
  // Store return value
  std::optional<Ret>* m_result_ptr = nullptr;

  // Class this API belongs to
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
    new_task.handle().promise().m_continuation = p_continuation;
    m_result_ptr = new_task.handle().promise().optional_result_address();
    // symmetric transfer!!
    return new_task.handle();
  }

  Ret await_resume()
  {
    return m_result_ptr->value();  // NOLINT
  }
};

struct defer
{};

// Example interface class
class interface
{
public:
  // Example method that returns a captured call
  auto evaluate(hal::coroutine_context& p_resource,
                std::span<hal::byte const> p_data)
  {
    return driver_evaluate(p_resource, p_data);
  }

  // Example method that returns a captured call
  auto evaluate(std::span<hal::byte const> p_data)
  {
    return forward_context(this, &interface::deferred_evaluate, p_data);
  }

  auto evaluate(defer, std::span<hal::byte const> p_data)
  {
    return hal::deferred_coroutine(this, &interface::driver_evaluate, p_data);
  }

  // Example method that returns a captured call
  auto hash(hal::coroutine_context& p_resource,
            std::span<hal::byte const> p_data)
  {
    return driver_hash(p_resource, p_data);
  }

  // Example method that returns a captured call
  auto hash(std::span<hal::byte const> p_data)
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
    co_return sum + p_data.size();
  }

  hal::task<hal::usize> driver_hash(hal::coroutine_context& p_resource,
                                    std::span<hal::byte const> p_data) override
  {
    unsigned const sum = std::accumulate(p_data.begin(), p_data.end(), 0);
    auto const eval = co_await driver_evaluate(p_resource, p_data);
    co_return sum + eval + p_data.size();
  }

  interface* m_impl = nullptr;
};

hal::task<hal::usize> my_task(hal::coroutine_context& p_context,
                              interface& p_impl,
                              std::span<hal::byte> p_buff)
{
#if 0
  auto value = co_await p_impl.evaluate(p_context, p_buff);
  auto value1 = co_await p_impl.evaluate(p_context, p_buff);
  auto value2 = co_await p_impl.evaluate(p_context, p_buff);
  co_return value + value1 + value2;
#elif 1
  auto value = co_await p_impl.evaluate(defer{}, p_buff);
  auto value1 = co_await p_impl.evaluate(defer{}, p_buff);
  auto value2 = co_await p_impl.evaluate(defer{}, p_buff);
  co_return value + value1 + value2;
#else
  auto value = co_await p_impl.hash(p_context, p_buff);
  auto value1 = co_await p_impl.hash(p_context, p_buff);
  auto value2 = co_await p_impl.hash(p_context, p_buff);
  co_return value + value1 + value2;
#endif
}

hal::debug_monotonic_buffer_resource<1024UZ * 10> buff;
hal::coroutine_context context(&buff);

boost::ut::suite<"coro_exper"> coro_expr = []() {
  using namespace boost::ut;
  "angular velocity sensor interface test"_test = []() {
    my_implementation impl;
    my_implementation2 impl2(impl);

    std::array<hal::byte, 5> special_buffer{};

    auto handle = my_task(context, impl2, special_buffer).handle();

    while (not handle.done()) {
      handle.resume();
    }

    auto value = handle.promise().result();
    // auto value2 = handle2.result();
    std::println("value = {}", value);
  };
};
