#include <algorithm>
#include <coroutine>
#include <memory_resource>
#include <print>
#include <vector>

#include <boost/ut.hpp>

import hal;
import strong_ptr;

bool smart_motor_exists = false;

class tracking_allocator : public std::pmr::memory_resource
{
public:
  explicit tracking_allocator(
    std::pmr::memory_resource* p_upstream = std::pmr::new_delete_resource())
    : m_upstream(p_upstream)
  {
  }

  struct record
  {
    void* ptr;
    std::size_t bytes;
  };

  bool deallocate_called = false;
  hal::usize allocate_count = 0;
  hal::usize deallocate_count = 0;
  void* last_allocated_ptr = nullptr;
  void* last_deallocated_ptr = nullptr;
  std::size_t last_allocated_bytes = 0;
  std::size_t last_deallocated_bytes = 0;
  std::vector<record> allocations;
  std::vector<record> deallocations;

  [[nodiscard]] bool was_deallocated(void* p_ptr, std::size_t p_bytes) const
  {
    return std::ranges::any_of(deallocations, [&](record const& p_record) {
      return p_record.ptr == p_ptr && p_record.bytes == p_bytes;
    });
  }

private:
  void* do_allocate(std::size_t p_bytes, std::size_t p_alignment) override
  {
    allocate_count++;
    last_allocated_bytes = p_bytes;
    last_allocated_ptr = m_upstream->allocate(p_bytes, p_alignment);
    allocations.push_back({ last_allocated_ptr, p_bytes });
    return last_allocated_ptr;
  }

  void do_deallocate(void* p_ptr,
                     std::size_t p_bytes,
                     std::size_t p_alignment) override
  {
    deallocate_called = true;
    deallocate_count++;
    last_deallocated_ptr = p_ptr;
    last_deallocated_bytes = p_bytes;
    deallocations.push_back({ p_ptr, p_bytes });
    m_upstream->deallocate(p_ptr, p_bytes, p_alignment);
  }

  [[nodiscard]] bool do_is_equal(
    std::pmr::memory_resource const& p_other) const noexcept override
  {
    return this == &p_other;
  }

  std::pmr::memory_resource* m_upstream;
};

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

  ~smart_motor()
  {
    smart_motor_exists = false;
  }
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
  smart_motor_exists = true;
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
    return hal::allocate<test_awaitable_serial>(p_allocator);
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
    mem::scatter_span<hal::byte const> p_data) override
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
  tracking_allocator tracker;
  hal::allocator alloc{ &tracker };
  auto serial = test_awaitable_serial::create(alloc);
  async::inplace_context<1024> ctx;
  using namespace boost::ut;

  auto const allocations_before_create = tracker.allocations.size();
  std::vector<tracking_allocator::record> motor_allocations;
  {
    auto motor = smart_motor::create(ctx, alloc, serial);
    motor_allocations.assign(
      tracker.allocations.begin() +
        static_cast<std::ptrdiff_t>(allocations_before_create),
      tracker.allocations.end());

    expect(that % smart_motor_exists);
    expect(that % not tracker.deallocate_called);
    expect(that % tracker.last_deallocated_ptr == nullptr);
    expect(that % tracker.last_deallocated_bytes == 0);
  }
  expect(that % not smart_motor_exists);
  expect(that % tracker.deallocate_called);
  expect(that % not motor_allocations.empty());
  for (auto const& allocation : motor_allocations) {
    expect(that % tracker.was_deallocated(allocation.ptr, allocation.bytes));
  }
}
