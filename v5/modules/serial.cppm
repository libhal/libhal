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

#include <span>

export module hal:serial;

export import async_context;
export import strong_ptr;

export import :units;
export import :scatter_span;
export import :containers;

using namespace mp_units;
using namespace mp_units::si::unit_symbols;

namespace hal::inline v5 {
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
 * - Using interrupts when a serial peripheral's queue has filled to a point.
 *   Refrain from using interrupts if the peripheral's byte queue is only of
 *   size 1. This is bad for runtime performance and can result in missed bytes.
 *
 */
export class serial
{
public:
  /// Generic settings for a standard serial device.
  struct settings
  {
    /// Set of available stop bits options
    enum class stop_bits : u8
    {
      one = 0,
      two,
    };

    /// Set of parity bit options
    enum class parity : u8
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
    hertz baud_rate = 115200 * Hz;

    /// Number of stop bits for each frame
    stop_bits stop = stop_bits::one;

    /// Parity bit type for each frame
    parity parity = parity::none;

    /**
     * @brief Enables default comparison
     *
     */
    auto operator<=>(settings const&) const = default;
  };

  /**
   * @brief Configure serial to match the settings supplied
   *
   * Implementing drivers must verify if the settings can be applied to hardware
   * before modifying the hardware. This will ensure that if this operation
   * fails, the state of the serial device has not changed.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_settings - settings to apply to serial driver
   * @throws hal::operation_not_supported - if the settings could not be
   * achieved.
   */
  async::future<void> configure(async::context& p_context,
                                settings const& p_settings)
  {
    return driver_configure(p_context, p_settings);
  }

  /**
   * @brief Write data to the transmitter line of the serial port
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_data - data to be transmitted over the serial port
   */
  async::future<void> write(async::context& p_context,
                            scatter_span<hal::byte const> p_data)
  {
    return driver_write(p_context, p_data);
  }

  /**
   * @brief Returns this serial driver's receive buffer
   *
   * Use this along with the receive_cursor() in order to determine if new data
   * has been read into the receive buffer. See the docs for `receive_cursor()`
   * for more details.
   *
   * @return circular_span<hal::byte const> - a const span to the
   * receive buffer used by the serial port. Calling `size()` on the span will
   * always return a value of at least 1.
   */
  [[nodiscard]] circular_span<hal::byte const> receive_buffer()
  {
    return driver_receive_buffer();
  }

  /**
   * @brief Returns the current write position of the circular receive buffer
   *
   * Receive cursor represents the position where the next byte of data will be
   * written in to the receive buffer. This position advances as new data
   * arrives. To determine how much new data has arrived, store the previous
   * cursor position and compare it with the current cursor position.
   *
   * The cursor value will ALWAYS monotonically increase and will not wrap
   * around until the cursor reaches the max of `usize`. Using this fact along
   * with the size of the receive_buffer can tell you how many bytes have been
   * written to the receive buffer since this object has been created.
   *
   * Because `receive_buffer()` returns a `circular_span`, you can pass the raw
   * cursor value directly as an index — the modulo wrapping is handled for you:
   *
   * @code
   *
   * auto buffer = serial.receive_buffer();
   * auto byte = buffer[serial.receive_cursor()];
   *
   * @endcode
   *
   * Just note that a valid index does not imply the byte at that position
   * contains useful data; it may be uninitialized or stale if no new data has
   * arrived.
   *
   * Example:
   *
   *   auto old_head = port.receive_cursor();
   *   // ... wait for new data ...
   *   auto new_head = port.receive_cursor();
   *   // Account for circular wraparound when calculating bytes received
   *   auto buffer_size = port.receive_buffer().size();
   *   auto bytes_received = (new_head + buffer_size - old_head) % buffer_size;
   *
   * Use this along with receive_buffer() to access newly received data. The
   * data between your last saved position and the current cursor position
   * represents the newly received bytes. When reading the data, remember that
   * it may wrap around from the end of the buffer back to the beginning.
   *
   * @return usize - position of the write cursor for the
   * circular buffer
   */
  [[nodiscard]] usize receive_cursor()
  {
    return driver_receive_cursor();
  }

  virtual ~serial() = default;

private:
  virtual async::future<void> driver_configure(async::context& p_context,
                                               settings const& p_settings) = 0;
  virtual async::future<void> driver_write(
    async::context& p_context,
    scatter_span<hal::byte const> p_data) = 0;
  virtual circular_span<hal::byte const> driver_receive_buffer() = 0;
  virtual usize driver_receive_cursor() = 0;
};

/// The set of serial receive events that can be awaited on by
export enum class serial_event : u8 {
  /// Suspend until at least one new byte has been written into the receive
  /// buffer since the call was made. The cursor position at the time of the
  /// call is used as the baseline. Resumption occurs when the cursor advances
  /// past that position.
  rx = 0,

  /// Suspend until the RX line has transitioned from active to idle after
  /// receiving one or more bytes. Implementations without hardware idle
  /// detection must emulate this behavior, typically by monitoring cursor
  /// movement with a timer. The exact idle timeout is implementation-defined.
  idle = 1,
};

/**
 * @brief Extension of serial with asynchronous RX event notification.
 *
 * Extends the serial interface with the ability to suspend a coroutine until
 * a specific receive event occurs. Use this interface when the underlying
 * hardware or driver can signal RX activity or line idle conditions via
 * interrupt.
 *
 * Drivers that cannot natively signal RX events should not implement this
 * interface. Use the polling free functions with a clock against the base
 * serial interface instead.
 */
export class awaitable_serial : public serial
{
public:
  /**
   * @brief Suspend the coroutine until the specified RX event occurs.
   *
   * For `event::rx`: resumes when at least one new byte has arrived in the
   * receive buffer. The cursor is snapshotted at the time of the call and
   * resumption occurs once it advances.
   *
   * For `event::idle`: resumes when the RX line goes idle after a burst of
   * data. Implementations without a hardware idle interrupt must emulate this.
   *
   * @param p_context - the async context to suspend while waiting
   * @param p_event - the RX condition to wait for
   */
  async::future<void> wait_for(async::context& p_context, serial_event p_event)
  {
    return driver_wait_for(p_context, p_event);
  }

  virtual ~awaitable_serial() = default;

private:
  virtual async::future<void> driver_wait_for(async::context& p_context,
                                              serial_event p_event) = 0;
};
}  // namespace hal::inline v5
