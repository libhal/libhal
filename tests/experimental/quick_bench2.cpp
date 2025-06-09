#include <array>
#include <bit>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <memory_resource>
#include <span>
#include <stdexcept>
#include <utility>

namespace hal {
using byte = std::uint8_t;
using usize = std::size_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

class async_context
{
public:
  explicit async_context(std::pmr::memory_resource& p_resource,
                         hal::usize p_total_stack_size)
    : m_resource(&p_resource)
    , m_total_stack_size(p_total_stack_size)
  {
    m_buffer = std::pmr::polymorphic_allocator<hal::byte>(m_resource)
                 .allocate(p_total_stack_size);
  }

  async_context() = default;

  [[nodiscard]] constexpr auto* resource()
  {
    return m_resource;
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

  ~async_context()
  {
    std::pmr::polymorphic_allocator<hal::byte>(m_resource)
      .deallocate(m_buffer, m_total_stack_size);
  }

private:
  std::coroutine_handle<> m_active_handle = std::noop_coroutine();
  std::pmr::memory_resource* m_resource;
  hal::byte* m_buffer = nullptr;
  usize m_total_stack_size = 0;
  usize m_stack_pointer = 0;
  usize m_last_allocation_size = 0;
};

class task_promise_base
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
  task_promise_base(async_context& p_context)
    : m_context(&p_context)
  {
  }

  // Constructor for functions accepting arguments
  template<typename... Args>
  task_promise_base(async_context& p_context, Args&&...)
    : m_context(&p_context)
  {
  }

  // Constructor for member functions (handles 'this' parameter)
  template<typename Class>
  task_promise_base(Class&, async_context& p_context)
    : m_context(&p_context)
  {
  }

  // Constructor for member functions with additional parameters
  template<typename Class, typename... Args>
  task_promise_base(Class&, async_context& p_context, Args&&...)
    : m_context(&p_context)
  {
  }

  constexpr std::suspend_always initial_suspend() noexcept
  {
    return {};
  }

  template<typename U>
  constexpr U&& await_transform(U&& awaitable) noexcept
  {
    return static_cast<U&&>(awaitable);
  }

  void unhandled_exception() noexcept
  {
    std::terminate();
    // m_exception_ptr = std::current_exception();
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
  // NOLINTNEXTLINE(bugprone-throw-keyword-missing)
  // std::exception_ptr m_exception_ptr{};
};

template<typename T>
class async;

// Helper type for void
struct void_placeholder
{};

// Type selection for void vs non-void
template<typename T>
using void_to_placeholder_t =
  std::conditional_t<std::is_void_v<T>, void_placeholder, T>;

template<typename T>
class task_promise_type : public task_promise_base
{
public:
  using task_promise_base::task_promise_base;  // Inherit constructors
  using task_promise_base::operator new;

  // Add regular delete operators for normal coroutine destruction
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
      std::coroutine_handle<task_promise_type<U>> p_self) noexcept
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

  void_to_placeholder_t<T> m_value{};
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

  async<void> get_return_object() noexcept;

  // Add regular delete operators for normal coroutine destruction
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
      std::coroutine_handle<task_promise_type<U>> p_self) noexcept
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
};

template<typename T = void>
class async
{
public:
  using promise_type = task_promise_type<T>;
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
      if constexpr (not std::is_void_v<T>) {
        return T{};
      } else {
        return;
      }
    }
    auto& context = m_handle.promise().context();

    while (not m_handle.done()) {
      auto active = context.active_handle();
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
      if constexpr (not std::is_void_v<T>) {
        if (m_handle) {
          return m_handle.promise().result();
        }
      }
    }
  };

  [[nodiscard]] constexpr awaiter operator co_await() const noexcept
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
    if (m_handle) {
      auto& context = m_handle.promise().context();
      m_handle.destroy();
      context.deallocate(m_frame_size);
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
async<T> task_promise_type<T>::get_return_object() noexcept
{
  auto handle =
    std::coroutine_handle<task_promise_type<T>>::from_promise(*this);
  m_context->active_handle(handle);
  return async<T>{ handle, m_context->last_allocation_size() };
}

async<void> task_promise_type<void>::get_return_object() noexcept
{
  auto handle =
    std::coroutine_handle<task_promise_type<void>>::from_promise(*this);
  m_context->active_handle(handle);
  return async<void>{ handle, m_context->last_allocation_size() };
}

class coroutine_stack_memory_resource : public std::pmr::memory_resource
{
public:
  constexpr coroutine_stack_memory_resource(std::span<hal::byte> p_memory)
    : m_memory(p_memory)
  {
    if (p_memory.data() == nullptr || p_memory.size() < 32) {
      throw std::runtime_error(
        "Coroutine stack memory invalid! Must be non-null and size > 32.");
    }
  }

private:
  void* do_allocate(std::size_t p_bytes, std::size_t) override
  {
    auto* const new_stack_pointer = m_memory.data();
    m_allocated_memory += p_bytes;
    return new_stack_pointer;
  }

  void do_deallocate(void*, std::size_t p_bytes, std::size_t) override
  {
    m_allocated_memory -= p_bytes;
  }

  [[nodiscard]] bool do_is_equal(
    std::pmr::memory_resource const& other) const noexcept override
  {
    return this == &other;
  }

  std::span<hal::byte> m_memory;
  hal::usize m_allocated_memory = 0;
};
}  // namespace hal

class sync_output_pin
{
public:
  auto level(bool p_high)
  {
    return driver_level(p_high);
  }

private:
  virtual void driver_level(bool p_high) = 0;
};

class async_output_pin
{
public:
  auto level(hal::async_context& p_context, bool p_high)
  {
    return driver_level(p_context, p_high);
  }

private:
  virtual hal::async<void> driver_level(hal::async_context& p_context,
                                        bool p_high) = 0;
};

struct gpio_t
{
  hal::u32 volatile crl;
  hal::u32 volatile crh;
  hal::u32 volatile idr;
  hal::u32 volatile odr;
  hal::u32 volatile bsrr;
  hal::u32 volatile brr;
  hal::u32 volatile lckr;
};

inline static std::array<gpio_t, 8> gpio_reg_map_obj{};
inline static std::array<gpio_t*, 8> gpio_reg_map{
  &gpio_reg_map_obj[0], &gpio_reg_map_obj[1], &gpio_reg_map_obj[2],
  &gpio_reg_map_obj[3], &gpio_reg_map_obj[4], &gpio_reg_map_obj[5],
  &gpio_reg_map_obj[6],
};

inline static gpio_t& gpio_reg(hal::u8 p_port)
{
  auto const offset = p_port - 'A';
  return *gpio_reg_map[offset];
}

// Add a volatile operation to prevent optimization
[[maybe_unused]] uint32_t volatile dummy = 0;

class sync_output_pin_impl : public sync_output_pin
{
private:
  virtual void driver_level(bool p_high)
  {
    if (p_high) {
      // The first 16 bits of the register set the output state
      gpio_reg(m_port).bsrr = 1 << m_pin;
    } else {
      // The last 16 bits of the register reset the output state
      gpio_reg(m_port).bsrr = 1 << (16 + m_pin);
    }
  }

  hal::u8 m_port = 'B';
  hal::u8 m_pin = 13;
};

class coro_sync_pin : public async_output_pin
{
private:
  hal::async<void> driver_level(hal::async_context&, bool p_high) override
  {
    dummy = gpio_reg(m_port).odr;  // Force a read

    if (p_high) {
      // The first 16 bits of the register set the output state
      gpio_reg(m_port).bsrr = 1 << m_pin;
    } else {
      // The last 16 bits of the register reset the output state
      gpio_reg(m_port).bsrr = 1 << (16 + m_pin);
    }
    // co_return;
    return hal::async<void>{};  // sync return
  }

  hal::u8 m_port = 'B';
  hal::u8 m_pin = 13;
};
class coro_async_pin : public async_output_pin
{
private:
  hal::async<void> driver_level(hal::async_context&, bool p_high) override
  {
    if (p_high) {
      // The first 16 bits of the register set the output state
      gpio_reg(m_port).bsrr = 1 << m_pin;
    } else {
      // The last 16 bits of the register reset the output state
      gpio_reg(m_port).bsrr = 1 << (16 + m_pin);
    }
    co_return;
  }

  hal::u8 m_port = 'B';
  hal::u8 m_pin = 13;
};

class coro_forever_async_pin : public async_output_pin
{
private:
  hal::async<void> driver_level(hal::async_context&, bool p_high) override
  {
    while (true) {
      if (p_high) {
        // The first 16 bits of the register set the output state
        gpio_reg(m_port).bsrr = 1 << m_pin;
      } else {
        // The last 16 bits of the register reset the output state
        gpio_reg(m_port).bsrr = 1 << (16 + m_pin);
      }
      co_await std::suspend_always{};
    }
    co_return;
  }

  hal::u8 m_port = 'B';
  hal::u8 m_pin = 13;
};

std::array<hal::byte, 512 * 2> coroutine_stack;
hal::coroutine_stack_memory_resource coroutine_resource(coroutine_stack);
hal::async_context ctx(coroutine_resource, sizeof(coroutine_stack));

std::array<hal::byte, 512 * 2> coroutine_stack2;
hal::coroutine_stack_memory_resource coroutine_resource2(coroutine_stack2);
hal::async_context ctx2(coroutine_resource2, sizeof(coroutine_stack2));

void set_pin(hal::u8 p_port, hal::u8 p_pin, bool p_state)
{
  if (p_state) {
    // The first 16 bits of the register set the output state
    gpio_reg(p_port).bsrr = 1 << p_pin;
  } else {
    // The last 16 bits of the register reset the output state
    gpio_reg(p_port).bsrr = 1 << (16 + p_pin);
  }
}

coro_sync_pin coro_pin_obj;
coro_async_pin coro_pin_async_obj;
coro_forever_async_pin coro_pin_forever_async_obj;
sync_output_pin_impl sync_output_pin_impl_obj;
bool volatile switcher = false;

async_output_pin* get_sync_coro_pin()
{
  if (not switcher) {
    return &coro_pin_obj;
  }
  return &coro_pin_async_obj;
}
async_output_pin* get_async_coro_pin()
{
  if (not switcher) {
    return &coro_pin_async_obj;
  }
  return &coro_pin_obj;
}

struct c_output_pin_vtable
{
  decltype(set_pin)* set_pin_funct = nullptr;
};

c_output_pin_vtable get_c_output_pin_vtable()
{
  if (not switcher) {
    return { .set_pin_funct = &set_pin };
  }
  return { nullptr };  // Will crash
}

struct c_output_pin_object
{
  void (*set_pin_funct)(c_output_pin_object*, bool p_state) = nullptr;
  hal::u8 m_port = 0;
  hal::u8 m_pin = 0;
};

static void my_set_pin_funct(c_output_pin_object* p_self, bool p_state)
{
  if (p_state) {
    // The first 16 bits of the register set the output state
    gpio_reg(p_self->m_port).bsrr = 1 << p_self->m_pin;
  } else {
    // The last 16 bits of the register reset the output state
    gpio_reg(p_self->m_port).bsrr = 1 << (16 + p_self->m_pin);
  }
}

c_output_pin_object get_c_output_pin_object(hal::u8 p_port, hal::u8 p_pin)
{
  if (not switcher) {
    return {
      .set_pin_funct = &my_set_pin_funct,
      .m_port = p_port,
      .m_pin = p_pin,
    };
  }
  return {};  // Will crash
}

sync_output_pin* get_sync_virtual_pin()
{
  if (not switcher) {
    return &sync_output_pin_impl_obj;
  }
  return nullptr;
}

hal::async<void> sync_coro_set_pin(hal::async_context&,
                                   hal::u8 p_port,
                                   hal::u8 p_pin,
                                   bool p_state)
{
  if (p_state) {
    // The first 16 bits of the register set the output state
    gpio_reg(p_port).bsrr = 1 << p_pin;
  } else {
    // The last 16 bits of the register reset the output state
    gpio_reg(p_port).bsrr = 1 << (16 + p_pin);
  }
  return {};
}

hal::async<void> async_coro_set_pin(hal::async_context&,
                                    hal::u8 p_port,
                                    hal::u8 p_pin,
                                    bool p_state)
{
  if (p_state) {
    // The first 16 bits of the register set the output state
    gpio_reg(p_port).bsrr = 1 << p_pin;
  } else {
    // The last 16 bits of the register reset the output state
    gpio_reg(p_port).bsrr = 1 << (16 + p_pin);
  }
  co_return;
}

hal::async<void> coro_pin_set_level(hal::async_context& p_ctx,
                                    async_output_pin& p_pin,
                                    bool p_level)
{
  co_return co_await p_pin.level(p_ctx, p_level);
}

hal::async<void> coro_await_task_loop_deeper(hal::async_context& p_ctx,
                                             async_output_pin& p_pin,
                                             int p_cycle_count)
{
  for (int i = 0; i < p_cycle_count; i++) {
    co_await coro_pin_set_level(p_ctx, p_pin, false);
    co_await coro_pin_set_level(p_ctx, p_pin, true);
  }
  co_return;
}

hal::async<void> coro_await_task_loop(hal::async_context& p_ctx,
                                      async_output_pin& p_pin,
                                      int p_cycle_count)
{
  for (int i = 0; i < p_cycle_count; i++) {
    co_await p_pin.level(p_ctx, false);
    co_await p_pin.level(p_ctx, true);
  }
  co_return;
}

async_output_pin* get_forever_coro_pin()
{
  if (not switcher) {
    return &coro_pin_forever_async_obj;
  }
  return &coro_pin_async_obj;
}

struct simulated_coroutine_frame
{
  hal::task_promise_type<void> promise;
  std::coroutine_handle<hal::task_promise_type<void>> handle;
  void* frame_address = nullptr;

  // Add custom operator new that propagates context
  static void* operator new(std::size_t p_size, hal::async_context& p_context)
  {
    return p_context.resource()->allocate(p_size);
  }

  static void operator delete(void*, std::size_t) noexcept
  {
    // Note: We can't easily get the context here, so this is a limitation
    // For benchmark purposes, we'll handle cleanup manually
  }

  // Constructor that initializes everything
  simulated_coroutine_frame(hal::async_context& p_ctx)
    : promise(p_ctx)
    , handle(std::coroutine_handle<hal::task_promise_type<void>>::from_promise(
        promise))
    , frame_address(this)
  {
    promise.continuation(std::noop_coroutine());
    p_ctx.active_handle(handle);
  }
};

#if 0
void application(resource_list& p_map)
{
  auto& clock = *p_map.clock.value();
  async_output_pin* coro_pin = get_sync_coro_pin();
  async_output_pin* coro_pin_async = get_async_coro_pin();
  async_output_pin* coro_forever = get_forever_coro_pin();

  while (true) {
    using namespace std::chrono_literals;
    constexpr auto delay_time = 500us;

    hal::delay(clock, delay_time);
    auto const creation_overhead_start = fast_uptime();
    auto set_high = coro_forever->level(ctx, true);
    auto const creation_overhead_end = fast_uptime();
    creation_overhead = creation_overhead_end - creation_overhead_start;

    auto const creation_and_resume_overhead_start = fast_uptime();
    auto set_low = coro_forever->level(ctx2, false);
    set_low.resume();
    auto const creation_and_resume_overhead_end = fast_uptime();
    creation_and_resume_overhead =
      creation_and_resume_overhead_end - creation_and_resume_overhead_start;
    for (int i = 0; i < 75; i++) {
      set_low.resume();
      set_high.resume();
    }

    hal::delay(clock, delay_time);
    std::array<hal::async<void>*, 2> list{
      &set_low,
      &set_high,
    };
    for (int i = 0; i < 50; i++) {  // round robin
      for (auto task : list) {
        if (task->handle() && not task->handle().done()) {
          task->resume();
        }
      }
    }

    hal::delay(clock, delay_time * 15);
  }
}
#endif

static void direct_function_call(benchmark::State& state)
{
  for (auto _ : state) {
    set_pin('B', 13, false);
    set_pin('B', 13, true);
  }
}
BENCHMARK(direct_function_call);

static void sync_coro_set_pin_test(benchmark::State& state)
{
  for (auto _ : state) {
    sync_coro_set_pin(ctx, 'B', 13, false).sync_result();
    sync_coro_set_pin(ctx, 'B', 13, true).sync_result();
  }
}
BENCHMARK(sync_coro_set_pin_test);

static void c_vtable_set_pin_function(benchmark::State& state)
{
  auto c_vtable = get_c_output_pin_vtable();
  for (auto _ : state) {
    c_vtable.set_pin_funct('B', 13, false);
    c_vtable.set_pin_funct('B', 13, true);
  }
}
BENCHMARK(c_vtable_set_pin_function);

static void virtual_obj_set_pin_function(benchmark::State& state)
{
  auto& virtual_obj = *get_sync_virtual_pin();
  for (auto _ : state) {
    virtual_obj.level(false);
    virtual_obj.level(true);
  }
}
BENCHMARK(virtual_obj_set_pin_function);

static void c_output_pin_obj_set_pin_function(benchmark::State& state)
{
  auto c_output_pin_obj = get_c_output_pin_object('B', 13);
  for (auto _ : state) {
    c_output_pin_obj.set_pin_funct(&c_output_pin_obj, false);
    c_output_pin_obj.set_pin_funct(&c_output_pin_obj, true);
  }
}
BENCHMARK(c_output_pin_obj_set_pin_function);

static void coro_forever_test(benchmark::State& state)
{
  async_output_pin* coro_forever = get_forever_coro_pin();
  auto set_high = coro_forever->level(ctx, true);
  auto set_low = coro_forever->level(ctx2, false);
  for (auto _ : state) {
    set_low.resume();
    set_high.resume();
  }
}
BENCHMARK(coro_forever_test);

static void coro_pin_sync_virtual_level(benchmark::State& state)
{
  async_output_pin* coro_pin = get_sync_coro_pin();
  for (auto _ : state) {
    coro_pin->level(ctx, false).sync_result();
    coro_pin->level(ctx, true).sync_result();
  }
}
BENCHMARK(coro_pin_sync_virtual_level);

static void coro_await_task_loop_sync_test(benchmark::State& state)
{
  async_output_pin* coro_pin = get_sync_coro_pin();
  for (auto _ : state) {
    coro_await_task_loop(ctx, *coro_pin, 1).sync_result();
  }
}
BENCHMARK(coro_await_task_loop_sync_test);

static void async_coro_set_pin_test(benchmark::State& state)
{
  for (auto _ : state) {
    async_coro_set_pin(ctx, 'B', 13, false).sync_result();
    async_coro_set_pin(ctx, 'B', 13, true).sync_result();
  }
}
BENCHMARK(async_coro_set_pin_test);

static void coro_pin_async_virtual_level(benchmark::State& state)
{
  async_output_pin* coro_pin_async = get_async_coro_pin();
  for (auto _ : state) {
    coro_pin_async->level(ctx, false).sync_result();
    coro_pin_async->level(ctx, true).sync_result();
  }
}
BENCHMARK(coro_pin_async_virtual_level);

static void coro_await_task_loop_test(benchmark::State& state)
{
  async_output_pin* coro_pin_async = get_async_coro_pin();
  for (auto _ : state) {
    coro_await_task_loop(ctx, *coro_pin_async, 1).sync_result();
    ;
  }
}
BENCHMARK(coro_await_task_loop_test);

static void coro_await_task_loop_deeper_sync_test(benchmark::State& state)
{
  async_output_pin* coro_pin = get_sync_coro_pin();
  for (auto _ : state) {
    coro_await_task_loop_deeper(ctx, *coro_pin, 1).sync_result();
  }
}
BENCHMARK(coro_await_task_loop_deeper_sync_test);

static void coro_await_task_loop_deeper_async_test(benchmark::State& state)
{
  async_output_pin* coro_pin_async = get_async_coro_pin();
  for (auto _ : state) {
    coro_await_task_loop_deeper(ctx, *coro_pin_async, 1).sync_result();
  }
}
BENCHMARK(coro_await_task_loop_deeper_async_test);
