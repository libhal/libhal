// Copyright 2024 - 2025 Khalil Estell and the libhal contributors
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

#pragma once

#include <cstddef>
#include <span>

#include "units.hpp"

namespace hal {
/**
 * @brief Serial peripheral interface (SPI) communication protocol hardware
 * abstraction interface for individual channels
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
 * ## Manual Chip Select as Bus Access Control
 *
 * Many SPI peripherals have a dedicated chip select pin that can be controlled
 * automatically. Automatic chip select control asserts the chip
 * select for the duration of the transfer and de-asserts it at the end, which
 * can be problematic for drivers needing to perform multiple transfers while
 * keeping the chip select asserted. For instance, if there are components of a
 * payload in read-only memory that are not contiguous, the driver or
 * application would have to copy the contents of those components to a buffer
 * before a full transfer can occur. This requirement for a memory buffer equal
 * to the length of the whole transaction is wasteful and can be prohibitively
 * expensive in some applications.
 *
 * For example, like SD cards, require the chip select to be held with a
 * sequence of clock cycles until the SD card responds with actual data. The
 * number of cycles can vary, so manual chip select control allows more precise
 * and  memory-efficient operations.
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
 * to an SD card driver and operate at 12MHz whereas another spi channel can be
 * set to 1.2MHz to communicate with a sensor that can only handle speeds up
 * to 1.2MHz maximum.
 *
 */
class spi_channel
{
public:
  /// Default filler data placed on the bus in place of actual write data when
  /// the write buffer has been exhausted.
  static constexpr hal::byte default_filler = hal::byte{ 0xFF };

  /**
   * @brief Mode settings which control when data is sampled and shifted out
   *
   */
  enum class mode : u8
  {
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
   * @brief Generic settings for a standard SPI device.
   *
   */
  struct settings
  {
    /**
     * @brief Best-effort clock rate to set the spi bus to
     *
     * This field denotes the clock rate that an application or driver would
     * like to set the bus to. This clock rate is "best-effort" meaning that the
     * spi channel will try its best to reach this clock rate but will not
     * guarantee the exact clock rate.
     *
     * The clock rate returned from the API `clock_rate()` will always return a
     * value less than or equal to this value. If the clock rate is above what
     * the spi bus can achieve, then the clock rate will be set to the maximum
     * the spi bus can manage.
     */
    u32 clock_rate = 100_kHz;

    /**
     * @brief Bus mode select field
     *
     * Use this to select how the spi data and clock are sampled.
     */
    mode bus_mode = mode::m0;

    /**
     * @brief Enables default comparison
     *
     */
    bool operator<=>(settings const&) const = default;
  };

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
   * @throws hal::operation_not_supported - if the mode cannot be accommodated
   * by the spi bus hardware or implementation of spi.
   */
  void configure(settings const& p_settings)
  {
    return driver_configure(p_settings);
  }

  /**
   * @brief Return the clock rate that the SPI channel will operate at when a
   * transfer occurs.
   *
   * The the clock rate returned is the clock rate the spi channel will be
   * configured to when `chip_select(true)` and is not the current clock rate of
   * the bus.
   *
   * The clock rate reported here is guaranteed to be less than or equal to the
   * value `clock_rate` provided to the configure API. This function can be
   * called by the program to determine the delta between the requested
   * frequency passed to `configure()` and the actual clock frequency the bus
   * will be set to.
   *
   * @return u32 - the approximate clock rate of this spi channel when
   * `chip_select(true)` is called.
   */
  u32 clock_rate()
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
   * available. Implementations are encouraged to utilize `hal::io_waiter` for
   * this. When set to false, releases exclusive control over the bus and
   * de-asserts chip select.
   */
  void chip_select(bool p_select)
  {
    return driver_chip_select(p_select);
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
  void transfer(std::span<byte const> p_data_out,
                std::span<byte> p_data_in = {},
                byte p_filler = default_filler)
  {
    return driver_transfer(p_data_out, p_data_in, p_filler);
  }

  /**
   * @brief API to satisfy the `lock()` API of C++'s BasicLockable trait.
   *
   * Calls `chip_select(true)`
   */
  void lock()
  {
    chip_select(true);
  }

  /**
   * @brief API to satisfy the `unlock()` API of C++'s BasicLockable trait.
   *
   * Calls `chip_select(false)`
   */
  void unlock()
  {
    chip_select(false);
  }

  virtual ~spi_channel() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual u32 driver_clock_rate() = 0;
  virtual void driver_chip_select(bool p_select) = 0;
  virtual void driver_transfer(std::span<byte const> p_data_out,
                               std::span<byte> p_data_in,
                               byte p_filler) = 0;
};

/**
 * @brief Serial peripheral interface (SPI) communication protocol hardware
 * abstraction interface
 * @deprecated This class is deprecated and will be removed in libhal
 * version 5.0.0 in place of `hal::spi_channel`. `hal::spi_channel` associates a
 * chip select with a spi channel as if it were a lock which provides access
 * access control to the SPI bus.
 *
 * This interface supports the most common SPI features:
 *
 * 1. Word length locked to 8-bits
 * 2. Byte transfer is always MSB first
 * 3. Chip select is not controlled by this driver
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
 * ## Manual Chip Select Control
 *
 * Many SPI peripherals have a dedicated chip select pin that can be controlled
 * manually. Automatic chip select control asserts the chip select for the
 * duration of the transfer and de-asserts it at the end, which can be
 * problematic for drivers needing to perform multiple transfers while keeping
 * the chip select asserted. For instance, if payload parts are in ROM, copying
 * them to a buffer for a full transfer requires more stack space than calling
 * the transfer multiple times with the ROM data stream. Some devices, like SD
 * cards, require the chip select to be held with a sequence of clock cycles
 * until they respond with actual data. This sequence can vary, so manual chip
 * select control allows more precise and memory-efficient operations.
 *
 */
class spi
{
public:
  /**
   * @brief Generic settings for a standard SPI device.
   *
   */
  struct settings
  {
    /**
     * @brief Serial clock frequency in hertz
     *
     */
    hertz clock_rate = 100.0_kHz;
    /**
     * @brief The polarity of the pins when the signal is idle
     *
     * CPOL == 0 == false
     * CPOL == 1 == true
     */
    union
    {
      bool clock_idles_high = false;
      bool clock_polarity;
      bool cpol;
    };
    /**
     * @brief The phase of the clock signal when communicating
     *
     * CPHA == 0 == false
     * CPHA == 1 == true
     */
    union
    {
      bool data_valid_on_trailing_edge = false;
      bool clock_phase;
      bool cpha;
    };
  };

  /// Default filler data placed on the bus in place of actual write data when
  /// the write buffer has been exhausted.
  static constexpr hal::byte default_filler = hal::byte{ 0xFF };

  /**
   * @brief Configure spi to match the settings supplied
   *
   * @param p_settings - settings to apply to spi
   * @throws hal::operation_not_supported - if the settings could not be
   * achieved.
   */
  void configure(settings const& p_settings)
  {
    return driver_configure(p_settings);
  }

  /**
   * @brief Send and receive data between a selected device on the spi bus.
   * This function will block until the entire transfer is finished.
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
  void transfer(std::span<byte const> p_data_out,
                std::span<byte> p_data_in,
                hal::byte p_filler = default_filler)
  {
    return driver_transfer(p_data_out, p_data_in, p_filler);
  }

  virtual ~spi() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual void driver_transfer(std::span<hal::byte const> p_data_out,
                               std::span<hal::byte> p_data_in,
                               hal::byte p_filler) = 0;
};

}  // namespace hal

namespace hal::v5 {
/**
 * @brief Serial peripheral interface (SPI) communication protocol hardware
 * abstraction interface for individual channels
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
 * ## Manual Chip Select as Bus Access Control
 *
 * Many SPI peripherals have a dedicated chip select pin that can be controlled
 * automatically. Automatic chip select control asserts the chip
 * select for the duration of the transfer and de-asserts it at the end, which
 * can be problematic for drivers needing to perform multiple transfers while
 * keeping the chip select asserted. For instance, if there are components of a
 * payload in read-only memory that are not contiguous, the driver or
 * application would have to copy the contents of those components to a buffer
 * before a full transfer can occur. This requirement for a memory buffer equal
 * to the length of the whole transaction is wasteful and can be prohibitively
 * expensive in some applications.
 *
 * For example, like SD cards, require the chip select to be held with a
 * sequence of clock cycles until the SD card responds with actual data. The
 * number of cycles can vary, so manual chip select control allows more precise
 * and  memory-efficient operations.
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
 * to an SD card driver and operate at 12MHz whereas another spi channel can be
 * set to 1.2MHz to communicate with a sensor that can only handle speeds up
 * to 1.2MHz maximum.
 *
 */
class spi_channel
{
public:
  /// Default filler data placed on the bus in place of actual write data when
  /// the write buffer has been exhausted.
  static constexpr hal::byte default_filler = hal::byte{ 0xFF };

  /**
   * @brief Mode settings which control when data is sampled and shifted out
   *
   */
  enum class mode : u8
  {
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
   * @brief Generic settings for a standard SPI device.
   *
   */
  struct settings
  {
    /**
     * @brief Best-effort clock rate to set the spi bus to
     *
     * This field denotes the clock rate that an application or driver would
     * like to set the bus to. This clock rate is "best-effort" meaning that the
     * spi channel will try its best to reach this clock rate but will not
     * guarantee the exact clock rate.
     *
     * The clock rate returned from the API `clock_rate()` will always return a
     * value less than or equal to this value. If the clock rate is above what
     * the spi bus can achieve, then the clock rate will be set to the maximum
     * the spi bus can manage.
     */
    hertz clock_rate = 100_kHz;

    /**
     * @brief Bus mode select field
     *
     * Use this to select how the spi data and clock are sampled.
     */
    mode bus_mode = mode::m0;

    /**
     * @brief Enables default comparison
     *
     */
    bool operator<=>(settings const&) const = default;
  };

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
   * @throws hal::operation_not_supported - if the mode cannot be accommodated
   * by the spi bus hardware or implementation of spi.
   */
  void configure(settings const& p_settings)
  {
    return driver_configure(p_settings);
  }

  /**
   * @brief Return the clock rate that the SPI channel will operate at when a
   * transfer occurs.
   *
   * The the clock rate returned is the clock rate the spi channel will be
   * configured to when `chip_select(true)` and is not the current clock rate of
   * the bus.
   *
   * The clock rate reported here is guaranteed to be less than or equal to the
   * value `clock_rate` provided to the configure API. This function can be
   * called by the program to determine the delta between the requested
   * frequency passed to `configure()` and the actual clock frequency the bus
   * will be set to.
   *
   * @return u32 - the approximate clock rate of this spi channel when
   * `chip_select(true)` is called.
   */
  u32 clock_rate()
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
   * available. Implementations are encouraged to utilize `hal::io_waiter` for
   * this. When set to false, releases exclusive control over the bus and
   * de-asserts chip select.
   */
  void chip_select(bool p_select)
  {
    return driver_chip_select(p_select);
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
  void transfer(std::span<byte const> p_data_out,
                std::span<byte> p_data_in = {},
                byte p_filler = default_filler)
  {
    return driver_transfer(p_data_out, p_data_in, p_filler);
  }

  /**
   * @brief API to satisfy the `lock()` API of C++'s BasicLockable trait.
   *
   * Calls `chip_select(true)`
   */
  void lock()
  {
    chip_select(true);
  }

  /**
   * @brief API to satisfy the `unlock()` API of C++'s BasicLockable trait.
   *
   * Calls `chip_select(false)`
   */
  void unlock()
  {
    chip_select(false);
  }

  virtual ~spi_channel() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual u32 driver_clock_rate() = 0;
  virtual void driver_chip_select(bool p_select) = 0;
  virtual void driver_transfer(std::span<byte const> p_data_out,
                               std::span<byte> p_data_in,
                               byte p_filler) = 0;
};
}  // namespace hal::v5
