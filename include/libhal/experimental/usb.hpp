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

#include <cstdint>
#include <span>

#include "../functional.hpp"
#include "../units.hpp"

namespace hal::experimental {
/**
 * @brief USB Control Endpoint Interface
 *
 * This class represents the control endpoint of a USB device. The control
 * endpoint is crucial for USB communication as it handles device enumeration,
 * configuration, and general control operations.
 *
 * Use cases:
 * - Initiating USB connections
 * - Handling USB enumeration process
 * - Setting device addresses
 * - Responding to standard USB requests
 * - Sending and receiving control data
 *
 */
class usb_control_endpoint
{
public:
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
   * Used to set the device address during the USB enumeration process.
   *
   * @param p_address The address to set for the USB device
   */
  void set_address(std::uint8_t p_address)
  {
    driver_set_address(p_address);
  }
  /**
   * @brief Write data to the control endpoint
   *
   * Used to send data from the device to the host over the control endpoint.
   *
   * @param p_data The data to be written
   */
  void write(std::span<hal::byte const> p_data)
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
  void on_request(hal::callback<void(std::span<hal::byte>)> p_callback)
  {
    driver_on_request(p_callback);
  }

private:
  virtual void driver_connect(bool p_should_connect) = 0;
  virtual void driver_set_address(std::uint8_t p_address) = 0;
  virtual void driver_write(std::span<hal::byte const> p_data) = 0;
  virtual void driver_on_request(
    hal::callback<void(std::span<hal::byte>)> p_callback) = 0;
};

/**
 * @brief USB Interrupt IN Endpoint Interface
 *
 * This class represents an interrupt IN endpoint of a USB device. Interrupt IN
 * endpoints are used for small, time-sensitive data transfers from the device
 * to the host.
 *
 * Use cases:
 * - Sending periodic status updates
 * - Transmitting small amounts of data with guaranteed latency
 * - Ideal for devices like keyboards, mice, or game controllers
 */
class usb_interrupt_in_endpoint
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
  void write(std::span<hal::byte const> p_data)
  {
    driver_write(p_data);
  }

  /**
   * @brief The number of the endpoint
   *
   * Used during enumeration to tell the host which endpoints are used for
   * specific functionality.
   *
   * @return hal::u8 - the endpoint number
   */
  hal::u8 number()
  {
    return driver_number();
  }

private:
  virtual void driver_write(std::span<hal::byte const> p_data) = 0;
  virtual hal::u8 driver_number() = 0;
};

/**
 * @brief USB Interrupt OUT Endpoint Interface
 *
 * This class represents an interrupt OUT endpoint of a USB device. Interrupt
 * OUT endpoints are used for small, time-sensitive data transfers from the host
 * to the device.
 *
 * Use cases:
 * - Receiving periodic commands or settings
 * - Handling small amounts of data with guaranteed latency
 * - Ideal for devices that need quick responses to host commands
 */
class usb_interrupt_out_endpoint
{
public:
  virtual ~usb_interrupt_out_endpoint() = default;

  /**
   * @brief Set a callback function for receiving data on the interrupt OUT
   * endpoint
   *
   * Used to handle incoming data on a interrupt OUT endpoint.
   *
   * @param p_callback The callback function to be called when data is received
   */
  void on_receive(hal::callback<void(std::span<hal::byte>)> p_callback)
  {
    driver_on_receive(p_callback);
  }

  /**
   * @brief The number of the endpoint
   *
   * Used during enumeration to tell the host which endpoints are used for
   * specific functionality.
   *
   * @return hal::u8 - the endpoint number
   */
  hal::u8 number()
  {
    return driver_number();
  }

private:
  virtual void driver_on_receive(
    hal::callback<void(std::span<hal::byte>)> p_callback) = 0;
  virtual hal::u8 driver_number() = 0;
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
  void write(std::span<hal::byte const> p_data)
  {
    driver_write(p_data);
  }

  /**
   * @brief The number of the endpoint
   *
   * Used during enumeration to tell the host which endpoints are used for
   * specific functionality.
   *
   * @return hal::u8 - the endpoint number
   */
  hal::u8 number()
  {
    return driver_number();
  }

private:
  virtual void driver_write(std::span<hal::byte const> p_data) = 0;
  virtual hal::u8 driver_number() = 0;
};

/**
 * @brief USB Bulk OUT Endpoint Interface
 *
 * This class represents a bulk OUT endpoint of a USB device. Bulk OUT endpoints
 * are used for large, non-time-critical data transfers from the host to the
 * device.
 *
 * Use cases:
 * - Receiving large amounts of data
 * - Handling data when timing is not critical
 * - Ideal for devices like printers, scanners, or external storage
 */
class usb_bulk_out_endpoint
{
public:
  virtual ~usb_bulk_out_endpoint() = default;
  /**
   * @brief Set a callback function for receiving data on the bulk OUT endpoint
   *
   * Used to handle incoming data on a bulk OUT endpoint.
   *
   * @param p_callback The callback function to be called when data is received
   */
  void on_receive(hal::callback<void(std::span<hal::byte>)> p_callback)
  {
    driver_on_receive(p_callback);
  }

  /**
   * @brief The number of the endpoint
   *
   * Used during enumeration to tell the host which endpoints are used for
   * specific functionality.
   *
   * @return hal::u8 - the endpoint number
   */
  hal::u8 number()
  {
    return driver_number();
  }

private:
  virtual void driver_on_receive(
    hal::callback<void(std::span<hal::byte>)> p_callback) = 0;
  virtual hal::u8 driver_number() = 0;
};
}  // namespace hal::experimental
