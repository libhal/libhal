#include <array>
#include <coroutine>
#include <cstddef>
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

#include <libhal/coroutine.hpp>

#include <boost/ut.hpp>

// Example interface class
class interface
{
public:
  // Example method that returns a captured call
  auto evaluate(hal::v5::async_context& p_resource,
                std::span<hal::byte const> p_data)
  {
    return driver_evaluate(p_resource, p_data);
  }

  // Example method that returns a captured call
  auto hash(hal::v5::async_context& p_resource,
            std::span<hal::byte const> p_data)
  {
    return driver_hash(p_resource, p_data);
  }

  virtual ~interface() = default;

private:
  virtual hal::v5::async<hal::usize> driver_evaluate(
    hal::v5::async_context&,
    std::span<hal::byte const> p_data) = 0;

  virtual hal::v5::async<hal::usize> driver_hash(
    hal::v5::async_context&,
    std::span<hal::byte const> p_data) = 0;
};

class my_implementation : public interface
{
public:
  my_implementation() = default;

private:
  hal::v5::async<hal::usize> driver_evaluate(
    hal::v5::async_context& p_context,
    std::span<hal::byte const> p_data) override
  {
    // Implementation here
    std::println("__impl__::coro_evaluate!");

    for (int i = 0; i < 3; i++) {
      std::println("__impl__::coro_evaluate suspend loop {}!", i);
      co_await std::suspend_always();
    }

    co_return p_data.size();
  }

  hal::v5::async<hal::usize> driver_hash(
    hal::v5::async_context&,
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
  hal::v5::async<hal::usize> driver_evaluate(
    hal::v5::async_context& p_context,
    std::span<hal::byte const> p_data) override
  {
    using namespace std::chrono_literals;
    std::println("impl2::drive_evaluate CORO!");
    auto const sum = co_await m_impl->evaluate(p_context, p_data);
    std::println("impl2::drive_evaluate CORO! 2");
    auto const sum2 = sum + co_await m_impl->evaluate(p_context, p_data);
    std::println("impl2::drive_evaluate CORO! 3 -> suspend always");
    co_await std::suspend_always();
    std::println("impl2::drive_evaluate CORO! 4 -> await time");
    std::println("impl2::drive_evaluate CORO! 5 -> await time");
    co_await 100ms;
    // Implementation here
    co_return sum2 + p_data.size();
  }

  hal::v5::async<hal::usize> coro_hash(hal::v5::async_context& p_resource,
                                       std::span<hal::byte const> p_data)
  {
    std::println("impl2::coro_hash!");
    unsigned const sum = std::accumulate(p_data.begin(), p_data.end(), 0);
    auto const eval = co_await driver_evaluate(p_resource, p_data);
    co_return sum + eval + p_data.size();
  }

  hal::v5::async<hal::usize> driver_hash(
    hal::v5::async_context& p_resource,
    std::span<hal::byte const> p_data) override
  {
    co_return co_await coro_hash(p_resource, p_data);
  }

  interface* m_impl = nullptr;
};

hal::v5::async<hal::usize> my_task(hal::v5::async_context& p_context,
                                   interface& p_impl,
                                   std::span<hal::byte> p_buff)
{
  std::println("my task! 1");
  auto value1 = co_await p_impl.evaluate(p_context, p_buff);
  std::println("my task! 2");
  auto value2 = co_await p_impl.evaluate(p_context, p_buff);
  co_return value1 + value2;
}

std::array<hal::byte, 400> coroutine_stack;
std::pmr::monotonic_buffer_resource coroutine_resource(
  coroutine_stack.data(),
  coroutine_stack.size(),
  std::pmr::null_memory_resource());
auto handler = [](hal::v5::async_context& p_context,
                  hal::v5::blocked_by p_previous_state,
                  hal::v5::block_info p_block_info) noexcept {
  std::println("🔄 previous_state = {}", static_cast<int>(p_previous_state));
  if (std::holds_alternative<hal::v5::time_info>(p_block_info)) {
    std::println("⏲️ Simulate waiting for {}...",
                 std::get<hal::v5::time_info>(p_block_info).duration);
  }
  p_context.unblock();
  p_context.busy_loop_until_unblocked();
};

hal::v5::async_context ctx(coroutine_resource, 400, handler);

boost::ut::suite<"coro_exper"> coro_expr = []() {
  using namespace boost::ut;
  "angular velocity sensor interface test"_test = []() {
    my_implementation impl;
    my_implementation2 impl2(impl);

    std::array<hal::byte, 5> special_buffer{};

    auto coro = my_task(ctx, impl2, special_buffer);
    std::println("Coro initialized! now calling result()!");
    auto const value = coro.sync_result();
    std::println("value = {}", value);
  };
};
