#include <chrono>
#include <cstddef>
#include <memory_resource>
#include <print>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>

#include <libhal/experimental/coroutine.hpp>

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

// Setup common test infrastructure
std::array<hal::byte, 4096> test_stack;
std::pmr::monotonic_buffer_resource test_resource(
  test_stack.data(),
  test_stack.size(),
  std::pmr::null_memory_resource());

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
                       hal::v5::block_info p_block_info) noexcept {
  global_test_state.transition_count++;
  global_test_state.last_state = p_state;

  std::println("🔄 State transition #{} to #{}",
               global_test_state.transition_count,
               static_cast<int>(p_state));

  switch (p_state) {
    case hal::v5::blocked_by::time:
      global_test_state.time_handler_called = true;
      if (std::holds_alternative<hal::v5::time_info>(p_block_info)) {
        auto duration = std::get<hal::v5::time_info>(p_block_info).duration;
        std::println(
          "⏲️  Time block: {}ms",
          std::chrono::duration_cast<std::chrono::milliseconds>(duration)
            .count());
      }
      p_context.unblock_without_notification();
      break;

    case hal::v5::blocked_by::io:
      global_test_state.io_handler_called = true;
      std::println("🔌 I/O block detected");
      // Simulate I/O completion after a short delay
      std::thread([&p_context]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        p_context.unblock_without_notification();
      }).detach();
      break;

    case hal::v5::blocked_by::sync:
      global_test_state.sync_handler_called = true;
      std::println("🔒 Sync block detected");
      p_context.unblock_without_notification();
      break;

    case hal::v5::blocked_by::inbox_empty:
      global_test_state.inbox_handler_called = true;
      std::println("📪 Inbox empty block detected");
      p_context.unblock_without_notification();
      break;

    case hal::v5::blocked_by::outbox_full:
      global_test_state.outbox_handler_called = true;
      std::println("📤 Outbox full block detected");
      p_context.unblock_without_notification();
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

hal::v5::async_context test_ctx(test_resource, test_stack.size(), test_handler);

// Test suite implementation
boost::ut::suite<"advanced_coroutine_tests"> advanced_tests = []() {
  using namespace boost::ut;
  using namespace std::chrono_literals;

  "State transitions test"_test = []() {
    reset_test_state();

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

    coro.resume();
    expect(that % global_test_state.time_handler_called == true);
    expect(that % global_test_state.io_handler_called == false);
    expect(that % global_test_state.sync_handler_called == false);
    expect(that % global_test_state.inbox_handler_called == false);
    expect(that % global_test_state.outbox_handler_called == false);
    coro.resume();
    expect(that % global_test_state.io_handler_called == true);
    expect(that % global_test_state.sync_handler_called == false);
    expect(that % global_test_state.inbox_handler_called == false);
    expect(that % global_test_state.outbox_handler_called == false);
    coro.resume();
    expect(that % global_test_state.sync_handler_called == true);
    expect(that % global_test_state.inbox_handler_called == false);
    expect(that % global_test_state.outbox_handler_called == false);
    coro.resume();
    expect(that % global_test_state.inbox_handler_called == true);
    expect(that % global_test_state.outbox_handler_called == false);
    coro.resume();
    expect(that % global_test_state.outbox_handler_called == true);
    coro.resume();
    expect(that % coro.done());

    auto result = coro.wait();
    expect(that % result == 12345);
    expect(that % global_test_state.transition_count >= 5);
  };

  "Exception handling - immediate throw"_test = []() {
    reset_test_state();
    exception_thrower thrower;

    auto coro = thrower.throw_immediately(test_ctx);

    expect(throws<std::runtime_error>([&coro]() { coro.wait(); }));
  };

  "Exception handling - throw after suspend"_test = []() {
    reset_test_state();
    exception_thrower thrower;

    auto coro = thrower.throw_after_suspend(test_ctx);

    expect(throws<std::logic_error>([&coro]() { coro.wait(); }));
  };

  "Exception handling - throw after time wait"_test = []() {
    reset_test_state();
    exception_thrower thrower;

    auto coro = thrower.throw_after_time_wait(test_ctx);

    expect(throws<std::invalid_argument>([&coro]() { coro.wait(); }));

    expect(that % global_test_state.time_handler_called == true);
  };

  "Exception handling - nested exception"_test = []() {
    reset_test_state();
    exception_thrower thrower;

    auto coro = thrower.nested_exception_call(test_ctx);

    expect(throws<std::runtime_error>([&coro]() { coro.wait(); }));
  };

  "Complex composition test"_test = []() {
    reset_test_state();
    compositor comp;

    auto coro = comp.complex_operation(test_ctx);
    auto result = coro.wait();

    expect(that % result == 142);  // 42 (lock_value) + 100 (io_result bonus)
    expect(that % global_test_state.time_handler_called == true);
    expect(that % global_test_state.io_handler_called == true);
    expect(that % global_test_state.sync_handler_called == true);
    expect(that % global_test_state.outbox_handler_called == true);
  };

  "Exception recovery test"_test = []() {
    reset_test_state();
    compositor comp;

    auto coro = comp.exception_recovery(test_ctx);
    auto result = coro.wait();

    expect(that % result == std::string("Recovered: Hello from message queue"));
    expect(that % global_test_state.inbox_handler_called == true);
  };

  "Multiple coroutine chaining"_test = []() {
    reset_test_state();

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
    auto result = coro.wait();

    expect(that % result == 25);  // ((10 * 2) + 5) = 25
    expect(that % global_test_state.time_handler_called == true);
    expect(that % global_test_state.transition_count >=
           3);  // At least 3 time waits
  };

  "Void coroutine test"_test = []() {
    reset_test_state();

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
    coro.wait();  // Should not throw

    expect(that % global_test_state.time_handler_called == true);
    expect(that % global_test_state.sync_handler_called == true);
  };

  "Memory resource test"_test = []() {
    reset_test_state();

    // Test with limited memory
    std::array<hal::byte, 128> small_stack;
    std::pmr::monotonic_buffer_resource small_resource(
      small_stack.data(), small_stack.size(), std::pmr::null_memory_resource());

    hal::v5::async_context small_ctx(
      small_resource, small_stack.size(), test_handler);

    auto memory_test = [](hal::v5::async_context& ctx) -> hal::v5::async<int> {
      std::println("📊 Testing with limited memory");
      co_await 1ms;
      co_return 999;
    };

    auto coro = memory_test(small_ctx);
    auto result = coro.wait();

    expect(that % result == 999);
    expect(that % global_test_state.time_handler_called == true);
  };

  "Coroutine state inspection"_test = []() {
    reset_test_state();

    auto inspection_test =
      [](hal::v5::async_context& ctx) -> hal::v5::async<int> {
      expect(ctx.state() == hal::v5::blocked_by::nothing);

      co_await 1ms;
      // After resume, should be unblock_without_notificationed again
      expect(ctx.state() == hal::v5::blocked_by::nothing);

      ctx.block_by_io();
      co_await std::suspend_always{};

      expect(ctx.state() == hal::v5::blocked_by::io);
      co_return 777;
    };

    auto coro = inspection_test(test_ctx);
    auto result = coro.wait();

    expect(that % result == 777);
  };
};

// Additional interface-based tests similar to the original
boost::ut::suite<"interface_coroutine_tests"> interface_tests = []() {
  using namespace boost::ut;

  "Interface exception propagation"_test = []() {
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

    expect(throws<std::domain_error>([&coro]() { coro.wait(); }));
  };

  "Interface composition with exception handling"_test = []() {
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
    auto result = coro.wait();  // Should not throw

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

// Test infrastructure
std::array<hal::byte, 2048> sync_test_stack;
std::pmr::monotonic_buffer_resource sync_test_resource(
  sync_test_stack.data(),
  sync_test_stack.size(),
  std::pmr::null_memory_resource());

struct sync_test_state
{
  int transition_count = 0;
  bool any_time_blocks = false;
  bool any_async_operations = false;
};

sync_test_state global_sync_state;

auto sync_test_handler = [](hal::v5::async_context& p_context,
                            hal::v5::blocked_by p_state,
                            hal::v5::block_info) noexcept {
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

hal::v5::async_context sync_test_ctx(sync_test_resource,
                                     sync_test_stack.size(),
                                     sync_test_handler);

}  // namespace sync_tests

// Test suite for synchronous returns
boost::ut::suite<"synchronous_return_tests"> sync_return_tests = []() {
  using namespace boost::ut;
  using namespace sync_tests;

  "Free function immediate returns"_test = []() {
    reset_sync_test_state();

    // Test immediate int return
    auto mult_coro = sync_multiply(sync_test_ctx, 6, 7);
    expect(that % mult_coro.done() == true);  // Should be immediately done
    auto mult_result = mult_coro.wait();
    expect(that % mult_result == 42);
    expect(that % global_sync_state.any_async_operations == false);

    // Test immediate string return
    auto format_coro = sync_format_number(sync_test_ctx, 123);
    expect(that % format_coro.done() == true);
    auto format_result = format_coro.wait();
    expect(that % format_result == std::string("Number: 123"));

    // Test immediate bool return
    auto even_coro = sync_is_even(sync_test_ctx, 8);
    expect(that % even_coro.done() == true);
    auto even_result = even_coro.wait();
    expect(that % even_result == true);

    // Test immediate void return
    auto log_coro = sync_log_message(sync_test_ctx, "Test message");
    expect(that % log_coro.done() == true);
    log_coro.wait();  // Should not throw

    expect(that % global_sync_state.transition_count == 0);
  };

  "Conditional sync/async free functions"_test = []() {
    reset_sync_test_state();

    // Test immediate factorial (n=1)
    auto fact1_coro = conditional_factorial(sync_test_ctx, 1);
    expect(that % fact1_coro.done() == true);
    auto fact1_result = fact1_coro.wait();
    expect(that % fact1_result == 1);

    // Test sync factorial (n=4)
    auto fact4_coro = conditional_factorial(sync_test_ctx, 4);
    expect(that % fact4_coro.done() == true);
    auto fact4_result = fact4_coro.wait();
    expect(that % fact4_result == 24);

    // Test sync factorial (n=8)
    reset_sync_test_state();
    auto fact8_coro = conditional_factorial(sync_test_ctx, 8);
    expect(that % fact8_coro.done() == true);
    auto fact8_result = fact8_coro.wait();
    expect(that % fact8_result == 40320);
    expect(that % global_sync_state.any_time_blocks == false);

    // Test string processing
    reset_sync_test_state();

    // Empty string - immediate
    auto empty_coro = conditional_process_string(sync_test_ctx, "");
    expect(that % empty_coro.done() == true);
    auto empty_result = empty_coro.wait();
    expect(that % empty_result == std::string("Empty"));

    // Short string - sync
    auto short_coro = conditional_process_string(sync_test_ctx, "Hello");
    expect(that % short_coro.done() == true);
    auto short_result = short_coro.wait();
    expect(that % short_result == std::string("Short: Hello"));

    // Long string - sync
    reset_sync_test_state();
    auto long_coro = conditional_process_string(
      sync_test_ctx, "This is a very long string for testing");
    expect(that % long_coro.done() == true);
    auto long_result = long_coro.wait();
    expect(that % long_result == std::string("Long: This is a ..."));
    expect(that % global_sync_state.any_time_blocks == false);
  };

  "Interface immediate returns"_test = []() {
    reset_sync_test_state();
    immediate_impl impl;

    // Test immediate calculation
    auto calc_coro = impl.calculate(sync_test_ctx, 15, 27);
    expect(that % calc_coro.done() == true);
    auto calc_result = calc_coro.wait();
    expect(that % calc_result == 42);

    // Test immediate status
    auto status_coro = impl.get_status(sync_test_ctx);
    expect(that % status_coro.done() == true);
    auto status_result = status_coro.wait();
    expect(that % status_result == std::string("Ready"));

    // Test immediate validation
    std::array<hal::byte, 5> test_data{ 1, 2, 3, 4, 5 };
    auto valid_coro = impl.validate_data(sync_test_ctx, test_data);
    expect(that % valid_coro.done() == true);
    auto valid_result = valid_coro.wait();
    expect(that % valid_result == true);

    // Test immediate void processing
    auto process_coro = impl.process_conditionally(sync_test_ctx, true);
    expect(that % process_coro.done() == true);
    process_coro.wait();

    expect(that % global_sync_state.transition_count == 0);
  };

  "Interface conditional sync/async"_test = []() {
    reset_sync_test_state();
    conditional_impl impl;

    // Small calculation - should be async but fast
    auto small_calc = impl.calculate(sync_test_ctx, 3, 4);
    expect(that % small_calc.done() ==
           false);  // Uses co_return, so still async
    auto small_result = small_calc.wait();
    expect(that % small_result == 7);

    // Large calculation - async with delay
    reset_sync_test_state();
    auto large_calc = impl.calculate(sync_test_ctx, 15, 20);
    expect(that % large_calc.done() == false);
    auto large_result = large_calc.wait();
    expect(that % large_result == 35);
    expect(that % global_sync_state.any_time_blocks == true);

    // Status - always immediate
    reset_sync_test_state();
    auto status_coro = impl.get_status(sync_test_ctx);
    expect(that % status_coro.done() == true);
    auto status_result = status_coro.wait();
    expect(that % status_result == std::string("Conditional Ready"));
    expect(that % global_sync_state.any_async_operations == false);

    // Small data validation - immediate
    std::array<hal::byte, 5> small_data{ 1, 2, 3, 4, 5 };
    auto small_valid = impl.validate_data(sync_test_ctx, small_data);
    expect(that % small_valid.done() == true);
    auto small_valid_result = small_valid.wait();
    expect(that % small_valid_result == true);

    // Large data validation - sync
    reset_sync_test_state();
    std::array<hal::byte, 50> large_data{};
    auto large_valid = impl.validate_data(sync_test_ctx, large_data);
    expect(that % large_valid.done() == true);
    auto large_valid_result = large_valid.wait();
    expect(that % large_valid_result == true);
    expect(that % global_sync_state.any_time_blocks == false);

    // No processing - immediate void
    reset_sync_test_state();
    auto no_process = impl.process_conditionally(sync_test_ctx, false);
    expect(that % no_process.done() == true);
    no_process.wait();
    expect(that % global_sync_state.any_async_operations == false);

    // With processing - sync void
    reset_sync_test_state();
    auto with_process = impl.process_conditionally(sync_test_ctx, true);
    expect(that % with_process.done() == true);
    with_process.wait();
    expect(that % global_sync_state.any_time_blocks == false);
  };

  "Mixed sync/async composition"_test = []() {
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
    auto result = coro.wait();

    expect(that % result ==
           std::string("mult=42, even=true, fact=720, format='Number: 720'"));
  };

  "Performance comparison test"_test = []() {
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
    auto result = coro.wait();

    // 0*2 + 1*2 + 2*2 + ... + 9*2 + 720 = 90 + 720 = 810
    expect(that % result == 810);
    expect(that % global_sync_state.transition_count == 0);
  };

  "Error handling with immediate returns"_test = []() {
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
    auto success_result = success_coro.wait();
    expect(that % success_result == 15);

    // Test immediate exception
    expect(throws<std::invalid_argument>(
      [&impl]() { impl.calculate(sync_test_ctx, -1, 5).wait(); }));
  };
};
