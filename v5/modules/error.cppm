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

module;

#include <system_error>
#include <type_traits>

export module hal:error;

export import :units;

/**
 * @defgroup Error Error
 *
 */
namespace hal::inline v5 {
export [[noreturn]] inline void halt()
{
  while (true) {
    continue;
  }
}

/**
 * @brief Error objects, templates, and constants.
 *
 */
namespace error {
/**
 * @brief Used for defining static_asserts that should always fail, but only if
 * the static_assert line is hit via `if constexpr` control block. Prefer to NOT
 * use this directly but to use `invalid_option` instead
 *
 * @tparam options ignored by the application but needed to create a non-trivial
 * specialization of this class which allows its usage in static_assert.
 */
export template<auto... options>
struct invalid_option_t : std::false_type
{};
/**
 * @brief Helper definition to simplify the usage of invalid_option_t.
 *
 * @tparam options ignored by the application but needed to create a non-trivial
 * specialization of this class which allows its usage in static_assert.
 */
export template<auto... options>
inline constexpr bool invalid_option = invalid_option_t<options...>::value;
}  // namespace error

/**
 * @brief This class is not constructable on purpose. Its only purpose is to be
 * compared to the hal::exception to ensure that the ABI of hal::exception stays
 * exactly the same from day one. This class should not be changed or modified
 * at any point in the code.
 *
 */
export class exception_abi_origin_v0
{
private:
  exception_abi_origin_v0()
  {
    static_cast<void>(m_instance);
    static_cast<void>(m_error_code);
    static_cast<void>(m_reserved0);
    static_cast<void>(m_reserved1);
    static_cast<void>(m_reserved2);
    static_cast<void>(m_reserved3);
  }

  void const* m_instance = nullptr;
  std::errc m_error_code{};
  u32 m_reserved0{};
  u32 m_reserved1{};
  u32 m_reserved2{};
  u32 m_reserved3{};
};

/**
 * @ingroup Error
 * @brief Base exception class for all hal related exceptions
 *
 */
export class exception
{
public:
  constexpr exception(std::errc p_error_code, void const* p_instance)
    : m_instance(p_instance)
    , m_error_code(p_error_code)
  {
    static_cast<void>(m_reserved0);
    static_cast<void>(m_reserved1);
    static_cast<void>(m_reserved2);
    static_cast<void>(m_reserved3);
  }

  /**
   * @brief address of the object that threw an exception
   *
   * If the exception was thrown by a function, this will be a nullptr. The
   * purpose of this field is to allow exception handlers additional insight
   * about what may have gone wrong with the system.
   *
   * Use cases:
   *
   *  1. Logging:
   *      a. Log the instance object address raw
   *      b. Use the instance address to determine which driver failed and
   *         craft a more accurate log message.
   *  2. Recovery:
   *      a. Compare the instance address against drivers which are known to
   *         throw recoverable errors. When the driver or object is found,
   *         perform operations on that driver to bring the system back into a
   *         normal state.
   *
   * NOTE: about using this for recovery. The instance address should not be
   * used directly but only with objects that are still accessible by the error
   * handler. This address should be used for lookup. DO NOT COST away the const
   * or C-cast this into an object for use. If the object you mean to manipulate
   * is not at least within the scope catch handler, then the lifetime of that
   * object cannot be guaranteed and is considered the strongest form of UB.
   *
   */
  [[nodiscard]] void const* instance() const
  {
    return m_instance;
  }

  /**
   * @brief Convert this exception to the closest C++ error code
   *
   * Main Use Case: Translation from C++ exceptions to C error codes
   *
   * Lets consider a situation when a C++ program must interface with C code or
   * code that uses a C API to operate. Normally C++ code calls C code, but if
   * C++ code is given to a C API like a callback and that C api expects an
   * error code back for its own error handling purposes, this class function
   * provides that error code. Simply catch the hal::exception, and return an
   * error code. Perform any other recovery or handling required to make the
   * system perform as expected.
   *
   * Other use cases:
   *
   * 1. Logging
   *
   * Log the error code value (or the stringified version) of this exception to
   * alert developers of the kind of underlying exception that was thrown.
   *
   * 2. Recovery
   *
   * This can technically be used for recovery, but it is HIGHLY RECOMMENDED to
   * use the derived classes in their own catch blocks to recover from a
   * specific error rather than using the base class and extracting its error
   * code.
   *
   * @return std::errc - error code represented by the exception
   */
  [[nodiscard]] std::errc error_code() const
  {
    return m_error_code;
  }

private:
  void const* m_instance = nullptr;
  std::errc m_error_code{};
  /// Reserved memory for future use without breaking the ABI
  /// To keep the layout the same with `exception_abi_origin_v0`, these MUST
  /// stay u32 into the future. Their names can be changed, and their
  /// contents can be any format, but they must stay u32 for the
  /// static_assert check.
  u32 m_reserved0{};
  u32 m_reserved1{};
  u32 m_reserved2{};
  u32 m_reserved3{};
};

static_assert(sizeof(exception_abi_origin_v0) == sizeof(exception));
static_assert(std::is_trivially_destructible_v<exception>,
              "hal::exception MUST be trivially "
              "destructible.");

// Disabled for now with the "&& 0" as it seems to fail on GCC 12.3.
#if __cpp_lib_is_layout_compatible == 201907L && 0
static_assert(std::is_layout_compatible<exception_abi_origin_v0, exception>,
              "The ABI memory layout of ");
#endif

/**
 * @ingroup Error
 * @brief Raised when an device was expected to exist and did not
 *
 * # How to recover from this?
 *
 * ## 1. Scanning
 *
 * Lets consider I2C. A developer may want a system that can scan a bus for
 * devices and determine if a device is present. Normally, in I2C, devices will
 * acknowledge the i2c bus controller if they see their address on the bus. So
 * this is a rare type of error. But when scanning, its quite common as many of
 * the available addresses will not have devices to listen on them. In this
 * case, the error can be handled locally to the call and each call that does
 * not result in an exception is an address with a device.
 *
 * ## 2. Polling after reset
 *
 * Lets consider I2C. In this situation, lets consider a device that must be
 * reset in order for a configuration setting to be used. In this situation, a
 * driver may have to poll on a device, attempting to get it to acknowledge it
 * before it can use the device. This happens when a driver resets a device
 * before configuring it to put it in a known state. This could also happen for
 * drivers that determine that the ROM settings of a device are not configured
 * to a stat that it can handle, it modifies the ROM and then requests a
 * software reset of the device.
 *
 * To handle this, simply do:
 *
 * ```
 * for (int i = 0; i < number_of_attempts; i++) {
 *    try {
 *      hal::read(i2c, m_address, buffer);
 *    } catch (const hal::no_such_device&) {
 *      continue;
 *    }
 * }
 * ```
 *
 * or use `hal::probe(i2c)` in the while loop that performs the try catch for
 * you and returns a bool if it was successfully able to communicate with the
 * device.
 *
 * ## 3. Temporary/Intermittent
 *
 * There are some cases where communication with a device is intermittent in
 * nature. In these cases, such an exception is temporary in nature and another
 * attempt at communication should work appropriately. In these situations,
 * drivers should either handle the error locally and poll on the device to
 * until it ACKs or until a timeout time. The user may request at the
 * construction of the object to skip catching the error and allow it propagate
 * out of the driver such that user code can handle.
 *
 * ## 4. Else, not recoverable
 *
 * If none of the above apply to your application, this error is considered,
 * generally non-recoverable and will terminate your application. This would be
 * an unexpected bug due to hardware misconfiguration or some runtime hardware
 * fault.
 */
export struct no_such_device : public exception
{
  /**
   * @brief Construct a new timed_out exception
   *
   * @param p_address - the address of the device represented in 32-bits. For
   * I2C this can be the 7-bit device address.
   * @param p_instance - must point to the instance of the driver that threw
   * this exception.
   */
  constexpr no_such_device(u32 p_address, void const* p_instance)
    : exception(std::errc::no_such_device, p_instance)
    , address(p_address)
  {
  }

  u32 address;
};

/**
 * @ingroup Error
 * @brief Raised to indicate an issue with low level I/O
 *
 * I/O errors are error at a low fundamental level. For I2C that could mean that
 * the signals it saw on the bus did not align with the protocol. This exception
 * is reserved for errors that can be detected that are purely a hardware
 * communication error in nature. This are usually considered NOT recoverable.
 *
 * Unless the system, application, or developer understands exactly how to
 * recover from this error, this error should be allowed to terminate the
 * application. Systems should be designed to use an appropriate terminate
 * handler to manage what happens during termination and attempt to put the
 * system in a reliable or safe state.
 *
 * In very specific niche situations could this be recoverable. Such as testing
 * a system to determine the maximum clock rate before data corruption
 * begins to occur.
 *
 */
export struct io_error : public exception
{
  /**
   * @brief Construct a new timed_out exception
   *
   * @param p_instance - must point to the instance of the driver that threw
   * this exception. If this was thrown from a free function pass nullptr.
   */
  io_error(void const* p_instance)
    : exception(std::errc::io_error, p_instance)
  {
  }
};

/**
 * @ingroup Error
 * @brief Raised when a resource is unavailable but another attempt may work
 * @deprecated I2c no longer uses this exception when another controller on the
 * bus is currently performing a transaction. The new behavior is to wait for
 * the bus to become available. `hal::io_waiter` can be used to get out of the
 * i2c if a deadline has been exceeded. There are no other interfaces for which
 * this exception makes sense and thus an example describing when to use this
 * and how to recover from it does not exist. Until such an example is found,
 * this error type should not be thrown and should be considered deprecated. If
 * such a use case becomes known, then this comment will be updated, deprecation
 * notice will be removed, and this type can be used in those situations.
 *
 */
export struct resource_unavailable_try_again : public exception
{
  /**
   * @brief Construct a new timed_out exception
   *
   * @param p_instance - must point to the instance of the driver that threw
   * this exception.
   */
  resource_unavailable_try_again(void const* p_instance)
    : exception(std::errc::resource_unavailable_try_again, p_instance)
  {
  }
};

/**
 * @ingroup Error
 * @brief Raised when a hardware resource is in use and cannot be acquired.
 *
 * # When to Raise this
 *
 * This exception should be raised when application code attempts to construct
 * or acquire a hardware resource that is already in use by another driver
 * object.
 *
 * # How to recover from this?
 *
 * ## 1. Use an alternative resource
 *
 * If an alternative resource exists that the application an take advantage of,
 * then the application can catch this exception and attempt to acquire the
 * alternative resource.
 *
 * For example, consider `hal::can_identifier_filter` and
 * `hal::can_mask_filter`. One could use a mask filter in place of an identifier
 * filter. If an attempt to acquire an identifier filter fails, then the code
 * could fallback to acquiring a mask filter.
 *
 * ## 2. Otherwise, this is a runtime bug
 *
 * If none of the above apply to your application, then this exception is
 * considered a bug in the application. The application should handle this error
 * as it would any other bugs in the application.
 */
export struct device_or_resource_busy : public exception
{
  device_or_resource_busy(void const* p_instance)
    : exception(std::errc::device_or_resource_busy, p_instance)
  {
  }
};

/**
 * @ingroup Error
 * @brief Raised when an operation reaches a deadline before completing
 *
 * Time outs can be generated from time sources such as a hal::steady_clock or
 * hal::timer as well as external sources such as an interrupt or input pin.
 *
 * NOTE: Do not throw exceptions in an interrupt context! Interrupts have
 * priority over the entire scheduler and must be made to be as small as
 * possible. If an interrupt is needed to signal a timeout, the interrupt should
 * set a some flag, signal, semaphore etc that indicates to the application that
 * something is done.
 *
 * # How do to recover from this?
 *
 * ## 1. Context Required
 *
 * This entirely depends on your application. If a timeout is too small, it may
 * not give calling code enough time to perform the operation. But in many
 * cases, the meaning of a timeout depends on the operation that was called. The
 * caller should have context for what a timeout means. If the calling function
 * does not know how to handle a timeout with its level context, it should
 * inform the developer via documentation that its functions can throw a
 * hal::timed_out exception and what it means.
 *
 */
export struct timed_out : public exception
{
  /**
   * @brief Construct a new timed_out exception
   *
   * @param p_instance - must point to the instance of the driver timed out. If
   * the timeout occurred from a free function, pass nullptr.
   */
  timed_out(void const* p_instance)
    : exception(std::errc::timed_out, p_instance)
  {
  }
};

/**
 * @ingroup Error
 * @brief Raised exclusively when a driver cannot configure itself based on the
 * settings passed to it.
 *
 * # How do to recover from this?
 *
 * ## 1. Adjust settings at runtime if applicable
 *
 * This option requires the settings for a driver to be fluid. In most cases, a
 * configuration is a HARD requirement and cannot be simply adjusted to get the
 * code to work. But in some cases, settings can be adjusted such as an spi
 * clock rate. Most devices can deal with a wide range of clock rates for spi,
 * so adjusting the clock rate could work if the application can handle this
 * change as well.
 *
 * ## 2. Else?
 *
 * Normally, the configuration settings of an application are determined early
 * at boot. So if a driver was expected to work with a specific setting such as
 * a baud rate for hal::serial, but it could not achieve that rate due to the
 * peripheral clock rate being too low or an overall limitation on the hardware,
 * then the application was never valid to begin with. This is then would be
 * considered a bug and NOT RECOVERABLE. Either the code must be modified or the
 * hardware changed to resolve this error.
 *
 * # How to Log this?
 *
 * Get the address of the offending object using the `instance()` function
 * and compare it to the series of objects used within the try block and log a
 * message for any object in the try that matches the instance address. If the
 * settings are also accessible and within scope, that can be logged as well to
 * inform a user of the application what went wrong and for what driver.
 *
 */
export struct operation_not_supported : public exception
{
  /**
   * @brief Construct a new operation_not_supported exception
   *
   * @param p_instance - must point to the instance of the driver that could not
   * be configured with the configuration settings passed to it.
   */
  operation_not_supported(void const* p_instance)
    : exception(std::errc::operation_not_supported, p_instance)
  {
  }
};

/**
 * @ingroup Error
 * @brief Raised when an operation could not be performed because it is no
 * longer permitted to use a resource.
 *
 * # How do to recover from this?
 *
 * ## 1. hal::can
 *
 * See the API docs for `hal::can::bus_on()` and `hal::can::send()` for details
 * on how to recover from this. It will explain in detail how to recover from
 * this situation.
 *
 * ## 2. Else?
 *
 * This would be context specific and the API that throws this error should
 * explain why the operation is not permitted and how to allow the operation to
 * become permitted.
 *
 */
export struct operation_not_permitted : public exception
{
  operation_not_permitted(void const* p_instance)
    : exception(std::errc::operation_not_permitted, p_instance)
  {
  }
};

/**
 * @ingroup Error
 * @brief Raised when an input passed to a function is outside the domain of the
 * function.
 *
 * Thrown by interfaces and drivers with necessarily unbounded inputs. For
 * example, hal::servo takes a position variable centered at a zero position. A
 * servo will have a bounded range. But servos also have a variety of possible
 * ranges and thus the interface must allow for an unbounded set of input
 * positions. When an input position is given that exceeds what the servo can
 * perform, it raises this exception.
 *
 * # How to recover from this?
 *
 * TBD: but in most cases this is a bug in the application and not recoverable.
 * In general, this is only recoverable if an application is trying to determine
 * the bounds of a servo.
 */
export struct argument_out_of_domain : public exception
{
  argument_out_of_domain(void const* p_instance)
    : exception(std::errc::argument_out_of_domain, p_instance)
  {
  }
};

/**
 * @ingroup Error
 * @brief Raised when a message, packet, or payload is greater than what can be
 * sent.
 *
 * # How do to recover from this?
 *
 * ## Attempt Largest Packet Size, Trim & Retry
 *
 * This error can be recovered from by catching the exception and using the
 * `max_size` field to split your packets into smaller chunks and then calling
 * the API with each chunk in order.
 *
 * A good policy for using libhal libraries is to ask for forgiveness not
 * permission. Meaning attempt to send the greatest amount of data at once and
 * see if you can get away with it. Wrap such calls to send/transfer like APIs
 * with a catch for this exception type and use it to determine the new largest
 * packet size to send.
 *
 */
export struct message_size : public exception
{
  message_size(u32 p_max_size, void const* p_instance)
    : exception(std::errc::message_size, p_instance)
    , max_size(p_max_size)
  {
  }

  u32 max_size;
};

/**
 * @ingroup Error
 * @brief Raised when an operation is attempted on an unestablished connection.
 *
 * This error is typically encountered in network programming, serial
 * communication, or other contexts where a logical or physical connection is
 * required before communication can occur. It signifies that the connection
 * expected for the operation is not currently established.
 *
 * # How to recover from this?
 *
 * ## Check Connection Status, Re-establish & Retry
 *
 * Recovery from this error involves ensuring that the connection has been
 * successfully established before retrying the operation. This might include:
 *
 * - Checking the connection status through appropriate API calls.
 * - Attempting to re-establish the connection using the connection setup or
 *   initialization function.
 * - Once the connection is confirmed or successfully re-established, retry the
 *   failed operation.
 *
 * It's advisable to implement a robust error handling mechanism that includes
 * connection status checks, retries with exponential backoff, and clear
 * feedback to the user or system when a connection cannot be established within
 * a reasonable amount of attempts.
 *
 */
export struct not_connected : public exception
{
  not_connected(void const* p_instance)
    : exception(std::errc::not_connected, p_instance)
  {
  }
};

/**
 * @ingroup Error
 * @brief Raised when the error does not match any known or expected error from
 * a device or system.
 *
 * This error should ONLY be used when a system returns an error code that is
 * not apart of a defined list of expected errors. For example, lets consider a
 * C api that returns an int from 0 to 5 and each is mapped to a specific error.
 * Now lets consider that the API is called and the number 18375 is returned. In
 * this case, this is an unknown error value and thus this exception can be
 * thrown.
 *
 * If this is not the case, use any other hal::exception that fits.
 */
export struct unknown : public exception
{
  unknown(void const* p_instance)
    : exception(std::errc{}, p_instance)
  {
  }
};

/**
 * @ingroup Error
 * @brief Raised when an API attempts to access elements outside of a container
 * or resource.
 *
 */
export struct out_of_range : public exception
{
  struct info
  {
    usize m_index;
    usize m_capacity;
  };

  out_of_range(void const* p_instance, info p_info)
    : exception(std::errc::invalid_argument, p_instance)
    , m_info(p_info)
  {
  }

  info m_info;
};
}  // namespace hal::inline v5
