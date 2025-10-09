#pragma once

#include <array>
#include <span>
#include <type_traits>

#include "units.hpp"

namespace hal {

/**
 * @brief Initializer base type
 *
 * Initializers are compile time constructed objects with numeric values. These
 * numeric values are sanity checked at compile time by the function.
 *
 * @tparam value - constant value of the selector
 */
template<u64 value>
struct selector_t
{
  /// Compile time storage for the value
  static constexpr auto val = value;

  /**
   * @brief Extracts the value of the initializer object
   *
   * @return constexpr u64 - value of the selector
   */
  constexpr u64 operator()() const
  {
    return value;
  }
};

/**
 * @brief A parameter type that represents a "port"
 *
 * Port numbers should be used for things such as "serial" and other systems
 * where there is a 1-to-1 connection between things.
 *
 * This can also be used for designating a port and pin combination
 *
 * @tparam value - port number
 */
template<u64 value>
struct port_t : selector_t<value>
{};

/**
 * @brief A parameter type that represents a "pin"
 *
 * Should be used for things that represent a pin such as output pin, input
 * pins, etc.
 *
 * @tparam value - pin number
 */
template<u64 value>
struct pin_t : selector_t<value>
{};

/**
 * @brief A parameter type that represents a "bus"
 *
 * Should be used for such things as a i2c, spi, can, and usb which are all bus
 * type things. Although it could be argued that usb is more of a 1-to-1 port
 * architecture.
 *
 * @tparam value - bus number
 */
template<u64 value>
struct bus_t : selector_t<value>
{};

/**
 * @brief A parameter type that represents a "bus"
 *
 * Should be used for such things as an adc, dac, or pwm channel. These devices
 * tend to have a single peripheral manage multiple adc, dac, or pwm channels.
 *
 * @tparam value - channel number
 */
template<u64 value>
struct channel_t : selector_t<value>
{};

/**
 * @brief A parameter that indicates the desired "buffer" size
 *
 * This should be used to indicate the size of a buffer that should be
 * statically allocated at compile time for an object.
 *
 * @tparam value - buffer size in bytes
 */
template<u64 value>
struct buffer_t : selector_t<value>
{};

// The following concepts are used in function arguments in order to accept
// value and types that represent them.

/**
 * @brief Concept for a port type parameter
 *
 * USAGE:
 *
 *     void accept_port(hal::port_param auto p_port);
 *
 * @tparam T - port type
 */
template<typename T>
concept port_param = std::is_same_v<port_t<T::val>, T>;

/**
 * @brief Concept for a pin type parameter
 *
 * USAGE:
 *
 *     void accept_pin(hal::pin_param auto p_pin);
 *
 * @tparam T - pin type
 */
template<typename T>
concept pin_param = std::is_same_v<pin_t<T::val>, T>;

/**
 * @brief Concept for a bus type parameter
 *
 * USAGE:
 *
 *     void accept_bus(hal::bus_param auto p_bus);
 *
 * @tparam T - bus type
 */
template<typename T>
concept bus_param = std::is_same_v<bus_t<T::val>, T>;

/**
 * @brief Concept for a channel type parameter
 *
 * USAGE:
 *
 *     void accept_channel(hal::channel_param auto p_channel);
 *
 * @tparam T - channel type
 */
template<typename T>
concept channel_param = std::is_same_v<channel_t<T::val>, T>;

/**
 * @brief Concept for a buffer type parameter
 *
 * USAGE:
 *
 *     void accept_buffer(hal::buffer_param auto p_buffer);
 *
 * @tparam T - buffer type
 */
template<typename T>
concept buffer_param = std::is_same_v<buffer_t<T::val>, T>;

/**
 * @brief port_t creation object
 *
 * USAGE:
 *
 *      hal::example::serial serial(port<5>);
 *
 * @tparam value - port number
 */
template<u64 value>
inline constexpr port_t<value> port{};

/**
 * @brief pin_t creation object
 *
 * USAGE:
 *
 *      hal::example::output_pin pin(port<1>, pin<17>);
 *
 * @tparam value - pin number
 */
template<u64 value>
inline constexpr pin_t<value> pin{};

/**
 * @brief bus_t creation object
 *
 * USAGE:
 *
 *      hal::example::i2c i2c(bus<3>);
 *
 * @tparam value - bus number
 */
template<u64 value>
inline constexpr bus_t<value> bus{};

/**
 * @brief channel_t creation object
 *
 * USAGE:
 *
 *      hal::example::adc adc(channel<0>);
 *
 * @tparam value - bus number
 */
template<u64 value>
inline constexpr channel_t<value> channel{};

/**
 * @brief buffer_t creation object
 *
 * USAGE:
 *
 *      hal::example::serial serial(port<3>, buffer<1024>);
 *
 * @tparam value - buffer size in bytes
 */
template<u64 value>
inline constexpr buffer_t<value> buffer{};

/**
 * @brief Generate a static byte buffer at compile time.
 *
 * Each call to this function will generate a new statically allocated buffer
 * at compile time.
 *
 * NEVER: call this function with the template argument set like so:
 *
 *        // NEVER DO THIS!
 *        auto buffer = create_unique_static_buffer<float>();
 *
 * This is dangerous because if any other code invoked this function with the
 * same input type, and the same `p_buffer_size`, it will result in the same
 * exact instance of the buffer being returned. Having two parts of the code
 * writing to this buffer is undefined behavior.
 *
 * @tparam decltype([]() {}) - Do not modify or set this. This generates a new
 * type instance each time it is called.
 * @param p_buffer_size - Buffer parameter type to construct the static array
 * with.
 * @return constexpr std::span<hal::byte> - Span pointing to the statically
 * allocated buffer.
 */
template<class = decltype([]() {})>
std::span<hal::byte> create_unique_static_buffer(
  buffer_param auto p_buffer_size)
{
  static std::array<hal::byte, p_buffer_size()> buffer{};
  return buffer;
}

/// This tag indicates to the reader that the function or constructor used will
/// perform some runtime sanity checks on its inputs and may return an error or
/// throw an exception if the inputs are not valid.
struct runtime
{};

/// This tag indicates to the reader that the function or constructor is an
/// unsafe operation that cannot be checked at runtime. It is the responsibility
/// of the caller to ensure that the inputs provided are valid.
struct unsafe
{};
}  // namespace hal
