// Copyright 2026 Khalil Estell and the libhal contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <array>
#include <chrono>
#include <memory_resource>
#include <type_traits>
#include <vector>

#include <boost/ut.hpp>

import hal;

using namespace mp_units;
using namespace mp_units::si::unit_symbols;

namespace {
class test_spi : public hal::spi_channel
{
public:
  hal::spi_channel::settings configured_settings{};
  hal::hertz reported_clock_rate = 100'000 * si::hertz;
  bool chip_selected{ false };
  std::array<hal::byte, 256> last_data_out{};
  std::array<hal::byte, 256> last_data_in{};
  hal::usize last_data_out_size{};
  hal::usize last_data_in_size{};
  hal::byte last_filler{ hal::spi_channel::default_filler };

private:
  void driver_configure(hal::spi_channel::settings const& p_settings) override
  {
    configured_settings = p_settings;
  }

  hal::hertz driver_clock_rate() override
  {
    return reported_clock_rate;
  }

  void driver_chip_select(bool p_select) override
  {
    chip_selected = p_select;
  }

  void driver_transfer(mem::scatter_span<hal::byte const> p_data_out,
                       mem::scatter_span<hal::byte> p_data_in,
                       hal::byte p_filler) override
  {
    last_filler = p_filler;

    last_data_out_size = 0;
    for (auto const& span : p_data_out) {
      for (auto byte : span) {
        if (last_data_out_size >= last_data_out.size()) {
          break;
        }
        last_data_out[last_data_out_size] = byte;
        last_data_out_size++;
      }
    }

    last_data_in_size = 0;
    for (auto const& span : p_data_in) {
      last_data_in_size += span.size();
    }
  }
};

void spi_configure_test() noexcept
{
  using namespace boost::ut;

  "configure() passes settings to driver"_test = [&]() {
    // Setup
    test_spi test;
    hal::spi_channel::settings expected_settings{
      .clock_rate = 1 * MHz,
      .bus_mode = hal::spi_channel::mode::m1,
    };

    // Exercise
    test.configure(expected_settings);

    // Verify
    expect(expected_settings == test.configured_settings);
  };

  "configure() with default settings"_test = [&]() {
    // Setup
    test_spi test;
    hal::spi_channel::settings default_settings{};

    // Exercise
    test.configure(default_settings);

    // Verify
    expect((100 * kHz) == test.configured_settings.clock_rate);
    expect(hal::spi_channel::mode::m0 == test.configured_settings.bus_mode);
  };
}

void spi_clock_rate_test() noexcept
{
  using namespace boost::ut;

  "clock_rate() returns value from driver"_test = [&]() {
    // Setup
    test_spi test;
    auto const expected_freq = 4'000'000 * si::hertz;
    test.reported_clock_rate = expected_freq;

    // Exercise
    auto rate = test.clock_rate();

    // Verify
    expect(expected_freq == rate);
  };
}

void spi_chip_select_test() noexcept
{
  using namespace boost::ut;

  "chip_select(true) asserts chip select"_test = [&]() {
    // Setup
    test_spi test;

    // Exercise
    test.chip_select(true);

    // Verify
    expect(test.chip_selected);
  };

  "chip_select(false) de-asserts chip select"_test = [&]() {
    // Setup
    test_spi test;
    test.chip_select(true);

    // Exercise
    test.chip_select(false);

    // Verify
    expect(not test.chip_selected);
  };
}

void spi_lock_unlock_test() noexcept
{
  using namespace boost::ut;

  "lock() asserts chip select"_test = [&]() {
    // Setup
    test_spi test;

    // Exercise
    test.lock();

    // Verify
    expect(test.chip_selected);
  };

  "unlock() de-asserts chip select"_test = [&]() {
    // Setup
    test_spi test;
    test.lock();

    // Exercise
    test.unlock();

    // Verify
    expect(not test.chip_selected);
  };
}

void spi_transfer_test() noexcept
{
  using namespace boost::ut;

  "transfer() with write-only data"_test = [&]() {
    // Setup
    test_spi test;
    std::array<hal::byte, 4> write_buffer = { 0x01, 0x02, 0x03, 0x04 };

    // Exercise
    test.transfer({ write_buffer }, {});

    // Verify
    expect(that % 4 == test.last_data_out_size);
    expect(that % 0 == test.last_data_in_size);
  };

  "transfer() with read-only data"_test = [&]() {
    // Setup
    test_spi test;
    std::array<hal::byte, 8> read_buffer{};

    // Exercise
    test.transfer({}, { read_buffer });

    // Verify
    expect(that % 0 == test.last_data_out_size);
    expect(that % 8 == test.last_data_in_size);
  };

  "transfer() with write-and-read data"_test = [&]() {
    // Setup
    test_spi test;
    std::array<hal::byte, 3> write_buffer = { 0xAA, 0xBB, 0xCC };
    std::array<hal::byte, 5> read_buffer{};

    // Exercise
    test.transfer({ write_buffer }, { read_buffer });

    // Verify
    expect(that % 3 == test.last_data_out_size);
    expect(that % 5 == test.last_data_in_size);
  };

  "transfer() with empty data"_test = [&]() {
    // Setup
    test_spi test;

    // Exercise
    test.transfer({}, {});

    // Verify
    expect(that % 0 == test.last_data_out_size);
    expect(that % 0 == test.last_data_in_size);
  };

  "transfer() uses default filler when not specified"_test = [&]() {
    // Setup
    test_spi test;

    // Exercise
    test.transfer({}, {});

    // Verify
    expect(hal::spi_channel::default_filler == test.last_filler);
  };

  "transfer() uses custom filler when specified"_test = [&]() {
    // Setup
    test_spi test;
    hal::byte custom_filler{ 0x00 };

    // Exercise
    test.transfer({}, {}, custom_filler);

    // Verify
    expect(custom_filler == test.last_filler);
  };

  "transfer() write data content is correct"_test = [&]() {
    // Setup
    test_spi test;
    std::array<hal::byte, 3> write_buffer = { hal::byte{ 0x11 },
                                              hal::byte{ 0x22 },
                                              hal::byte{ 0x33 } };

    // Exercise
    test.transfer({ write_buffer }, {});

    // Verify
    expect(hal::byte{ 0x11 } == test.last_data_out[0]);
    expect(hal::byte{ 0x22 } == test.last_data_out[1]);
    expect(hal::byte{ 0x33 } == test.last_data_out[2]);
  };
}

// A bus that records what the manager asks of it
class fake_bus final : public hal::raw_spi_bus
{
public:
  std::vector<hal::spi_settings> configured{};
  hal::usize transfer_count = 0;
  hal::hertz max_rate = 10 * MHz;
  bool throw_on_transfer = false;

private:
  void driver_configure(hal::spi_settings const& p_settings) override
  {
    configured.push_back(p_settings);
  }

  hal::hertz driver_clock_rate(hal::spi_settings const& p_settings) override
  {
    if (p_settings.bus_mode == hal::spi_mode::m3) {
      throw hal::operation_not_supported(this);
    }
    return p_settings.clock_rate < max_rate ? p_settings.clock_rate : max_rate;
  }

  void driver_transfer(mem::scatter_span<hal::byte const>,
                       mem::scatter_span<hal::byte>,
                       hal::byte) override
  {
    transfer_count++;
    if (throw_on_transfer) {
      throw hal::io_error(this);
    }
  }
};

class fake_pin final : public hal::output_pin
{
public:
  bool high = false;
  hal::usize writes = 0;

private:
  void driver_configure(hal::pin_settings const&) override
  {
  }

  bool driver_level() override
  {
    return high;
  }

  void driver_level(bool p_high) override
  {
    high = p_high;
    writes++;
  }
};

class fake_lock final : public hal::basic_lock
{
public:
  bool locked = false;
  hal::usize lock_count = 0;

private:
  void driver_lock() override
  {
    locked = true;
    lock_count++;
  }

  void driver_unlock() override
  {
    locked = false;
  }
};

// A bus that can't be copied or moved, like a platform's, and reports to a
// log that outlives it, since an owning manager keeps the bus out of reach.
class pinned_bus final : public hal::raw_spi_bus
{
public:
  explicit pinned_bus(std::vector<hal::spi_settings>& p_log)
    : m_log(&p_log)
  {
  }
  pinned_bus(pinned_bus const&) = delete;
  pinned_bus& operator=(pinned_bus const&) = delete;
  pinned_bus(pinned_bus&&) = delete;
  pinned_bus& operator=(pinned_bus&&) = delete;
  ~pinned_bus() = default;

private:
  void driver_configure(hal::spi_settings const& p_settings) override
  {
    m_log->push_back(p_settings);
  }

  hal::hertz driver_clock_rate(hal::spi_settings const& p_settings) override
  {
    return p_settings.clock_rate;
  }

  void driver_transfer(mem::scatter_span<hal::byte const>,
                       mem::scatter_span<hal::byte>,
                       hal::byte) override
  {
  }

  std::vector<hal::spi_settings>* m_log;
};

void spi_bus_manager_test()
{
  using namespace boost::ut;

  "a new channel starts with its device deselected"_test = []() {
    // Setup
    fake_bus bus;
    fake_pin chip_select;
    hal::spi_bus_manager manager(bus);

    // Exercise
    auto channel = manager.acquire_channel(chip_select);

    // Verify
    expect(chip_select.high);
    expect(bus.configured.empty());
  };

  "selecting applies the channel's settings and drives chip select low"_test =
    []() {
      // Setup
      fake_bus bus;
      fake_pin chip_select;
      hal::spi_bus_manager manager(bus);
      hal::spi_settings const settings{ .clock_rate = 1 * MHz,
                                        .bus_mode = hal::spi_mode::m1 };
      auto channel = manager.acquire_channel(chip_select, settings);

      // Exercise
      channel.chip_select(true);
      bool const low_while_selected = not chip_select.high;
      channel.chip_select(false);

      // Verify
      expect(low_while_selected);
      expect(chip_select.high);
      expect(that % 1 == bus.configured.size());
      expect(settings == bus.configured.front());
    };

  "each channel brings its own settings to the bus"_test = []() {
    // Setup
    fake_bus bus;
    fake_pin flash_select;
    fake_pin imu_select;
    hal::spi_bus_manager manager(bus);
    auto flash =
      manager.acquire_channel(flash_select, { .clock_rate = 4 * MHz });
    auto imu = manager.acquire_channel(imu_select, { .clock_rate = 1 * MHz });
    std::array<hal::byte, 1> data{};

    // Exercise
    flash.transfer({ data });
    imu.transfer({ data });

    // Verify
    expect(that % 2 == bus.configured.size());
    expect((4 * MHz) == bus.configured[0].clock_rate);
    expect((1 * MHz) == bus.configured[1].clock_rate);
    expect(flash_select.high);
    expect(imu_select.high);
  };

  "selecting a second channel while one is selected throws"_test = []() {
    // Setup
    fake_bus bus;
    fake_pin flash_select;
    fake_pin imu_select;
    hal::spi_bus_manager manager(bus);
    auto flash = manager.acquire_channel(flash_select);
    auto imu = manager.acquire_channel(imu_select);
    flash.chip_select(true);
    std::array<hal::byte, 1> data{};

    // Exercise + Verify
    expect(
      throws<hal::device_or_resource_busy>([&]() { imu.chip_select(true); }));
    expect(
      throws<hal::device_or_resource_busy>([&]() { imu.transfer({ data }); }));
    expect(imu_select.high);
    expect(not flash_select.high);
    expect(that % 0 == bus.transfer_count);

    // Once the first is deselected, the second may select
    flash.chip_select(false);
    imu.chip_select(true);
    expect(not imu_select.high);
  };

  "selecting the selected channel again does nothing"_test = []() {
    // Setup
    fake_bus bus;
    fake_pin chip_select;
    hal::spi_bus_manager manager(bus);
    auto channel = manager.acquire_channel(chip_select);
    channel.chip_select(true);

    // Exercise
    channel.chip_select(true);

    // Verify
    expect(that % 1 == bus.configured.size());
    expect(not chip_select.high);
  };

  "a transfer without selecting selects around it, even if it throws"_test =
    []() {
      // Setup
      fake_bus bus;
      fake_pin chip_select;
      hal::spi_bus_manager manager(bus);
      auto channel = manager.acquire_channel(chip_select);
      std::array<hal::byte, 1> data{};
      bus.throw_on_transfer = true;

      // Exercise
      expect(throws<hal::io_error>([&]() { channel.transfer({ data }); }));

      // Verify
      expect(chip_select.high);
      expect(that % 1 == bus.transfer_count);
      // The bus is free again
      bus.throw_on_transfer = false;
      channel.chip_select(true);
      expect(not chip_select.high);
    };

  "destroying a selected channel deselects it"_test = []() {
    // Setup
    fake_bus bus;
    fake_pin flash_select;
    fake_pin imu_select;
    hal::spi_bus_manager manager(bus);
    auto imu = manager.acquire_channel(imu_select);

    // Exercise
    {
      auto flash = manager.acquire_channel(flash_select);
      flash.chip_select(true);
    }

    // Verify
    expect(flash_select.high);
    imu.chip_select(true);
    expect(not imu_select.high);
  };

  "settings the bus can't use are rejected up front"_test = []() {
    // Setup
    fake_bus bus;
    fake_pin chip_select;
    hal::spi_bus_manager manager(bus);
    auto channel = manager.acquire_channel(chip_select);

    // Exercise + Verify
    expect(throws<hal::operation_not_supported>(
      [&]() { channel.configure({ .bus_mode = hal::spi_mode::m3 }); }));
    expect(throws<hal::operation_not_supported>([&]() {
      static_cast<void>(manager.acquire_channel(
        chip_select, { .bus_mode = hal::spi_mode::m3 }));
    }));
    // The rejected settings weren't kept
    channel.chip_select(true);
    expect(that % 1 == bus.configured.size());
    expect(hal::spi_mode::m0 == bus.configured.front().bus_mode);
  };

  "clock_rate() reports what the bus would run at"_test = []() {
    // Setup
    fake_bus bus;
    fake_pin chip_select;
    hal::spi_bus_manager manager(bus);
    auto channel =
      manager.acquire_channel(chip_select, { .clock_rate = 20 * MHz });

    // Exercise + Verify
    expect((10 * MHz) == channel.clock_rate());
    expect(bus.configured.empty());
  };

  "with a lock, selecting holds the lock until deselecting"_test = []() {
    // Setup
    fake_bus bus;
    fake_pin chip_select;
    fake_lock lock;
    hal::spi_bus_manager manager(bus, lock);
    auto channel = manager.acquire_channel(chip_select);
    std::array<hal::byte, 1> data{};

    // Exercise
    channel.chip_select(true);
    bool const locked_while_selected = lock.locked;
    channel.transfer({ data });  // already selected: doesn't lock again
    channel.chip_select(false);

    // Verify
    expect(locked_while_selected);
    expect(not lock.locked);
    expect(that % 1 == lock.lock_count);
  };

  "with a lock, a failed select releases the lock"_test = []() {
    // Setup
    fake_bus bus;
    fake_pin chip_select;
    fake_lock lock;
    hal::spi_bus_manager manager(bus, lock);
    auto channel = manager.acquire_channel(chip_select);
    std::array<hal::byte, 1> data{};
    bus.throw_on_transfer = true;

    // Exercise
    expect(throws<hal::io_error>([&]() { channel.transfer({ data }); }));

    // Verify
    expect(not lock.locked);
    expect(chip_select.high);
  };
  "an owning manager builds its bus from the factory"_test = []() {
    // Setup
    std::vector<hal::spi_settings> log;
    fake_pin chip_select;
    hal::spi_bus_manager manager([&log] { return pinned_bus(log); });
    static_assert(
      std::is_same_v<decltype(manager), hal::spi_bus_manager<pinned_bus>>);
    auto channel =
      manager.acquire_channel(chip_select, { .clock_rate = 2 * MHz });

    // Exercise
    channel.chip_select(true);
    channel.chip_select(false);

    // Verify
    expect(that % 1 == log.size());
    expect((2 * MHz) == log.front().clock_rate);
    expect(chip_select.high);
  };

  "an owning manager with a lock holds it while selected"_test = []() {
    // Setup
    std::vector<hal::spi_settings> log;
    fake_pin chip_select;
    fake_lock lock;
    hal::spi_bus_manager manager([&log] { return pinned_bus(log); }, lock);
    auto channel = manager.acquire_channel(chip_select);

    // Exercise
    channel.chip_select(true);
    bool const locked_while_selected = lock.locked;
    channel.chip_select(false);

    // Verify
    expect(locked_while_selected);
    expect(not lock.locked);
  };

  "every manager hands out the same channel type"_test = []() {
    // Setup
    std::vector<hal::spi_settings> log;
    fake_bus bus;
    hal::spi_bus_manager referring(bus);
    hal::spi_bus_manager owning([&log] { return pinned_bus(log); });

    // Verify
    static_assert(std::is_same_v<decltype(referring), hal::spi_bus_manager<>>);
    static_assert(
      std::is_same_v<decltype(referring)::channel, decltype(owning)::channel>);
    expect(true);
  };
}
}  // namespace

int main()
{
  spi_configure_test();
  spi_clock_rate_test();
  spi_chip_select_test();
  spi_lock_unlock_test();
  spi_transfer_test();
  spi_bus_manager_test();
}
