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
    u32 clock_rate = 100_kHz;

    /**
     * @brief The polarity of the pins when the signal is idle
     *
     * CPOL == 0 == false
     * CPOL == 1 == true
     */
    bool cpol = false;

    constexpr auto& clock_polarity(bool p_value)
    {
      cpol = p_value;
      return *this;
    }

    constexpr auto& clock_idles_high(bool p_value)
    {
      cpol = p_value;
      return *this;
    }

    /**
     * @brief The phase of the clock signal when communicating
     *
     * CPHA == 0 == false
     * CPHA == 1 == true
     */
    bool cpha = false;

    constexpr auto& clock_phase(bool p_value)
    {
      cpha = p_value;
      return *this;
    }

    constexpr auto& data_valid_on_trailing_edge(bool p_value)
    {
      cpha = p_value;
      return *this;
    }

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
    .cpol = false,
    .cpha = false,
  };

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
   * @param p_settings - the settings for the spi transaction
   * @param p_filler - filler data placed on the bus in place of actual write
   * data when p_data_out has been exhausted.
   */
  void transfer(std::span<hal::byte const> p_data_out,
                std::span<hal::byte> p_data_in,
                settings p_settings = default_settings,
                hal::byte p_filler = default_filler)
  {
    return driver_transfer(p_data_out, p_data_in, p_settings, p_filler);
  }

  virtual ~spi() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual void driver_transfer(std::span<hal::byte const> p_data_out,
                               std::span<hal::byte> p_data_in,
                               settings p_settings,
                               hal::byte p_filler) = 0;
};
}  // namespace hal
