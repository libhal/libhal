// Copyright 2024 Khalil Estell
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
#include <mutex>
#include <span>

#include "lock.hpp"
#include "output_pin.hpp"
#include "units.hpp"

namespace hal {
/**
 * @brief Serial peripheral interface (SPI) communication protocol hardware
 * abstraction interface
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
 * Note that spi_channel implementations are NOT sharable across multiple device
 * drivers. For every device driver that requires an spi_channel should be
 * provided with a unique spi_channel, where the chip select for that port is
 * connected to the device's chip select. The SPI manager object for your
 * platform will provide a means to generate multiple spi ports with their own
 * associated chip select pin, typically provided by passing in an output pin to
 * an
 * `::acquire_port(hal::output_pin&)` function.
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
 * rare and potentially unsupportable configures, optimizing the interface
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
class spi_channel
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
     * SPI peripheral will perform a best effort attempt to match the clock rate
     * passed. The actual bus clock rate will be less than or equal to the
     * desired clock rate. Implementations must ensure that the bus clock rate
     * is as close to the clock rate as achievable and does not exceed the clock
     * rate. We must assume that the clock rate provided is a maximum clock rate
     * for a device and exceeding it would cause undefined behavior. If the
     * clock rate exceeds the maximum clock rate for the SPI bus, then the SPI
     * bus sets the clock rate to its maximum achievable clock rate.
     */
    u32 clock_rate = 100_kHz;

    /**
     * @brief Set of available SPI Modes
     *
     * The SPI Mode numbers represent the conditions in which data is shifted
     * out and when data is sampled. These are denoted with CPOL and CPHA. CPOL
     * stands for clock polarity and CPHA stands for clock phase. Together these
     * set the SPI port to the
     */
    enum class mode : u8
    {
      /**
       * @brief Value of SPI mode 0
       *
       * - CPOL = 0
       * - CPHA = 0
       * - Data is shifted out on: falling SCLK and when CS activates
       * - Data is sampled on: rising SCLK
       */
      m0,
      /**
       * @brief Value of SPI mode 1
       *
       * - CPOL = 0
       * - CPHA = 1
       * - Data is shifted out on: rising SCLK
       * - Data is sampled on: falling SCLK
       *
       */
      m1,
      /**
       * @brief Mode 2
       *
       * - CPOL = 1
       * - CPHA = 0
       * - Data is shifted out on: rising SCLK, and when CS activates
       * - Data is sampled on: falling SCLK
       *
       */
      m2,
      /**
       * @brief Mode 3
       *
       * - CPOL = 1
       * - CPHA = 1
       * - Data is shifted out on: falling SCLK
       * - Data is sampled on: rising SCLK
       */
      m3,
    };

    /**
     * @brief Defines the selected SPI mode
     *
     * Default to m0 or SPI Mode 0.
     */
    mode select = mode::m0;

    /**
     * @brief Enables default comparison
     *
     */
    bool operator<=>(settings const&) const = default;
  };

  /**
   * @brief Default filler data
   *
   * Data placed on the bus in place of actual write data when the write buffer
   * has been exhausted.
   */
  static constexpr byte default_filler = 0xFF;

  /**
   * @brief Default settings for an SPI transaction
   *
   */
  static constexpr auto default_settings = settings{
    .clock_rate = 100_kHz,
    .select = settings::mode::m0,
  };

  /**
   * @brief Control the chip select state for this SPI channel
   *
   * When set to `true`, asserts the correct voltage level on the chip select
   * pin to cause the connected device to be "selected"
   *
   * In general, chip select is active low, meaning a true value would be LOW
   * voltage. Using an output pin internally for this would simply require an
   * inversion of the p_select value before passing it to the output pin.
   *
   * Setting this to "true" locks the spi bus to this device until the API is
   * called with "false". This interface supports the C++ named requirements
   * "BasicLockable" and thus it is advised to use std::lock_guard with this
   * class as the lock to activate the chip select and deactivate it
   * automatically, including in the event an exception is thrown.
   *
   * @param p_select - the state to set the chip select to.
   */
  void chip_select(bool p_select)
  {
    return driver_chip_select(p_select);
  }

  /**
   * @brief Send and receive data between a selected device on the spi bus.
   *
   * This function will block until the entire transfer is finished.
   *
   * If the
   *
   * This function will not begin the SPI transfer unless the underlying SPI bus
   * is available and no chip select  will block and will only begin an SPI
   * transfer-If an SPI transfer is currently on going or a chip select has been
   * asserted that shares the underlying spi bus for an spi driver The function
   * will block.
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
   * @param p_settings - the settings for the spi transaction
   * @param p_filler - filler data placed on the bus in place of actual write
   * data when p_data_out has been exhausted.
   */
  void transfer(std::span<hal::byte const> p_data_out,
                std::span<hal::byte> p_data_in,
                hal::byte p_filler = default_filler)
  {
    return driver_transfer(p_data_out, p_data_in, p_filler);
  }

  /**
   * @brief Set the SPI bus configure for the next bus transfer
   *
   * This API does not immediately apply the configure to the bus, but
   * caches the configure until this channel has acquired the bus, either by
   * calling `chip_select(true)` or a call to `transfer()`.
   *
   * @param p_settings - SPI bus settings to be used the next time this channel
   * acquires the bus.
   */
  void configure(settings const& p_settings)
  {
    driver_configure(p_settings);
  }

  /**
   * @brief Returns what the actual bus clock rate will be on bus acquisition.
   *
   * Some applications or drivers need the clock rates to be exact. In such
   * cases, the clock rate can be set using `configure()` then verified by using
   * this API.
   *
   * @return u32 - actual clock rate of the SPI bus
   */
  u32 clock_rate()
  {
    return driver_clock_rate();
  }

  /**
   * @brief Acquires a lock for the bus by asserting (calling) the chip select
   * with true.
   *
   * Blocks until the lock is acquired.
   *
   * This API implements part of the C++ named requirements "BasicLockable" for
   * std::lock_guard to work.
   */
  void lock()
  {
    chip_select(true);
  }

  /**
   * @brief Releases the lock on the bus by de-asserting (calling) the chip
   * select with false.
   *
   * This API implements part of the C++ named requirements "BasicLockable" for
   * std::lock_guard to work.
   */
  void unlock()
  {
    chip_select(false);
  }

  virtual ~spi_channel() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual u32 driver_clock_rate() = 0;
  virtual void driver_transfer(std::span<hal::byte const> p_data_out,
                               std::span<hal::byte> p_data_in,
                               settings p_settings,
                               hal::byte p_filler) = 0;
  virtual void driver_chip_select(bool p_select) = 0;
};

inline void spi_channel_example_usage()
{
  struct pseudo_spi_manager
  {
    pseudo_spi_manager(hal::basic_lock& p_bus_lock);
    hal::spi_channel& acquire_port(hal::output_pin& p_cs);
  };
  hal::basic_lock* spi_bus_lock = nullptr;  // just for exposition
  pseudo_spi_manager spi(*spi_bus_lock);
  hal::output_pin* cs1 = nullptr;  // just for exposition
  hal::output_pin* cs2 = nullptr;  // just for exposition
  hal::spi_channel& port1 = spi.acquire_port(*cs1);
  hal::spi_channel& port2 = spi.acquire_port(*cs2);

  struct sd_card
  {
    sd_card(hal::spi_channel& p_port);
  };

  struct ssd1303_display
  {
    ssd1303_display(hal::spi_channel& p_port);
  };

  sd_card sd(port1);
  ssd1303_display display(port2);

  // Usage:
  {
    std::lock_guard lock_spi_now(port1);
    u8 payload[3] = { 0x11, 0x22, 0x33 };
    u8 response[3];
    port1.transfer(payload, response);
  }
}
}  // namespace hal
