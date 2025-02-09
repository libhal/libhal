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

#include <span>

#include "../functional.hpp"
#include "../units.hpp"

namespace hal::experimental {

/**
 * @brief Basic information about an usb endpoint
 *
 */
struct usb_endpoint_info
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
    return number & (1 << 8);
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
 * `hal::usb_interface` interface to indicate to the HOST usb controller an
 * error on that endpoint or to signal to the HOST that the endpoint is no
 * longer available.
 *
 * The `info()` API can also be used by users in order to log information about
 * endpoints if that is useful to their application and testing.
 */
class usb_endpoint
{
public:
  virtual ~usb_endpoint() = default;

  /**
   * @brief Get info about this endpoint
   *
   * @return usb_endpoint_info - endpoint information
   */
  [[nodiscard]] usb_endpoint_info info() const
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

private:
  virtual usb_endpoint_info driver_info() const = 0;
  virtual void driver_stall(bool p_should_stall) = 0;
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
class usb_control_endpoint : public usb_endpoint
{
public:
  struct on_request_tag
  {};

  virtual ~usb_control_endpoint() = default;
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
   * @param p_should_connect
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
   * @param p_address The address to set for the USB device
   */
  void set_address(u8 p_address)
  {
    driver_set_address(p_address);
  }

  /**
   * @brief Write data to the control endpoint
   *
   * Used to send data from the device to the host over the control endpoint.
   * If the data is more than the size of the control IN endpoint, then the
   * transfer will be sent to the HOST in chunks based on the endpoint size. If
   * the size of the data transferred is divisible by the endpoint chunk size an
   * additional zero-length-packet will be sent to the HOST to denote the end of
   * data.
   *
   * @param p_data The data to be written
   */
  void write(std::span<byte const> p_data)
  {
    driver_write(p_data);
  }

  /**
   * @brief Set a callback function for incoming USB requests
   *
   * Used to handle incoming USB requests on the control endpoint.
   *
   * @param p_callback The callback function to be called when a request is
   * received
   */
  void on_request(callback<void(on_request_tag)> p_callback)
  {
    driver_on_request(p_callback);
  }

  /**
   * @brief Read 8 bytes of the USB request data from the control endpoint
   *
   * The read operation is set to 8 bytes because standard USB requests are
   * limited to a size of 8 bytes. The data stages for standard USB interfaces
   * is also limited to a max of 7 bytes which comes from CDC-ACM.
   *
   * If the read occurs for the data stage of a USB request, the number of bytes
   * will be smaller than 8, thus the remaining bytes will be set to the value
   * 0.
   *
   * @return std::optional<std::array<u8, 8>> - the 8 bytes of the USB request
   * or data stage. Returns `std::nullopt` if there is no data available in the
   * endpoint.
   */
  [[nodiscard]] std::optional<std::array<u8, 8>> read()
  {
    return driver_read();
  }

private:
  virtual void driver_connect(bool p_should_connect) = 0;
  virtual void driver_set_address(u8 p_address) = 0;
  virtual void driver_write(std::span<byte const> p_data) = 0;
  virtual void driver_on_request(callback<void(on_request_tag)> p_callback) = 0;
  virtual std::optional<std::array<u8, 8>> driver_read() = 0;
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
class usb_interrupt_in_endpoint : public usb_endpoint
{
public:
  virtual ~usb_interrupt_in_endpoint() = default;
  /**
   * @brief Write data to the interrupt IN endpoint
   *
   * Used to send data from the device to the host over an interrupt IN
   * endpoint.
   *
   * @param p_data The data to be written
   */
  void write(std::span<byte const> p_data)
  {
    driver_write(p_data);
  }

private:
  virtual void driver_write(std::span<byte const> p_data) = 0;
};

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
class usb_interrupt_out_endpoint : public usb_endpoint
{
public:
  struct on_receive_tag
  {};

  virtual ~usb_interrupt_out_endpoint() = default;

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
  void on_receive(callback<void(on_receive_tag)> p_callback)
  {
    driver_on_receive(p_callback);
  }

  /**
   * @brief Read contents of endpoint
   *
   * This function is callable from within the `on_receive` callback, meaning
   * this API should be callable within that interrupt service routine.
   *
   * When data is available in the endpoint, the endpoint will NAK all following
   * HOST commands to send more data. When all data from the endpoint has been
   * read, the endpoint will become valid again and can ACK the HOST packets.
   *
   * If a user of this interface wants to drain all of the data from the
   * endpoint, then the application interface should continually pass read
   * content from the endpoint until a result is size zero.
   *
   * @param p_buffer - buffer to fill with data
   * @return std::span<u8 const> - the same buffer that was passed into the read
   * function but with its size equal to the number of bytes read from the OUT
   * endpoint. The size will be 0 if no more data was present in the endpoint.
   */
  [[nodiscard]] std::span<u8 const> read(std::span<u8> p_buffer)
  {
    return driver_read(p_buffer);
  }

private:
  virtual void driver_on_receive(callback<void(on_receive_tag)> p_callback) = 0;
  virtual std::span<u8 const> driver_read(std::span<u8> p_buffer) = 0;
};

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
class usb_bulk_in_endpoint
{
public:
  virtual ~usb_bulk_in_endpoint() = default;
  /**
   * @brief Write data to the bulk IN endpoint
   *
   * Used to send data from the device to the host over a bulk IN endpoint.
   *
   * @param p_data The data to be written
   */
  void write(std::span<byte const> p_data)
  {
    driver_write(p_data);
  }

private:
  virtual void driver_write(std::span<byte const> p_data) = 0;
};

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
class usb_bulk_out_endpoint
{
public:
  struct on_receive_tag
  {};

  virtual ~usb_bulk_out_endpoint() = default;

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
  void on_receive(callback<void(on_receive_tag)> p_callback)
  {
    driver_on_receive(p_callback);
  }

  /**
   * @brief Read contents of endpoint
   *
   * This function is callable from within the `on_receive` callback, meaning
   * this API should be callable within that interrupt service routine.
   *
   * When data is available in the endpoint, the endpoint will NAK all following
   * HOST commands to send more data. When all data from the endpoint has been
   * read, the endpoint will become valid again and can ACK the HOST packets.
   *
   * If a user of this interface wants to drain all of the data from the
   * endpoint, then the application interface should continually pass read
   * content from the endpoint until a result is size zero.
   *
   * @param p_buffer - buffer to fill with data
   * @return std::span<u8 const> - the same buffer that was passed into the read
   * function but with its size equal to the number of bytes read from the OUT
   * endpoint. The size will be 0 if no more data was present in the endpoint.
   */
  [[nodiscard]] std::span<u8 const> read(std::span<u8> p_buffer)
  {
    return driver_read(p_buffer);
  }

private:
  virtual void driver_on_receive(callback<void(on_receive_tag)> p_callback) = 0;
  virtual std::span<u8 const> driver_read(std::span<u8> p_buffer) = 0;
};
}  // namespace hal::experimental
