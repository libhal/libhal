#include <chrono>
#include <coroutine>
#include <cstddef>
#include <memory_resource>
#include <print>
#include <stdexcept>
#include <string>

#include <libhal/experimental/coroutine.hpp>
#include <libhal/units.hpp>

#include <boost/ut.hpp>

// Test utilities and mock implementations
class mock_device
{
public:
  mock_device() = default;

  // Simulate an I/O operation that completes after some time
  hal::v5::async<bool> async_read(hal::v5::async_context& p_ctx,
                                  std::span<hal::byte> buffer)
  {
    std::println("🔌 Starting async I/O read...");

    // Block by I/O first
    p_ctx.block_by_io();
    co_await std::suspend_always{};

    // Simulate I/O completion
    std::println("🔌 I/O read completed");
    std::ranges::fill(buffer, 0xAB);
    co_return true;
  }

  // Simulate a sync operation that blocks on synchronization
  hal::v5::async<int> wait_for_lock(hal::v5::async_context& p_ctx)
  {
    std::println("🔒 Waiting for lock...");
    p_ctx.block_by_sync();
    co_await std::suspend_always{};

    std::println("🔒 Lock acquired");
    co_return 42;
  }

  // Simulate waiting for work
  hal::v5::async<std::string> wait_for_message(hal::v5::async_context& p_ctx)
  {
    std::println("📪 Waiting for message...");
    p_ctx.block_by_inbox_empty();
    co_await std::suspend_always{};

    std::println("📪 Message received");
    co_return "Hello from message queue";
  }

  // Simulate backpressure
  hal::v5::async<void> send_with_backpressure(hal::v5::async_context& p_ctx)
  {
    std::println("📤 Trying to send message...");
    p_ctx.block_by_outbox_full();
    co_await std::suspend_always{};

    std::println("📤 Message sent successfully");
    co_return;
  }
};

// Exception throwing implementations
class exception_thrower
{
public:
  exception_thrower() = default;

  hal::v5::async<int> throw_immediately(hal::v5::async_context&)
  {
    std::println("💥 About to throw immediately");
    throw std::runtime_error("Immediate exception");
    co_return 0;  // Never reached
  }

  hal::v5::async<int> throw_after_suspend(hal::v5::async_context&)
  {
    std::println("💥 Suspending before throw");
    co_await std::suspend_always{};
    std::println("💥 About to throw after suspend");
    throw std::logic_error("Exception after suspend");
    co_return 0;  // Never reached
  }

  hal::v5::async<int> throw_after_time_wait(hal::v5::async_context&)
  {
    using namespace std::chrono_literals;
    std::println("💥 Waiting 1ms before throw");
    co_await 1ms;
    std::println("💥 About to throw after time wait");
    throw std::invalid_argument("Exception after time wait");
    co_return 0;  // Never reached
  }

  hal::v5::async<int> nested_exception_call(hal::v5::async_context& p_ctx)
  {
    std::println("💥 Making nested call that will throw");
    try {
      co_return co_await throw_after_suspend(p_ctx);
    } catch (std::exception const& e) {
      std::println("💥 Caught and rethrowing: {}", e.what());
      throw std::runtime_error("Rethrown from nested call");
    }
  }
};

// Complex composition test class
class compositor
{
public:
  compositor() = default;

  hal::v5::async<int> complex_operation(hal::v5::async_context& p_ctx)
  {
    using namespace std::chrono_literals;

    std::println("🎭 Starting complex operation");

    // Step 1: Wait for some time
    std::println("🎭 Step 1: Waiting 10ms");
    co_await 10ms;

    // Step 2: Perform I/O
    std::println("🎭 Step 2: Performing I/O");
    std::array<hal::byte, 8> buffer{};
    auto io_result = co_await m_device.async_read(p_ctx, buffer);

    // Step 3: Wait for synchronization
    std::println("🎭 Step 3: Waiting for lock");
    auto lock_value = co_await m_device.wait_for_lock(p_ctx);

    // Step 4: Handle backpressure
    std::println("🎭 Step 4: Handling backpressure");
    co_await m_device.send_with_backpressure(p_ctx);

    std::println("🎭 Complex operation completed");
    co_return lock_value + (io_result ? 100 : 0);
  }

  hal::v5::async<std::string> exception_recovery(hal::v5::async_context& p_ctx)
  {
    std::println("🎭 Attempting operation with exception recovery");

    try {
      co_await m_thrower.throw_after_suspend(p_ctx);
      co_return "Should not reach here";
    } catch (std::logic_error const& e) {
      std::println("🎭 Recovered from logic_error: {}", e.what());
    }

    // Try a different operation
    auto message = co_await m_device.wait_for_message(p_ctx);
    co_return "Recovered: " + message;
  }

private:
  mock_device m_device;
  exception_thrower m_thrower;
};

// Base interface class for polymorphic coroutine testing
class interface
{
public:
  // Example method that returns a captured call
  auto evaluate(hal::v5::async_context& p_context,
                std::span<hal::byte const> p_data)
  {
    return driver_evaluate(p_context, p_data);
  }

  virtual ~interface() = default;

private:
  virtual hal::v5::async<hal::usize> driver_evaluate(
    hal::v5::async_context& p_context,
    std::span<hal::byte const> p_data) = 0;
};

// State tracking for tests
struct test_state
{
  int transition_count = 0;
  hal::v5::blocked_by last_state = hal::v5::blocked_by::nothing;
  bool time_handler_called = false;
  bool io_handler_called = false;
  bool sync_handler_called = false;
  bool inbox_handler_called = false;
  bool outbox_handler_called = false;
};

test_state global_test_state;

auto test_handler = [](hal::v5::async_context& p_context,
                       hal::v5::blocked_by p_state,
                       hal::time_duration p_duration) noexcept {
  global_test_state.transition_count++;
  global_test_state.last_state = p_state;

  std::println("🔄 State transition #{} to #{}",
               global_test_state.transition_count,
               static_cast<int>(p_state));

  switch (p_state) {
    case hal::v5::blocked_by::time:
      global_test_state.time_handler_called = true;
      std::println(
        "⏲️  Time block: {}ms",
        std::chrono::duration_cast<std::chrono::milliseconds>(p_duration)
          .count());
      p_context.unblock_without_notification();
      break;

    case hal::v5::blocked_by::io:
      global_test_state.io_handler_called = true;
      std::println("🔌 I/O block detected");
      break;

    case hal::v5::blocked_by::sync:
      global_test_state.sync_handler_called = true;
      std::println("🔒 Sync block detected");
      break;

    case hal::v5::blocked_by::inbox_empty:
      global_test_state.inbox_handler_called = true;
      std::println("📪 Inbox empty block detected");
      break;

    case hal::v5::blocked_by::outbox_full:
      global_test_state.outbox_handler_called = true;
      std::println("📤 Outbox full block detected");
      break;

    case hal::v5::blocked_by::nothing:
      std::println("✅ Ready to execute");
      break;
  }
};

void reset_test_state()
{
  global_test_state = {};
}

// Setup common test infrastructure
std::array<hal::byte, 1024 * 10uz> test_stack;

// Test suite implementation
boost::ut::suite<"advanced_coroutine_tests"> advanced_tests = []() {
  using namespace boost::ut;
  using namespace std::chrono_literals;

  "State transitions test"_test = []() {
    reset_test_state();

    std::pmr::monotonic_buffer_resource test_resource(
      test_stack.data(), test_stack.size(), std::pmr::null_memory_resource());
    hal::v5::async_runtime context(
      test_resource, test_stack.size(), test_handler);
    hal::v5::async_context& test_ctx = context[0];
    expect(that % 0uz == test_ctx.memory_used());

    auto test_coro = [](hal::v5::async_context& ctx) -> hal::v5::async<int> {
      // Test various blocking states
      co_await 5ms;  // time block

      ctx.block_by_io();
      co_await std::suspend_always{};  // io block

      ctx.block_by_sync();
      co_await std::suspend_always{};  // sync block

      ctx.block_by_inbox_empty();
      co_await std::suspend_always{};  // inbox block

      ctx.block_by_outbox_full();
      co_await std::suspend_always{};  // outbox block

      co_return 12345;
    };

    auto coro = test_coro(test_ctx);
    expect(that % 0uz < test_ctx.memory_used());

    coro.resume();
    expect(that % 0uz < test_ctx.memory_used());
    expect(that % global_test_state.time_handler_called == true);
    expect(that % global_test_state.io_handler_called == false);
    expect(that % global_test_state.sync_handler_called == false);
    expect(that % global_test_state.inbox_handler_called == false);
    expect(that % global_test_state.outbox_handler_called == false);
    coro.resume();
    expect(that % 0uz < test_ctx.memory_used());
    expect(that % global_test_state.io_handler_called == true);
    expect(that % global_test_state.sync_handler_called == false);
    expect(that % global_test_state.inbox_handler_called == false);
    expect(that % global_test_state.outbox_handler_called == false);
    coro.resume();
    expect(that % 0uz < test_ctx.memory_used());
    expect(that % global_test_state.sync_handler_called == true);
    expect(that % global_test_state.inbox_handler_called == false);
    expect(that % global_test_state.outbox_handler_called == false);
    coro.resume();
    expect(that % 0uz < test_ctx.memory_used());
    expect(that % global_test_state.inbox_handler_called == true);
    expect(that % global_test_state.outbox_handler_called == false);
    coro.resume();
    expect(that % 0uz < test_ctx.memory_used());
    expect(that % global_test_state.outbox_handler_called == true);
    coro.resume();
    expect(that % 0uz == test_ctx.memory_used());
    expect(that % coro.done());
    expect(that % 0uz == test_ctx.memory_used());

    auto result = coro.sync_wait();
    expect(that % result == 12345);
    expect(that % global_test_state.transition_count >= 5);
  };

  "Exception handling - immediate throw"_test = []() {
    reset_test_state();
    exception_thrower thrower;

    std::pmr::monotonic_buffer_resource test_resource(
      test_stack.data(), test_stack.size(), std::pmr::null_memory_resource());
    hal::v5::async_runtime manager(
      test_resource, test_stack.size(), test_handler);
    hal::v5::async_context& test_ctx = manager[0];

    expect(that % 0uz == test_ctx.memory_used());
    auto coro = thrower.throw_immediately(test_ctx);
    expect(that % 0uz < test_ctx.memory_used());

    expect(throws<std::runtime_error>([&coro]() { coro.sync_wait(); }));
    expect(that % 0uz == test_ctx.memory_used());
  };

  "Exception handling - throw after suspend"_test = []() {
    reset_test_state();
    exception_thrower thrower;

    std::pmr::monotonic_buffer_resource test_resource(
      test_stack.data(), test_stack.size(), std::pmr::null_memory_resource());
    hal::v5::async_runtime manager(
      test_resource, test_stack.size(), test_handler);
    hal::v5::async_context& test_ctx = manager[0];

    auto coro = thrower.throw_after_suspend(test_ctx);

    expect(throws<std::logic_error>([&coro]() { coro.sync_wait(); }));
  };

  "Exception handling - throw after time wait"_test = []() {
    reset_test_state();
    exception_thrower thrower;

    std::pmr::monotonic_buffer_resource test_resource(
      test_stack.data(), test_stack.size(), std::pmr::null_memory_resource());
    hal::v5::async_runtime manager(
      test_resource, test_stack.size(), test_handler);
    hal::v5::async_context& test_ctx = manager[0];

    auto coro = thrower.throw_after_time_wait(test_ctx);

    expect(throws<std::invalid_argument>([&coro]() { coro.sync_wait(); }));

    expect(that % global_test_state.time_handler_called == true);
  };

  "Exception handling - nested exception"_test = []() {
    reset_test_state();
    exception_thrower thrower;

    std::pmr::monotonic_buffer_resource test_resource(
      test_stack.data(), test_stack.size(), std::pmr::null_memory_resource());
    hal::v5::async_runtime manager(
      test_resource, test_stack.size(), test_handler);
    hal::v5::async_context& test_ctx = manager[0];

    auto coro = thrower.nested_exception_call(test_ctx);

    expect(throws<std::runtime_error>([&coro]() { coro.sync_wait(); }));
  };

  "Complex composition test"_test = []() {
    reset_test_state();
    compositor comp;

    std::pmr::monotonic_buffer_resource test_resource(
      test_stack.data(), test_stack.size(), std::pmr::null_memory_resource());
    hal::v5::async_runtime manager(
      test_resource, test_stack.size(), test_handler);
    hal::v5::async_context& test_ctx = manager[0];

    auto coro = comp.complex_operation(test_ctx);
    auto result = coro.sync_wait();

    expect(that % result == 142);  // 42 (lock_value) + 100 (io_result bonus)
    expect(that % global_test_state.time_handler_called == true);
    expect(that % global_test_state.io_handler_called == true);
    expect(that % global_test_state.sync_handler_called == true);
    expect(that % global_test_state.outbox_handler_called == true);
  };

  "Exception recovery test"_test = []() {
    reset_test_state();
    compositor comp;

    std::pmr::monotonic_buffer_resource test_resource(
      test_stack.data(), test_stack.size(), std::pmr::null_memory_resource());
    hal::v5::async_runtime manager(
      test_resource, test_stack.size(), test_handler);
    hal::v5::async_context& test_ctx = manager[0];

    auto coro = comp.exception_recovery(test_ctx);
    auto result = coro.sync_wait();

    expect(that % result == std::string("Recovered: Hello from message queue"));
    expect(that % global_test_state.inbox_handler_called == true);
  };

  "Multiple coroutine chaining"_test = []() {
    reset_test_state();

    std::pmr::monotonic_buffer_resource test_resource(
      test_stack.data(), test_stack.size(), std::pmr::null_memory_resource());
    hal::v5::async_runtime manager(
      test_resource, test_stack.size(), test_handler);
    hal::v5::async_context& test_ctx = manager[0];

    auto chain_test = [](hal::v5::async_context& ctx) -> hal::v5::async<int> {
      auto step1 = [](hal::v5::async_context& ctx) -> hal::v5::async<int> {
        co_await 1ms;
        co_return 10;
      };

      auto step2 = [](hal::v5::async_context& ctx,
                      int value) -> hal::v5::async<int> {
        co_await 2ms;
        co_return value * 2;
      };

      auto step3 = [](hal::v5::async_context& ctx,
                      int value) -> hal::v5::async<int> {
        co_await 3ms;
        co_return value + 5;
      };

      auto val1 = co_await step1(ctx);
      auto val2 = co_await step2(ctx, val1);
      auto val3 = co_await step3(ctx, val2);

      co_return val3;
    };

    auto coro = chain_test(test_ctx);
    auto result = coro.sync_wait();

    expect(that % result == 25);  // ((10 * 2) + 5) = 25
    expect(that % global_test_state.time_handler_called == true);
    expect(that % global_test_state.transition_count >=
           3);  // At least 3 time waits
  };

  "Void coroutine test"_test = []() {
    reset_test_state();

    std::pmr::monotonic_buffer_resource test_resource(
      test_stack.data(), test_stack.size(), std::pmr::null_memory_resource());
    hal::v5::async_runtime manager(
      test_resource, test_stack.size(), test_handler);
    hal::v5::async_context& test_ctx = manager[0];

    auto void_test = [](hal::v5::async_context& ctx) -> hal::v5::async<void> {
      std::println("🎯 Void coroutine starting");
      co_await 1ms;
      std::println("🎯 Void coroutine middle");

      ctx.block_by_sync();
      co_await std::suspend_always{};

      std::println("🎯 Void coroutine ending");
      co_return;
    };

    auto coro = void_test(test_ctx);
    coro.sync_wait();  // Should not throw

    expect(that % global_test_state.time_handler_called == true);
    expect(that % global_test_state.sync_handler_called == true);
  };

  "Memory resource test"_test = []() {
    reset_test_state();

    // Test with limited memory
    std::array<hal::byte, 128> small_stack;
    std::pmr::monotonic_buffer_resource small_resource(
      small_stack.data(), small_stack.size(), std::pmr::null_memory_resource());
    hal::v5::async_runtime small_manager(
      small_resource, small_stack.size(), test_handler);
    auto& small_ctx = small_manager[0];

    auto memory_test = [](hal::v5::async_context& ctx) -> hal::v5::async<int> {
      std::println("📊 Testing with limited memory");
      co_await 1ms;
      co_return 999;
    };

    auto coro = memory_test(small_ctx);
    auto result = coro.sync_wait();

    expect(that % result == 999);
    expect(that % global_test_state.time_handler_called == true);
  };

  "Coroutine state inspection"_test = []() {
    reset_test_state();

    std::pmr::monotonic_buffer_resource test_resource(
      test_stack.data(), test_stack.size(), std::pmr::null_memory_resource());
    hal::v5::async_runtime manager(
      test_resource, test_stack.size(), test_handler);
    hal::v5::async_context& test_ctx = manager[0];

    auto inspection_test =
      [](hal::v5::async_context& ctx) -> hal::v5::async<int> {
      expect(ctx.state() == hal::v5::blocked_by::nothing);

      co_await 1ms;
      // After resume, should be unblock_without_notification() again
      expect(ctx.state() == hal::v5::blocked_by::nothing);

      ctx.block_by_io();
      co_await std::suspend_always{};

      expect(ctx.state() == hal::v5::blocked_by::io);
      co_return 777;
    };

    auto coro = inspection_test(test_ctx);
    auto result = coro.sync_wait();

    expect(that % result == 777);
  };
};

// Additional interface-based tests similar to the original
boost::ut::suite<"interface_coroutine_tests"> interface_tests = []() {
  using namespace boost::ut;

  // Setup common test infrastructure
  std::array<hal::byte, 1024 * 10uz> test_stack;
  std::pmr::monotonic_buffer_resource test_resource(
    test_stack.data(), test_stack.size(), std::pmr::null_memory_resource());

  hal::v5::async_runtime manager(
    test_resource, test_stack.size(), test_handler);
  hal::v5::async_context& test_ctx = manager[0];

  "Interface exception propagation"_test = [&]() {
    class throwing_interface : public interface
    {
    private:
      hal::v5::async<hal::usize> driver_evaluate(
        hal::v5::async_context&,
        std::span<hal::byte const>) override
      {
        std::println("💥 Interface throwing exception");
        throw std::domain_error("Interface exception");
        co_return 0uz;
      }
    };

    throwing_interface impl;
    std::array<hal::byte, 10> buffer{};

    auto task = [&](hal::v5::async_context& ctx) -> hal::v5::async<hal::usize> {
      co_return co_await impl.evaluate(ctx, buffer);
    };

    auto coro = task(test_ctx);

    expect(throws<std::domain_error>([&coro]() { coro.sync_wait(); }));
  };

  "Interface composition with exception handling"_test = [&]() {
    class safe_wrapper : public interface
    {
    public:
      safe_wrapper(interface& impl)
        : m_impl(&impl)
      {
      }

    private:
      hal::v5::async<hal::usize> driver_evaluate(
        hal::v5::async_context& ctx,
        std::span<hal::byte const> p_data) override
      {
        try {
          co_return co_await m_impl->evaluate(ctx, p_data);
        } catch (std::exception const& e) {
          std::println("🛡️ Caught exception in wrapper: {}", e.what());
          co_return 0uz;  // Safe fallback
        }
      }

      interface* m_impl;
    };

    class throwing_impl : public interface
    {
    private:
      hal::v5::async<hal::usize> driver_evaluate(
        hal::v5::async_context&,
        std::span<hal::byte const>) override
      {
        throw std::runtime_error("Deliberate failure");
        co_return 0uz;
      }
    };

    throwing_impl dangerous_impl;
    safe_wrapper safe_impl(dangerous_impl);
    std::array<hal::byte, 5> buffer{};

    auto coro = safe_impl.evaluate(test_ctx, buffer);
    auto result = coro.sync_wait();  // Should not throw

    expect(that % result == 0);  // Safe fallback value
  };
};

// Synchronous return tests for libhal coroutines
// These tests demonstrate the async<T>{ value } constructor for immediate
// returns

namespace sync_tests {

// Interface for testing synchronous returns
class sync_interface
{
public:
  auto calculate(hal::v5::async_context& p_context, int a, int b)
  {
    return driver_calculate(p_context, a, b);
  }

  auto get_status(hal::v5::async_context& p_context)
  {
    return driver_get_status(p_context);
  }

  auto validate_data(hal::v5::async_context& p_context,
                     std::span<hal::byte const> data)
  {
    return driver_validate_data(p_context, data);
  }

  auto process_conditionally(hal::v5::async_context& p_context,
                             bool should_process)
  {
    return driver_process_conditionally(p_context, should_process);
  }

  virtual ~sync_interface() = default;

private:
  virtual hal::v5::async<int>
  driver_calculate(hal::v5::async_context& p_context, int a, int b) = 0;
  virtual hal::v5::async<std::string> driver_get_status(
    hal::v5::async_context& p_context) = 0;
  virtual hal::v5::async<bool> driver_validate_data(
    hal::v5::async_context& p_context,
    std::span<hal::byte const> data) = 0;
  virtual hal::v5::async<void> driver_process_conditionally(
    hal::v5::async_context& p_context,
    bool should_process) = 0;
};

// Implementation that always returns synchronously
class immediate_impl : public sync_interface
{
private:
  hal::v5::async<int> driver_calculate(hal::v5::async_context&,
                                       int a,
                                       int b) override
  {
    std::println("📊 Immediate calculation: {} + {} = {}", a, b, a + b);
    return hal::v5::async<int>{ a + b };  // Synchronous return
  }

  hal::v5::async<std::string> driver_get_status(
    hal::v5::async_context&) override
  {
    std::println("📊 Immediate status check");
    return hal::v5::async<std::string>{ "Ready" };  // Synchronous return
  }

  hal::v5::async<bool> driver_validate_data(
    hal::v5::async_context&,
    std::span<hal::byte const> data) override
  {
    std::println("📊 Immediate validation of {} bytes", data.size());
    bool valid = data.size() > 0 && data.size() < 1000;  // Simple validation
    return hal::v5::async<bool>{ valid };                // Synchronous return
  }

  hal::v5::async<void> driver_process_conditionally(
    hal::v5::async_context&,
    bool should_process) override
  {
    if (should_process) {
      std::println("📊 Processing data immediately");
    } else {
      std::println("📊 Skipping processing");
    }
    return hal::v5::async<void>{};  // Synchronous void return
  }
};

// Implementation that conditionally returns synchronously or asynchronously
class conditional_impl : public sync_interface
{
private:
  hal::v5::async<int> driver_calculate(hal::v5::async_context& p_context,
                                       int a,
                                       int b) override
  {
    // Return immediately for small values, async for large
    if (a < 10 && b < 10) {
      std::println("📊 Small values: immediate calculation");
      co_return a + b;  // This will actually suspend but complete quickly
    }

    std::println("📊 Large values: async calculation");
    using namespace std::chrono_literals;
    co_await 1ms;  // Simulate computation time
    co_return a + b;
  }

  hal::v5::async<std::string> driver_get_status(
    hal::v5::async_context&) override
  {
    // Always immediate for this implementation
    std::println("📊 Conditional status: always immediate");
    return hal::v5::async<std::string>{ "Conditional Ready" };
  }

  hal::v5::async<bool> driver_validate_data(
    hal::v5::async_context&,
    std::span<hal::byte const> data) override
  {
    // Immediate for small data, async for large
    if (data.size() <= 10) {
      std::println("📊 Small data: immediate validation");
      return hal::v5::async<bool>{ true };
    }

    std::println("📊 Large data: async validation");
    return data.size() < 1000;
  }

  hal::v5::async<void> driver_process_conditionally(
    hal::v5::async_context&,
    bool should_process) override
  {
    if (!should_process) {
      std::println("📊 Not processing: immediate return");
      return hal::v5::async<void>{};  // Immediate void return
    }

    std::println("📊 Processing complete");
    return {};
  }
};

// Free functions that return synchronously
hal::v5::async<int> sync_multiply(hal::v5::async_context&, int a, int b)
{
  std::println("🔢 Synchronous multiply: {} * {} = {}", a, b, a * b);
  return hal::v5::async<int>{ a * b };
}

hal::v5::async<std::string> sync_format_number(hal::v5::async_context&,
                                               int number)
{
  auto result = std::format("Number: {}", number);
  std::println("🔢 Synchronous format: {}", result);
  return hal::v5::async<std::string>{ std::move(result) };
}

hal::v5::async<bool> sync_is_even(hal::v5::async_context&, int number)
{
  bool result = (number % 2) == 0;
  std::println(
    "🔢 Synchronous even check: {} is {}", number, result ? "even" : "odd");
  return hal::v5::async<bool>{ result };
}

hal::v5::async<void> sync_log_message(hal::v5::async_context&,
                                      std::string_view message)
{
  std::println("🔢 Synchronous log: {}", message);
  return hal::v5::async<void>{};
}

// Free functions that conditionally return sync/async
hal::v5::async<int> conditional_factorial(hal::v5::async_context&, int n)
{
  if (n <= 1) {
    std::println("🔢 Base case factorial: immediate return");
    return hal::v5::async<int>{ 1 };
  }

  if (n <= 5) {
    std::println("🔢 Small factorial: sync calculation");
    int result = 1;
    for (int i = 2; i <= n; ++i) {
      result *= i;
    }
    return hal::v5::async<int>{ result };
  }

  std::println("🔢 Large factorial: async calculation");

  int result = 1;
  for (int i = 2; i <= n; ++i) {
    result *= i;
  }
  return result;
}

hal::v5::async<std::string> conditional_process_string(
  hal::v5::async_context&,
  std::string const& p_input)
{
  if (p_input.empty()) {
    std::println("🔢 Empty string: immediate return");
    return hal::v5::async<std::string>{ "Empty" };
  }

  if (p_input.length() <= 10) {
    std::println("🔢 Short string: sync processing");
    return hal::v5::async<std::string>{ "Short: " + p_input };
  }

  std::println("🔢 Long string: async processing");
  return "Long: " + p_input.substr(0, 10) + "...";
}

struct sync_test_state
{
  int transition_count = 0;
  bool any_time_blocks = false;
  bool any_async_operations = false;
};

sync_test_state global_sync_state;

auto sync_test_handler = [](hal::v5::async_context& p_context,
                            hal::v5::blocked_by p_state,
                            hal::time_duration) noexcept {
  global_sync_state.transition_count++;

  if (p_state == hal::v5::blocked_by::time) {
    global_sync_state.any_time_blocks = true;
    global_sync_state.any_async_operations = true;
    std::println("⏰ Sync test: time block detected");
    p_context.unblock_without_notification();
  } else if (p_state != hal::v5::blocked_by::nothing) {
    global_sync_state.any_async_operations = true;
    std::println("🔄 Sync test: other block type {}",
                 static_cast<int>(p_state));
    p_context.unblock_without_notification();
  }
};

void reset_sync_test_state()
{
  global_sync_state = {};
}
}  // namespace sync_tests

// Test suite for synchronous returns
boost::ut::suite<"synchronous_return_tests"> sync_return_tests = []() {
  using namespace boost::ut;
  using namespace sync_tests;

  // Test infrastructure
  std::array<hal::byte, 2048 * 10uz> sync_test_stack;
  std::pmr::monotonic_buffer_resource sync_test_resource(
    sync_test_stack.data(),
    sync_test_stack.size(),
    std::pmr::null_memory_resource());

  hal::v5::async_runtime sync_manager(
    sync_test_resource, sync_test_stack.size(), sync_test_handler);
  hal::v5::async_context sync_test_ctx = sync_manager[0];

  "Free function immediate returns"_test = [&]() {
    reset_sync_test_state();

    // Test immediate int return
    auto mult_coro = sync_multiply(sync_test_ctx, 6, 7);
    expect(that % mult_coro.done() == true);  // Should be immediately done
    auto mult_result = mult_coro.sync_wait();
    expect(that % 0uz == sync_test_ctx.memory_used());
    expect(that % mult_result == 42);
    expect(that % global_sync_state.any_async_operations == false);

    // Test immediate string return
    auto format_coro = sync_format_number(sync_test_ctx, 123);
    expect(that % format_coro.done() == true);
    expect(that % 0uz == sync_test_ctx.memory_used());
    auto format_result = format_coro.sync_wait();
    expect(that % format_result == std::string("Number: 123"));

    // Test immediate bool return
    auto even_coro = sync_is_even(sync_test_ctx, 8);
    expect(that % even_coro.done() == true);
    expect(that % 0uz == sync_test_ctx.memory_used());
    auto even_result = even_coro.sync_wait();
    expect(that % even_result == true);

    // Test immediate void return
    auto log_coro = sync_log_message(sync_test_ctx, "Test message");
    expect(that % log_coro.done() == true);
    log_coro.sync_wait();
    expect(that % 0uz == sync_test_ctx.memory_used());

    expect(that % global_sync_state.transition_count == 0);
  };

  "Conditional sync/async free functions"_test = [&]() {
    reset_sync_test_state();

    // Test immediate factorial (n=1)
    auto fact1_coro = conditional_factorial(sync_test_ctx, 1);
    expect(that % fact1_coro.done() == true);
    auto fact1_result = fact1_coro.sync_wait();
    expect(that % fact1_result == 1);
    expect(that % 0uz == sync_test_ctx.memory_used());

    // Test sync factorial (n=4)
    auto fact4_coro = conditional_factorial(sync_test_ctx, 4);
    expect(that % fact4_coro.done() == true);
    auto fact4_result = fact4_coro.sync_wait();
    expect(that % fact4_result == 24);
    expect(that % 0uz == sync_test_ctx.memory_used());

    // Test sync factorial (n=8)
    reset_sync_test_state();
    auto fact8_coro = conditional_factorial(sync_test_ctx, 8);
    expect(that % fact8_coro.done() == true);
    auto fact8_result = fact8_coro.sync_wait();
    expect(that % fact8_result == 40320);
    expect(that % global_sync_state.any_time_blocks == false);
    expect(that % 0uz == sync_test_ctx.memory_used());

    // Test string processing
    reset_sync_test_state();

    // Empty string - immediate
    auto empty_coro = conditional_process_string(sync_test_ctx, "");
    expect(that % empty_coro.done() == true);
    auto empty_result = empty_coro.sync_wait();
    expect(that % empty_result == std::string("Empty"));
    expect(that % 0uz == sync_test_ctx.memory_used());

    // Short string - sync
    auto short_coro = conditional_process_string(sync_test_ctx, "Hello");
    expect(that % short_coro.done() == true);
    auto short_result = short_coro.sync_wait();
    expect(that % short_result == std::string("Short: Hello"));
    expect(that % 0uz == sync_test_ctx.memory_used());

    // Long string - sync
    reset_sync_test_state();
    auto long_coro = conditional_process_string(
      sync_test_ctx, "This is a very long string for testing");
    expect(that % long_coro.done() == true);
    auto long_result = long_coro.sync_wait();
    expect(that % long_result == std::string("Long: This is a ..."));
    expect(that % global_sync_state.any_time_blocks == false);
    expect(that % 0uz == sync_test_ctx.memory_used());
  };

  "Interface immediate returns"_test = [&]() {
    reset_sync_test_state();
    immediate_impl impl;

    // Test immediate calculation
    auto calc_coro = impl.calculate(sync_test_ctx, 15, 27);
    expect(that % calc_coro.done() == true);
    auto calc_result = calc_coro.sync_wait();
    expect(that % calc_result == 42);

    // Test immediate status
    auto status_coro = impl.get_status(sync_test_ctx);
    expect(that % status_coro.done() == true);
    auto status_result = status_coro.sync_wait();
    expect(that % status_result == std::string("Ready"));

    // Test immediate validation
    std::array<hal::byte, 5> test_data{ 1, 2, 3, 4, 5 };
    auto valid_coro = impl.validate_data(sync_test_ctx, test_data);
    expect(that % valid_coro.done() == true);
    auto valid_result = valid_coro.sync_wait();
    expect(that % valid_result == true);

    // Test immediate void processing
    auto process_coro = impl.process_conditionally(sync_test_ctx, true);
    expect(that % process_coro.done() == true);
    process_coro.sync_wait();

    expect(that % global_sync_state.transition_count == 0);
  };

  "Interface conditional sync/async"_test = [&]() {
    reset_sync_test_state();
    conditional_impl impl;

    // Small calculation - should be async but fast
    auto small_calc = impl.calculate(sync_test_ctx, 3, 4);
    expect(that % small_calc.done() ==
           false);  // Uses co_return, so still async
    auto small_result = small_calc.sync_wait();
    expect(that % small_result == 7);

    // Large calculation - async with delay
    reset_sync_test_state();
    auto large_calc = impl.calculate(sync_test_ctx, 15, 20);
    expect(that % large_calc.done() == false);
    auto large_result = large_calc.sync_wait();
    expect(that % large_result == 35);
    expect(that % global_sync_state.any_time_blocks == true);

    // Status - always immediate
    reset_sync_test_state();
    auto status_coro = impl.get_status(sync_test_ctx);
    expect(that % status_coro.done() == true);
    auto status_result = status_coro.sync_wait();
    expect(that % status_result == std::string("Conditional Ready"));
    expect(that % global_sync_state.any_async_operations == false);

    // Small data validation - immediate
    std::array<hal::byte, 5> small_data{ 1, 2, 3, 4, 5 };
    auto small_valid = impl.validate_data(sync_test_ctx, small_data);
    expect(that % small_valid.done() == true);
    auto small_valid_result = small_valid.sync_wait();
    expect(that % small_valid_result == true);

    // Large data validation - sync
    reset_sync_test_state();
    std::array<hal::byte, 50> large_data{};
    auto large_valid = impl.validate_data(sync_test_ctx, large_data);
    expect(that % large_valid.done() == true);
    auto large_valid_result = large_valid.sync_wait();
    expect(that % large_valid_result == true);
    expect(that % global_sync_state.any_time_blocks == false);

    // No processing - immediate void
    reset_sync_test_state();
    auto no_process = impl.process_conditionally(sync_test_ctx, false);
    expect(that % no_process.done() == true);
    no_process.sync_wait();
    expect(that % global_sync_state.any_async_operations == false);

    // With processing - sync void
    reset_sync_test_state();
    auto with_process = impl.process_conditionally(sync_test_ctx, true);
    expect(that % with_process.done() == true);
    with_process.sync_wait();
    expect(that % global_sync_state.any_time_blocks == false);
  };

  "Mixed sync/async composition"_test = [&]() {
    reset_sync_test_state();

    auto mixed_task =
      [](hal::v5::async_context& ctx) -> hal::v5::async<std::string> {
      // Start with immediate operations
      auto mult_result = co_await sync_multiply(ctx, 6, 7);
      auto even_check = co_await sync_is_even(ctx, mult_result);

      auto fact_result = co_await conditional_factorial(ctx, 6);

      // More immediate work
      auto format_result = co_await sync_format_number(ctx, fact_result);

      // Final result composition
      co_return std::format("mult={}, even={}, fact={}, format='{}'",
                            mult_result,
                            even_check,
                            fact_result,
                            format_result);
    };

    auto coro = mixed_task(sync_test_ctx);
    auto result = coro.sync_wait();

    expect(that % result ==
           std::string("mult=42, even=true, fact=720, format='Number: 720'"));
  };

  "Performance comparison test"_test = [&]() {
    reset_sync_test_state();

    auto performance_test =
      [](hal::v5::async_context& ctx) -> hal::v5::async<int> {
      int total = 0;

      // Many immediate operations should be fast
      for (int i = 0; i < 10; ++i) {
        auto mult = co_await sync_multiply(ctx, i, 2);
        total += mult;
      }

      auto fact = co_await conditional_factorial(ctx, 6);
      total += fact;

      co_return total;
    };

    auto coro = performance_test(sync_test_ctx);
    auto result = coro.sync_wait();

    // 0*2 + 1*2 + 2*2 + ... + 9*2 + 720 = 90 + 720 = 810
    expect(that % result == 810);
    expect(that % global_sync_state.transition_count == 0);
  };

  "Error handling with immediate returns"_test = [&]() {
    class error_prone_impl : public sync_interface
    {
    private:
      hal::v5::async<int> driver_calculate(hal::v5::async_context&,
                                           int a,
                                           int b) override
      {
        if (a < 0 || b < 0) {
          throw std::invalid_argument("Negative numbers not allowed");
        }
        return hal::v5::async<int>{ a + b };
      }

      hal::v5::async<std::string> driver_get_status(
        hal::v5::async_context&) override
      {
        return hal::v5::async<std::string>{ "Error-prone status" };
      }

      hal::v5::async<bool> driver_validate_data(
        hal::v5::async_context&,
        std::span<hal::byte const>) override
      {
        return hal::v5::async<bool>{ false };
      }

      hal::v5::async<void> driver_process_conditionally(hal::v5::async_context&,
                                                        bool) override
      {
        return hal::v5::async<void>{};
      }
    };

    error_prone_impl impl;

    // Test successful immediate operation
    auto success_coro = impl.calculate(sync_test_ctx, 5, 10);
    expect(that % success_coro.done() == true);
    auto success_result = success_coro.sync_wait();
    expect(that % success_result == 15);

    // Test immediate exception
    expect(throws<std::invalid_argument>([&sync_test_ctx, &impl]() {
      impl.calculate(sync_test_ctx, -1, 5).sync_wait();
    }));
  };
};

namespace {

struct split_test_state
{
  std::vector<int> context_transitions;  // Track which context had transitions
  std::vector<hal::v5::blocked_by> transition_types;
  int total_transitions = 0;
  bool all_contexts_used = false;
};

split_test_state global_split_state;

auto split_test_handler = [](hal::v5::async_context& p_context,
                             hal::v5::blocked_by p_state,
                             hal::time_duration p_duration) noexcept {
  global_split_state.total_transitions++;
  global_split_state.transition_types.push_back(p_state);

  // Try to identify which context this is (simple heuristic based on address)
  auto context_addr = reinterpret_cast<uintptr_t>(&p_context);
  int context_id = static_cast<int>(context_addr) % 100;  // Simple ID
  global_split_state.context_transitions.push_back(context_id);

  std::println("🔄 Split context #{} transition to {}",
               context_id,
               static_cast<int>(p_state));

  switch (p_state) {
    case hal::v5::blocked_by::time:
      std::println(
        "⏰ Split context time block: {}ms",
        std::chrono::duration_cast<std::chrono::milliseconds>(p_duration)
          .count());
      p_context.unblock_without_notification();
      break;

    case hal::v5::blocked_by::io:
      std::println("🔌 Split context I/O block");
      // Simulate async I/O completion
      p_context.unblock_without_notification();
      break;

    case hal::v5::blocked_by::sync:
      std::println("🔒 Split context sync block");
      p_context.unblock_without_notification();
      break;

    default:
      break;
  }
};

void reset_split_test_state()
{
  global_split_state = {};
}

// Simple coroutine tasks for testing split contexts
hal::v5::async<int> simple_task(hal::v5::async_context& ctx,
                                int id,
                                int delay_ms)
{
  using namespace std::chrono_literals;

  std::println("🎯 Task {} starting with {}ms delay", id, delay_ms);
  co_await std::chrono::milliseconds(delay_ms);

  std::println("🎯 Task {} middle operation", id);
  ctx.block_by_sync();
  co_await std::suspend_always{};

  std::println("🎯 Task {} completing", id);
  co_return id * 10;
}

hal::v5::async<std::string> io_task(hal::v5::async_context& ctx,
                                    std::string name)
{
  std::println("🔌 I/O Task {} starting", name);

  ctx.block_by_io();
  co_await std::suspend_always{};

  std::println("🔌 I/O Task {} completing", name);
  co_return "Result from " + name;
}

hal::v5::async<void> void_task(hal::v5::async_context& ctx, int iterations)
{
  using namespace std::chrono_literals;

  for (int i = 0; i < iterations; ++i) {
    std::println("🔄 Void task iteration {}", i);
    co_await 1ms;
  }

  std::println("🔄 Void task completed {} iterations", iterations);
  co_return;
}

// Test concurrent execution coordinator
template<hal::usize N>
struct concurrent_executor
{
  hal::v5::async_runtime<N>* contexts;
  std::vector<hal::v5::async<int>> tasks;

  concurrent_executor(hal::v5::async_runtime<N>& manager)
    : contexts(&manager)
  {
  }

  void start_simple_tasks()
  {
    for (hal::usize i = 0; i < N; ++i) {
      std::println("Creating task {}...", i);
      tasks.push_back(
        simple_task((*contexts)[i], static_cast<int>(i), (i + 1) * 10));
    }
  }

  std::array<int, N> wait_all()
  {
    std::array<int, N> results{};
    for (size_t i = 0; i < N; ++i) {
      results[i] = tasks[i].sync_wait();
    }
    return results;
  }
};

}  // namespace

// Test suite for split context functionality
boost::ut::suite<"split_context_tests"> split_context_tests = []() {
  using namespace boost::ut;
  using namespace std::chrono_literals;

  std::println("\nStarting split context tests...\n");

  "Basic split context creation"_test = [&]() {
    reset_split_test_state();

    std::array<hal::byte, 8192> split_test_stack;
    std::pmr::monotonic_buffer_resource split_test_resource(
      split_test_stack.data(),
      split_test_stack.size(),
      std::pmr::null_memory_resource());

    hal::v5::async_runtime<2> contexts(
      split_test_resource, split_test_stack.size() / 2, split_test_handler);

    auto context0 = contexts.lease(0);
    auto context1 = contexts.lease(1);

    // Verify contexts are different objects
    expect(&(*context0) != &(*context1));

    // Test that we can create simple coroutines on each context
    auto task1 = simple_task(*context0, 1, 5);
    auto task2 = simple_task(*context1, 2, 10);

    auto result1 = task1.sync_wait();
    auto result2 = task2.sync_wait();
    // TODO(kammce): MUST FIX this will infinite loop!
    // auto task3 = simple_task(*context1, 2, 15);
    // auto result3 = task3.wait();

    expect(that % result1 == 10);
    expect(that % result2 == 20);
    expect(that % global_split_state.total_transitions > 0);
  };

  "Concurrent execution with split contexts"_test = [&]() {
    reset_split_test_state();

    constexpr auto context_count = 3;
    std::array<hal::byte, 8192> split_test_stack;
    std::pmr::monotonic_buffer_resource split_test_resource(
      split_test_stack.data(),
      split_test_stack.size(),
      std::pmr::null_memory_resource());

    hal::v5::async_runtime<context_count> manager(split_test_resource,
                                                  split_test_stack.size() /
                                                    context_count,
                                                  split_test_handler);

    // Test with 3 concurrent contexts
    concurrent_executor<context_count> executor(manager);
    executor.start_simple_tasks();

    auto results = executor.wait_all();

    expect(that % results[0] == 0);   // 0 * 10
    expect(that % results[1] == 10);  // 1 * 10
    expect(that % results[2] == 20);  // 2 * 10

    // Should have had transitions from multiple contexts
    expect(that % global_split_state.total_transitions >=
           6);  // At least 2 per task
    expect(that % global_split_state.transition_types.size() >= 6);
  };

  "Different task types on split contexts"_test = [&]() {
    reset_split_test_state();

    constexpr auto context_count = 3;
    std::array<hal::byte, 8192> split_test_stack;
    std::pmr::monotonic_buffer_resource split_test_resource(
      split_test_stack.data(),
      split_test_stack.size(),
      std::pmr::null_memory_resource());

    hal::v5::async_runtime<context_count> contexts(
      split_test_resource, split_test_stack.size() / 3, split_test_handler);

    // Run different types of tasks on each context
    auto int_task = simple_task(contexts[0], 42, 5);
    auto io_task_coro = io_task(contexts[1], "TestDevice");
    auto void_task_coro = void_task(contexts[2], 2);

    // Wait for all to complete
    auto int_result = int_task.sync_wait();
    auto io_result = io_task_coro.sync_wait();
    void_task_coro.sync_wait();  // void return

    expect(that % int_result == 420);
    expect(that % io_result == std::string("Result from TestDevice"));

    // Verify different block types were used
    bool has_time_block = false;
    bool has_io_block = false;
    bool has_sync_block = false;

    for (auto block_type : global_split_state.transition_types) {
      if (block_type == hal::v5::blocked_by::time)
        has_time_block = true;
      if (block_type == hal::v5::blocked_by::io)
        has_io_block = true;
      if (block_type == hal::v5::blocked_by::sync)
        has_sync_block = true;
    }

    expect(that % has_time_block == true);
    expect(that % has_io_block == true);
    expect(that % has_sync_block == true);
  };

  "Memory isolation between split contexts"_test = [&]() {
    reset_split_test_state();

    constexpr auto context_count = 2;
    std::array<hal::byte, 8192> split_test_stack;
    std::pmr::monotonic_buffer_resource split_test_resource(
      split_test_stack.data(),
      split_test_stack.size(),
      std::pmr::null_memory_resource());

    hal::v5::async_runtime<context_count> contexts(split_test_resource,
                                                   split_test_stack.size() /
                                                     context_count,
                                                   split_test_handler);

    // Create tasks that use memory differently
    auto memory_task1 = [](hal::v5::async_context& ctx) -> hal::v5::async<int> {
      // Allocate some memory via coroutine frame
      std::array<int, 50> large_array{};
      large_array.fill(1);

      co_await 1ms;

      int sum = 0;
      for (auto val : large_array) {
        sum += val;
      }
      co_return sum;
    };

    auto memory_task2 = [](hal::v5::async_context& ctx) -> hal::v5::async<int> {
      // Different memory usage pattern
      std::array<int, 30> smaller_array{};
      smaller_array.fill(2);

      co_await 2ms;

      int product = 1;
      for (size_t i = 0; i < 5; ++i) {
        product *= smaller_array[i];
      }
      co_return product;
    };

    auto task1 = memory_task1(contexts[0]);
    auto task2 = memory_task2(contexts[1]);

    auto result1 = task1.sync_wait();
    auto result2 = task2.sync_wait();

    expect(that % result1 == 50);  // 50 * 1
    expect(that % result2 == 32);  // 2^5
  };

  "Large number of split contexts"_test = [&]() {
    reset_split_test_state();

    constexpr auto context_count = 8;
    std::array<hal::byte, 8192> split_test_stack;
    std::pmr::monotonic_buffer_resource split_test_resource(
      split_test_stack.data(),
      split_test_stack.size(),
      std::pmr::null_memory_resource());

    hal::v5::async_runtime<context_count> contexts(split_test_resource,
                                                   split_test_stack.size() /
                                                     context_count,
                                                   split_test_handler);

    // Create a simple task for each context
    std::vector<hal::v5::async<int>> tasks;
    tasks.reserve(context_count);
    for (size_t i = 0; i < context_count; ++i) {
      tasks.push_back(simple_task(contexts[i], static_cast<int>(i), 1));
    }

    // Wait for all and verify results
    for (size_t i = 0; i < context_count; ++i) {
      auto result = tasks[i].sync_wait();
      expect(that % result == static_cast<int>(i * 10));
    }

    // Should have many transitions from different contexts
    expect(that % global_split_state.total_transitions >= context_count * 2);
  };

  "Split context error handling"_test = [&]() {
    constexpr auto context_count = 2;
    std::array<hal::byte, 8192> split_test_stack;
    std::pmr::monotonic_buffer_resource split_test_resource(
      split_test_stack.data(),
      split_test_stack.size(),
      std::pmr::null_memory_resource());

    hal::v5::async_runtime<context_count> contexts(split_test_resource,
                                                   split_test_stack.size() /
                                                     context_count,
                                                   split_test_handler);

    auto throwing_task = [](hal::v5::async_context& ctx,
                            bool should_throw) -> hal::v5::async<int> {
      co_await 1ms;

      if (should_throw) {
        throw std::runtime_error("Split context exception");
      }

      co_return 42;
    };

    auto good_task = throwing_task(contexts[0], false);
    auto bad_task = throwing_task(contexts[1], true);

    // Good task should complete normally
    auto good_result = good_task.sync_wait();
    expect(that % good_result == 42);

    // Bad task should throw
    expect(throws<std::runtime_error>([&bad_task]() { bad_task.sync_wait(); }));
  };
};
