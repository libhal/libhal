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

export import scatter_span;

export import :units;
export import :containers;
export import :lifetime;
export import :subscription;

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
  void write(mem::scatter_span<hal::byte const> p_data)
  {
    driver_write(p_data);
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

protected:
  ~serial() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual void driver_write(mem::scatter_span<hal::byte const> p_data) = 0;
  virtual circular_span<hal::byte const> driver_receive_buffer() = 0;
  virtual usize driver_receive_cursor() = 0;
};

/**
 * @brief Receive conditions reported to a `serial_receive_callback`
 *
 * The values are bit flags, and a single callback may report several at once.
 * Test them with `hal::has()`, never by comparing for equality, so that flags
 * added later do not break existing callbacks.
 *
 * An event means "check `receive_cursor()`", not "N bytes arrived". Events can
 * arrive close together, for example `high_water` and `idle` when a burst ends
 * right at the middle of the receive buffer, so callbacks must not count
 * events to track data.
 */
export enum class serial_event : u8 {
  /// The RX line went idle after receiving one or more bytes. The idle timeout
  /// is implementation-defined, typically one or a few character times.
  idle = 0b0001,

  /// At least half of `receive_buffer().size()` has been received since the
  /// last event. Unread data is approaching the buffer's capacity: read soon
  /// or it will be overwritten.
  high_water = 0b0010,

  // 0b0100 is reserved for a future overrun flag.
};

/**
 * @brief Combine serial event flags
 *
 * @param p_lhs - flags to combine
 * @param p_rhs - flags to combine
 * @return serial_event - the union of both sets of flags
 */
export constexpr serial_event operator|(serial_event p_lhs,
                                        serial_event p_rhs) noexcept
{
  return static_cast<serial_event>(static_cast<unsigned>(p_lhs) |
                                   static_cast<unsigned>(p_rhs));
}

/**
 * @brief Check whether a set of serial event flags contains a flag
 *
 * @param p_events - the flags reported to a callback
 * @param p_flag - the flag to look for
 * @return true - if every bit of p_flag is set in p_events
 */
export [[nodiscard]] constexpr bool has(serial_event p_events,
                                        serial_event p_flag) noexcept
{
  return (static_cast<unsigned>(p_events) & static_cast<unsigned>(p_flag)) ==
         static_cast<unsigned>(p_flag);
}

/**
 * @brief Callback object invoked by a `serial_interrupt` on receive events
 *
 * The callback runs in interrupt context. It must be short, must not block,
 * and must not throw: an exception cannot unwind out of an interrupt service
 * routine. Read data through `serial::receive_buffer()` outside of the
 * callback.
 */
export struct serial_receive_callback
{
  /**
   * @brief Invoked when one or more serial receive conditions occur
   *
   * @param p_events - the conditions that occurred, as bit flags. Never zero.
   */
  virtual void callback(serial_event p_events) noexcept = 0;

protected:
  ~serial_receive_callback() = default;
};

/**
 * @brief Serial receive event interrupt hardware abstraction
 *
 * Use this interface to run a callback when the serial port's RX line goes
 * idle or its receive buffer is filling up. Drivers whose hardware can signal
 * these conditions via interrupt should vend this interface alongside
 * `hal::serial`. Drivers whose hardware cannot detect an idle line or an RX
 * timeout should not implement it; poll `receive_cursor()` instead.
 *
 * Drivers must not invoke the callback once per received byte. The callback
 * is invoked when the RX line goes idle, or after at least half of the
 * receive buffer has filled since the last event. A driver may combine
 * conditions found in the same interrupt into one call, but does not have to.
 * If the application falls more than half a buffer behind, unread data is
 * overwritten without notice.
 *
 * Contract: at most ONE subscriber at a time. Subscribing while one is active
 * throws hal::subscription_busy.
 *
 * Interrupt context: never call on_receive() from an interrupt service routine,
 * including from inside a callback, and never destroy a subscription there. To
 * change behavior, change state inside the callback object. To swap callbacks,
 * hold the subscription in a `std::optional<hal::subscription>`, then reset()
 * it and subscribe again from outside of interrupt context.
 */
export class serial_interrupt
{
public:
  /**
   * @brief Register a callback for the rest of the program
   *
   * Receive events before a callback is installed are not reported, although
   * the data is still written into the receive buffer.
   *
   * The registration is permanent: the subscriber slot stays taken for the rest
   * of the program.
   *
   * @warning Never call on_receive() from an interrupt service routine,
   * including from inside a callback. It throws `hal::subscription_busy` if a
   * subscriber is already registered, and an exception cannot unwind out of
   * an interrupt service routine.
   *
   * @param p_callback - callback with static storage duration
   * @throws hal::subscription_busy - if a subscriber is already registered
   */
  void on_receive(static_ref<serial_receive_callback> p_callback)
  {
    driver_on_receive(*p_callback);
  }

  /**
   * @brief Register a callback until the returned subscription is destroyed
   *
   * Invoked on serial receive events. Receive events before a callback is
   * installed are not reported, although the data is still written into the
   * receive buffer.
   *
   * @warning Never call on_receive() from an interrupt service routine,
   * including from inside a callback. It throws `hal::subscription_busy` if a
   * subscriber is already registered, and an exception cannot unwind out of
   * an interrupt service routine. Destroy the returned subscription outside
   * of interrupt context too.
   *
   * @param p_callback - the callback to invoke
   * @return subscription - unregisters the callback when destroyed
   * @throws hal::subscription_busy - if a subscriber is already registered
   */
  [[nodiscard]] subscription on_receive(serial_receive_callback& p_callback
                                        [[clang::lifetimebound]])
    [[clang::lifetimebound]]
  {
    driver_on_receive(p_callback);
    return subscription::make<&serial_interrupt::driver_clear_receive>(*this);
  }

protected:
  ~serial_interrupt() = default;

private:
  /// Store the callback. Throw hal::subscription_busy if one is already set.
  virtual void driver_on_receive(serial_receive_callback& p_callback) = 0;
  /// Clear the callback. Called from a destructor; must not throw. The store
  /// must be atomic with respect to the ISR (single word store, or mask IRQ).
  virtual void driver_clear_receive() noexcept = 0;
};
}  // namespace hal::inline v5
