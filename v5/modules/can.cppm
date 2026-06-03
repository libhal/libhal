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

#include <array>
#include <optional>

export module hal:can;

export import async_context;
import :units;
import :containers;

namespace hal::inline v5 {
/**
 * @brief A standard CAN message
 *
 */
export struct can_message
{
  /**
   * @brief Memory containing a standard or extended CAN ID
   *
   * Bits 29 to 31 are reserved and should only be set to 0s.
   *
   */
  hal::u32 id = 0;

  /**
   * @brief Determines if this message ID is an extended ID or not
   *
   * When set to `true`, will treat all 29 bits of the ID of the message.
   * When set to `false`, then this is a standard can message and only the first
   * 11 bits will be used for the message ID.
   */
  bool extended = false;

  /**
   * @brief Determines if this message is a remote request
   *
   * In general, using remote request frames are bad practice and cause issues
   * on the CAN BUS.
   *
   */
  bool remote_request = false;

  /**
   * @brief The number of valid elements in the payload
   *
   * Can be between 0 and 8. A length value above 8 should be considered
   * invalid and can be discarded.
   */
  u8 length = 0;

  /**
   * @brief Reserved aligning byte allocated to 4 for potential further changes
   *
   * Never set this value to anything other than 0.
   */
  u8 reserved0 = 0;

  /**
   * @brief Message data contents
   *
   */
  std::array<hal::byte, 8> payload{};

  /**
   * @brief Compares a can message to itself to verify if they are the same
   *
   * NOTE: This comparison only checks the valid payload bytes in the payload
   * based on the length. If the length is 1 and the second byte in the payload
   * between messages do not match, then the can messages are still considered
   * equal.
   *
   * @param p_other - the other can_message to compare against
   * @return true - the two can messages are identical
   * @return false - the two can messages are not identical
   */
  constexpr bool operator==(can_message const& p_other) const
  {
    if (length != p_other.length) {
      return false;
    }
    if (id != p_other.id) {
      return false;
    }
    if (remote_request != p_other.remote_request) {
      return false;
    }
    if (extended != p_other.extended) {
      return false;
    }
    for (usize i = 0; i < length; i++) {
      if (payload[i] != p_other.payload[i]) {
        return false;
      }
    }

    return true;
  }
};

constexpr auto can_message_size = sizeof(can_message);
static_assert(can_message_size == 16, "sizeof(hal::can_message) != 16 Bytes");

/**
 * @brief Controller Area Network (CAN bus) hardware abstraction interface with
 * message buffering.
 *
 * Implementations of this interface are sharable across multiple applications
 * and device drivers.
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
export class can_transceiver
{
public:
  /**
   * @return the device's operating baud rate in hertz
   */
  async::future<hertz> baud_rate(async::context& p_context)
  {
    return driver_baud_rate(p_context);
  }

  /**
   * @brief Send a can message over the can network
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_message - a message to be sent over the can network
   * @return async::future<void> - completes when the message is sent
   * @throws hal::operation_not_permitted - or a derivative of this class, if
   *         the can device has entered the "bus-off" state. This can happen if
   *         a critical fault in the bus has occurred. A call to `bus_on()`
   *         will need to be issued to attempt to talk on the bus again. See
   *         `bus_on()` for more details.
   *
   */
  async::future<void> send(async::context& p_context,
                           can_message const& p_message)
  {
    return driver_send(p_context, p_message);
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
   * @return circular_span<can_message const> - a const span to the
   * receive buffer used by the transceiver. Calling `size()` on the span will
   * always return a value of at least 1. Holds messages accepted by the can
   * message filter that were seen on the can bus.
   */
  circular_span<can_message const> receive_buffer()
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
   * @return usize - position of the write cursor for the circular buffer.
   */
  usize receive_cursor()
  {
    return driver_receive_cursor();
  }

  virtual ~can_transceiver() = default;

private:
  virtual async::future<hertz> driver_baud_rate(async::context& p_context) = 0;
  virtual async::future<void> driver_send(async::context& p_context,
                                          can_message const& p_message) = 0;
  virtual circular_span<can_message const> driver_receive_buffer() = 0;
  virtual usize driver_receive_cursor() = 0;
};

/**
 * @brief CAN Bus message reception hardware abstraction interface
 *
 * Implementations of this interface are NOT sharable across multiple device or
 * applications drivers. If shared, only the last handler set will be the one
 * that will execute on message reception.
 *
 * Implementations of this interface allow interrupts to be fired when a new
 * message is received. If message filtering is enabled, then the callback will
 * only be for messages received through the filter.
 */
export class can_interrupt
{
public:
  /**
   * @brief Wait for a new message reception event
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<void> - completes when a message is received
   */
  async::future<void> on_receive(async::context& p_context)
  {
    return driver_on_receive(p_context);
  }

  virtual ~can_interrupt() = default;

private:
  virtual async::future<void> driver_on_receive(async::context& p_context) = 0;
};

/**
 * @brief Message acceptance mode for the can bus
 *
 */
export enum class can_message_acceptance : u8 {
  /// Accept no messages over the bus
  none,
  /// Accept all messages, ignore all filters
  all,
  /// Only accept messages that pass through filters. If no filters have been
  /// setup, then no messages will be received.
  filtered,
};

/**
 * @brief CAN Bus configuration & control hardware abstraction interface
 *
 * Implementations of this interface are NOT sharable across multiple device or
 * applications drivers. Generally a single application or process should
 * control the CAN bus.
 *
 */
export class can_bus_manager
{
public:
  /**
   * @brief Set the can bus baud rate
   *
   * The baud rate is the communication rate of the can bus. At a baud rate of
   * 1MHz, each bit has a width of 1us. At 100kHz, each bit has a width of 10us.
   * The developer must ensure that the baud rate is set to the correct
   * communication rate for all devices on the bus. If all other devices on the
   * bus communicate at 100kHz then all other devices on the bus MUST
   * communicate at 100kHz.
   *
   * This API should be called before passing a `hal::can_transceiver`,
   * corrsponding to this can bus to a device driver for usage.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_hertz - baud rate in hertz
   * @return async::future<void> - completes when the baud rate is set
   * @throws hal::operation_not_supported if the baud rate is above or below
   * what the device can support.
   */
  async::future<void> baud_rate(async::context& p_context, hal::u32 p_hertz)
  {
    return driver_baud_rate(p_context, p_hertz);
  }

  /**
   * @brief Set the filter mode for the can bus
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_accept - defines the set of messages that will be accepted. See
   * the `can_message_acceptance` enum class for details about each option and
   * what they do.
   * @return async::future<void> - completes when the filter mode is set
   */
  async::future<void> filter_mode(async::context& p_context,
                                  can_message_acceptance p_accept)
  {
    return driver_filter_mode(p_context, p_accept);
  }

  /**
   * @brief Wait for a bus-off event
   *
   * The BUS-OFF state for CAN is denoted by the occurrence of too many
   * transmission errors (TEC > 255) causing the CAN controller to disconnect
   * from the bus to prevent further network disruption. During bus-off,
   * the node cannot transmit or receive any messages.
   *
   * On construction of the can driver, the device starts in the "bus-on" state.
   * When this event completes, the `send()` API will throw the
   * `hal::operation_not_permitted` exception and the `receive_cursor()` API
   * will not update.
   *
   * Care should be taken when awaiting this event, as it will most likely be
   * triggered from an interrupt context.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<void> - completes when the device enters bus-off
   * state
   */
  async::future<void> on_bus_off(async::context& p_context)
  {
    return driver_on_bus_off(p_context);
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
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<void> - completes when the device transitions to
   * bus-on
   */
  async::future<void> bus_on(async::context& p_context)
  {
    return driver_bus_on(p_context);
  }

  virtual ~can_bus_manager() = default;

private:
  virtual async::future<void> driver_baud_rate(async::context& p_context,
                                               hal::u32 p_hertz) = 0;
  virtual async::future<void> driver_filter_mode(
    async::context& p_context,
    can_message_acceptance p_accept) = 0;
  virtual async::future<void> driver_on_bus_off(async::context& p_context) = 0;
  virtual async::future<void> driver_bus_on(async::context& p_context) = 0;
};

/**
 * @brief Base interface for CAN message filtering
 *
 * Provides a generic mechanism for implementing CAN message filters that can
 * accept or reject messages based on a template parameter type. Implementations
 * must override the driver_allow method to handle the actual filtering logic.
 *
 * @tparam Allowed - The type of filtering criteria (e.g., u16 for ID, can_mask
 *                   for mask-based filtering, can_range for range-based
 *                   filtering)
 */
export template<typename Allowed>
class can_filter
{
public:
  /**
   * @brief Configure the filter acceptance criteria
   *
   * Asynchronously updates the filter to accept messages matching the specified
   * criteria. If p_allowed is nullopt, the filter may be cleared or disabled
   * depending on the implementation.
   *
   * @param p_context - The async execution context for this operation
   * @param p_allowed - The filtering criteria; nullopt to clear the filter
   * @return async::future<void> - Completes when the filter is configured
   */
  async::future<void> allow(async::context& p_context,
                            std::optional<Allowed> p_allowed)
  {
    return driver_allow(p_context, p_allowed);
  }
  virtual ~can_filter() = default;

private:
  virtual async::future<void> driver_allow(async::context&,
                                           std::optional<Allowed>) = 0;
};

/**
 * @brief Standard CAN ID mask filter configuration
 *
 * Defines a mask-based filter for standard (11-bit) CAN message IDs using
 * an id value and a bit mask. A message matches the filter if:
 * (message_id & mask) == id
 *
 */
export struct can_mask
{
  u16 id = 0;
  u16 mask = 0;
  constexpr auto operator<=>(can_mask const&) const = default;
};

/**
 * @brief Extended CAN ID mask filter configuration
 *
 * Defines a mask-based filter for extended (29-bit) CAN message IDs using
 * an id value and a bit mask. A message matches the filter if:
 * (message_id & mask) == id
 *
 */
export struct can_mask_ext
{
  u32 id = 0;
  u32 mask = 0;
  constexpr auto operator<=>(can_mask_ext const&) const = default;
};

/**
 * @brief Standard CAN ID range filter configuration
 *
 * Defines a range-based filter for standard (11-bit) CAN message IDs using
 * lower and upper ID bounds (id_1 and id_2). A message matches the filter if:
 * id_1 <= message_id <= id_2
 *
 */
export struct can_range
{
  u16 id_1 = 0;
  u16 id_2 = 0;
  constexpr auto operator<=>(can_range const&) const = default;
};

/**
 * @brief Extended CAN ID range filter configuration
 *
 * Defines a range-based filter for extended (29-bit) CAN message IDs using
 * lower and upper ID bounds (id_1 and id_2). A message matches the filter if:
 * id_1 <= message_id <= id_2
 *
 */
export struct can_range_ext
{
  u32 id_1 = 0;
  u32 id_2 = 0;
  constexpr auto operator<=>(can_range_ext const&) const = default;
};

/**
 * @brief Filter accepting standard CAN message IDs
 *
 * Filters messages by single 11-bit CAN identifier values.
 */
export using can_id_filter = can_filter<u16>;

/**
 * @brief Filter accepting extended CAN message IDs
 *
 * Filters messages by single 29-bit extended CAN identifier values.
 */
export using can_id_ext_filter = can_filter<u32>;

/**
 * @brief Filter accepting standard CAN IDs using mask-based matching
 *
 * Filters messages by applying a bitwise mask to standard (11-bit) IDs.
 */
export using can_mask_filter = can_filter<can_mask>;

/**
 * @brief Filter accepting extended CAN IDs using mask-based matching
 *
 * Filters messages by applying a bitwise mask to extended (29-bit) IDs.
 */
export using can_mask_ext_filter = can_filter<can_mask_ext>;

/**
 * @brief Filter accepting standard CAN IDs within a range
 *
 * Filters messages by accepting those with standard (11-bit) IDs falling
 * within a specified range.
 */
export using can_range_filter = can_filter<can_range>;

/**
 * @brief Filter accepting extended CAN IDs within a range
 *
 * Filters messages by accepting those with extended (29-bit) IDs falling
 * within a specified range.
 */
export using can_range_ext_filter = can_filter<can_range_ext>;
}  // namespace hal::inline v5
