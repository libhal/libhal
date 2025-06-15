#include <array>
#include <chrono>
#include <coroutine>
#include <cstddef>
#include <memory_resource>
#include <print>
#include <span>
#include <stdexcept>
#include <thread>
#include <variant>

#include <libhal/coroutine.hpp>

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
    co_return;  // TODO(kammce): This is where the problem is, only for return
                // void?
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

  // Example method that returns a captured call
  auto hash(hal::v5::async_context& p_context,
            std::span<hal::byte const> p_data)
  {
    return driver_hash(p_context, p_data);
  }

  virtual ~interface() = default;

private:
  virtual hal::v5::async<hal::usize> driver_evaluate(
    hal::v5::async_context& p_context,
    std::span<hal::byte const> p_data) = 0;

  virtual hal::v5::async<hal::usize> driver_hash(
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
      p_context.unblock();
      break;

    case hal::v5::blocked_by::io:
      global_test_state.io_handler_called = true;
      std::println("🔌 I/O block detected");
      // Simulate I/O completion after a short delay
      std::thread([&p_context]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        p_context.unblock();
      }).detach();
      break;

    case hal::v5::blocked_by::sync:
      global_test_state.sync_handler_called = true;
      std::println("🔒 Sync block detected");
      p_context.unblock();
      break;

    case hal::v5::blocked_by::inbox_empty:
      global_test_state.inbox_handler_called = true;
      std::println("📪 Inbox empty block detected");
      p_context.unblock();
      break;

    case hal::v5::blocked_by::outbox_full:
      global_test_state.outbox_handler_called = true;
      std::println("📤 Outbox full block detected");
      p_context.unblock();
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

    expect(global_test_state.time_handler_called == true);
    expect(global_test_state.io_handler_called == true);
    expect(global_test_state.sync_handler_called == true);
    expect(global_test_state.inbox_handler_called == true);
    expect(global_test_state.outbox_handler_called == true);
    expect(coro.done());

    auto result = coro.sync_result();
    expect(result == 12345);
    expect(global_test_state.transition_count >= 5);
  };

  "Exception handling - immediate throw"_test = []() {
    reset_test_state();
    exception_thrower thrower;

    auto coro = thrower.throw_immediately(test_ctx);

    expect(throws<std::runtime_error>([&coro]() { coro.sync_result(); }));
  };

  "Exception handling - throw after suspend"_test = []() {
    reset_test_state();
    exception_thrower thrower;

    auto coro = thrower.throw_after_suspend(test_ctx);

    expect(throws<std::logic_error>([&coro]() { coro.sync_result(); }));
  };

  "Exception handling - throw after time wait"_test = []() {
    reset_test_state();
    exception_thrower thrower;

    auto coro = thrower.throw_after_time_wait(test_ctx);

    expect(throws<std::invalid_argument>([&coro]() { coro.sync_result(); }));

    expect(global_test_state.time_handler_called == true);
  };

  "Exception handling - nested exception"_test = []() {
    reset_test_state();
    exception_thrower thrower;

    auto coro = thrower.nested_exception_call(test_ctx);

    expect(throws<std::runtime_error>([&coro]() { coro.sync_result(); }));
  };

  "Complex composition test"_test = []() {
    reset_test_state();
    compositor comp;

    auto coro = comp.complex_operation(test_ctx);
    auto result = coro.sync_result();

    expect(result == 142);  // 42 (lock_value) + 100 (io_result bonus)
    expect(that % global_test_state.time_handler_called == true);
    expect(that % global_test_state.io_handler_called == true);
    expect(that % global_test_state.sync_handler_called == true);
    expect(that % global_test_state.outbox_handler_called == true);
  };

  "Exception recovery test"_test = []() {
    reset_test_state();
    compositor comp;

    auto coro = comp.exception_recovery(test_ctx);
    auto result = coro.sync_result();

    expect(result == "Recovered: Hello from message queue");
    expect(global_test_state.inbox_handler_called == true);
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
    auto result = coro.sync_result();

    expect(result == 25);  // ((10 * 2) + 5) = 25
    expect(global_test_state.time_handler_called == true);
    expect(global_test_state.transition_count >= 3);  // At least 3 time waits
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
    coro.sync_result();  // Should not throw

    expect(global_test_state.time_handler_called == true);
    expect(global_test_state.sync_handler_called == true);
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
    auto result = coro.sync_result();

    expect(result == 999);
    expect(global_test_state.time_handler_called == true);
  };

  "Coroutine state inspection"_test = []() {
    reset_test_state();

    auto inspection_test =
      [](hal::v5::async_context& ctx) -> hal::v5::async<int> {
      expect(ctx.state() == hal::v5::blocked_by::nothing);

      co_await 1ms;
      // After resume, should be unblocked again
      expect(ctx.state() == hal::v5::blocked_by::nothing);

      ctx.block_by_io();
      co_await std::suspend_always{};

      expect(ctx.state() == hal::v5::blocked_by::io);
      co_return 777;
    };

    auto coro = inspection_test(test_ctx);
    auto result = coro.sync_result();

    expect(result == 777);
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

      hal::v5::async<hal::usize> driver_hash(
        hal::v5::async_context&,
        std::span<hal::byte const> p_data) override
      {
        co_return p_data.size();
      }
    };

    throwing_interface impl;
    std::array<hal::byte, 10> buffer{};

    auto task = [&](hal::v5::async_context& ctx) -> hal::v5::async<hal::usize> {
      co_return co_await impl.evaluate(ctx, buffer);
    };

    auto coro = task(test_ctx);

    expect(throws<std::domain_error>([&coro]() { coro.sync_result(); }));
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

      hal::v5::async<hal::usize> driver_hash(
        hal::v5::async_context& ctx,
        std::span<hal::byte const> p_data) override
      {
        co_return co_await m_impl->hash(ctx, p_data);
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

      hal::v5::async<hal::usize> driver_hash(
        hal::v5::async_context&,
        std::span<hal::byte const> p_data) override
      {
        co_return p_data.size();
      }
    };

    throwing_impl dangerous_impl;
    safe_wrapper safe_impl(dangerous_impl);
    std::array<hal::byte, 5> buffer{};

    auto coro = safe_impl.evaluate(test_ctx, buffer);
    auto result = coro.sync_result();  // Should not throw

    expect(result == 0);  // Safe fallback value
  };
};
