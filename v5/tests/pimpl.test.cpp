#include <coroutine>
#include <memory_resource>
#include <print>
#include <span>
#include <thread>

import hal;
import strong_ptr;

class smart_motor : public hal::pimpl<smart_motor>
{
public:
  struct impl;  // forward declaration only

  // Create Factory Function
  static async::future<hal::ptr<smart_motor>> create(
    async::context&,
    hal::allocator p_allocator,
    hal::ptr<hal::awaitable_serial> const& p_serial);

  // Position rotation with velocity + torque control
  static async::future<hal::ptr<hal::veltor_servo>> acquire_veltor_servo(
    async::context&,
    hal::allocator);

  // Continuous rotation with velocity + torque control
  static async::future<hal::ptr<hal::motor>> acquire_motor(async::context&);

  // Must start with `acquire_` and should either be name of interface or
  // something descriptive like `acquire_rear_left_motor`.

  smart_motor(private_key,
              hal::allocator p_allocator,
              hal::ptr<hal::awaitable_serial> const& p_serial);
};

// in impl file
struct smart_motor::impl
{
  hal::ptr<hal::awaitable_serial> serial;
  hal::u8 address = 0;
};

smart_motor::smart_motor(private_key,
                         hal::allocator p_allocator,
                         hal::ptr<hal::awaitable_serial> const& p_serial)
  : pimpl(p_allocator, smart_motor::impl{ .serial = p_serial, .address = 0 })
{
  std::println("Hello, World!");
}

async::future<hal::ptr<smart_motor>> smart_motor::create(
  [[maybe_unused]] async::context& p_ctx,
  hal::allocator p_allocator,
  hal::ptr<hal::awaitable_serial> const& p_serial)
{
  return hal::allocate<smart_motor>(
    p_allocator, private_key{}, p_allocator, p_serial);
}

class test_awaitable_serial : public hal::awaitable_serial
{
public:
  static hal::ptr<test_awaitable_serial> create(hal::allocator p_allocator)
  {
    return mem::make_strong_ptr<test_awaitable_serial>(p_allocator);
  }

  hal::serial::settings configured_settings{};
  std::array<hal::byte, 256> last_data_out{};
  hal::usize last_data_out_size{};
  std::array<hal::byte, 256> rx_buffer{};
  hal::usize rx_cursor{ 0 };
  hal::serial_event last_event{};
  bool wait_for_called{ false };

  ~test_awaitable_serial() override = default;

private:
  async::future<void> driver_configure(
    async::context&,
    hal::serial::settings const& p_settings) override
  {
    configured_settings = p_settings;
    return {};
  }

  async::future<void> driver_write(
    async::context&,
    hal::scatter_span<hal::byte const> p_data) override
  {
    last_data_out_size = 0;
    for (auto const& span : p_data) {
      for (auto byte : span) {
        if (last_data_out_size >= last_data_out.size()) {
          break;
        }
        last_data_out[last_data_out_size] = byte;
        last_data_out_size++;
      }
    }
    return {};
  }

  hal::circular_span<hal::byte const> driver_receive_buffer() override
  {
    return rx_buffer;
  }

  hal::usize driver_receive_cursor() override
  {
    return rx_cursor;
  }

  async::future<void> driver_wait_for(async::context&,
                                      hal::serial_event p_event) override
  {
    last_event = p_event;
    wait_for_called = true;
    return {};
  }
};

int main()
{
  mem::monotonic_allocator<1024> alloc;
  auto serial = test_awaitable_serial::create(alloc);
  async::inplace_context<1024> ctx;
  auto motor = smart_motor::create(ctx, alloc, serial);

  using namespace std::chrono_literals;
  // std::this_thread::sleep_for(15s);
}
