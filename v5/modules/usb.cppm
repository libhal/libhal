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

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>

export module hal:usb;

export import async_context;
import :units;
import :scatter_span;

namespace hal::inline v5::usb {
/**
 * @brief Basic information about an usb endpoint
 *
 */
struct endpoint_info
{
  /**
   * @brief The max number of bytes the endpoint can work with
   *
   */
  u16 size;

  /**
   * @brief Endpoint number in USB format
   *
   * bits [0-4]: logical endpoint number
   * bits [5-6]: reserved and set to zero
   * bits   [7]: 1 == IN endpoint ; 0 == OUT endpoint
   */
  u8 number;

  /**
   * @brief Set to `true` if the endpoint is stalled/HALTED
   *
   */
  bool stalled;

  /**
   * @brief Returns if the direction of the endpoint is an IN endpoint
   *
   * @return true - endpoint is an IN endpoint
   * @return false - endpoint is an OUT endpoint
   */
  [[nodiscard]] bool in_direction()
  {
    return number >> 7;
  }

  /**
   * @brief Return the endpoint number without the IN/OUT flag
   *
   * @return u8 - the logical endpoint number from 0 to 16.
   */
  [[nodiscard]] u8 logical_number()
  {
    return number & 0xF;
  }
};

/**
 * @brief USB host-level events forwarded to @ref interface implementations from
 * the enumerator.
 *
 * The enumerator calls @ref interface::handle_host_event on all usb interfaces
 * it has control over based on bus events and setup packets received from the
 * host.
 *
 * Interface implementations use these events to manage their own state,
 * such as enabling or disabling hardware, stopping or resuming data
 * queuing, or resetting internal transfer state.
 */
enum class host_event : hal::u8
{
  /// @brief The host has driven SE0 for >10ms, triggering re-enumeration,
  ///        or the application has called @ref control_endpoint::connect
  ///        to force a reconnect.
  ///
  /// All endpoint state has been cleared by hardware. Interfaces should
  /// discard any pending transfer state and reset internal state machines.
  /// The enumerator will re-run the full enumeration sequence and deliver
  /// @ref enumerated again when complete.
  reset,

  /// @brief Enumeration is complete. The host has issued SET_CONFIGURATION
  ///        and the device is now fully operational.
  ///
  /// Interfaces should begin normal operation and may start queuing data
  /// to their endpoints. Devices start in a suspended state and must
  /// wait for this event before transmitting.
  enumerated,

  /// @brief The bus has resumed from any sleep or suspend state.
  ///
  /// Covers exit from L1, L2, U1, U2, and U3. Interfaces may resume
  /// normal endpoint operation. Any endpoint writes that were blocked
  /// during suspend will now complete.
  resume,

  /// @brief The host has suspended the bus and has granted the device
  ///        permission to initiate remote wakeup (L2/U3).
  ///
  /// Interfaces should monitor for new data. When new data arrives,
  /// calling write() on an endpoint will automatically assert resume
  /// signaling to wake the host before completing the transfer.
  ///
  /// The device must reduce bus current draw to <= 2.5mA within 7ms.
  suspend_with_wakeup,

  /// @brief The host has suspended the bus without granting remote wakeup
  ///        permission (L2/U3).
  ///
  /// Interfaces should cease all activity and queue no data to endpoints.
  /// Any endpoint writes attempted while in this state will block
  /// indefinitely until the host resumes the bus on its own.
  ///
  /// The device must reduce bus current draw to <= 2.5mA within 7ms.
  suspend_without_wakeup,

  /// @brief The host has requested a fast sleep (L1/LPM) transition.
  ///
  /// Exit latency is in the range of microseconds. There are no strict
  /// power draw requirements in this state. Interfaces may choose to
  /// remain operational or reduce activity. The endpoint write path will
  /// automatically handle resume signaling if remote wakeup is enabled.
  ///
  /// Only fired on hardware with USB 2.0 LPM support.
  sleep,

  /// @brief The USB 3.0+ link has entered U1 standby.
  ///
  /// Very fast exit (~microseconds). Hardware-managed transition.
  /// Interfaces typically need not respond to this event.
  u1_sleep,

  /// @brief The USB 3.0+ link has entered U2 standby.
  ///
  /// Fast exit (~milliseconds). Hardware-managed transition.
  /// Interfaces may optionally reduce activity in response.
  u2_sleep,
};

/**
 * @brief Raw USB bus-level events reported by hardware drivers to the
 *        enumerator.
 *
 * This is the hardware-visible set of bus activity. Drivers report these
 * events via the @ref control_endpoint::on_bus_event callback. The enumerator
 * consumes them, updates its own protocol state (enumeration, remote wakeup
 * permission, etc.), and translates them into the richer @ref host_event
 * vocabulary before forwarding to interface implementations.
 */
enum class bus_event : u8
{
  /// @brief The host has driven SE0 for >2.5ms, or the application has
  ///        called @ref control_endpoint::connect to force a reconnect.
  ///
  /// All endpoint state has been cleared by hardware. The enumerator will
  /// discard pending transfer state, reset its internal state machine, and
  /// re-run the full enumeration sequence. Remote wakeup permission is
  /// implicitly revoked on reset and must be re-granted by the host via
  /// SET_FEATURE(DEVICE_REMOTE_WAKEUP) during the new enumeration.
  reset,

  /// @brief A SETUP packet is available in the control endpoint buffer.
  ///
  /// The enumerator should call @ref control_endpoint::read to retrieve
  /// the 8-byte setup packet and process the request.
  setup_packet,

  /// @brief A data OUT packet is available in the control endpoint buffer.
  ///
  /// Delivered during the data phase of a host-to-device control transfer,
  /// after a setup packet with wLength > 0 has been processed. The
  /// enumerator should call @ref control_endpoint::read to retrieve the
  /// data and complete the pending request.
  data_packet,

  /// @brief The bus has resumed from any suspend or sleep state.
  ///
  /// Covers exit from L1, L2, U1, U2, and U3. The enumerator forwards this
  /// as @ref host_event::resume to all active interfaces.
  resume,

  /// @brief The host has suspended the bus (L2/U3).
  ///
  /// SOF packets have been absent for >= 3ms. The hardware cannot determine
  /// whether remote wakeup was granted; the enumerator resolves this from
  /// its tracked SET_FEATURE / CLEAR_FEATURE state and forwards either
  /// @ref host_event::suspend_with_wakeup or
  /// @ref host_event::suspend_without_wakeup to interfaces.
  ///
  /// The device must reduce bus current draw to <= 2.5mA within 7ms.
  suspend,

  /// @brief The host has requested a fast sleep (L1/LPM) transition.
  ///
  /// Exit latency is in the range of microseconds. The enumerator forwards
  /// this as @ref host_event::sleep to all active interfaces.
  ///
  /// Only fired on hardware with USB 2.0 LPM support.
  sleep,

  /// @brief The USB 3.0+ link has entered U1 standby.
  ///
  /// Very fast exit (~microseconds). Hardware-managed transition. The
  /// enumerator forwards this as @ref host_event::u1_sleep to interfaces.
  u1_sleep,

  /// @brief The USB 3.0+ link has entered U2 standby.
  ///
  /// Fast exit (~milliseconds). Hardware-managed transition. The enumerator
  /// forwards this as @ref host_event::u2_sleep to interfaces.
  u2_sleep,
};

/**
 * @brief Generic usb endpoint interface
 *
 * This interface exists to mandate specific generic APIs for all usb endpoints.
 * Specifically that each endpoint must provide an info() and stall(bool) API
 * which are used by the enumeration process to stall or un-stall an endpoint so
 * a specific feature can be used.
 *
 * The `stall(bool)` can also be used by implementations of the
 * `hal::interface` interface to indicate to the HOST usb controller an
 * error on that endpoint or to signal to the HOST that the endpoint is no
 * longer available.
 *
 * The `info()` API can also be used by users in order to log information about
 * endpoints if that is useful to their application and testing.
 */
class endpoint
{
public:
  virtual ~endpoint() = default;

  /**
   * @brief Get info about this endpoint
   *
   * @return endpoint_info - endpoint information
   */
  [[nodiscard]] endpoint_info info() const
  {
    return driver_info();
  }

  /**
   * @brief Stall or un-stall an endpoint
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_should_stall - set to true to stall this endpoint, set to false to
   * un-stall the endpoint.
   * @return async::future<void> - completes when the stall operation is
   * complete
   */
  async::future<void> stall(async::context& p_context, bool p_should_stall)
  {
    return driver_stall(p_context, p_should_stall);
  }

  /**
   * @brief Reset the USB endpoint
   *
   * This method resets the USB endpoint to its default state. A reset operation
   * clears any pending transactions, resets the endpoint's data toggle bit to
   * zero, and terminates any ongoing transfers on the endpoint. This is
   * typically used during USB enumeration when transitioning between different
   * device states, or when recovering from error conditions.
   *
   * The reset operation affects the following aspects of the endpoint:
   * - Clears the data toggle bit (used for USB protocol handshaking)
   * - Terminates any pending or active transfers
   * - Resets the endpoint's internal state machine
   * - Clears any error conditions that may be present
   *
   * This method is typically called during:
   * - USB device enumeration when transitioning from Default to Addressed state
   * - Recovery from USB protocol errors
   * - When a new configuration is being set
   * - During USB bus reset operations
   *
   * After a reset, the endpoint will be in a clean state ready to accept new
   * transactions. The host controller will typically re-initialize the endpoint
   * when the next data transfer occurs.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<void> - completes when the reset operation is
   * complete
   */
  async::future<void> reset(async::context& p_context)
  {
    return driver_reset(p_context);
  }

private:
  [[nodiscard]] virtual endpoint_info driver_info() const = 0;
  virtual async::future<void> driver_stall(async::context& p_context,
                                           bool p_should_stall) = 0;
  virtual async::future<void> driver_reset(async::context& p_context) = 0;
};

/**
 * @brief Bitmask descriptor of USB Link Power Management (LPM) states
 *        supported by a device or host.
 *
 * USB LPM allows the host and device to negotiate low-power states during
 * periods of inactivity without requiring a full disconnect/reconnect cycle.
 * USB 2.0 LPM defines the L1 sleep state; USB 3.x SuperSpeed defines the
 * U1 and U2 link states.
 *
 * Use the setter overloads (which return `*this`) to build a value with
 * method chaining:
 *
 * @code
 * lpm_support capabilities{};
 * capabilities.remote_wakeup_supported(true)
 *             .l1_supported(true)
 *             .u1_supported(true);
 * @endcode
 */
struct lpm_support
{
  /// @brief Bitmask for USB 2.0 remote wakeup capability.
  static constexpr hal::u8 remote_wakeup_mask = 1 << 0;

  /// @brief Bitmask for the USB 2.0 L1 (sleep) LPM state.
  static constexpr hal::u8 l1_mask = 1 << 1;

  /// @brief Bitmask for the USB 3.x U1 link state.
  static constexpr hal::u8 u1_mask = 1 << 2;

  /// @brief Bitmask for the USB 3.x U2 link state.
  static constexpr hal::u8 u2_mask = 1 << 3;

  /**
   * @brief Returns whether USB 2.0 remote wakeup is supported.
   *
   * @return true - remote wakeup is supported
   * @return false - remote wakeup is not supported
   */
  [[nodiscard]] constexpr bool remote_wakeup_supported() const noexcept
  {
    return bits & remote_wakeup_mask;
  }

  /**
   * @brief Returns whether the USB 2.0 L1 sleep state is supported.
   *
   * @return true - L1 LPM is supported
   * @return false - L1 LPM is not supported
   */
  [[nodiscard]] constexpr bool l1_supported() const noexcept
  {
    return bits & l1_mask;
  }

  /**
   * @brief Returns whether the USB 3.x U1 link state is supported.
   *
   * @return true - U1 is supported
   * @return false - U1 is not supported
   */
  [[nodiscard]] constexpr bool u1_supported() const noexcept
  {
    return bits & u1_mask;
  }

  /**
   * @brief Returns whether the USB 3.x U2 link state is supported.
   *
   * @return true - U2 is supported
   * @return false - U2 is not supported
   */
  [[nodiscard]] constexpr bool u2_supported() const noexcept
  {
    return bits & u2_mask;
  }

  /**
   * @brief Enables or disables remote wakeup support in the bitmask.
   *
   * @param p_enable - true to mark remote wakeup as supported, false to clear
   * it
   * @return lpm_support& - reference to this object for method chaining
   */
  constexpr lpm_support& remote_wakeup_supported(bool p_enable) noexcept
  {
    set(remote_wakeup_mask, p_enable);
    return *this;
  }

  /**
   * @brief Enables or disables L1 support in the bitmask.
   *
   * @param p_enable - true to mark L1 as supported, false to clear it
   * @return lpm_support& - reference to this object for method chaining
   */
  constexpr lpm_support& l1_supported(bool p_enable) noexcept
  {
    set(l1_mask, p_enable);
    return *this;
  }

  /**
   * @brief Enables or disables U1 support in the bitmask.
   *
   * @param p_enable - true to mark U1 as supported, false to clear it
   * @return lpm_support& - reference to this object for method chaining
   */
  constexpr lpm_support& u1_supported(bool p_enable) noexcept
  {
    set(u1_mask, p_enable);
    return *this;
  }

  /**
   * @brief Enables or disables U2 support in the bitmask.
   *
   * @param p_enable - true to mark U2 as supported, false to clear it
   * @return lpm_support& - reference to this object for method chaining
   */
  constexpr lpm_support& u2_supported(bool p_enable) noexcept
  {
    set(u2_mask, p_enable);
    return *this;
  }

  /**
   * @brief Returns whether any LPM state is supported.
   *
   * @return true - at least one LPM state bit is set
   * @return false - no LPM states are supported
   */
  [[nodiscard]] constexpr bool any() const noexcept
  {
    return bits != 0;
  }

  /**
   * @brief Returns whether no LPM states are supported.
   *
   * @return true - no LPM state bits are set
   * @return false - at least one LPM state is supported
   */
  [[nodiscard]] constexpr bool none() const noexcept
  {
    return bits == 0;
  }

  /**
   * @brief Sets or clears a single LPM state bit.
   *
   * @param p_mask - bitmask of the state to modify (e.g. `l1_mask`)
   * @param p_enable - true to set the bit, false to clear it
   */
  constexpr void set(hal::u8 p_mask, bool p_enable) noexcept
  {
    if (p_enable) {
      bits |= p_mask;
    } else {
      bits &= ~p_mask;
    }
  }

  constexpr bool operator<=>(lpm_support const& rhs) const = default;

  /// @brief Raw bitmask of supported LPM states; zero means none supported.
  hal::u8 bits = 0;
};

/**
 * @brief USB Control Endpoint Interface
 *
 * This class represents the control endpoint of a USB device. The control
 * endpoint is crucial for USB communication as it handles device enumeration,
 * configuration, and general control operations.
 *
 * Use cases:
 *
 * - Initiating USB connections
 * - Handling USB enumeration process
 * - Setting device addresses
 * - Responding to standard USB requests
 * - Sending and receiving control data
 *
 */
class control_endpoint : public endpoint
{
public:
  /**
   * @brief Signal to connect/enable USB peripheral
   *
   * Used to initiate a connection to a host machine and begin enumeration.
   * Can be used to perform a disconnect and reconnect to the host/hub.
   *
   * Enumeration only involves the control endpoints and cannot happen without
   * it, thus the responsibility to initiate enumeration is on the control
   * endpoint.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_should_connect - set to true to connect, set to false to
   * disconnect.
   * @return async::future<void> - completes when the connect operation is
   * complete
   */
  async::future<void> connect(async::context& p_context, bool p_should_connect)
  {
    return driver_connect(p_context, p_should_connect);
  }

  /**
   * @brief Set the USB device address
   *
   * Used to set the device address during the USB enumeration process. This
   * address must come from a USB request on the control endpoint by the HOST.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_address The address assigned to this device from the HOST
   * @return async::future<void> - completes when the address is set
   */
  async::future<void> set_address(async::context& p_context, u8 p_address)
  {
    return driver_set_address(p_context, p_address);
  }

  /**
   * @brief Write data to the control endpoint's memory
   *
   * This API will copy the contents of the span of byte spans into the endpoint
   * memory. When the endpoint memory is full, this API will ACK the next HOST
   * IN packet and the data in the endpoint will be transmitted. This is
   * repeated until no more data is left. To finish the transfer of data, call
   * `flush()` OR call this API with an empty p_data field:
   *
   * ```C++
   * co_await usb.write(context, {}); // Finishes the transfer with an ZLP or
   * the last of the data
   * ```
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_data - a scatter span of bytes to be written to the endpoint
   * memory and sent over USB.
   * @return async::future<void> - completes when the data is written
   */
  async::future<void> write(async::context& p_context,
                            scatter_span<byte const> p_data)
  {
    return driver_write(p_context, p_data);
  }

  /**
   * @brief Read contents of endpoint
   *
   * This API is not to be assumed to be callable from within the `on_receive`
   * callback.
   *
   * When data is available in the endpoint, the endpoint will be configured to
   * NAK all following HOST packet requests. When all data from the endpoint has
   * been read, the endpoint will become valid again and can ACK the HOST
   * packets.
   *
   * If a caller wants to drain all of the data from the endpoint's memory, then
   * the caller should continually call read until it returns an empty span.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_buffer - a scatter span of bytes to fill with data
   * @return async::future<usize> - completes with the number of bytes read into
   * the buffers provided by p_buffer.
   */
  [[nodiscard]] async::future<usize> read(async::context& p_context,
                                          scatter_span<byte> p_buffer)
  {
    return driver_read(p_context, p_buffer);
  }

  /**
   * @brief Suspend until the next setup or data packet is received
   *
   * Multiple coroutines may concurrently await this function. All registered
   * waiters are unblocked when the next packet arrives. If the implementation's
   * internal waiter capacity is exceeded, the caller will block by sync until a
   * slot becomes available.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<void> - completes when a setup or data packet has
   *         been received on the control endpoint
   */
  async::future<void> on_receive(async::context& p_context)
  {
    return driver_on_receive(p_context);
  }

  /**
   * @brief Suspend until the next bus-level event occurs
   *
   * Multiple coroutines may concurrently await this function. All registered
   * waiters are unblocked when the next bus event arrives. If the
   * implementation's internal waiter capacity is exceeded, the caller will
   * block by sync until a slot becomes available.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<bus_event> - completes with the @ref bus_event that
   *         occurred
   */
  async::future<bus_event> on_bus_event(async::context& p_context)
  {
    return driver_on_bus_event(p_context);
  }

  /**
   * @brief Grant or revoke the device's permission to initiate remote wakeup.
   *
   * Called by the enumerator in response to SET_FEATURE(DEVICE_REMOTE_WAKEUP)
   * and CLEAR_FEATURE(DEVICE_REMOTE_WAKEUP) setup packets from the host.
   *
   * When enabled, IN endpoint writes issued while the bus is suspended will
   * automatically assert resume signaling before completing the transfer,
   * waking the host if it is suspended.
   *
   * When disabled, IN endpoint writes while suspended will block until the
   * host resumes the bus on its own.
   *
   * This API does nothing if not implemented.
   *
   * @param p_enabled - true if the host has granted remote wakeup permission,
   *                    false if the host has revoked it.
   */
  void remote_wakeup_enable(bool p_enabled)
  {
    return driver_remote_wakeup_enable(p_enabled);
  }

  /**
   * @brief Query whether the host has granted remote wakeup permission.
   *
   * Returns the current remote wakeup grant state as last set by the enumerator
   * via @ref remote_wakeup_enable. Returns `false` by default if not overridden
   * by the driver.
   *
   * @return true - the host has granted remote wakeup permission
   * @return false - remote wakeup permission has not been granted, was
   * revoked, or the hardware isn't capable of performing a wake K-state on the
   * bus.
   */
  [[nodiscard]] bool remote_wakeup_granted()
  {
    return driver_remote_wakeup_granted();
  }

  /**
   * @brief Accept or reject an incoming L1 LPM sleep request from the host.
   *
   * Called by the enumerator when an LPM token is received from the host
   * requesting the device enter the L1 (sleep) power state. The implementation
   * must respond to the host by either acknowledging or rejecting the
   * transition.
   *
   * Accepting the request causes the hardware to enter L1 sleep, after which
   * the enumerator dispatches `bus_event::sleep` to all registered interfaces.
   * Rejecting the request keeps the device in L0 (active) state and no event
   * is dispatched.
   *
   * This function is invoked exclusively by the enumerator and must not be
   * called directly by interface implementors or application code.
   *
   * This API does nothing if not implemented
   *
   * @param p_accept - `true` to acknowledge the L1 sleep request and allow the
   *                 device to enter L1; `false` to reject it and remain in L0.
   */
  void acknowledge_sleep(bool p_accept)
  {
    return driver_acknowledge_sleep(p_accept);
  }

  /**
   * @brief Query which USB Link Power Management states the hardware supports.
   *
   * The enumerator calls this during enumeration to determine which LPM
   * capability descriptors to include. For USB 2.0, a Binary Object Store
   * (BOS) descriptor advertising L1 support is included only when
   * `lpm_support::l1_supported()` is true. For USB 3.x, U1/U2 capability
   * descriptors follow the same pattern. If no bits are set the relevant
   * descriptors are omitted and the host will never enter those link states,
   * meaning `acknowledge_sleep()` will never be called.
   *
   * Implementations should return a compile-time or hardware-register-derived
   * constant. The result must remain stable for the lifetime of the
   * `control_endpoint` object.
   *
   * @return lpm_support - bitmask of supported LPM states; defaults to an
   *   all-zero value (no LPM support) if not overridden by the driver.
   */
  [[nodiscard]] lpm_support supports_lpm()
  {
    return driver_supports_lpm();
  }

private:
  virtual async::future<void> driver_connect(async::context& p_context,
                                             bool p_should_connect) = 0;
  virtual async::future<void> driver_set_address(async::context& p_context,
                                                 u8 p_address) = 0;
  virtual async::future<void> driver_write(async::context& p_context,
                                           scatter_span<byte const> p_data) = 0;
  virtual async::future<usize> driver_read(async::context& p_context,
                                           scatter_span<byte> p_buffer) = 0;
  virtual async::future<void> driver_on_receive(async::context& p_context) = 0;
  [[nodiscard]] virtual std::optional<bool> driver_has_setup() const noexcept
  {
    return {};
  }
  virtual async::future<bus_event> driver_on_bus_event(
    async::context& p_context) = 0;
  virtual void driver_remote_wakeup_enable(bool)
  {
  }
  virtual bool driver_remote_wakeup_granted()
  {
    return false;
  }
  virtual void driver_acknowledge_sleep(bool)
  {
  }
  virtual lpm_support driver_supports_lpm()
  {
    return {};
  }
};

/**
 * @brief The generic USB IN Endpoint Interface
 *
 * This class represents a generic IN endpoint of a USB device. It does not
 * specify the exact endpoint type. This interface is a convince interface for
 * common APIs. It is not meant to be used directly by drivers. Use the
 * interfaces that inherit from this interface.
 */
class in_endpoint : public endpoint
{
public:
  /**
   * @brief Write data to the control endpoint's memory
   *
   * This API will copy the contents of the span of byte spans into the endpoint
   * memory. When the endpoint memory is full, this API will ACK the next HOST
   * IN packet and the data in the endpoint will be transmitted. This is
   * repeated until no more data is left. To finish the transfer of data, call
   * `flush()` OR call this API with an empty p_data field:
   *
   * ```C++
   * co_await usb.write(context, {}); // Finishes the transfer with an ZLP or
   * the last of the data
   * ```
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_data - a scatter span of bytes to be written to the endpoint
   * memory and sent over USB.
   * @return async::future<void> - completes when the data is written
   * @throws hal::operation_not_permitted - if the USB is suspended and a write
   * is attempted.
   */
  async::future<void> write(async::context& p_context,
                            scatter_span<byte const> p_data)
  {
    return driver_write(p_context, p_data);
  }

private:
  virtual async::future<void> driver_write(async::context& p_context,
                                           scatter_span<byte const> p_data) = 0;
};

/**
 * @brief The generic USB OUT Endpoint Interface
 *
 * This class represents a generic OUT endpoint of a USB device. It does not
 * specify the exact endpoint type. This interface is a convince interface for
 * common APIs. It is not meant to be used directly by drivers. Use the
 * interfaces that inherit from this interface.
 */
class out_endpoint : public endpoint
{
public:
  /**
   * @brief Suspend until data is available on the endpoint
   *
   * Multiple coroutines may concurrently await this function. All registered
   * waiters are unblocked when the next data packet arrives. If the
   * implementation's internal waiter capacity is exceeded, the caller will
   * block by sync until a slot becomes available.
   *
   * After this function completes, the endpoint shall be set to NAK all
   * requests from the HOST until the contents are read via the read() API. Once
   * all data has been read from the endpoint, the endpoint will become valid
   * once again and can ACK the host if it wants to transmit more data.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<void> - completes when data is available in the
   *         endpoint
   */
  async::future<void> on_receive(async::context& p_context)
  {
    return driver_on_receive(p_context);
  }

  /**
   * @brief Read contents of endpoint
   *
   * This API is not to be assumed to be callable from within the `on_receive`
   * callback.
   *
   * When data is available in the endpoint, the endpoint will be configured to
   * NAK all following HOST packet requests. When all data from the endpoint has
   * been read, the endpoint will become valid again and can ACK the HOST
   * packets.
   *
   * If a caller wants to drain all of the data from the endpoint's memory, then
   * the caller should continually call read until it returns an empty span.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_buffer - buffer to fill with data
   * @return async::future<usize> - completes with the number of bytes read from
   * the OUT endpoint. The size will be 0 if no more data was present in the
   * endpoint.
   */
  [[nodiscard]] async::future<usize> read(async::context& p_context,
                                          scatter_span<byte> p_buffer)
  {
    return driver_read(p_context, p_buffer);
  }

private:
  virtual async::future<void> driver_on_receive(async::context& p_context) = 0;
  virtual async::future<usize> driver_read(async::context& p_context,
                                           scatter_span<byte> p_buffer) = 0;
};

/**
 * @brief USB Interrupt IN Endpoint Interface
 *
 * This class represents an interrupt IN endpoint of a USB device. Interrupt
 * IN endpoints are used for small, time-sensitive data transfers from the
 * device to the host.
 *
 * Use cases:
 * - Sending periodic status updates
 * - Transmitting small amounts of data with guaranteed latency
 * - Ideal for devices like keyboards, mice, or game controllers
 */
struct interrupt_in_endpoint : public in_endpoint
{};

/**
 * @brief USB Interrupt OUT Endpoint Interface
 *
 * This class represents an interrupt OUT endpoint of a USB device. Interrupt
 * OUT endpoints are used for small, time-sensitive data transfers from the
 * host to the device.
 *
 * Use cases:
 * - Receiving periodic commands or settings
 * - Handling small amounts of data with guaranteed latency
 * - Ideal for devices that need quick responses to host commands
 */
struct interrupt_out_endpoint : public out_endpoint
{};

/**
 * @brief USB Bulk IN Endpoint Interface
 *
 * This class represents a bulk IN endpoint of a USB device. Bulk IN endpoints
 * are used for large, non-time-critical data transfers from the device to the
 * host.
 *
 * Use cases:
 * - Transferring large amounts of data
 * - Sending data when timing is not critical
 * - Ideal for devices like printers, scanners, or external storage
 */
struct bulk_in_endpoint : public in_endpoint
{};

/**
 * @brief USB Bulk OUT Endpoint Interface
 *
 * This class represents a bulk OUT endpoint of a USB device. Bulk OUT
 * endpoints are used for large, non-time-critical data transfers from the
 * host to the device.
 *
 * Use cases:
 * - Receiving large amounts of data
 * - Handling data when timing is not critical
 * - Ideal for devices like printers, scanners, or external storage
 */
struct bulk_out_endpoint : public out_endpoint
{};

template<class T>
concept out_endpoint_type = std::is_base_of_v<out_endpoint, T> ||
                            std::is_base_of_v<control_endpoint, T> ||
                            std::is_base_of_v<bulk_out_endpoint, T> ||
                            std::is_base_of_v<interrupt_out_endpoint, T>;

template<class T>
concept in_endpoint_type =
  std::is_base_of_v<in_endpoint, T> || std::is_base_of_v<control_endpoint, T> ||
  std::is_base_of_v<bulk_in_endpoint, T> ||
  std::is_base_of_v<interrupt_in_endpoint, T>;

/**
 * @brief USB Setup Request packet definition
 *
 * This structure represents the standard 8-byte USB setup packet that is sent
 * by the host to initiate control transfers. Setup packets are used for device
 * enumeration, configuration, and various control operations as defined by the
 * USB specification.
 *
 * The setup packet format follows the USB specification exactly:
 * - Byte 0: bmRequestType (direction, type, and recipient)
 * - Byte 1: bRequest (specific request code)
 * - Bytes 2-3: wValue (request-specific value, little-endian)
 * - Bytes 4-5: wIndex (request-specific index, little-endian)
 * - Bytes 6-7: wLength (data phase length, little-endian)
 */
struct setup_packet
{
  constexpr static usize value_offset = 2;
  constexpr static usize index_offset = 4;
  constexpr static usize length_offset = 6;

  /**
   * @brief Request type classification for setup packets
   *
   * Defines the type of USB request as specified in bits 6-5 of bmRequestType.
   * This determines how the request should be interpreted and who is
   * responsible for handling it.
   */
  enum class request_type : hal::byte
  {
    /// Standard USB requests defined by the USB specification (e.g.,
    /// GET_DESCRIPTOR)
    standard = 0,
    /// Class-specific requests defined by USB device classes (e.g., HID, CDC,
    /// MSC)
    class_t = 1,
    /// Vendor-specific requests for custom functionality
    vendor = 2,
    /// Invalid or reserved request type
    invalid
  };

  /**
   * @brief Request recipient classification for setup packets
   *
   * Defines the target of the USB request as specified in bits 4-0 of
   * bmRequestType. This determines which component of the USB device should
   * handle the request.
   */
  enum class request_recipient : hal::byte
  {
    /// Request targets the entire device (e.g., SET_ADDRESS, GET_CONFIGURATION)
    device = 0,
    /// Request targets a specific interface (e.g., SET_INTERFACE, class
    /// requests)
    interface = 1,
    /// Request targets a specific endpoint (e.g., CLEAR_FEATURE on endpoint)
    endpoint = 2,
    /// Invalid or reserved recipient
    invalid
  };

  /**
    @brief Argument struct for building a setup packet from scratch

   * @param type Request type (standard, class, or vendor)
   * @param recipient Request recipient (device, interface, or endpoint)
   * @param request bRequest field value
   * @param value wValue field value
   * @param index wIndex field value
   * @param length wLength field value (data phase length)
   */
  struct args
  {
    bool device_to_host;
    request_type type;
    request_recipient recipient;
    u8 request;
    u16 value;
    u16 index;
    u16 length;
  };

  constexpr setup_packet() = default;

  /**
   * @brief Construct setup packet from semantic components
   *
   * @param p_args Arguments for the individual fields of the packet
   */
  constexpr setup_packet(args p_args)
  {
    raw_request_bytes[0] = p_args.device_to_host << 7 |
                           static_cast<byte>(p_args.type) << 5 |
                           static_cast<byte>(p_args.recipient);
    raw_request_bytes[1] = p_args.request;

    set_le_u16<value_offset>(p_args.value);
    set_le_u16<index_offset>(p_args.index);
    set_le_u16<length_offset>(p_args.length);
  }

  /**
   * @brief Construct setup packet from raw USB data
   *
   * Parses an 8-byte setup packet received from the USB host. This constructor
   * handles the little-endian byte ordering conversion for multi-byte fields.
   * This is typically used when processing setup packets received from the
   * control endpoint.
   *
   * @param p_raw_req 8-byte span containing the raw setup packet data
   *
   * Raw packet format:
   * - p_raw_req[0]: bmRequestType
   * - p_raw_req[1]: bRequest
   * - p_raw_req[2-3]: wValue (little-endian)
   * - p_raw_req[4-5]: wIndex (little-endian)
   * - p_raw_req[6-7]: wLength (little-endian)
   */
  constexpr setup_packet(std::array<byte, 8> const& p_raw_req)
    : raw_request_bytes(p_raw_req)
  {
  }

  constexpr bool operator==(setup_packet const& rhs) const
  {
    return std::ranges::equal(raw_request_bytes, rhs.raw_request_bytes);
  }

  /**
   * @brief Extract the request type from bmRequestType field
   *
   * Parses bits 6-5 of the bmRequestType byte to determine if this is a
   * standard, class, or vendor-specific request.
   *
   * @return type The request type, or type::invalid if bits indicate reserved
   * value
   */
  [[nodiscard]] constexpr request_type get_type() const
  {
    u8 const t = (bm_request_type() >> 5) & 0b11;
    if (t > 2) {
      return request_type::invalid;
    }

    return static_cast<request_type>(t);
  }

  /**
   * @brief Extract the request recipient from bmRequestType field
   *
   * Parses bits 4-0 of the bmRequestType byte to determine the target
   * component for this request.
   *
   * @return recipient The request recipient, or recipient::invalid if bits
   * indicate reserved value
   */
  [[nodiscard]] constexpr request_recipient get_recipient() const
  {
    u8 const r = bm_request_type() & 0b1111;
    if (r > 2) {
      return request_recipient::invalid;
    }

    return static_cast<request_recipient>(r);
  }

  /**
   * @brief Check the data phase direction of the request
   *
   * Examines bit 7 of bmRequestType to determine the direction of data
   * transfer in the data phase (if wLength > 0).
   *
   * @return true if data flows from device to host (IN direction)
   * @return false if data flows from host to device (OUT direction)
   */
  [[nodiscard]] constexpr bool is_device_to_host() const
  {
    return (bm_request_type() & (1 << 7)) != 0;
  }

  [[nodiscard]] constexpr u8 bm_request_type() const
  {
    return raw_request_bytes[0];
  }

  [[nodiscard]] constexpr u8 request() const
  {
    return raw_request_bytes[1];
  }

  /**
   * @brief Get wValue field in native endianness for program use
   */
  [[nodiscard]] constexpr u16 value() const
  {
    return from_le_bytes(raw_request_bytes[value_offset],
                         raw_request_bytes[value_offset + 1]);
  }

  /**
   * @brief Get wValue field as raw LE bytes for USB bus transmission
   */
  [[nodiscard]] constexpr std::span<hal::byte const> value_bytes() const
  {
    return std::span(raw_request_bytes).subspan(value_offset, sizeof(u16));
  }

  /**
   * @brief Get wIndex field in native endianness for program use
   */
  [[nodiscard]] constexpr u16 index() const
  {
    return from_le_bytes(raw_request_bytes[index_offset],
                         raw_request_bytes[index_offset + 1]);
  }

  /**
   * @brief Get wIndex field as raw LE bytes for USB bus transmission
   */
  [[nodiscard]] constexpr std::span<hal::byte const> index_bytes() const
  {
    return std::span(raw_request_bytes).subspan(index_offset, sizeof(u16));
  }

  /**
   * @brief Get wLength field in native endianness for program use
   */
  [[nodiscard]] constexpr u16 length() const
  {
    return from_le_bytes(raw_request_bytes[length_offset],
                         raw_request_bytes[length_offset + 1]);
  }

  /**
   * @brief Get wLength field as raw LE bytes for USB bus transmission
   */
  [[nodiscard]] constexpr std::span<hal::byte const> length_bytes() const
  {
    return std::span(raw_request_bytes).subspan(length_offset, sizeof(u16));
  }

  /**
   * @brief Set wValue field from native endianness value (stored as LE)
   */
  constexpr void value(u16 p_value)
  {
    set_le_u16<value_offset>(p_value);
  }

  /**
   * @brief Set wIndex field from native endianness value (stored as LE)
   */
  constexpr void index(u16 p_index)
  {
    set_le_u16<index_offset>(p_index);
  }

  /**
   * @brief Set wLength field from native endianness value (stored as LE)
   */
  constexpr void length(u16 p_length)
  {
    set_le_u16<length_offset>(p_length);
  }

  /**
  @brief Emplace a 16 bit value into the internal setup_packet array at a given
  offset in little endian form.

  @tparam offset - The offset into the setup packet array (Array after the
  offset needs to be at least two bytes)

  @param p_value - 16-bit value to emplace
   */
  template<usize offset>
  constexpr void set_le_u16(u16 p_value)
  {
    static_assert(offset < 7,
                  "Offset greater than size of setup bytes, need at least two "
                  "bytes to emplace the u16");
    static_assert(0 == (offset & 0b1),
                  "Offset must be even number (2-byte/16-bit aligned)");

    raw_request_bytes[offset + 0] = static_cast<hal::byte>(p_value & 0xFF);
    raw_request_bytes[offset + 1] =
      static_cast<hal::byte>((p_value >> 8) & 0xFF);
  }

  /**
   * @brief Convert two bytes from little-endian to host byte order
   *
   * Helper function to combine two bytes in little-endian format into a 16-bit
   * value in host byte order. Used internally for parsing multi-byte fields
   * from raw USB data.
   *
   * @param p_first Low byte (least significant)
   * @param p_second High byte (most significant)
   * @return u16 Combined 16-bit value in host byte order
   */
  constexpr static u16 from_le_bytes(hal::byte p_first, hal::byte p_second)
  {
    return static_cast<u16>(p_second) << 8 | p_first;
  }

  /**
   * @brief Convert 16-bit value to little-endian byte array
   *
   * Helper function to convert a 16-bit value into two bytes in little-endian
   * format. Used when constructing USB descriptor data or response packets.
   *
   * @param p_value 16-bit value to convert
   * @return std::array<hal::byte, 2> Array with low byte first, high byte
   * second
   */
  [[nodiscard]] constexpr static std::array<hal::byte, 2> to_le_u16(u16 p_value)
  {
    return { static_cast<hal::byte>(p_value & 0xFF),
             static_cast<hal::byte>((p_value >> 8) & 0xFF) };
  }

  std::array<byte, 8> raw_request_bytes;
};

/**
 * @brief Standard USB request types as defined by the USB specification
 *
 * These request types are used in setup packets to perform standard USB
 * operations during device enumeration and configuration. Each request type
 * corresponds to a specific USB operation that all USB devices must support.
 */
enum class standard_request_types : hal::byte
{
  get_status = 0x00,
  clear_feature = 0x01,
  set_feature = 0x03,
  set_address = 0x05,
  get_descriptor = 0x06,
  set_descriptor = 0x07,
  get_configuration = 0x08,
  set_configuration = 0x09,
  get_interface = 0x0A,
  set_interface = 0x11,
  synch_frame = 0x12,
  invalid
};

/**
 * @brief Validates and converts a setup packet request to a standard request
 * type
 *
 * This function examines a USB setup packet and determines if it contains a
 * valid standard request. It performs validation on the request type and
 * request value to ensure they conform to the USB specification.
 *
 * The function validates:
 * - The request type is marked as 'standard' in the setup packet
 * - The request value is within the valid range for standard requests
 * - The request value is not 0x04 (which is reserved)
 *
 * @param p_packet The setup packet to analyze
 * @return standard_request_types The corresponding standard request type,
 *         or standard_request_types::invalid if the packet doesn't contain
 *         a valid standard request
 *
 */
[[nodiscard]] constexpr standard_request_types determine_standard_request(
  setup_packet p_packet)
{
  if (p_packet.get_type() != setup_packet::request_type::standard ||
      p_packet.request() == 0x04 || p_packet.request() > 0x12) {
    return standard_request_types::invalid;
  }

  return static_cast<standard_request_types>(p_packet.request());
}
/**
 * @brief Endpoint I/O interface for USB request handling
 *
 * This interface provides read and write access to endpoint data during USB
 * control transfer handling. It is passed to `interface` methods such as
 * `handle_request`, `write_descriptors`, and `write_string_descriptor` to
 * allow interface implementations to send or receive data during the data
 * phase of control transfers.
 *
 * The direction of data transfer depends on the setup packet:
 *
 * - Device-to-host (IN): Use `write()` to send response data to the host
 * - Host-to-device (OUT): Use `read()` to receive data sent by the host
 *
 * @throws hal::operation_not_permitted - If a new setup packet is received
 * before the current control transfer completes, this exception is thrown to
 * interrupt the data phase. Only the enumerator should handle this exception;
 * it must catch it and evaluate the new setup packet. If the underlying
 * control endpoint implementation does not support setup_packet detection,
 * this should not be thrown and the enumerator will have to make assumptions
 * about which packets are setup packets and which are not.
 */
class endpoint_io
{
public:
  /**
   * @brief Read data from the endpoint
   *
   * Reads data that was sent by the host during a host-to-device (OUT) control
   * transfer. The data is copied into the provided buffer from the endpoint.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_buffer - scatter span of byte buffers to fill with data from the
   * endpoint
   * @return async::future<usize> - completes with the number of bytes read into
   * the provided buffers. Value is 0 if there is no data available within the
   * endpoint.
   * @throws hal::operation_not_permitted - See class documentation.
   */
  async::future<usize> read(async::context& p_context,
                            scatter_span<byte> p_buffer)
  {
    return driver_read(p_context, p_buffer);
  }

  /**
   * @brief Write data to the endpoint
   *
   * Writes data to be sent to the host during a device-to-host (IN) control
   * transfer. The data is copied from the provided buffer to the endpoint.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_buffer - scatter span of const byte buffers containing data to
   * write to the endpoint
   * @return async::future<usize> - completes with the number of bytes written
   * from the provided buffers
   * @throws hal::operation_not_permitted - See class documentation.
   */
  async::future<usize> write(async::context& p_context,
                             scatter_span<byte const> p_buffer)
  {
    return driver_write(p_context, p_buffer);
  }

  virtual ~endpoint_io() = default;

private:
  virtual async::future<usize> driver_read(async::context& p_context,
                                           scatter_span<byte> p_buffer) = 0;
  virtual async::future<usize> driver_write(
    async::context& p_context,
    scatter_span<byte const> p_buffer) = 0;
};

/**
 * @brief USB Interface class for implementing specific USB device functionality
 *
 * This class represents a USB interface, which defines a specific function or
 * capability of a USB device. Examples include HID devices (keyboards, mice),
 * audio devices, storage devices, or communication
 * devices.
 *
 * A USB interface is designed to be anything under a configuration, typically
 * this will be at the interface level but could also be multiple USB interface
 * descriptors that are associated for a given functionality.
 *
 * It is responsible for:
 * - Providing its own descriptors during enumeration
 * - Managing string descriptors for its functionality
 * - Handling interface-specific and endpoint-specific USB requests
 * - Implementing the specific protocol or behavior of the interface type
 *
 */
class interface
{
public:
  /**
   * @brief Encapsulates the number of interface and string descriptors
   *
   * This structure is returned by write_descriptors() to inform the USB
   * configuration how many descriptors were written and how many resources
   * (interface numbers and string indices) were consumed.
   */
  struct descriptor_count
  {
    /// Number of interface descriptors written by this interface
    u8 interface;

    /// Number of string descriptors provided by this interface
    u8 string;
    constexpr bool operator<=>(descriptor_count const& rhs) const = default;
  };

  /**
   * @brief Starting indices for interface numbers and string descriptors
   *
   * This structure is passed to write_descriptors() to inform the interface
   * what numbering it should use for its descriptors. The interface must use
   * sequential numbering starting from these values.
   */
  struct descriptor_start
  {
    /// Defines the starting index value for this interface. For example, if
    /// this usb interface contains two sub interfaces, then the interface
    /// number should be `interface` and the second should be `interface + 1`.
    std::optional<u8> interface;

    /// Defines the starting index value for this interface's strings. For
    /// example, if this usb interface has 3 strings, then the starting ID for
    /// those strings would be this value. String 1 would be this value, then
    /// string 2 would be `value + 1` and string 3 would be `value + 2`.
    std::optional<u8> string;

    constexpr bool operator<=>(descriptor_start const& rhs) const = default;
  };

  /**
   * @brief Writes descriptors that this interface is responsible for via the
   * endpoint I/O interface. This function may be called multiple times during
   * (re)enumeration.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_start - the starting values for interface numbers and string
   * indexes. The `string` field should be cached by the interface in order to
   * allow `write_string_descriptor` to work correctly.
   * @param p_endpoint - endpoint I/O interface used to write descriptor data to
   * the host.
   * @return async::future<descriptor_count> - completes with the descriptor
   * count
   */
  [[nodiscard]] async::future<descriptor_count> write_descriptors(
    async::context& p_context,
    descriptor_start p_start,
    endpoint_io& p_endpoint)
  {
    return driver_write_descriptors(p_context, p_start, p_endpoint);
  }

  /**
   * @brief Write a specific string descriptor
   *
   * Invoked to write a string given a specific index that the interface is
   * responsible for. The string will be written in the encoding of UTF-16LE.
   *
   * The interface must have evaluated all string indexes by invoking
   * write_descriptors() to properly identify its string descriptors.
   * String descriptors use the USB string descriptor format with length
   * and descriptor type fields.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_index - Which string index's descriptor should be written.
   * @param p_endpoint - endpoint I/O interface used to write the string
   * descriptor to the host.
   *
   * @return async::future<bool> - completes with true if the string was located
   * and written via the endpoint I/O.
   * @returns false - if the string requested does not belong to this interface.
   */
  [[nodiscard]] async::future<bool> write_string_descriptor(
    async::context& p_context,
    u8 p_index,
    endpoint_io& p_endpoint)
  {
    return driver_write_string_descriptor(p_context, p_index, p_endpoint);
  }
  /**
   * @brief Handle USB requests directed to this interface or its endpoints
   *
   * This method is called when the USB device receives a setup packet that may
   * target this interface (interface-specific requests) or one of its
   * endpoints (endpoint-specific requests). The interface should examine
   * the setup packet and handle any requests it recognizes. If the setup packet
   * is not for this interface, this API must return false.
   *
   * The interface must handle:
   *
   * - Standard requests specific to this interface type
   * - Class-specific requests defined by the interface's USB class
   * - Vendor-specific requests for custom functionality
   * - Endpoint-specific requests for its endpoints
   *
   * The `endpoint_io` object is constructed by the enumerator to provide
   * limited access to the control endpoint.
   *
   * If the request has a data phase (wLength > 0), use the endpoint I/O
   * interface to send data from device-to-host via the `write` API or
   * receive data from host-to-device via the `read` API.
   *
   * When a new setup packet is received from the HOST, the enumerator ensures
   * that `hal::operation_not_permitted` is thrown from all `endpoint_io` APIs.
   * Implementations of `driver_handle_request` MUST NOT catch this exception;
   * it must propagate to the enumerator for handling. Correct use of RAII to
   * undo state changes is advised. If cleanup requires a catch block, the
   * exception must be rethrown via `throw;` before exiting.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_setup - Setup request from the host.
   * @param p_endpoint - endpoint I/O interface for reading or writing data to
   * the host via the control endpoint during the data phase of the control
   * transfer.
   * @return async::future<bool> - completes with true if the request was
   * handled by the interface.
   * @return false - if the request could not be handled by interface.
   * @throws hal::operation_not_permitted - If a new setup packet is received
   * before the current control transfer completes. This exception must not be
   * caught by the implementation; it is intended for the enumerator.
   */
  async::future<bool> handle_request(async::context& p_context,
                                     setup_packet const& p_setup,
                                     endpoint_io& p_endpoint)
  {
    return driver_handle_request(p_context, p_setup, p_endpoint);
  }
  /**
   * @brief Suspend until the next host event is dispatched by the enumerator.
   *
   * Multiple coroutines may concurrently await this function. All registered
   * waiters are unblocked when the next host event arrives. If the
   * implementation's internal waiter capacity is exceeded, the caller will
   * block by sync until a slot becomes available.
   *
   * Implementations that need to react to power transitions, suspend/resume, or
   * bus reset should await this method in a loop.
   *
   * `host_event::setup_packet` and `host_event::data_packet` are consumed
   * internally by the enumerator and are never forwarded here.
   *
   * **Event-specific guidance:**
   *
   * - @ref host_event::reset — Discard all pending transfer state. The
   *   enumerator will re-run enumeration and re-initialize all descriptors
   *   before normal operation resumes.
   *
   * - @ref host_event::enumerated — The enumerator has completed enumeration
   *   and is ready for normal operation. Interfaces may begin queuing data to
   *   their endpoints.
   *
   * - @ref host_event::suspend_with_wakeup — The bus has entered L2/U3
   *   suspend and the host has granted remote wakeup capability. The interface
   *   may trigger a wakeup by writing to any IN endpoint; the endpoint driver
   *   handles remote wakeup signaling internally.
   *
   * - @ref host_event::suspend_without_wakeup — The bus has entered L2/U3
   *   suspend without remote wakeup capability. The interface must not attempt
   *   to initiate wakeup. Any IN endpoint write attempted during this state
   *   will throw hal::operation_not_permitted, as the device has no means to
   *   deliver data or signal the host to resume.
   *
   * - @ref host_event::resume — The bus has returned to L0/U0 active state
   *   from any sleep or suspend condition. Normal endpoint operation may
   *   resume.
   *
   * - @ref host_event::sleep — The host has granted an L1 LPM sleep request.
   *   Cease queuing data to endpoints. Any pending IN writes will block until
   *   the bus resumes.
   *
   * - @ref host_event::u1_sleep / @ref host_event::u2_sleep — USB 3.0+ U1/U2
   *   link power states entered. Behavior mirrors `sleep` for interface
   *   purposes.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<host_event> - completes with the @ref host_event
   *         that occurred
   */
  async::future<host_event> on_host_event(async::context& p_context)
  {
    return driver_on_host_event(p_context);
  }

  virtual ~interface() = default;

private:
  virtual async::future<descriptor_count> driver_write_descriptors(
    async::context& p_context,
    descriptor_start p_start,
    endpoint_io& p_endpoint) = 0;
  virtual async::future<bool> driver_write_string_descriptor(
    async::context& p_context,
    u8 p_index,
    endpoint_io& p_endpoint) = 0;
  virtual async::future<bool> driver_handle_request(
    async::context& p_context,
    setup_packet const& p_setup,
    endpoint_io& p_endpoint) = 0;
  virtual async::future<host_event> driver_on_host_event(
    async::context& p_context) = 0;
};
}  // namespace hal::inline v5::usb

namespace hal::usb {
using hal::usb::bulk_in_endpoint;
using hal::usb::bulk_out_endpoint;
using hal::usb::control_endpoint;
using hal::usb::endpoint;
using hal::usb::endpoint_info;
using hal::usb::endpoint_io;
using hal::usb::in_endpoint;
using hal::usb::in_endpoint_type;
using hal::usb::interface;
using hal::usb::interrupt_in_endpoint;
using hal::usb::interrupt_out_endpoint;
using hal::usb::out_endpoint;
using hal::usb::out_endpoint_type;
using hal::usb::setup_packet;
using hal::usb::standard_request_types;
}  // namespace hal::usb
