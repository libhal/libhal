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

#include <optional>
#include <span>

#include "can.hpp"
#include "functional.hpp"

namespace hal {

/**
 * @brief Controller Area Network (CAN bus) hardware abstraction interface with
 * message buffering.
 *
 * Buffered can provides easy access to all can messages received by the CAN bus
 * allowing multiple device drivers to use the same buffered can for their
 * operations.
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
 * All implementations MUST allow the user to supply their own message buffer of
 * arbitrary size. This allows a developer the ability to tailored the buffer
 * size to the needs of the application.
 *
 * All CAN messages that pass through the hardware filter will be added to the,
 * message circular buffer.
 */
class buffered_can
{
public:
  /**
   * @return the device's operating baud rate in hertz
   */
  u32 baud_rate()
  {
    return driver_baud_rate();
  }

  /**
   * @brief Send a can message over the can network
   *
   * @param p_message - a message to be sent over the can network
   * @throws hal::operation_not_permitted - or a derivative of this class, if
   *         the can device has entered the "bus-off" state. This can happen if
   *         a critical fault in the bus has occurred. A call to `bus_on()`
   *         will need to be issued to attempt to talk on the bus again. See
   *         `bus_on()` for more details.
   *
   */
  void send(can::message_t const& p_message)
  {
    driver_send(p_message);
  }

  /**
   * @brief Returns this CAN driver's message receive buffer
   *
   * Use this along with the receive_cursor() in order to determine if new data
   * has been read into the receive buffer. See the docs for `receive_cursor()`
   * for more details.
   *
   * This API will work even if the CAN peripheral is "bus-off".
   *
   * @return std::span<message const> - constant span to the message receive
   *         buffer used by this driver. Assume the lifetime of the buffer is
   *         the same as the class's lifetime. When the memory of the owning
   *         object is invalidated, so is this span. Calling `size()` on the
   *         span will always return a value of at least 1.
   */
  std::span<can::message_t const> receive_buffer()
  {
    return driver_receive_buffer();
  }

  /**
   * @brief Returns the current write position of the circular receive buffer
   *
   * Receive head represents the position where the next message will be written
   * in the receive buffer. This position advances as new messages arrives. To
   * determine how much new data has arrived, store the previous head position
   * and compare it with the current head position, accounting for buffer
   * wraparound.
   *
   * The cursor value will ALWAYS follow this equation:
   *
   *          0 <= cursor && cursor < receive_buffer().size()
   *
   * Thus this expression is always a valid memory access but may not return
   * useful information:
   *
   *         can.receive_buffer()[ can.cursor() ];
   *
   * Example:
   *
   *   auto old_head = port.receive_cursor();
   *   // ... wait for new data ...
   *   auto new_head = port.receive_cursor();
   *   // Account for circular wraparound when calculating messages received
   *   auto buffer_size = port.receive_buffer().size();
   *   auto messages_received =
   *          (new_head + buffer_size - old_head) % buffer_size;
   *
   * The data between your last saved position and the current head position
   * represents the newly received messages. When reading the data, remember
   * that it may wrap around from the end of the buffer back to the beginning.
   *
   * This function will not change if the state of the system is "bus-off".
   *
   * @return std::size_t - position of the write cursor for the circular buffer.
   */
  std::size_t receive_cursor()
  {
    return driver_receive_cursor();
  }

  virtual ~buffered_can() = default;

private:
  virtual u32 driver_baud_rate() = 0;
  virtual void driver_send(can::message_t const& p_message) = 0;
  virtual std::span<can::message_t const> driver_receive_buffer() = 0;
  virtual std::size_t driver_receive_cursor() = 0;
};
}  // namespace hal
