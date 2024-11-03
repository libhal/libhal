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

#include "buffered_can.hpp"
#include "functional.hpp"

namespace hal {
/**
 * @brief Controller Area Network (CAN bus) hardware abstraction interface.
 *
 * This interface does not provide APIs for CAN message hardware filtering. The
 * hardware implementation for CAN message filter varies wildly across devices
 * and thus a common API is infeasible. So we rely on the concrete classes, the
 * implementations of this interface to provide APIs for setting the CAN filter
 * for the specific hardware. Hardware filtering is a best effort with the
 * resources available. It is often not possible to filter every possible ID in
 * hardware that your application is interested in. Thus, must expect that the
 * CAN message receive buffer.
 *
 * This interface provides a means to interrupt on every message received. If
 * this behavior is not desirable, consider `hal::buffered_can` instead.
 */
class can
{
public:
  /**
   * @brief Can message ID type trait
   *
   * This was added for backwards API compatibility.
   *
   */
  using id_t = can_id;

  /**
   * @brief Definition of settings using the common `hal::can_settings`
   *
   * This was added for backwards API compatibility.
   *
   */
  using settings = hal::can_settings;

  /**
   * @brief Definition of message_t using the common `hal::can_message`
   *
   * This was added for backwards API compatibility.
   *
   */
  using message_t = hal::can_message;

  /**
   * @brief Receive handler for can messages
   *
   */
  using handler = void(message_t const& p_message);

  /**
   * @brief Configure this can bus port to match the settings supplied
   *
   * @param p_settings - settings to apply to can driver
   * @throws hal::operation_not_supported - if the settings could not be
   * achieved.
   */
  void configure(settings const& p_settings)
  {
    driver_configure(p_settings);
  }

  /**
   * @brief Transition the CAN device from "bus-off" to "bus-on"
  @verbatim embed:rst
  ```{warning}
  Calling this function when the device is already in "bus-on" will
  have no effect. This function is not necessary to call after creating the
  CAN driver as the driver should already be "bus-on" on creation.
  ```
  @endverbatim
   *
   * Can devices have two counters to determine system health. These two
   * counters are the "transmit error counter" and the "receive error counter".
   * Transmission errors can occur when the device attempts to communicate on
   * the bus and either does not get an acknowledge or sees an unexpected or
   * erroneous signal on the bus during its own transmission. When transmission
   * errors reach 255 counts, the device will go into the "bus-off" state.
   *
   * In the "bus-off" state, the CAN peripheral can no longer communicate on the
   * bus. Any calls to `send()` will throw the error
   * `hal::operation_not_permitted`. If this occurs, this function must be
   * called to re-enable bus communication.
   *
   */
  void bus_on()
  {
    driver_bus_on();
  }

  /**
   * @brief Send a can message
   *
   * @param p_message - the message to be sent
   * @throws hal::operation_not_permitted - if the can device has entered the
   * "bus-off" state. This can happen if a critical fault in the bus has
   * occurred. A call to `bus_on()` will need to be issued to attempt to talk on
   * the bus again. See `bus_on()` for more details.
   */
  void send(message_t const& p_message)
  {
    driver_send(p_message);
  }

  /**
   * @brief Set the message reception handler
   *
   * All messages received before a message handler is installed are dropped.
   *
   * @param p_handler - this handler will be called when a message has been
   * received.
   */
  void on_receive(hal::callback<handler> p_handler)
  {
    driver_on_receive(p_handler);
  }

  virtual ~can() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual void driver_bus_on() = 0;
  virtual void driver_send(message_t const& p_message) = 0;
  virtual void driver_on_receive(hal::callback<handler> p_handler) = 0;
};
}  // namespace hal
