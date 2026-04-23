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

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>

#include "functional.hpp"
#include "scatter_span.hpp"
#include "units.hpp"

namespace hal::v5::usb {
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
 * @brief USB bus-level events forwarded to @ref interface implementations.
 *
 * This is the subset of @ref host_event that is relevant to interface
 * drivers. The enumerator translates @ref host_event values and forwards
 * the appropriate host_event to all active interfaces via
 * @ref interface::on_host_event.
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

  /// @brief A SETUP packet is available in the control endpoint buffer.
  ///
  /// The enumerator should call @ref control_endpoint::read to retrieve
  /// the 8-byte setup packet and process the request.
  setup_packet,

  /// @brief A data OUT packet is available in the control endpoint buffer.
  ///
  /// Delivered during the data phase of a host-to-device control transfer,
  /// after a setup packet with wLength > 0 has been processed. The
  /// enumerator or active interface should call @ref control_endpoint::read
  /// to retrieve the data.
  data_packet,

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
   * @param p_should_stall - set to true to stall this endpoint, set to false to
   * un-stall the endpoint.
   */
  void stall(bool p_should_stall)
  {
    return driver_stall(p_should_stall);
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
   */
  void reset()
  {
    return driver_reset();
  }

private:
  [[nodiscard]] virtual endpoint_info driver_info() const = 0;
  virtual void driver_stall(bool p_should_stall) = 0;
  virtual void driver_reset() = 0;
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
  struct on_receive_tag
  {};

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
   * @param p_should_connect - set to true to connect, set to false to
   * disconnect.
   */
  void connect(bool p_should_connect)
  {
    driver_connect(p_should_connect);
  }

  /**
   * @brief Set the USB device address
   *
   * Used to set the device address during the USB enumeration process. This
   * address must come from a USB request on the control endpoint by the HOST.
   *
   * @param p_address The address assigned to this device from the HOST
   */
  void set_address(u8 p_address)
  {
    driver_set_address(p_address);
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
   * usb.write({}); // Finishes the transfer with an ZLP or the last of the data
   * ```
   *
   * @param p_data - a scatter span of bytes to be written to the endpoint
   * memory and sent over USB.
   */
  void write(scatter_span<byte const> p_data)
  {
    driver_write(p_data);
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
   * @param p_buffer - a scatter span of bytes to fill with data
   * @return usize - the number of bytes read into the buffers provided by
   * p_buffer.
   */
  [[nodiscard]] usize read(scatter_span<byte> p_buffer)
  {
    return driver_read(p_buffer);
  }

  /**
   * @brief Set a callback function for when USB requests are received
   *
   * @param p_callback The callback function to be called when a USB request
   * command is received on the control endpoint.
   */
  void on_receive(callback<void(on_receive_tag)> const& p_callback)
  {
    driver_on_receive(p_callback);
  }

  /**
   * @brief Returns whether the endpoint's receive buffer contains a setup
   * packet.
   *
   * @deprecated Use on_host_event and the host_event type to detect if a setup
   * command has arrived.
   *
   * Returns `true` until all 8 bytes of the setup packet have been extracted
   * via `read()`. Returns `false` once the setup packet has been fully consumed
   * or if the last received packet was not a setup packet.
   *
   * Returns `std::nullopt` if the implementation does not support setup packet
   * detection. Enumerators that require this information should accept a
   * callable (as a template parameter or type-erased input parameter) to work
   * around legacy implementations.
   *
   * @note Safe to call from ISR context. Implementations must not throw,
   * call throwing functions, or perform I/O within this method.
   *
   * @return `std::nullopt` if setup packet detection is unsupported.
   * @return `true` if the receive buffer contains an unconsumed setup packet.
   * @return `false` otherwise.
   */
  [[deprecated("This API is deprecated in favor of on_host_event() with "
               "callback. To determine if the event means there is a new setup "
               "packet, check the host_event and see if it equals "
               "host_event::setup_packet.")]] [[nodiscard]] std::optional<bool>
  has_setup() const noexcept
  {
    return driver_has_setup();
  }

  /**
   * @brief Register a callback for control endpoint and bus-level events.
   *
   * The registered callback is invoked from interrupt context when any
   * @ref host_event occurs. The callback must not perform any blocking
   * operations.
   *
   * Replaces the @ref on_receive callbacks. Only one callback may be registered
   * at a time. Calling this function again replaces the previously registered
   * callback.
   *
   * The enumerator registers this callback internally. Applications should
   * not call this directly.
   *
   * This API does nothing if not implemented
   *
   * @param p_callback - callback to invoke when an event occurs, receives
   *                     the @ref host_event indicating which event fired.
   * @throws hal::operation_not_supported - if not implemented by driver
   */
  void on_host_event(hal::callback<void(host_event)> const& p_callback)
  {
    driver_on_host_event(p_callback);
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
   * This API does nothing if not implemented
   *
   * @param p_enabled - true if the host has granted remote wakeup permission,
   *                    false if the host has revoked it.
   */
  void set_remote_wakeup_enabled(bool p_enabled)
  {
    return driver_set_remote_wakeup_enabled(p_enabled);
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
   * the enumerator dispatches `host_event::sleep` to all registered interfaces.
   * Rejecting the request keeps the device in L0 (active) state and no event
   * is dispatched.
   *
   * This function is invoked exclusively by the enumerator and must not be
   * called directly by interface implementors or application code.
   *
   * This API does nothing if not implemented
   *
   * @param p_accept `true` to acknowledge the L1 sleep request and allow the
   *   device to enter L1; `false` to reject it and remain in L0.
   */
  void acknowledge_sleep(bool p_accept)
  {
    return driver_acknowledge_sleep(p_accept);
  }

  /**
   * @brief Query whether the hardware supports USB 2.0 L1 Link Power
   * Management.
   *
   * The enumerator calls this during enumeration to determine whether to
   * include a Binary Object Store (BOS) descriptor advertising LPM capability.
   * If this returns `false`, the BOS descriptor is omitted and the host will
   * never issue LPM tokens, meaning `acknowledge_sleep()` will never be called.
   *
   * Implementations should return a compile-time or hardware-register-derived
   * constant. The result must remain stable for the lifetime of the
   * `control_endpoint` object.
   *
   * @return `true` if the hardware supports L1 LPM and the enumerator should
   *   advertise BOS/LPM capability to the host; `false` otherwise. If not
   *   implemented by driver, then defaults to return false.
   */
  [[nodiscard]] bool supports_lpm()
  {
    return driver_supports_lpm();
  }

private:
  virtual void driver_connect(bool p_should_connect) = 0;
  virtual void driver_set_address(u8 p_address) = 0;
  virtual void driver_write(scatter_span<byte const> p_data) = 0;
  virtual usize driver_read(scatter_span<byte> p_buffer) = 0;
  virtual void driver_on_receive(
    callback<void(on_receive_tag)> const& p_callback) = 0;
  [[nodiscard]] virtual std::optional<bool> driver_has_setup() const noexcept
  {
    return {};
  }
  virtual void driver_on_host_event(callback<void(host_event)> const&)
  {
  }
  virtual void driver_set_remote_wakeup_enabled(bool)
  {
  }
  virtual void driver_acknowledge_sleep(bool)
  {
  }
  virtual bool driver_supports_lpm()
  {
    return false;
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
   * usb.write({}); // Finishes the transfer with an ZLP or the last of the data
   * ```
   *
   * @param p_data - a scatter span of bytes to be written to the endpoint
   * memory and sent over USB.
   * @throws hal::operation_not_permitted - if the USB is suspended and a write
   * is attempted.
   */
  void write(scatter_span<byte const> p_data)
  {
    driver_write(p_data);
  }

private:
  virtual void driver_write(scatter_span<byte const> p_data) = 0;
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
  struct on_receive_tag
  {};

  /**
   * @brief Set a callback for when data is available on the endpoint
   *
   * After this callback is called, the endpoint shall be set to NAK all
   * requests from the HOST until the contents are read via the read() API. Once
   * all data has been read from the endpoint, will this endpoint become valid
   * once again and can ACK the host if it wants to transmit more data.
   *
   * @param p_callback - The callback function to be called when data is
   * available in the endpoint.
   */
  void on_receive(callback<void(on_receive_tag)> const& p_callback)
  {
    driver_on_receive(p_callback);
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
   * @param p_buffer - buffer to fill with data
   * @return usize - the same buffer that was passed into the read
   * function but with its size equal to the number of bytes read from the OUT
   * endpoint. The size will be 0 if no more data was present in the endpoint.
   */
  [[nodiscard]] usize read(scatter_span<byte> p_buffer)
  {
    return driver_read(p_buffer);
  }

private:
  virtual void driver_on_receive(
    callback<void(on_receive_tag)> const& p_callback) = 0;
  virtual usize driver_read(scatter_span<byte> p_buffer) = 0;
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

  @param n - 16-bit value to emplace
   */
  template<usize offset>
  constexpr void set_le_u16(u16 n)
  {
    static_assert(offset < 7,
                  "Offset greater than size of setup bytes, need at least two "
                  "bytes to emplace the u16");
    static_assert(0 == (offset & 0b1),
                  "Offset must be even number (2-byte/16-bit aligned)");

    raw_request_bytes[offset + 0] = static_cast<hal::byte>(n & 0xFF);
    raw_request_bytes[offset + 1] = static_cast<hal::byte>((n >> 8) & 0xFF);
  }

  /**
   * @brief Convert two bytes from little-endian to host byte order
   *
   * Helper function to combine two bytes in little-endian format into a 16-bit
   * value in host byte order. Used internally for parsing multi-byte fields
   * from raw USB data.
   *
   * @param first Low byte (least significant)
   * @param second High byte (most significant)
   * @return u16 Combined 16-bit value in host byte order
   */
  constexpr static u16 from_le_bytes(hal::byte first, hal::byte second)
  {
    return static_cast<u16>(second) << 8 | first;
  }

  /**
   * @brief Convert 16-bit value to little-endian byte array
   *
   * Helper function to convert a 16-bit value into two bytes in little-endian
   * format. Used when constructing USB descriptor data or response packets.
   *
   * @param n 16-bit value to convert
   * @return std::array<hal::byte, 2> Array with low byte first, high byte
   * second
   */
  [[nodiscard]] constexpr static std::array<hal::byte, 2> to_le_u16(u16 n)
  {
    return { static_cast<hal::byte>(n & 0xFF),
             static_cast<hal::byte>((n >> 8) & 0xFF) };
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
   * @param p_buffer - scatter span of byte buffers to fill with data from the
   * endpoint
   * @return usize - the number of bytes read into the provided buffers. Value
   * is 0 if there is no data available within the endpoint.
   * @throws hal::operation_not_permitted - See class documentation.
   */
  usize read(scatter_span<byte> p_buffer)
  {
    return driver_read(p_buffer);
  }

  /**
   * @brief Write data to the endpoint
   *
   * Writes data to be sent to the host during a device-to-host (IN) control
   * transfer. The data is copied from the provided buffer to the endpoint.
   *
   * @param p_buffer - scatter span of const byte buffers containing data to
   * write to the endpoint
   * @return usize - the number of bytes written from the provided buffers
   * @throws hal::operation_not_permitted - See class documentation.
   */
  usize write(scatter_span<byte const> p_buffer)
  {
    return driver_write(p_buffer);
  }

  virtual ~endpoint_io() = default;

private:
  virtual usize driver_read(scatter_span<byte> p_buffer) = 0;
  virtual usize driver_write(scatter_span<byte const> p_buffer) = 0;
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
   * @param p_start - the starting values for interface numbers and string
   * indexes. The `string` field should be cached by the interface in order to
   * allow `write_string_descriptor` to work correctly.
   * @param p_endpoint - endpoint I/O interface used to write descriptor data to
   * the host.
   */
  [[nodiscard]] descriptor_count write_descriptors(descriptor_start p_start,
                                                   endpoint_io& p_endpoint)
  {
    return driver_write_descriptors(p_start, p_endpoint);
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
   * @param p_index - Which string index's descriptor should be written.
   * @param p_endpoint - endpoint I/O interface used to write the string
   * descriptor to the host.
   *
   * @returns true - if the string was located and written via the endpoint I/O.
   * @returns false - if the string requested does not belong to this interface.
   */
  [[nodiscard]] bool write_string_descriptor(u8 p_index,
                                             endpoint_io& p_endpoint)
  {
    return driver_write_string_descriptor(p_index, p_endpoint);
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
   * @param p_setup - Setup request from the host.
   * @param p_endpoint - endpoint I/O interface for reading or writing data to
   * the host via the control endpoint during the data phase of the control
   * transfer.
   * @return true - if the request was handled by the interface.
   * @return false - if the request could not be handled by interface.
   * @throws hal::operation_not_permitted - If a new setup packet is received
   * before the current control transfer completes. This exception must not be
   * caught by the implementation; it is intended for the enumerator.
   */
  bool handle_request(setup_packet const& p_setup, endpoint_io& p_endpoint)
  {
    return driver_handle_request(p_setup, p_endpoint);
  }
  /**
   * @brief Respond to bus-level host events dispatched by the enumerator.
   *
   * Called by the enumerator after it has processed a @ref host_event and
   * updated its own state. Implementations that need to react to power
   * transitions, suspend/resume, or bus reset should override this method.
   *
   * `host_event::setup_packet` and `host_event::data_packet` are consumed
   * internally by the enumerator and are never forwarded here.
   *
   * This method is called from the enumerator's main context, not from
   * interrupt context.
   *
   * **Event-specific guidance:**
   *
   * - @ref host_event::reset — Discard all pending transfer state. The
   *   enumerator will re-run enumeration and re-initialize all descriptors
   *   before normal operation resumes.
   *
   * - @ref host_event::enumerated — The device has completed enumeration and
   *   is ready for normal operation. Interfaces may begin queuing data to
   *   their endpoints.
   *
   * - @ref host_event::sleep — The host has granted an L1 LPM sleep request.
   *   Cease queuing data to endpoints. Any pending IN writes will block until
   *   the bus resumes.
   *
   * - @ref host_event::u1_sleep / @ref host_event::u2_sleep — USB 3.0+ U1/U2
   *   link power states entered. Behavior mirrors `sleep` for interface
   *   purposes.
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
   * The default implementation is a no-op.
   *
   * @param p_event The host event that occurred.
   */
  void handle_host_event(host_event p_event)
  {
    return driver_handle_host_event(p_event);
  }

  virtual ~interface() = default;

private:
  virtual descriptor_count driver_write_descriptors(
    descriptor_start p_start,
    endpoint_io& p_endpoint) = 0;
  virtual bool driver_write_string_descriptor(u8 p_index,
                                              endpoint_io& p_endpoint) = 0;
  virtual bool driver_handle_request(setup_packet const& p_setup,
                                     endpoint_io& p_endpoint) = 0;
  virtual void driver_handle_host_event(host_event)
  {
  }
};
}  // namespace hal::v5::usb

namespace hal::usb {
using v5::usb::bulk_in_endpoint;
using v5::usb::bulk_out_endpoint;
using v5::usb::control_endpoint;
using v5::usb::endpoint;
using v5::usb::endpoint_info;
using v5::usb::in_endpoint;
using v5::usb::in_endpoint_type;
using v5::usb::interrupt_in_endpoint;
using v5::usb::interrupt_out_endpoint;
using v5::usb::out_endpoint;
using v5::usb::out_endpoint_type;

using v5::usb::endpoint_io;
using v5::usb::interface;
using v5::usb::setup_packet;
using v5::usb::standard_request_types;
}  // namespace hal::usb
