
#include <coroutine>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <tuple>
#include <utility>

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
