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

#include <array>
#include <optional>
#include <span>

#include "functional.hpp"
#include "units.hpp"

namespace hal {
/**
 * @deprecated This interface has too much responsibility and has been split up
 * into `hal::can_transceiver`, `hal::can_interrupt`, `hal::can_bus_manager`,
 * `hal::can_identifier_filter`, `hal::can_extended_identifier_filter`,
 * `hal::can_mask_filter`, `hal::can_extended_mask_filter`,
 * `hal::can_range_filter`, and `hal::can_extended_range_filter`.
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
  using id_t = std::uint32_t;

  /**
   * @brief A standard CAN message
   *
   */
  struct message_t
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
    bool operator<=>(message_t const&) const = default;
  };

  /**
   * @brief Generic settings for a can peripheral
   *
   */
  struct settings
  {
    /**
     * @brief Bus clock rate in hertz
     *
     * CAN Bit Quanta Timing Diagram of:
     *
     * ```text
     *                               | <--- sjw ---> |
     *         ____    ______    __________    __________
     *      _/ SYNC \/  PROP  \/   PHASE1   \/   PHASE2   \_
     *       \______/\________/\____________/\____________/
     *                                       ^ Sample point
     * ```
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
    bool operator<=>(settings const&) const = default;
  };

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

/**
 * @brief A standard CAN message
 *
 */
struct can_message
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
    for (std::size_t i = 0; i < length; i++) {
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
class can_transceiver
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

  virtual ~can_transceiver() = default;

private:
  virtual u32 driver_baud_rate() = 0;
  virtual void driver_send(can_message const& p_message) = 0;
  virtual std::span<can_message const> driver_receive_buffer() = 0;
  virtual std::size_t driver_receive_cursor() = 0;
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
class can_interrupt
{
public:
  /**
   * @brief Disambiguation tag object for message receive events
   *
   */
  struct on_receive_tag
  {};

  /**
   * @brief Receive handler signature
   *
   */
  using receive_handler = void(on_receive_tag, can_message const&);

  /**
   * @brief Optional receive buffer
   *
   * Either contains a can message receive handler OR is std::nullopt which
   * means "disable" message interrupt.
   *
   */
  using optional_receive_handler =
    std::optional<hal::callback<receive_handler>>;

  /**
   * @brief Set a callback to occur when a new message has been received
   *
   * @param p_callback - callback to be called on message reception. Set to
   * std::nullopt to disable the callback. Setting this parameter to
   * std::nullopt, may or may not disable the receive interrupt entirely. This
   * depends on the implementation.
   */
  void on_receive(optional_receive_handler p_callback)
  {
    driver_on_receive(p_callback);
  }

  virtual ~can_interrupt() = default;

private:
  virtual void driver_on_receive(optional_receive_handler p_callback) = 0;
};

/**
 * @brief CAN Bus configuration & control hardware abstraction interface
 *
 * Implementations of this interface are NOT sharable across multiple device or
 * applications drivers. Generally a single application or process should
 * control the CAN bus.
 *
 */
class can_bus_manager
{
public:
  /**
   * @brief Disambiguation tag object for bus off events
   *
   */
  struct bus_off_tag
  {};

  /**
   * @brief Message acceptance mode for the can bus
   *
   */
  enum class accept : u8
  {
    /// Accept no messages over the bus
    none,
    /// Accept all messages, ignore all filters
    all,
    /// Only accept messages that pass through filters. If no filters have been
    /// setup, then no messages will be received.
    filtered,
  };

  /**
   * @brief Bus off handler signature
   *
   */
  using bus_off_handler = void(bus_off_tag);

  /**
   * @brief Optional bus off handler signature
   *
   * Either contains a bus off handler OR is std::nullopt which means "disable"
   * message interrupt.
   */
  using optional_bus_off_handler =
    std::optional<hal::callback<bus_off_handler>>;

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
   * @param p_hertz - baud rate in hertz
   * @throws hal::operation_not_supported if the baud rate is above or below
   * what the device can support.
   */
  void baud_rate(hal::u32 p_hertz)
  {
    driver_baud_rate(p_hertz);
  }

  /**
   * @brief Set the filter mode for the can bus
   *
   * @param p_accept - defines the set of messages that will be accepted. See
   * the `accept` enum class for details about each option and what they do.
   */
  void filter_mode(accept p_accept)
  {
    driver_filter_mode(p_accept);
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
   * occurs. If `std::nullopt` is passed, any previously set callback will be
   * cleared. The callback takes no parameters and returns void.
   */
  void on_bus_off(optional_bus_off_handler p_callback)
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

  virtual ~can_bus_manager() = default;

private:
  virtual void driver_baud_rate(hal::u32 p_hertz) = 0;
  virtual void driver_filter_mode(accept p_accept) = 0;
  virtual void driver_on_bus_off(optional_bus_off_handler p_callback) = 0;
  virtual void driver_bus_on() = 0;
};

/**
 * @brief CAN message ID filter hardware abstraction interface
 *
 * On construction, implementations acquire/reserve resources for filtering the
 * message passed to the `allow()` API. On destruction, those resources should
 * be freed such that another can_identifier_filter can claim them. In general,
 * filters of all sorts are limited in the number and type of filters they
 * support.
 */
class can_identifier_filter
{
public:
  /**
   * @brief Allow messages with this identifier to pass the can bus filter
   *
   * This filter does not distinguish between remote request frames and data
   * frames. Frames of either type will be allowed so long as the message
   * identifier matches.
   *
   * @param p_id - Allow messages with this ID through to the can message
   * filter. To stop allowing messages from this filter, set this parameter to
   * `std::nullopt`.
   */
  void allow(std::optional<u16> p_id)
  {
    driver_allow(p_id);
  }

  virtual ~can_identifier_filter() = default;

private:
  virtual void driver_allow(std::optional<u16> p_id) = 0;
};

/**
 * @brief CAN message extended ID filter hardware abstraction interface
 *
 * On construction, implementations acquire/reserve resources for filtering the
 * message passed to the `allow()` API. On destruction, those resources should
 * be freed such that another can_identifier_filter can claim them. In general,
 * filters of all sorts are limited in the number and type of filters they
 * support.
 */
class can_extended_identifier_filter
{
public:
  /**
   * @brief Allow messages with this identifier to pass the can bus filter
   *
   * This filter does not distinguish between remote request frames and data
   * frames. Frames of either type will be allowed so long as the message
   * identifier matches.
   *
   * @param p_id - Allow messages with this ID through to the can message
   * filter. To stop allowing messages from this filter, set this parameter to
   * `std::nullopt`.
   */
  void allow(std::optional<u32> p_id)
  {
    driver_allow(p_id);
  }

  virtual ~can_extended_identifier_filter() = default;

private:
  virtual void driver_allow(std::optional<u32> p_id) = 0;
};

/**
 * @brief CAN message mask filter hardware abstraction interface
 *
 * On construction, implementations acquire/reserve resources for filtering the
 * message passed to the `allow()` API. On destruction, those resources should
 * be freed such that another can_identifier_filter can claim them. In general,
 * filters of all sorts are limited in the number and type of filters they
 * support.
 */
class can_mask_filter
{
public:
  /**
   * @brief Mask filter
   *
   * The following equation must be true in order for the message to pass the
   * can bus filter.
   *
   *     received_message_id & mask == id & mask
   */
  struct pair
  {
    /**
     * @brief Specifies an ID to match against
     *
     */
    u16 id = 0;
    /**
     * @brief Specifies which bits of the ID to consider.
     *
     * For example, a mask of 0x7FF would match all bits in a 11-bit ID since
     * all 11 bits are set. A mask of 0x7F0, would allow messages with the most
     * significant 7 bits of the ID and the lower 4 bits can be any value.
     *
     */
    u16 mask = 0;
    /**
     * @brief Enables default comparison
     *
     */
    constexpr bool operator<=>(pair const&) const = default;
  };

  /**
   * @brief Allow messages that correspond to this mask pass the can bus filter
   *
   * This filter does not distinguish between remote request frames and data
   * frames. Frames of either type will be allowed so long as the message
   * identifier matches.
   *
   * @param p_filter_pair - mask filter used to filter incoming messages. To
   * stop allowing messages from this filter, set this parameter to
   * `std::nullopt`.
   */
  void allow(std::optional<pair> p_filter_pair)
  {
    driver_allow(p_filter_pair);
  }

  virtual ~can_mask_filter() = default;

private:
  virtual void driver_allow(std::optional<pair> p_filter_pair) = 0;
};

/**
 * @brief CAN message extended mask filter hardware abstraction interface
 *
 * On construction, implementations acquire/reserve resources for filtering the
 * message passed to the `allow()` API. On destruction, those resources should
 * be freed such that another can_identifier_filter can claim them. In general,
 * filters of all sorts are limited in the number and type of filters they
 * support.
 */
class can_extended_mask_filter
{
public:
  /**
   * @brief Extended mask filter
   *
   * The following equation must be true in order for the message to pass the
   * can bus filter.
   *
   *     received_message_id & mask == id & mask
   */
  struct pair
  {
    /**
     * @brief Specifies an ID to match against
     *
     */
    u32 id = 0;
    /**
     * @brief Specifies which bits of the ID to consider.
     *
     * For example, a mask of 0x1FFFFFFF would match all bits in a 29-bit ID
     * since all 29 bits are set. A mask of 0x1FFFFFF0, would allow messages
     * with the most significant 25 bits of the ID and the lower 4 bits can be
     * any value.
     *
     */
    u32 mask = 0;
    /**
     * @brief Enables default comparison
     *
     */
    constexpr bool operator<=>(pair const&) const = default;
  };

  /**
   * @brief Set the allowed messages through a mask filter.
   *
   * This filter does not distinguish between remote request frames and data
   * frames. Frames of either type will be allowed so long as the message
   * identifier matches.
   *
   * This filter can be used to both standard and extended identifier frames.
   * Implements should disregard the IDE bit if their platforms support
   * filtering based on this bit.
   *
   * @param p_filter_pair - mask filter used to filter incoming messages. Set
   * this to `std::nullopt` to disengage this filter.
   */
  void allow(std::optional<pair> p_filter_pair)
  {
    driver_allow(p_filter_pair);
  }

  virtual ~can_extended_mask_filter() = default;

private:
  virtual void driver_allow(std::optional<pair> p_filter_pair) = 0;
};

/**
 * @brief CAN message range filter hardware abstraction interface
 *
 * On construction, implementations acquire/reserve resources for filtering the
 * message passed to the `allow()` API. On destruction, those resources should
 * be freed such that another can_identifier_filter can claim them. In general,
 * filters of all sorts are limited in the number and type of filters they
 * support.
 */
class can_range_filter
{
public:
  /**
   * @brief filter pair information
   *
   * id_1 and id_2 constitute a range of allowed IDs. They do NOT need to be
   * ordered relative to each other. Meaning id_1 < id_2 is not required or
   * necessary. Setting id_1 and id_2 to the same value allowed and will act
   * like can_identifier_mask.
   */
  struct pair
  {
    /**
     * @brief Specifies one end of the id range bounds.
     *
     */
    u16 id_1 = 0;
    /**
     * @brief Specifies the other end of the id range bounds
     *
     */
    u16 id_2 = 0;
    /**
     * @brief Enables default comparison
     *
     */
    constexpr bool operator<=>(pair const&) const = default;
  };

  /**
   * @brief Allow messages within this id range to pass the can bus filter
   *
   * @param p_filter_pair - allow this range of IDs through the can filter. To
   * stop allowing messages from this filter, set this parameter to
   * `std::nullopt`.
   */
  void allow(std::optional<pair> p_filter_pair)
  {
    driver_allow(p_filter_pair);
  }

  virtual ~can_range_filter() = default;

private:
  virtual void driver_allow(std::optional<pair> p_filter_pair) = 0;
};

/**
 * @brief CAN message extended range filter hardware abstraction interface
 *
 * On construction, implementations acquire/reserve resources for filtering the
 * message passed to the `allow()` API. On destruction, those resources should
 * be freed such that another can_identifier_filter can claim them. In general,
 * filters of all sorts are limited in the number and type of filters they
 * support.
 */
class can_extended_range_filter
{
public:
  /**
   * @brief Range filter
   *
   * id_1 and id_2 constitute a range of allowed IDs. They do NOT need to be
   * ordered relative to each other. Meaning id_1 < id_2 is not required or
   * necessary. Setting id_1 and id_2 to the same value allowed and will act
   * like can_identifier_mask.
   */
  struct pair
  {
    /**
     * @brief Specifies one end of the id range bounds.
     *
     */
    u32 id_1 = 0;
    /**
     * @brief Specifies the other end of the id range bounds
     *
     */
    u32 id_2 = 0;
    /**
     * @brief Enables default comparison
     *
     */
    constexpr bool operator<=>(pair const&) const = default;
  };

  /**
   * @brief Set the allowed messages through a range filter.
   *
   * @param p_filter_pair - allow this range of IDs through the can filter. To
   * stop allowing messages from this filter, set this parameter to
   * `std::nullopt`.
   */
  void allow(std::optional<pair> p_filter_pair)
  {
    driver_allow(p_filter_pair);
  }

  virtual ~can_extended_range_filter() = default;

private:
  virtual void driver_allow(std::optional<pair> p_filter_pair) = 0;
};
}  // namespace hal

// v5 namespace added for API backwards compatibility
namespace hal::v5 {
using hal::can_bus_manager;
using hal::can_extended_identifier_filter;
using hal::can_extended_mask_filter;
using hal::can_extended_range_filter;
using hal::can_identifier_filter;
using hal::can_mask_filter;
using hal::can_message;
using can_message_interrupt = hal::can_interrupt;
using hal::can_message_size;
using hal::can_range_filter;
using hal::can_transceiver;
}  // namespace hal::v5
