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

#include <array>
#include <optional>
#include <span>

#include "functional.hpp"
#include "units.hpp"

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
 * arbitrary size up to the limits of what hardware can support. This allows a
 * developer the ability to tailored the buffer size to the needs of the
 * application.
 *
 * All CAN messages that pass through the hardware filter will be added to the,
 * message circular buffer.
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
  using id_t = u32;

  /**
   * @brief A standard CAN message
   *
   */
  struct message
  {
    /**
     * @brief ID of the message
     *
     */
    id_t id;
    /**
     * @brief Message data contents
     *
     */
    std::array<byte, 8> payload{};
    /**
     * @brief The number of valid elements in the payload
     *
     * Can be between 0 and 8. A length value above 8 should be considered
     * invalid and can be discarded.
     */
    u8 length = 0;
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
    bool operator<=>(message const&) const = default;
  };

  enum class accept
  {
    /// Do not allow any messages through to the receive buffer. Setting this
    /// mode will clear any messages in the filter.
    none,
    /// Allow all messages on the bus through to the receive buffer. Setting
    /// this mode will clear any messages in the filter.
    all,
    /// Only allow messages with message IDs passed to `allow_message_id`
    /// through to the receive buffer. If this is selected, no messages will be
    /// received until a call to `allow_message_id` is made in order to allow
    /// those messages through.
    filtered,
  };

  /**
   * @brief Disambiguation tag object for bus off events
   *
   */
  struct on_bus_off_tag
  {};

  /**
   * @brief @brief Returns the baud rate of the can bus
   *
   * @return u32 - baud rate
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
   * the can device has entered the "bus-off" state. This can happen if a
   * critical fault in the bus has occurred. A call to `bus_on()` will need to
   * be issued to attempt to talk on the bus again. See `bus_on()` for more
   * details.
   */
  void send(can::message const& p_message)
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
  std::span<can::message const> receive_buffer()
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
   * @return u32 - position of the write cursor for the circular buffer.
   */
  u32 receive_cursor()
  {
    return driver_receive_cursor();
  }

  /**
   * @brief Set a callback for when the CAN device goes bus-off
   *
   * The BUS-OFF state for CAN is denoted by the occurrence of too many
   * transmission errors (TEC > 255) causing the CAN controller to disconnect
   * from the bus to prevent further network disruption. During bus-off,
   * the node cannot transmit or receive any messages.
   *
   * This callback will likely be executed in an interrupt context so care
   * should be taken when writing the callback.
   *
   * @param p_callback - Optional callback function to be executed when a
   * bus-off event occurs. The callback can be disabled by passing
   * `std::nullopt`.
   */
  void on_bus_off(std::optional<hal::callback<void(on_bus_off_tag)>> p_callback)
  {
    driver_on_bus_off(p_callback);
  }

  /**
   * @brief Set a callback for when the CAN device receives a message
   *
   * This callback will likely be executed in an interrupt context so care
   * should be taken when writing the callback.
   *
   * @param p_callback - Optional callback function to be executed when a
   * message is received occurs. The callback can be disabled by passing
   * `std::nullopt`.
   */
  void on_receive(
    std::optional<hal::callback<void(on_receive_tag, message const&)>>
      p_callback)
  {
    driver_on_receive(p_callback);
  }

  /**
   * @brief Transition the CAN device from "bus-off" to "bus-on"
   *
   * In the "bus-off" state, the CAN peripheral can no longer communicate on the
   * bus. Any calls to `send()` will throw the error
   * `hal::operation_not_permitted`. If this occurs, this function must be
   * called to re-enable bus communication.
   *
   * Can devices have two counters to determine system health. These two
   * counters are the "transmit error counter" and the "receive error counter".
   * Transmission errors can occur when the device attempts to communicate on
   * the bus and either does not get an acknowledge or sees an unexpected or
   * erroneous signal on the bus during its own transmission. When transmission
   * errors reach 255 counts, the device will go into the "bus-off" state.
   *
   * Calling this function when the device is already "bus-on" will have no
   * effect. This function is not necessary to call after creating the CAN
   * driver as the driver should already be "bus-on" on creation.
   *
   */
  void bus_on()
  {
    driver_bus_on();
  }

  virtual ~can() = default;

private:
  virtual u32 driver_baud_rate() = 0;
  virtual void driver_send(can::message const& p_message) = 0;
  virtual std::span<can::message const> driver_receive_buffer() = 0;
  virtual u32 driver_receive_cursor() = 0;
  virtual void driver_on_bus_off(
    std::optional<hal::callback<void(on_bus_off_tag)>> p_callback) = 0;
  virtual void driver_bus_on() = 0;
};

class can_message_interrupt
{
  /**
   * @brief Disambiguation tag object for message receive events
   *
   */
  struct on_receive_tag
  {};

  /**
   * @brief
   *
   */
  using on_receive_handler = void(on_receive_tag, can::message const&);

  void on_receive(std::optional<hal::callback<on_receive_handler>> p_callback)
  {
    driver_on_receive(p_callback);
  }

  virtual ~can_message_interrupt() = default;

private:
  virtual void driver_on_receive(
    std::optional<hal::callback<on_receive_handler>> p_callback) = 0;
};

class can_identifier_filter
{
  void set(id_t p_id)
  {
    driver_set(p_id);
  }

  virtual ~can_identifier_filter() = default;

private:
  virtual void driver_set(id_t p_id) = 0;
};

class can_mask_filter
{
  void set(id_t p_id, id_t p_mask)
  {
    driver_set(p_id, p_mask);
  }

  virtual ~can_mask_filter() = default;

private:
  virtual void driver_set(id_t p_id, id_t p_mask) = 0;
};

class can_range_filter
{
  void set(id_t p_id_1, id_t p_id_2)
  {
    driver_set(p_id_1, p_id_2);
  }

  virtual ~can_range_filter() = default;

private:
  virtual void driver_set(id_t p_id_1, id_t p_id_2) = 0;
};
}  // namespace hal
