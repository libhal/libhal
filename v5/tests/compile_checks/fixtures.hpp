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

// Types shared by the compile checks (see libhal_add_compile_checks() in
// libhal-cmake-util).

#pragma once

#include <memory_resource>

import hal;

// ---- hal:lifetime ----------------------------------------------------------

struct i2c : hal::no_bare_new_mixin
{
  virtual int read() = 0;

protected:
  ~i2c() = default;
};

struct fake_i2c final : i2c
{
  explicit fake_i2c(int p_port)
    : port(p_port)
  {
  }
  fake_i2c(fake_i2c const&) = delete;
  fake_i2c& operator=(fake_i2c const&) = delete;
  int read() override
  {
    return port;
  }
  int port;
};

class sensor
{
public:
  explicit sensor(hal::ref<i2c> p_bus [[clang::lifetimebound]])
    : m_bus(p_bus)
  {
  }
  [[nodiscard]] int read() const
  {
    return m_bus->read();
  }

private:
  hal::ref<i2c> m_bus;
};

inline fake_i2c g_bus{ 7 };

inline hal::owner<fake_i2c> acquire_i2c1()
{
  static constinit hal::reusable<fake_i2c> storage;
  return emplace(storage, 1);
}

inline hal::owner<fake_i2c> make_bus()
{
  return hal::make_owner<fake_i2c>(std::pmr::new_delete_resource(), 1);
}

class i2c_manager
{
public:
  [[nodiscard]] hal::owner<i2c> acquire(
    std::pmr::polymorphic_allocator<> p_allocator) [[clang::lifetimebound]]
  {
    return hal::make_owner<fake_i2c>(p_allocator, 1);
  }
};

// A resource that can't be copied or moved, handed out by value.
class pinned_i2c final : public i2c
{
public:
  pinned_i2c(pinned_i2c const&) = delete;
  pinned_i2c& operator=(pinned_i2c const&) = delete;
  pinned_i2c(pinned_i2c&&) = delete;
  pinned_i2c& operator=(pinned_i2c&&) = delete;
  int read() override
  {
    return port;
  }
  int port;

private:
  friend class pin_manager;
  explicit pinned_i2c(int p_port)
    : port(p_port)
  {
  }
};

class pin_manager
{
public:
  [[nodiscard]] pinned_i2c acquire() [[clang::lifetimebound]]
  {
    return pinned_i2c(1);
  }
};

struct imu
{
  virtual int acceleration() = 0;

protected:
  ~imu() = default;
};

// A device driver that borrows its bus.
class fake_imu final : public imu
{
public:
  explicit fake_imu(hal::ref<i2c> p_bus [[clang::lifetimebound]])
    : m_bus(p_bus)
  {
  }
  fake_imu(fake_imu const&) = delete;
  fake_imu& operator=(fake_imu const&) = delete;
  int acceleration() override
  {
    return m_bus->read();
  }

private:
  hal::ref<i2c> m_bus;
};

// ---- hal:subscription ------------------------------------------------------

struct fake_pin final : hal::edge_triggered_interrupt
{
  hal::edge_triggered_callback* callback = nullptr;

private:
  void driver_configure(settings const&) override
  {
  }
  void driver_on_trigger(hal::edge_triggered_callback& p_callback) override
  {
    if (callback) {
      throw hal::subscription_busy(this);
    }
    callback = &p_callback;
  }
  void driver_clear_trigger() noexcept override
  {
    callback = nullptr;
  }
};

class button_counter : public hal::edge_triggered_callback
{
public:
  explicit button_counter(hal::ref<hal::edge_triggered_interrupt> p_pin
                          [[clang::lifetimebound]])
    : m_subscription(p_pin->on_trigger(*this))
  {
  }
  [[nodiscard]] hal::u32 count() const
  {
    return m_count;
  }

private:
  void callback(bool) noexcept override
  {
    ++m_count;
  }
  hal::u32 m_count = 0;
  hal::subscription m_subscription;  // last
};

// Subscribes as the last statement of its constructor body.
class late_button_counter : public hal::edge_triggered_callback
{
public:
  explicit late_button_counter(hal::ref<hal::edge_triggered_interrupt> p_pin
                               [[clang::lifetimebound]])
  {
    m_subscription = p_pin->on_trigger(*this);
  }

private:
  void callback(bool) noexcept override
  {
  }
  hal::subscription m_subscription;  // last
};

struct counter final : hal::edge_triggered_callback
{
  void callback(bool) noexcept override
  {
  }
};

// ---- hal:spi ---------------------------------------------------------------

struct fake_spi_bus final : hal::spi_bus
{
private:
  void driver_configure(hal::spi_settings const&) override
  {
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
};

struct fake_output_pin final : hal::output_pin
{
private:
  void driver_configure(hal::pin_settings const&) override
  {
  }
  bool driver_level() override
  {
    return true;
  }
  void driver_level(bool) override
  {
  }
};
