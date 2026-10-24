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

module;

#include <concepts>
#include <mutex>
#include <tuple>
#include <type_traits>

export module hal:spi;

export import scatter_span;

import :units;
import :error;
import :gpio;
import :lifetime;
import :lock;

namespace hal::inline v5 {
/**
 * @brief Mode settings which control when data is sampled and shifted out
 *
 */
export enum class spi_mode : u8 {
  /**
   * @brief spi mode 0
   *
   * - Data is shifted out on: falling SCLK, and when CS activates
   * - Data is sampled on: rising SCLK
   * - CPOL (clock polarity): 0
   * - CPHA (clock phase): 0
   */
  m0,

  /**
   * @brief spi mode 1
   *
   * - Data is shifted out on: rising SCLK
   * - Data is sampled on: falling SCLK
   * - CPOL (clock polarity): 0
   * - CPHA (clock phase): 1
   */
  m1,

  /**
   * @brief spi mode 2
   *
   * - Data is shifted out on: rising SCLK, and when CS activates
   * - Data is sampled on: falling SCLK
   * - CPOL (clock polarity): 1
   * - CPHA (clock phase): 0
   */
  m2,

  /**
   * @brief spi mode 3
   *
   * - Data is shifted out on: falling SCLK
   * - Data is sampled on: rising SCLK
   * - CPOL (clock polarity): 1
   * - CPHA (clock phase): 1
   */
  m3,
};

/**
 * @brief Generic settings for a standard SPI device or bus.
 *
 */
export struct spi_settings
{
  /**
   * @brief Best-effort clock rate to set the spi bus to
   *
   * This field denotes the clock rate that an application or driver would
   * like to set the bus to. This clock rate is "best-effort" meaning that the
   * spi bus or channel will try its best to reach this clock rate but will
   * not guarantee the exact clock rate.
   *
   * A request faster than the bus can go runs at the bus's fastest rate,
   * since running a device slower than asked is safe. A request slower than
   * the bus can go is rejected by `configure()`, since running a device faster
   * than asked is not. `clock_rate()` reports the exact rate either way, so a
   * driver can check before configuring.
   */
  hertz clock_rate = 100 * si::unit_symbols::kHz;

  /**
   * @brief Bus mode select field
   *
   * Use this to select how the spi data and clock are sampled.
   */
  spi_mode bus_mode = spi_mode::m0;

  /**
   * @brief Enables default comparison
   *
   */
  bool operator<=>(spi_settings const&) const = default;
};

/**
 * @brief Serial peripheral interface (SPI) bus hardware abstraction interface
 *
 * A bus shifts bytes out and in. It has no chip select and no notion of
 * devices. It is what a platform implements for its SPI peripheral, and what
 * a driver that uses SPI as a signal generator (for example, for WS2812 LEDs)
 * uses directly.
 *
 * A driver for a device that shares a bus with others takes a
 * `hal::spi_channel` instead. `hal::spi_bus_manager` makes channels from a
 * bus: it selects one device at a time and applies each channel's settings
 * when it selects it.
 *
 * Platforms implement this interface, not `hal::spi_channel`. The manager
 * gives device drivers channels, so the logic that keeps devices from
 * talking over each other is written once, in one place.
 *
 * This interface supports the most common SPI features:
 *
 * 1. Word length locked to 8-bits
 * 2. Byte transfer is always MSB first
 *
 * # Design Philosophy
 *
 * By restricting the SPI interface requirements, we ensure compatibility with
 * nearly all devices that communicate over SPI. This approach simplifies both
 * the requirements and the code for SPI drivers and implementations.
 *
 * ## 8-bit Transfers
 *
 * The 8-bit word length is the most common SPI word length, compatible with
 * almost any SPI peripheral driver, SPI converter driver, and most devices.
 * Devices that support 16-bit word transfers can split the 16-bit word into two
 * bytes. Devices using word formats that aren't multiples of 8 bits are very
 * rare.
 *
 * ## MSB First Transfers
 *
 * The most common bit order for SPI is MSB first. Devices that use LSB first
 * are rare. Drivers using this SPI interface must handle bit reversal to
 * comply with the MSB first requirement. This decision helps to eliminate
 * rare and potentially unsupportable configurations, optimizing the interface
 * for the majority of use cases.
 *
 * ## No Chip Select on the Bus
 *
 * Many SPI peripherals have a dedicated chip select pin that can be controlled
 * automatically. Automatic chip select control asserts the chip select for the
 * duration of the transfer and de-asserts it at the end, which can be
 * problematic for drivers needing to perform multiple transfers while keeping
 * the chip select asserted. For instance, if there are components of a payload
 * in read-only memory that are not contiguous, the driver or application would
 * have to copy the contents of those components to a buffer before a full
 * transfer can occur. This requirement for a memory buffer equal to the length
 * of the whole transaction is wasteful and can be prohibitively expensive in
 * some applications.
 *
 * SD cards, for example, require the chip select to be held with a sequence of
 * clock cycles until the SD card responds with actual data. The number of
 * cycles can vary, so manual chip select control allows more precise and
 * memory-efficient operations.
 *
 * So the bus leaves chip select to its user, and implementations should leave
 * a peripheral's hardware chip select unused. `hal::spi_bus_manager` drives a
 * `hal::output_pin` for each channel instead.
 */
export class raw_spi_bus
{
public:
  /// Default filler data placed on the bus in place of actual write data when
  /// the write buffer has been exhausted.
  static constexpr hal::byte default_filler = hal::byte{ 0xFF };

  /**
   * @brief Configure the bus
   *
   * @param p_settings - settings to apply. The bus runs at the clock rate
   * that `clock_rate(p_settings)` returns.
   * @throws hal::operation_not_supported - if the bus can't use the mode, or
   * can't run as slowly as the requested clock rate (`clock_rate(p_settings)`
   * is greater than `p_settings.clock_rate`).
   */
  void configure(spi_settings const& p_settings)
  {
    driver_configure(p_settings);
  }

  /**
   * @brief Report the exact clock rate the bus would run at with these
   * settings
   *
   * Doesn't change the bus, and doesn't reject anything: a request outside
   * the bus's range gets the nearest rate it can do. For example, a bus that
   * runs from 1 kHz to 50 MHz reports 50 MHz for a 100 MHz request and 1 kHz
   * for a 1 Hz request.
   *
   * A result greater than `p_settings.clock_rate` means the bus can't go that
   * slowly, and `configure(p_settings)` would throw. Otherwise it is the rate
   * `configure(p_settings)` gives.
   *
   * @param p_settings - settings to check. Only the clock rate matters.
   * @return hertz - the clock rate for `p_settings`
   */
  [[nodiscard]] hertz clock_rate(spi_settings const& p_settings)
  {
    return driver_clock_rate(p_settings);
  }

  /**
   * @brief Exchange bytes over the bus
   *
   * Blocks until every byte has been exchanged. Asserting a chip select, if
   * any, is up to the caller.
   *
   * @param p_data_out - bytes to send. Once they run out, `p_filler` is sent
   * until `p_data_in` is full.
   * @param p_data_in - buffer for the received bytes. Once it is full, the
   * remaining received bytes are dropped.
   * @param p_filler - byte sent after `p_data_out` runs out
   */
  void transfer(mem::scatter_span<byte const> p_data_out,
                mem::scatter_span<byte> p_data_in = {},
                byte p_filler = default_filler)
  {
    driver_transfer(p_data_out, p_data_in, p_filler);
  }

protected:
  ~raw_spi_bus() = default;

private:
  virtual void driver_configure(spi_settings const& p_settings) = 0;
  virtual hertz driver_clock_rate(spi_settings const& p_settings) = 0;
  virtual void driver_transfer(mem::scatter_span<byte const> p_data_out,
                               mem::scatter_span<byte> p_data_in,
                               byte p_filler) = 0;
};

/**
 * @brief Serial peripheral interface (SPI) communication protocol hardware
 * abstraction interface for individual channels
 *
 * A channel is one device on a bus. Device drivers take this interface. Words
 * are 8 bits and are sent MSB first, and the device's chip select is under
 * the driver's control; see `hal::raw_spi_bus` for why.
 *
 * # Implementing this interface
 *
 * Don't, if you can implement `hal::raw_spi_bus` instead.
 * `hal::spi_bus_manager` turns any `hal::raw_spi_bus` into channels that
 * meet every requirement below, so a platform with an SPI peripheral and
 * GPIOs implements only the bus.
 *
 * Implement `hal::spi_channel` directly only when you don't control chip
 * select or bus arbitration, for example:
 *
 * - An operating system SPI driver such as Linux spidev, where the kernel
 *   owns the chip select line and shares the bus between processes.
 * - A bridge chip whose transfer command carries the chip select, so it
 *   can't be driven as a separate pin.
 * - A test fake for a device driver's unit tests.
 *
 * ## Chip Select as Bus Access Control
 *
 * A single spi bus can communicate with different devices via a chip select
 * pin. The chip select pin acts like a binary decoder where only one output is
 * active at a time based on its inputs. It is important that two chip select
 * signals are not active at one time as this could cause bus contention where
 * multiple devices are attempting to communicate on the spi controller input
 * data line, known as CIPO (controller in, peripheral out) also known as MISO
 * (master in slave out).
 *
 * This interface mandates that spi channels utilizing the same physical spi bus
 * enforce bus mutual exclusion on that bus via the `chip_select()` API. The spi
 * channel can lock/acquire control over the spi bus with `chip_select(true)`
 * and releases control via `chip_select(false)`. See API documentation for more
 * details.
 *
 * ## Each channel remembers its configuration
 *
 * Each channel is mandated to remember its last configuration so that, when the
 * API `chip_select()` is called with value `true`, the spi bus is configured to
 * the last configuration passed to `configure()`. This allows each spi channels
 * to be configured to their use case. For example, a spi channel can be passed
 * to an SD card driver and operate at 12 MHz whereas another spi channel can be
 * set to 1 MHz to communicate with a sensor that can only handle speeds up
 * to 1 MHz maximum.
 *
 */
export class spi_channel
{
public:
  /// Default filler data placed on the bus in place of actual write data when
  /// the write buffer has been exhausted.
  static constexpr hal::byte default_filler = hal::byte{ 0xFF };

  /// SPI mode. The same type as `hal::spi_mode`.
  using mode = spi_mode;

  /// Channel settings. The same type as `hal::spi_settings`.
  using settings = spi_settings;

  /**
   * @brief Set the spi settings for this channel
   *
   * The settings passed to this API will be stored by the implementation of
   * this interface for use when `chip_select(true)` is called.
   *
   * On construction, the default settings are defined by the default values
   * within the `settings` structure.
   *
   * @param p_settings - settings to configure the spi bus to when control is
   * acquired by this channel.
   * @throws hal::operation_not_supported - if the bus can't run as slowly as
   * the requested clock rate, or can't use the mode. An implementation that
   * can only find out about the mode when it configures the bus throws from
   * `chip_select(true)` instead.
   */
  void configure(settings const& p_settings)
  {
    driver_configure(p_settings);
  }

  /**
   * @brief Return the clock rate that the SPI channel will operate at when a
   * transfer occurs.
   *
   * The the clock rate returned is the clock rate the spi channel will be
   * configured to when `chip_select(true)` and is not the current clock rate of
   * the bus.
   *
   * The clock rate reported here is the exact rate, and is less than or equal
   * to the value `clock_rate` provided to the configure API, since
   * `configure()` rejects rates the bus can't go down to. This function can be
   * called by the program to determine the delta between the requested
   * frequency passed to `configure()` and the actual clock frequency the bus
   * will be set to.
   *
   * @return hertz - the clock rate of this spi channel when
   * `chip_select(true)` is called.
   */
  [[nodiscard]] hertz clock_rate()
  {
    return driver_clock_rate();
  }

  /**
   * @brief Control both chip select & exclusive access to the spi bus
   *
   * This API controls access over a shared spi bus and controls the state of
   * the channel's chip select voltage signal. When this API is called with the
   * value `true`, this function returns when the spi channel gains exclusive
   * control over the spi bus. Implementation must do the following:
   *
   * 1. Wait until the bus is no longer currently in use. This happens when
   *    another `spi_channel` object that uses the same spi bus is currently
   *    using the bus.
   * 2. Acquire exclusive access over the bus in such a way that other
   *    `spi_channel` objects using the same underlying spi are blocked until
   *    the bus is available.
   * 3. Configure spi bus to the settings of this spi channel.
   * 3. Assert the chip select signal. libhal considers "chip select" as
   *    negative polarity meaning that a LOW voltage is required to assert them.
   *    If the chip select is driven by a `hal::output_pin` then call the API
   *    `hal::output_pin::level(false)` should be called to assert the chip
   *    select.
   * 4. Return
   *
   * Implementations are encouraged to use a `hal::basic_lock` for steps 1
   * and 2.
   *
   * If this API is called with the value `false` and exclusive access has been
   * granted over the bus, then following should occur:
   *
   * 1. De-assert chip select signal. If the chip select is driven by a
   *    `hal::output_pin` then call the API `hal::output_pin::level(true)`
   *    should be called to assert the chip select.
   * 2. Release access to the bus. If a `hal::basic_lock` is used, then call the
   *    `hal::basic_lock::unlock()` API.
   *
   * If exclusive access has not been granted over the bus and this is called
   * with the value `false`, then nothing should occur. Implementations can
   * utilize a boolean variable or the state of their chip select to determine
   * if the bus previously acquired.
   *
   * Notice that there is no step to configure the spi bus on de-assertion. This
   * is because configuration should only occur during bus acquisition. There is
   * no need to perform work twice.
   *
   * On construction, the chip select state should be as if this API was called
   * with `false`.
   *
   * @param p_select - if set to true will acquire exclusive access to the spi
   * bus and asserts chip select. If another channel already has exclusive
   * access over the spi bus, then this function waits until access is made
   * available. Implementations are encouraged to block through a
   * `hal::notifier` for this. When set to false, releases exclusive control
   * over the bus and de-asserts chip select.
   */
  void chip_select(bool p_select)
  {
    driver_chip_select(p_select);
  }

  /**
   * @brief Send and receive data between a selected device on the spi bus.
   *
   * This function will block until the entire transfer is finished. This API's
   * behavior changes depending on the last call to `chip_select()`:
   *
   * If `chip_select(true)` was called, then this function transfers the data on
   * the bus as defined by the parameter descriptions.
   *
   * If `chip_select(false)` was called prior to this API being called, then
   * this function will temporarily acquire access over the bus in the following
   * way:
   *
   * 1. Wait to gain exclusive control over the bus
   * 2. Assert chip select
   * 3. Perform data transfer over the bus
   * 4. De-assert chip select
   * 5. Release exclusive control over the bus
   * 6. Return
   *
   * This temporary access ensures that this API is always safe to call without
   * concern of bus contention.
   *
   * @param p_data_out - buffer to write data to the bus. If this is set to
   * null/empty then writing is ignored and the p_filler will be written to
   * the bus. If the length is less than p_data_in, then p_filler will be
   * written to the bus after this buffer has been sent.
   * @param p_data_in - buffer to read the data off of the bus. If this is
   * null/empty, then the transfer will be write only and the incoming data will
   * be ignored. If the length of this buffer is less than p_data_out, once this
   * buffer has been filled, the rest of the received bytes on the bus will be
   * dropped.
   * @param p_filler - filler data placed on the bus in place of actual write
   * data when p_data_out has been exhausted.
   */
  void transfer(mem::scatter_span<byte const> p_data_out,
                mem::scatter_span<byte> p_data_in = {},
                byte p_filler = default_filler)
  {
    driver_transfer(p_data_out, p_data_in, p_filler);
  }

  /**
   * @brief API to satisfy the `lock()` API of C++'s BasicLockable trait.
   *
   * Calls `chip_select(true)`
   *
   */
  void lock()
  {
    chip_select(true);
  }

  /**
   * @brief API to satisfy the `unlock()` API of C++'s BasicLockable trait.
   *
   * Calls `chip_select(false)`
   *
   */
  void unlock()
  {
    chip_select(false);
  }

protected:
  ~spi_channel() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual hertz driver_clock_rate() = 0;
  virtual void driver_chip_select(bool p_select) = 0;
  virtual void driver_transfer(mem::scatter_span<byte const> p_data_out,
                               mem::scatter_span<byte> p_data_in,
                               byte p_filler) = 0;
};
/**
 * @brief The part of `hal::spi_bus_manager` that doesn't depend on the bus
 * type
 *
 * Every manager, whatever bus type it holds, hands out this one channel type.
 */
class spi_bus_manager_base
{
public:
  class channel;

  spi_bus_manager_base(spi_bus_manager_base const&) = delete;
  spi_bus_manager_base& operator=(spi_bus_manager_base const&) = delete;
  spi_bus_manager_base(spi_bus_manager_base&&) = delete;
  spi_bus_manager_base& operator=(spi_bus_manager_base&&) = delete;

  /**
   * @brief Acquire a channel for one device on the bus
   *
   * Drives the chip select pin high (deselected).
   *
   * @param p_chip_select - output pin that selects the device. It is driven
   * low while the channel has the device selected and high otherwise.
   * @param p_settings - settings applied to the bus whenever this channel
   * selects its device
   * @return channel - the channel, which can't be copied or moved
   * @throws hal::operation_not_supported - if the bus can't run as slowly as
   * `p_settings.clock_rate`. A mode the bus can't use is reported when the
   * channel selects its device.
   */
  [[nodiscard]] channel acquire_channel(hal::ref<output_pin> p_chip_select
                                        [[clang::lifetimebound]],
                                        spi_settings const& p_settings = {})
    [[clang::lifetimebound]];

protected:
  spi_bus_manager_base(raw_spi_bus& p_bus, basic_lock& p_lock)
    : m_bus(p_bus)
    , m_lock(p_lock)
  {
  }

  ~spi_bus_manager_base() = default;

private:
  void select(channel& p_channel);
  void deselect(channel& p_channel);
  void reject_too_slow(spi_settings const& p_settings);

  hal::ref<raw_spi_bus> m_bus;
  hal::ref<basic_lock> m_lock;
  /// The channel whose device is selected, if any
  channel* m_selected = nullptr;
};

/**
 * @brief One device on a bus shared by `hal::spi_bus_manager`
 *
 * Only a manager's `acquire_channel()` creates one.
 */
class spi_bus_manager_base::channel final : public spi_channel
{
public:
  channel(channel const&) = delete;
  channel& operator=(channel const&) = delete;
  channel(channel&&) = delete;
  channel& operator=(channel&&) = delete;

  /// Deselect the device if this channel still has it selected
  ~channel()
  {
    m_manager->deselect(*this);
  }

private:
  friend class spi_bus_manager_base;

  channel(spi_bus_manager_base& p_manager,
          output_pin& p_chip_select,
          spi_settings const& p_settings)
    : m_manager(p_manager)
    , m_chip_select(p_chip_select)
    , m_settings(p_settings)
  {
    // Start deselected, as if chip_select(false) were called.
    m_chip_select->level(true);
  }

  void driver_configure(spi_settings const& p_settings) override
  {
    m_manager->reject_too_slow(p_settings);
    m_settings = p_settings;
  }

  hertz driver_clock_rate() override
  {
    return m_manager->m_bus->clock_rate(m_settings);
  }

  void driver_chip_select(bool p_select) override
  {
    if (p_select) {
      m_manager->select(*this);
    } else {
      m_manager->deselect(*this);
    }
  }

  void driver_transfer(mem::scatter_span<byte const> p_data_out,
                       mem::scatter_span<byte> p_data_in,
                       byte p_filler) override
  {
    if (m_manager->m_selected == this) {
      m_manager->m_bus->transfer(p_data_out, p_data_in, p_filler);
      return;
    }

    m_manager->select(*this);
    // Deselects even if the transfer throws.
    struct deselect_on_exit
    {
      channel& self;
      ~deselect_on_exit()
      {
        self.m_manager->deselect(self);
      }
    } const guard{ .self = *this };
    m_manager->m_bus->transfer(p_data_out, p_data_in, p_filler);
  }

  hal::ref<spi_bus_manager_base> m_manager;
  hal::ref<output_pin> m_chip_select;
  spi_settings m_settings;
};

inline spi_bus_manager_base::channel spi_bus_manager_base::acquire_channel(
  hal::ref<output_pin> p_chip_select,
  spi_settings const& p_settings)
{
  // Before touching the pin.
  reject_too_slow(p_settings);
  return { *this, *p_chip_select, p_settings };
}

inline void spi_bus_manager_base::reject_too_slow(
  spi_settings const& p_settings)
{
  // The bus would run the device faster than it asked for.
  if (m_bus->clock_rate(p_settings) > p_settings.clock_rate) {
    throw hal::operation_not_supported(this);
  }
}

inline void spi_bus_manager_base::select(channel& p_channel)
{
  if (m_selected == &p_channel) {
    return;
  }

  // Waits while another context has a device selected. Releases the lock if
  // anything below throws.
  std::unique_lock guard(*m_lock);

  if (m_selected != nullptr) {
    // A lock that blocks keeps other contexts out while a device is selected,
    // so this is one context selecting a second device. A lock that doesn't
    // block, such as hal::no_op_lock, lets it through. Selecting this one too
    // would put two devices on the bus.
    throw hal::device_or_resource_busy(this);
  }

  m_bus->configure(p_channel.m_settings);
  // Chip select is active low.
  p_channel.m_chip_select->level(false);
  m_selected = &p_channel;
  // The device is selected, so the lock stays held until deselect().
  std::ignore = guard.release();
}

inline void spi_bus_manager_base::deselect(channel& p_channel)
{
  if (m_selected != &p_channel) {
    return;
  }
  p_channel.m_chip_select->level(true);
  m_selected = nullptr;
  m_lock->unlock();
}
/// Holds the bus of a `hal::spi_bus_manager` that owns it. It is a separate
/// base, listed first, so the bus is fully constructed before
/// `spi_bus_manager_base` refers to it.
template<class Bus>
class spi_bus_storage
{
protected:
  template<class Factory>
  explicit spi_bus_storage(Factory& p_factory)
    : m_bus(p_factory())
  {
  }

  Bus m_bus;
};

/// A manager that refers to a bus owned elsewhere stores nothing extra.
template<>
class spi_bus_storage<raw_spi_bus>
{};

/**
 * @brief Makes `hal::spi_channel`s from a `hal::raw_spi_bus`, selecting one
 * device at a time
 *
 * Each channel drives one device's chip select pin and remembers that
 * device's settings. Selecting a channel configures the bus with its settings
 * and drives its chip select low. This is how a platform's `hal::raw_spi_bus`
 * becomes channels for device drivers, without each platform implementing
 * channels itself.
 *
 * Only one channel may have its device selected at a time. Every manager
 * takes a `hal::basic_lock`: selecting takes it and deselecting releases it.
 *
 * - With a lock that blocks, such as an RTOS mutex, channels can be used from
 *   different contexts (threads, fibers). A second context waits for the bus.
 *   One context selecting two channels at once deadlocks on the lock, so
 *   don't.
 * - If every channel is used from one context, pass a `hal::no_op_lock`. A
 *   channel that selects while another channel has its device selected then
 *   throws `hal::device_or_resource_busy`, instead of putting two devices on
 *   the bus. A `hal::no_op_lock` doesn't stop two contexts from using the bus
 *   at once.
 *
 * The manager either owns its bus or refers to one that lives elsewhere.
 * Owning it is safer: built from a factory, the bus exists only inside the
 * manager, so nothing else can use it behind the manager's back.
 *
 * ```cpp
 * // Every channel below is used from one thread.
 * static hal::no_op_lock single_context;
 *
 * // Owns the bus. The factory's return type is the bus type.
 * static hal::spi_bus_manager spi1(
 *   [] { return hal::stm32f1::spi(hal::stm32f1::peripheral::spi1); },
 *   single_context);
 *
 * // Refers to a bus that lives elsewhere. Nothing else may use it.
 * static hal::stm32f1::spi bus2(hal::stm32f1::peripheral::spi2);
 * static hal::spi_bus_manager spi2(bus2, single_context);
 *
 * static auto flash = spi1.acquire_channel(flash_chip_select);
 * static auto imu = spi1.acquire_channel(imu_chip_select, imu_settings);
 * ```
 *
 * Every manager hands out the same channel type,
 * `hal::spi_bus_manager<>::channel`, whatever bus type it holds.
 *
 * The lock, every chip select pin, and a bus the manager doesn't own must
 * outlive the manager, and the manager must outlive its channels.
 *
 * @tparam Bus - the bus type the manager owns, or `hal::raw_spi_bus` (the
 * default) to refer to a bus that lives elsewhere
 */
export template<class Bus = raw_spi_bus>
class spi_bus_manager final
  : private spi_bus_storage<Bus>
  , public spi_bus_manager_base
{
public:
  static_assert(std::derived_from<Bus, raw_spi_bus>,
                "The bus type must implement hal::raw_spi_bus");

  /**
   * @brief Manage a bus that lives elsewhere
   *
   * @param p_bus - the bus to share between devices
   * @param p_lock - lock held from selecting a device until deselecting it.
   * Use `hal::no_op_lock` if every channel is used from one context.
   */
  spi_bus_manager(hal::ref<raw_spi_bus> p_bus [[clang::lifetimebound]],
                  hal::ref<basic_lock> p_lock [[clang::lifetimebound]])
    requires std::same_as<Bus, raw_spi_bus>
    : spi_bus_manager_base(*p_bus, *p_lock)
  {
  }

  /**
   * @brief Build the bus inside the manager
   *
   * @param p_factory - called once to build the bus in place. The bus can be
   * non-movable.
   * @param p_lock - lock held from selecting a device until deselecting it.
   * Use `hal::no_op_lock` if every channel is used from one context.
   */
  template<class Factory>
    requires(not std::same_as<Bus, raw_spi_bus> &&
             std::same_as<std::invoke_result_t<Factory&>, Bus>)
  spi_bus_manager(Factory p_factory [[clang::lifetimebound]],
                  hal::ref<basic_lock> p_lock [[clang::lifetimebound]])
    : spi_bus_storage<Bus>(p_factory)
    , spi_bus_manager_base(spi_bus_storage<Bus>::m_bus, *p_lock)
  {
  }

  spi_bus_manager(spi_bus_manager const&) = delete;
  spi_bus_manager& operator=(spi_bus_manager const&) = delete;
  spi_bus_manager(spi_bus_manager&&) = delete;
  spi_bus_manager& operator=(spi_bus_manager&&) = delete;
  ~spi_bus_manager() = default;
};

/// A bus object and a lock: the manager refers to the bus.
export spi_bus_manager(raw_spi_bus&, basic_lock&)
  -> spi_bus_manager<raw_spi_bus>;

/// A factory and a lock: the manager owns the bus the factory returns.
export template<class Factory>
  requires std::invocable<Factory&>
spi_bus_manager(Factory, basic_lock&)
  -> spi_bus_manager<std::invoke_result_t<Factory&>>;
}  // namespace hal::inline v5
