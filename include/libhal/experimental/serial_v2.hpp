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
#include <cstdint>
#include <span>

#include "../units.hpp"

namespace hal {
/**
 * @brief Hardware abstract interface for the serial communication protocol
 *
 * Use this interface for hardware that implements a serial protocol like UART,
 * RS232, RS485 and others that use a similar communication protocol but may use
 * different voltage schemes.
 *
 * This interface only works 8-bit serial data frames.
 *
 * Due to the asynchronous and unformatted nature of serial communication
 * protocols, all implementations of serial devices must be buffered. Buffered,
 * in this case, is defined as automatic storage of received bytes without
 * direct application intervention.
 *
 * All implementations MUST allow the user to supply their own buffer of
 * arbitrary size up to the limits of what hardware can support. This allows a
 * developer the ability to tailored the buffer size to the needs of the
 * application.
 *
 * Examples of buffering schemes are:
 *
 * - Using DMA to copy data from a serial peripheral to a region of memory
 * - Using interrupts when a serial peripheral's queue has filled to a point
 *
 */
class serial_v2
{
public:
  /// Generic settings for a standard serial device.
  struct settings
  {
    /// Set of available stop bits options
    enum class stop_bits : uint8_t
    {
      one = 0,
      two,
    };

    /// Set of parity bit options
    enum class parity : uint8_t
    {
      /// Disable parity bit as part of the frame
      none = 0,
      /// Enable parity and set 1 (HIGH) when the number of bits is odd
      odd,
      /// Enable parity and set 1 (HIGH) when the number of bits is even
      even,
      /// Enable parity bit and always return 1 (HIGH) for ever frame
      forced1,
      /// Enable parity bit and always return 0 (LOW) for ever frame
      forced0,
    };

    /// The operating speed of the baud rate (in units of bits per second)
    hertz baud_rate = 115200.0f;

    /// Number of stop bits for each frame
    stop_bits stop = stop_bits::one;

    /// Parity bit type for each frame
    parity parity = parity::none;
  };

  /**
   * @brief Configure serial to match the settings supplied
   *
   * Implementing drivers must verify if the settings can be applied to hardware
   * before modifying the hardware. This will ensure that if this operation
   * fails, the state of the serial device has not changed.
   *
   * @param p_settings - settings to apply to serial driver
   * @throws hal::operation_not_supported - if the settings could not be
   * achieved.
   */
  void configure(settings const& p_settings)
  {
    driver_configure(p_settings);
  }

  /**
   * @brief Write data to the transmitter line of the serial port
   *
   * @param p_data - data to be transmitted over the serial port
   */
  void write(std::span<hal::byte const> p_data)
  {
    driver_write(p_data);
  }

  /**
   * @brief Returns this serial driver's receive buffer
   *
   * Use this along with the receive_head() in order to determine if new data
   * has been read into the receive buffer. See the docs for `receive_head()`
   * for more details.
   *
   * @return std::span<hal::byte const> - a const span to the receive buffer
   * used by the serial port.
   */
  [[nodiscard]] std::span<hal::byte const> receive_buffer()
  {
    return driver_receive_buffer();
  }

  /**
   * @brief Returns the write position (head) of the circular receive buffer
   *
   * Receive head represents the position where the next byte of data will be
   * written in the receive buffer. This position advances as new data arrives.
   * To determine how much new data has arrived, store the previous head
   * position and compare it with the current head position, accounting for
   * buffer wraparound.
   *
   * Example:
   *
   *   auto old_head = port.receive_head();
   *   // ... wait for new data ...
   *   auto new_head = port.receive_head();
   *   // Account for circular wraparound when calculating bytes received
   *   auto buffer_size = port.receive_buffer().size();
   *   auto bytes_received = (new_head + buffer_size - old_head) % buffer_size;
   *
   * Use this along with receive_buffer() to access newly received data. The
   * data between your last saved position and the current head position
   * represents the newly received bytes. When reading the data, remember that
   * it may wrap around from the end of the buffer back to the beginning.
   *
   * @return std::size_t - Current write position in the circular receive buffer
   */
  [[nodiscard]] std::size_t receive_head()
  {
    return driver_read_index();
  }

  /**
   * @brief Flush working buffer
   *
   * The behavior of flushing the internal working buffer is this:
   *
   * 1. Stop data reception.
   * 2. Clear any received data stored in hardware FIFOs.
   * 3. Write zeros to circular buffer.
   * 4. Enable data reception.
   */
  void flush()
  {
    driver_flush();
  }

  virtual ~serial_v2() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual void driver_write(std::span<hal::byte const> p_data) = 0;
  virtual std::span<hal::byte const> driver_receive_buffer() = 0;
  virtual std::size_t driver_read_index() = 0;
  virtual void driver_flush() = 0;
};
}  // namespace hal
