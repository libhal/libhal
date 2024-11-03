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

#include <array>
#include <optional>
#include <span>

#include "functional.hpp"
#include "units.hpp"

namespace hal {

/**
 * @brief Can message ID type trait
 *
 */
using can_id = std::uint32_t;

/**
 * @brief A standard CAN message
 *
 */
struct can_message
{
  /**
   * @brief ID of the message
   *
   */
  can_id id;
  /**
   * @brief Message data contents
   *
   */
  std::array<hal::byte, 8> payload{};
  /**
   * @brief The number of valid elements in the payload
   *
   * Can be between 0 and 8. A length value above 8 should be considered
   * invalid and can be discarded.
   */
  uint8_t length = 0;
  /**
   * @brief Determines if the message is a remote request frame
   *
   * If true, then length and payload are ignored.
   */
  bool is_remote_request = false;

  /**
   * @brief Enables default comparison
   *
   */
  bool operator<=>(can_message const&) const = default;
};

/**
 * @brief Generic settings for a can peripheral
 *
 */
struct can_settings
{
  /**
   * @brief Bus clock rate in hertz
   *
   * CAN Bit Quanta Timing Diagram of:
   *
   *                               | <--- sjw ---> |
   *         ____    ______    __________    __________
   *      _/ SYNC \/  PROP  \/   PHASE1   \/   PHASE2   \_
   *       \______/\________/\____________/\____________/
   *                                       ^ Sample point
   *
   * A can bus bit is separated into the segments:
   *
   *   - Sync Segment (always 1qt): Initial sync transition, the start of a
   *     CAN bit.
   *   - Propagation Delay (1qt ... 8qt): Number of time quanta to
   *     compensate for signal/propagation delays across the network.
   *   - Phase Segment 1 (1qt ... 8qt): phase segment 1 acts as a buffer that
   *     can be lengthened to resynchronize with the bit stream via the
   *     synchronization jump width.
   *   - Phase Segment 2 (1qt ... 8qt): Occurs after the sampling point. Phase
   *     segment 2. can be shortened to resynchronize with the bit stream via
   *     the synchronization jump width.
   *   - Synchronization jump width (1qt ... 4qt): This is the maximum time by
   *     which the bit sampling period may be lengthened or shortened during
   *     each cycle to adjust for oscillator mismatch between nodes. This
   *     value must be smaller than phase_segment1 and phase_segment2.
   *
   * The exact value of these segments is calculated for you by the can
   * peripheral based on the input clock to the peripheral and the desired
   * baud rate.
   *
   * A conformant can bus peripheral driver will either choose from tq
   * starting from 25 and reducing it down to 8 or will have pre-configured
   * timing values for each frequency it supports.
   *
   */
  hertz baud_rate = 100.0_kHz;

  /**
   * @brief Enables default comparison
   *
   */
  bool operator<=>(can_settings const&) const = default;
};

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
 * arbitrary size up to the limits of what hardware can support. This allows a
 * developer the ability to tailored the buffer size to the needs of the
 * application.
 *
 * All CAN messages that pass through the hardware filter will be added to the,
 * message circular buffer.
 */
class buffered_can
{
public:
  /**
   * @brief Configure this can bus port to match the settings supplied
   *
   * @param p_settings - settings to apply to can driver
   * @throws hal::operation_not_supported - if the settings could not be
   *         achieved.
   */
  void configure(can_settings const& p_settings)
  {
    driver_configure(p_settings);
  }

  /**
   * @brief Set a callback for when the CAN device goes bus-off
   *
   * The BUS-OFF state for CAN is denoted by the occurrence of too many
   * transmission errors (TEC > 255) causing the CAN controller to disconnect
   * from the bus to prevent further network disruption. During bus-off,
   * the node cannot transmit or receive any messages.
   *
   * On construction of the can driver, the default callback for the bus-off
   * event is to do nothing. The `send()` API throw the
   * `hal::operation_not_permitted` exception and the `receive_cursor()` API
   * will not update.
   *
   * Care should be taken when writing the callback, as it will most likely be
   * executed in an interrupt context.
   *
   * @param p_callback - Optional callback function to be executed when bus-off
   *        occurs. If `std::nullopt` is passed, any previously set
   *        callback will be cleared. The callback takes no parameters and
   *        returns void.
   */
  void on_bus_off(std::optional<hal::callback<void(void)>> p_callback)
  {
    driver_on_bus_off(p_callback);
  }

  /**
   * @brief Transition the CAN device from "bus-off" to "bus-on"
   *
   * Calling this function when the device is already "bus-on" will have no
   * effect. This function is not necessary to call after creating the CAN
   * driver as the driver should already be "bus-on" on creation.
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
  void send(can_message const& p_message)
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
  std::span<can_message const> receive_buffer()
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
  virtual void driver_configure(can_settings const& p_settings) = 0;
  virtual void driver_on_bus_off(
    std::optional<hal::callback<void(void)>> p_callback) = 0;
  virtual void driver_bus_on() = 0;
  virtual void driver_send(can_message const& p_message) = 0;
  virtual std::span<can_message const> driver_receive_buffer() = 0;
  virtual std::size_t driver_receive_cursor() = 0;
};
}  // namespace hal
