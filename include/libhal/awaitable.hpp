#pragma once

#include <coroutine>
#include <memory_resource>

namespace hal::v5 {

// Promise type with context support
template<typename T>
struct io_operation_promise
{
  std::pmr::memory_resource* m_allocator = nullptr;
  T m_value;

  io_operation_promise(std::pmr::memory_resource& p_allocator)
    : m_allocator(&p_allocator)
  {
  }

  void* operator new(size_t p_size,
                     std::pmr::polymorphic_allocator<> p_allocator)
  {
    return p_allocator.allocate_bytes(p_size);
  }

  void operator delete(void* p_ptr,
                       size_t p_size,
                       std::pmr::polymorphic_allocator<> p_allocator)
  {
    p_allocator.deallocate_bytes(p_ptr, p_size);
    // Does nothing, deallocation should have in the destructor of the coroutine
  }
};

// Awaitable that captures a function and its arguments
template<typename ResultType, typename Ret, typename... Args>
class alloc_deferred_awaiter
{
public:
  template<typename Func>
  explicit alloc_deferred_awaiter(Func&& func, Args... args)
    : m_func(std::forward<Func>(func))
    , m_args(std::forward<Args>(args)...)
  {
  }

  [[nodiscard]] constexpr bool await_ready() const noexcept
  {
    return false;  // Always suspend to get access to the caller's context
  }

  template<typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle)
  {
    // Extract context from the promise
    auto& ctx = *handle.promise().m_context;

    // Apply the context to the function call
    return std::apply(
      [this, &ctx](auto&&... args) {
        return create_coroutine_with_context(
          ctx, m_func, std::forward<decltype(args)>(args)...);
      },
      m_args);
  }

  auto await_resume()
  {
    // In a complete implementation, this would return the result
    // For simplicity, we're not handling the return value here
  }

private:
  void* m_instance;
  using signature = Ret(void*, std::pmr::memory_resource&, Args...);
  signature* m_func;
  std::tuple<Args...> m_args;
};
}  // namespace hal::v5
