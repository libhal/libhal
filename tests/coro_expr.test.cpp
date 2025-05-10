
#include <coroutine>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <tuple>
#include <utility>

// #include <libhal/task.hpp>
#include <libhal/units.hpp>

#include <boost/ut.hpp>

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

namespace detail {
// Helper type for void
struct void_placeholder
{};

// Type selection for void vs non-void
template<typename T>
using void_to_placeholder_t =
  std::conditional_t<std::is_void_v<T>, void_placeholder, T>;
}  // namespace detail

template<typename T>
class task;

// Primary promise type
template<typename T>
class io_promise_type
{
private:
  using value_type = detail::void_to_placeholder_t<T>;

  coroutine_context* m_context{};
  std::exception_ptr m_exception = nullptr;
  std::coroutine_handle<> m_continuation = nullptr;
  [[no_unique_address]] value_type m_result{};

public:
  // Default constructor - allocator will be set via operator new
  io_promise_type() = default;

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

  // For void coroutines
  void return_void()
    requires(std::is_void_v<T>)
  {
    // Nothing to do
  }

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
  [[nodiscard]] auto& get_context() const noexcept
  {
    return *m_context;
  }

  // For nested coroutines that don't have an explicit context
  static void* operator new(std::size_t size, coroutine_context& p_context)
  {
    return std::pmr::polymorphic_allocator<>(p_context.resource())
      .allocate(size);
  }

  // Custom deletion - no-op as specified by the requirements
  static void operator delete(void*)
  {
  }
};

// Generic type-erased captured call with no dynamic memory allocation
template<class Interface, typename Ret, typename... Args>
class alloc_deferred_awaitable
{
private:
  // Pointer to the base interface class
  Interface* m_instance;

  // Function pointer to invoke the member function with type erasure
  Ret (*m_invoke_fn)(Interface*, coroutine_context&, Args...);

  // Stored arguments
  std::tuple<Args...> m_args;

  std::coroutine_handle<> m_handle;

public:
  // Constructor that captures instance, method, and arguments
  alloc_deferred_awaitable(Interface* instance,
                           Ret (*mem_fn)(Interface*,
                                         coroutine_context&,
                                         Args...),
                           Args... args)
    : m_instance(instance)
    , m_invoke_fn(mem_fn)
    , m_args(std::forward<Args>(args)...)
  {
  }

  /// This awaitable is NEVER ready because the point of it is to defer
  /// coroutine construction until suspend
  constexpr bool await_ready()
  {
    return false;
  }

  std::coroutine_handle<> await_suspend(std::coroutine_handle<> p_caller)
  {
    auto callee_coroutine = 5 /* */;
    return callee_coroutine;
  }

  template<typename T>
  std::coroutine_handle<> await_suspend(std::coroutine_handle<> p_caller)
  {
    auto callee_coroutine = 5 /* */;
    return callee_coroutine;
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

  // Execute the captured call
  Ret operator()(coroutine_context& p_resource) const
  {
    auto args = std::tuple_cat(std::make_tuple(m_instance, p_resource), m_args);
    return std::apply(m_invoke_fn, args);
  }
};

// Example interface class
class interface
{
public:
  // Example method that returns a captured call
  auto async_evaluate(std::span<hal::byte const> p_data)
  {
    return alloc_deferred_awaitable(
      this, &interface::deferred_evaluate, p_data);
  }

  virtual ~interface() = default;

private:
  static size_t deferred_evaluate(interface* p_instance,
                                  coroutine_context& p_resource,
                                  std::span<hal::byte const> p_data)
  {
    return p_instance->driver_evaluate(p_resource, p_data);
  }

  virtual size_t driver_evaluate(coroutine_context& p_resource,
                                 std::span<hal::byte const> p_data) = 0;
};

// The caller-level type
struct task
{
  // The coroutine level type
  struct promise_type
  {
    task get_return_object()
    {
      return {};
    }
    std::suspend_never initial_suspend()
    {
      return {};
    }
    std::suspend_never final_suspend() noexcept
    {
      return {};
    }
    void return_void()
    {
    }
    void unhandled_exception()
    {
    }
  };
};

task my_coroutine()
{
  co_return;  // make it a coroutine
}

auto example()
{
  class my_implementation : public interface
  {
  private:
    size_t driver_evaluate(coroutine_context&,
                           std::span<hal::byte const> p_data) override
    {
      // Implementation here
      return p_data.size();
    }
  };

  my_implementation impl;
  std::array<hal::byte, 5> buffer{ 0, 1, 2, 3 };
  // Capture a call to driver_evaluate through the interface
  auto call = impl.async_evaluate(buffer);
  std::pmr::monotonic_buffer_resource buff;
  // Execute the captured call later
  return call(&buff);
}

boost::ut::suite<"coro_exper"> coro_expr = []() {
  using namespace boost::ut;
  "angular velocity sensor interface test"_test = []() {
    auto result = example();
    expect(that % 0 == result);
  };
};
