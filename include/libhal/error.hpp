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

#include <system_error>
#include <type_traits>

namespace hal {

template<class thrown_t>
void safe_throw(thrown_t&& p_thrown_object)
{
  static_assert(
    std::is_trivially_destructible_v<thrown_t>,
    "safe_throw() only works with trivially destructible thrown types");

  throw p_thrown_object;
}

[[noreturn]] inline void halt()
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
template<auto... options>
struct invalid_option_t : std::false_type
{};
/**
 * @brief Helper definition to simplify the usage of invalid_option_t.
 *
 * @tparam options ignored by the application but needed to create a non-trivial
 * specialization of this class which allows its usage in static_assert.
 */
template<auto... options>
inline constexpr bool invalid_option = invalid_option_t<options...>::value;
}  // namespace error

/**
 * @brief This class is not constructable on purpose. Its only purpose is to be
 * compared to the hal::exception to ensure that the ABI of hal::exception stays
 * exactly the same from day one. This class should not be changed or modified
 * at any point in the code.
 *
 */
class exception_abi_origin_v0
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

  void* m_instance = nullptr;
  std::errc m_error_code{};
  std::uint32_t m_reserved0{};
  std::uint32_t m_reserved1{};
  std::uint32_t m_reserved2{};
  std::uint32_t m_reserved3{};
};

/**
 * @brief Base exception class for all hal related exceptions
 *
 */
class exception
{
public:
  constexpr exception(std::errc p_error_code, void* p_instance)
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
  const void* instance() const
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
  std::errc error_code() const
  {
    return m_error_code;
  }

private:
  void* m_instance = nullptr;
  std::errc m_error_code{};
  /// Reserved memory for future use without breaking the ABI
  /// To keep the layout the same with `exception_abi_origin_v0`, these MUST
  /// stay std::uint32_t into the future. Their names can be changed, and their
  /// contents can be any format, but they must stay std::uint32_t for the
  /// static_assert check.
  std::uint32_t m_reserved0{};
  std::uint32_t m_reserved1{};
  std::uint32_t m_reserved2{};
  std::uint32_t m_reserved3{};
};

static_assert(sizeof(exception_abi_origin_v0) == sizeof(exception));

#if __cpp_lib_is_layout_compatible == 201907L
static_assert(std::is_layout_compatible<exception_abi_origin_v0, exception>,
              "The ABI memory layout of ");
#endif

static_assert(std::is_trivially_destructible_v<exception>,
              "hal::exception MUST be trivially "
              "destructible.");

/**
 * @brief Raised when an device was expected to exist and did not
 *
 * # How to recover from this?
 *
 * 1. Scanning
 *
 * Lets consider I2C. A developer may want a system that can scan a bus for
 * devices and determine if a device is present. Normally, in I2C, devices will
 * acknowledge the i2c bus controller if they see their address on the bus. So
 * this is a rare type of error. But when scanning, its quite common as many of
 * the available addresses will not have devices to listen on them. In this
 * case, the error can be handled locally to the call and each call that does
 * not result in an exception is an address with a device.
 *
 * 2. Polling after reset
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
 * ```C++
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
 * 3. Temporary/Intermittent
 *
 * There are some cases where communication with a device is intermittent in
 * nature. In these cases, such an exception is temporary in nature and another
 * attempt at communication should work appropriately. In these situations,
 * drivers should either handle the error locally and poll on the device to
 * until it ACKs or until a timeout time. The user may request at the
 * construction of the object to skip catching the error and allow it propagate
 * out of the driver such that user code can handle.
 *
 * 4. Else, not recoverable
 *
 * If none of the above apply to your application, this error is considered,
 * generally non-recoverable and will terminate your application. This would be
 * an unexpected bug due to hardware misconfiguration or some runtime hardware
 * fault.
 */
struct no_such_device : public exception
{
  /**
   * @brief Construct a new timed_out exception
   *
   * @param p_address - the address of the device represented in 32-bits. For
   * I2C this can be the 7-bit device address.
   * @param p_instance - must point to the instance of the driver that threw
   * this exception.
   */
  constexpr no_such_device(std::uint32_t p_address, void* p_instance)
    : exception(std::errc::no_such_device, p_instance)
    , address(p_address)
  {
  }

  std::uint32_t address;
};

/**
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
struct io_error : public exception
{
  /**
   * @brief Construct a new timed_out exception
   *
   * @param p_instance - must point to the instance of the driver that threw
   * this exception. If this was thrown from a free function pass nullptr.
   */
  io_error(void* p_instance)
    : exception(std::errc::io_error, p_instance)
  {
  }
};

/**
 * @brief Raised when a resource is unavailable but another attempt would work
 *
 * Resources are typically busses, peripherals, and shared hardware devices.
 *
 * # How do to recover from this?
 *
 * 1. Retry!
 *
 * Consider I2C. I2C has the ability to have multiple controllers on the bus. If
 * two controllers attempt to control the bus at the same time, arbitration will
 * occur. One of the I2C controllers will succeed and the other will get a
 * hal::resource_unavailable_try_again exception. To handle this, simply attempt
 * the transaction again until a timeout occurs (time or retry based).
 *
 * 2. Else?
 *
 * This exception should only be raised for arbitration reasons and not I/O
 * reasons, so the only option is to retry. The time it takes for a retry to
 * work depends strongly on the applications and the drivers use.  If this does
 * not work for the application, then a change in architecture, drivers, or part
 * selection may be required.
 *
 */
struct resource_unavailable_try_again : public exception
{
  /**
   * @brief Construct a new timed_out exception
   *
   * @param p_instance - must point to the instance of the driver that threw
   * this exception.
   */
  resource_unavailable_try_again(void* p_instance)
    : exception(std::errc::resource_unavailable_try_again, p_instance)
  {
  }
};

/**
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
 * 1. Context Required
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
struct timed_out : public exception
{
  /**
   * @brief Construct a new timed_out exception
   *
   * @param p_instance - must point to the instance of the driver timed out. If
   * the timeout occurred from a free function, pass nullptr.
   */
  timed_out(void* p_instance)
    : exception(std::errc::timed_out, p_instance)
  {
  }
};

/**
 * @brief Raised exclusively when a driver cannot configure itself based on the
 * settings passed to it.
 *
 * # How do to recover from this?
 *
 * 1. Adjust settings at runtime if applicable
 *
 * This option requires the settings for a driver to be fluid. In most cases, a
 * configuration is a HARD requirement and cannot be simply adjusted to get the
 * code to work. But in some cases, settings can be adjusted such as an spi
 * clock rate. Most devices can deal with a wide range of clock rates for spi,
 * so adjusting the clock rate could work if the application can handle this
 * change as well.
 *
 * 2. Else?
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
struct operation_not_supported : public exception
{
  /**
   * @brief Construct a new operation_not_supported exception
   *
   * @param p_instance - must point to the instance of the driver that could not
   * be configured with the configuration settings passed to it.
   */
  operation_not_supported(void* p_instance)
    : exception(std::errc::operation_not_supported, p_instance)
  {
  }
};

/**
 * @brief Raised when an operation could not be performed because it is no
 * longer permitted to use a resource.
 *
 * # How do to recover from this?
 *
 * 1. hal::can
 *
 * See the API docs for `hal::can::bus_on()` and `hal::can::send()` for details
 * on how to recover from this. It will explain in detail how to recover from
 * this situation.
 *
 * 2. Else?
 *
 * This would be context specific and the API that throws this error should
 * explain why the operation is not permitted and how to allow the operation to
 * become permitted.
 *
 */
struct operation_not_permitted : public exception
{
  operation_not_permitted(void* p_instance)
    : exception(std::errc::operation_not_permitted, p_instance)
  {
  }
};

/**
 * @brief Raised when an input passed to a function is outside the domain of the
 * function.
 *
 * TODO: Missing recovery steps! Add them later!
 */
struct argument_out_of_domain : public exception
{
  argument_out_of_domain(void* p_instance)
    : exception(std::errc::argument_out_of_domain, p_instance)
  {
  }
};

}  // namespace hal
