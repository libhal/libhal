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
#include <span>

#include "units.hpp"

namespace hal {
/**
 * @brief Serial peripheral interface (SPI) communication protocol hardware
 * abstract interface
 *
 * This interface only supports the most common spi features:
 *
 *     1. Word length locked to 8-bits
 *     2. Byte transfer is always MSB first
 *     3. Chip select is not controlled by this driver
 *
 * # Why so strict?
 *
 * These strict requirements conform to what the vast majority of devices in
 * the wild will use. This simplifies the requirements for developing drivers
 * that use spi.
 *
 * ## Why 8-bit transfers
 *
 * 8-bit transfer word lengths are the most common spi word length. This length
 * works for almost any spi peripheral driver, spi converter driver, and most
 * devices that communicate work over spi. For devices that support 16-bit word
 * transfer, these can be supported by splitting up the 16-bit word into two
 * bytes. It is very rare to find devices that use any word format that isn't
 * exactly a multiple of 8.
 *
 * ## Why MSB first?
 *
 * By far the most common bit order for spi is msb first. Finding devices that
 * violate this are hard to find. Drivers using the spi interface to implement
 * their communication, will be responsible for reversing the bits of a word in
 * order to comply with this interface. libhal has decided that it is better to
 * eliminate a rare and potentially unsupportable configuration than to optimize
 * rare drivers that use lsb first word transfer.
 *
 * ## Why no automatic chip select?
 *
 * Many spi peripherals and hardware implementations have a dedicated chip
 * select pin. In general, that chip select pin can be converted into an output
 * pin and driven directly by the driver code. Manual control over a chip
 * select pin is common and typically required over automatic control. Automatic
 * chip select asserts the chip select for the duration of the transfer and
 * de-asserts the chip select at the end of the transfer. But this becomes
 * problematic if a driver needs to keep the chip select asserted and also
 * perform multiple transfers. Let assume the parts of the payload are in ROM,
 * then they must be copied to a buffer before a full transfer can be performed.
 * This requires more stack space than simply calling the transfer with the ROM
 * data stream multiple times. Some devices like SD cards require the chip
 * select to be held and a repeated sequence of clock cycles until it responds
 * with actual data. The number of cycles can vary depending on the device.
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
   * Transfer API will transmit each byte
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
  void transfer(std::span<hal::byte const> p_data_out,
                std::span<hal::byte> p_data_in,
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
