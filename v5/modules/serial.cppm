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

import :units;
import :containers;
import :lifetime;
import :subscription;

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
   * Use this with `receive_count()` to find out how many new bytes have
   * arrived and where they are. See the docs for `receive_count()`.
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
   * @brief Returns the total number of bytes received
   *
   * The count starts at 0 when the driver is created and increases by one for
   * every byte written into the receive buffer. It never goes back, except
   * that it wraps from the maximum value of `usize` to 0.
   *
   * Subtract an earlier count to find how many bytes arrived since then. The
   * subtraction is unsigned, so it stays correct across the wrap:
   *
   * @code
   *
   * auto const before = port.receive_count();
   * // ... wait for new data ...
   * auto const received = port.receive_count() - before;
   *
   * @endcode
   *
   * If `received` is larger than `receive_buffer().size()`, the oldest of
   * those bytes were overwritten before they could be read.
   *
   * `receive_buffer()` returns a `circular_span`, which wraps any index to its
   * size, so a count can index it directly. The next byte will be written at
   * `buffer[port.receive_count()]`, and the bytes that arrived since `before`
   * are, oldest first:
   *
   * @code
   *
   * auto const buffer = port.receive_buffer();
   * for (hal::usize i = 0; i < received; i++) {
   *   auto const& byte = buffer[before + i];
   * }
   *
   * @endcode
   *
   * When the count wraps to 0, indexes taken from it jump to 0 too, unless the
   * buffer's size is a power of two.
   *
   * @return usize - the number of bytes received since the driver was created,
   * modulo the range of `usize`
   */
  [[nodiscard]] usize receive_count()
  {
    return driver_receive_count();
  }

protected:
  ~serial() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual void driver_write(mem::scatter_span<hal::byte const> p_data) = 0;
  virtual circular_span<hal::byte const> driver_receive_buffer() = 0;
  virtual usize driver_receive_count() = 0;
};

/**
 * @brief Receive conditions reported to a `serial_receive_callback`
 *
 * The values are bit flags, and a single callback may report several at once.
 * Test them with `operator&`, for example `if (p_events &
 * hal::serial_event::idle)`, never by comparing for equality, so that flags
 * added later do not break existing callbacks.
 *
 * An event means "check `receive_count()`", not "N bytes arrived". Events can
 * arrive close together, for example `high_water` and `idle` when a burst ends
 * right at the middle of the receive buffer, so callbacks must not count
 * events to track data.
 */
export enum class serial_event : u8 {
  /// The RX line went idle after receiving one or more bytes. The idle timeout
  /// is implementation-defined, typically one or a few character times.
  idle = 1U << 0U,

  /// At least half of `receive_buffer().size()` has been received since the
  /// last event. Unread data is approaching the buffer's capacity: read soon
  /// or it will be overwritten.
  high_water = 1U << 1U,

  // 1U << 2U is reserved for a future overrun flag.
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
  auto const lhs = static_cast<unsigned>(p_lhs);
  auto const rhs = static_cast<unsigned>(p_rhs);
  return static_cast<serial_event>(lhs | rhs);
}

/**
 * @brief Check whether a set of serial event flags contains any of some flags
 *
 * Returns bool rather than serial_event so it can be used directly in a
 * condition: `if (p_events & hal::serial_event::idle)`.
 *
 * @param p_events - the flags reported to a callback
 * @param p_flags - the flag, or flags, to look for
 * @return true - if any flag in p_flags is set in p_events
 */
export [[nodiscard]] constexpr bool operator&(serial_event p_events,
                                              serial_event p_flags) noexcept
{
  auto const events = static_cast<unsigned>(p_events);
  auto const flags = static_cast<unsigned>(p_flags);
  return (events & flags) != 0U;
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
   * @param p_events - the conditions that occurred, as bit flags. Never zero,
   * and only ever flags that were requested in
   * `serial_interrupt::on_receive()`.
   */
  virtual void callback(serial_event p_events) noexcept = 0;

protected:
  ~serial_receive_callback() = default;
};

/**
 * @brief Serial receive event interrupt hardware abstraction
 *
 * Use this interface to run a callback when the serial port's RX line goes
 * idle or its receive buffer is filling up. Drivers vend this interface
 * alongside `hal::serial`.
 *
 * Drivers whose hardware can signal these conditions via interrupt use it
 * directly. Drivers whose hardware cannot can still implement this interface
 * by taking a `hal::timed_interrupt` and running it in periodic mode to check
 * the receive cursor: if the cursor moved and has since stopped, report
 * `idle`; if at least half of the receive buffer has filled since the last
 * event, report `high_water`. The tick period sets the idle timeout and how
 * far the buffer can fill before it is noticed.
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
 * including from inside a callback, and never destroy, reset() or assign a
 * subscription there. To change behavior, change state inside the callback
 * object. To swap callbacks, reset() the subscription, then assign it a new one
 * from outside of interrupt context.
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
   * @param p_events - the conditions to detect. The callback is only invoked
   * with these flags.
   * @throws hal::subscription_busy - if a subscriber is already registered
   * @throws hal::operation_not_supported - if the driver cannot detect one or
   * more of the conditions in p_events.
   */
  void on_receive(static_ref<serial_receive_callback> p_callback,
                  serial_event p_events)
  {
    driver_on_receive(*p_callback, p_events);
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
   * @param p_events - the conditions to detect. The callback is only invoked
   * with these flags.
   * @return subscription - unregisters the callback when destroyed
   * @throws hal::subscription_busy - if a subscriber is already registered
   * @throws hal::operation_not_supported - if the driver cannot detect one or
   * more of the conditions in p_events
   */
  subscription on_receive(serial_receive_callback& p_callback
                          [[clang::lifetimebound]],
                          serial_event p_events) [[clang::lifetimebound]]
  {
    driver_on_receive(p_callback, p_events);
    return subscription::make<&serial_interrupt::driver_clear_receive>(*this);
  }

protected:
  ~serial_interrupt() = default;

private:
  /// Store the callback and enable detection of p_events. Throw
  /// hal::subscription_busy if a callback is already set, or
  /// hal::operation_not_supported if any flag in p_events can't be detected.
  virtual void driver_on_receive(serial_receive_callback& p_callback,
                                 serial_event p_events) = 0;
  /// Clear the callback. Called from a destructor; must not throw. The store
  /// must be atomic with respect to the ISR (single word store, or mask IRQ).
  virtual void driver_clear_receive() noexcept = 0;
};
}  // namespace hal::inline v5
