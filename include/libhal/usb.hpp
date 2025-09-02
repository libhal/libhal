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
  void on_receive(hal::callback<void(on_receive_tag)> const& p_callback)
  {
    driver_on_receive(p_callback);
  }

private:
  virtual void driver_connect(bool p_should_connect) = 0;
  virtual void driver_set_address(u8 p_address) = 0;
  virtual void driver_write(scatter_span<byte const> p_data) = 0;
  virtual usize driver_read(scatter_span<byte> p_buffer) = 0;
  virtual void driver_on_receive(
    callback<void(on_receive_tag)> const& p_callback) = 0;
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
 */
struct setup_packet
{
  enum class type : hal::byte
  {
    standard = 0,
    class_t = 1,
    vendor = 2,
    invalid
  };

  enum class recipient : hal::byte
  {
    device = 0,
    interface = 1,
    endpoint = 2,
    invalid
  };

  [[nodiscard]] constexpr type get_type() const
  {
    u8 const t = request_type & (0b11 << 5);
    if (t > 2) {
      return type::invalid;
    }

    return static_cast<type>(t);
  }

  [[nodiscard]] constexpr recipient get_recipient() const
  {
    u8 const r = request_type & 0b1111;
    if (r > 2) {
      return recipient::invalid;
    }

    return static_cast<recipient>(r);
  }

  [[nodiscard]] constexpr bool is_device_to_host() const
  {
    return request_type & 1 << 7;
  }

  constexpr bool operator<=>(setup_packet const& rhs) const = default;

  constexpr setup_packet() = default;

  constexpr setup_packet(u8 p_request_type,  // NOLINT
                         u8 p_request,
                         u16 p_value,
                         u16 p_index,
                         u16 p_length)
    : request_type(p_request_type)
    , request(p_request)
    , value(p_value)
    , index(p_index)
    , length(p_length) {};

  constexpr setup_packet(bool p_device_to_host,
                         type p_type,
                         recipient p_recipient,  // NOLINT
                         u8 p_request,           // NOLINT
                         u16 p_value,
                         u16 p_index,
                         u16 p_length)
    : request_type((p_device_to_host << 7) | (static_cast<byte>(p_type) << 5) |
                   static_cast<byte>(p_recipient))
    , request(p_request)
    , value(p_value)
    , index(p_index)
    , length(p_length) {};

  constexpr setup_packet(std::span<byte> raw_req)
    : request_type(raw_req[0])
    , request(raw_req[1])
    , value(from_le_bytes(raw_req[2], raw_req[3]))
    , index(from_le_bytes(raw_req[4], raw_req[5]))
    , length(from_le_bytes(raw_req[6], raw_req[7]))

  {
  }

  constexpr static u16 from_le_bytes(hal::byte& first, hal::byte& second)
  {
    return static_cast<u16>(second) << 8 | first;
  }

  constexpr static std::array<hal::byte, 2> to_le_bytes(u16 n)
  {
    return { static_cast<hal::byte>(n & 0xFF),
             static_cast<hal::byte>((n & 0xFF << 8) >> 8) };
  }

  u8 request_type;
  u8 request;
  u16 value;
  u16 index;
  u16 length;
};

// TODO: Document
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

// TODO: Document
[[nodiscard]] constexpr standard_request_types determine_standard_request(
  setup_packet pkt)
{
  if (pkt.get_type() != setup_packet::type::standard || pkt.request == 0x04 ||
      pkt.request > 0x12) {
    return standard_request_types::invalid;
  }

  return static_cast<standard_request_types>(pkt.request);
}

/**
 * @brief A class representing a usb interface
 *
 * Examples of this could be a HID device, a microphone, a storage device, etc
 * Is to be paired with a given USB peripheral device and a configuration during
 * enumeration.
 *
 * TODO(#164): tech debt, clean up API descriptions
 */
class interface
{
public:
  using endpoint_writer = hal::callback<void(scatter_span<hal::byte const>)>;

  /**
   * @brief Encapsulates the number of interfaces and strings this usb interface
   * contains.
   */
  struct descriptor_count
  {
    u8 interface;
    u8 string;
    constexpr bool operator<=>(descriptor_count const& rhs) const = default;
  };

  /**
   * @brief Encapsulates the starting indexes for which this interface must use
   * for its interface descriptor.
   */
  struct descriptor_start
  {
    /// Defines the starting index value for this interface. For example, if
    /// this usb interface contains two sub interfaces, then the interface
    /// number should be `interface` and the second should be `interface + 1`.
    u8 interface;

    /// Defines the starting index value for this interface's strings. For
    /// example, if this usb interface has 3 strings, then the starting ID for
    /// those strings would be this value. String 1 would be this value, then
    /// string 2 would be `value + 1` and string 3 would be `value + 2`.
    u8 string;

    constexpr bool operator<=>(descriptor_start const& rhs) const = default;
  };

  /**
   * @brief Writes descriptors that this interface is responsible for via a
   * callback. This function may be called multiple times during
   * (re)enumeration.
   *
   * @param p_callback - A hal::callback that returns void and takes a span of
   * const bytes as a parameter, that span is where the descriptor stream will
   * be written to.
   * @param p_start - the starting values for interface numbers and string
   * indexes. The `string` field should be cached by the interface in order to
   * allow `write_string_descriptor` to work correctly.
   */
  [[nodiscard]] descriptor_count write_descriptors(
    descriptor_start p_start,
    endpoint_writer const& p_callback)
  {
    return driver_write_descriptors(p_start, p_callback);
  }

  /**
   * @brief Write string descriptor
   *
   * All usb interfaces MUST default initialize their starting string descriptor
   * index to 1 and update it based on the `write_descriptors()` function.
   *
   * @param p_index - Which string index's descriptor should be written.
   * @param p_callback - A callback used to write the string descriptor.
   *
   * @returns true - if the string was located and written over the
   * endpoint_writer.
   * @returns false - if the string requested does not belong to this interface.
   */
  [[nodiscard]] bool write_string_descriptor(u8 p_index,
                                             endpoint_writer const& p_callback)
  {
    return driver_write_string_descriptor(p_index, p_callback);
  }

  /**
   * @brief Generic handler for requests targeting the interface or below level
   * of a USB device. These could be standard requests or custom requests.
   *
   * @param p_setup - Setup request from the host.
   * @param p_callback - The callback to write out the response to the request
   * to the host if the setup has a data phase.
   * @return true - if the request was handled by the interface.
   * @return false - if the request could not be handled by interface.
   */
  bool handle_request(setup_packet const& p_setup,
                      endpoint_writer const& p_callback)
  {
    return driver_handle_request(p_setup, p_callback);
  }

  virtual ~interface() = default;

private:
  virtual descriptor_count driver_write_descriptors(
    descriptor_start p_start,
    endpoint_writer const& p_callback) = 0;
  virtual bool driver_write_string_descriptor(
    u8 p_index,
    endpoint_writer const& p_callback) = 0;
  virtual bool driver_handle_request(setup_packet const& p_setup,
                                     endpoint_writer const& p_callback) = 0;
};
}  // namespace hal::v5::usb
