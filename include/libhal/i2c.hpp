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

#include <span>

#include "functional.hpp"
#include "timeout.hpp"
#include "units.hpp"

namespace hal {
/**
 * @brief Inter-integrated Circuit (I2C) hardware abstract interface.
 *
 * Also known as Two Wire Interface (TWI) communication protocol. This is a very
 * commonly used protocol for communication with sensors and peripheral devices
 * because it only requires two connections SDA (data signal) and SCL (clock
 * signal). This is possible because the protocol for I2C is addressable.
 *
 * Some devices can utilize clock stretching as a means to pause the i2c
 * controller until the device is ready to respond. To ensure operations with
 * i2c are deterministic and reliably it is advised to NEVER use a clock
 * stretching device in your application.
 *
 */
class i2c
{
public:
  /**
   * @brief Generic settings for a standard I2C device
   *
   */
  struct settings
  {
    /**
     * @brief The serial clock rate in hertz.
     *
     */
    hertz clock_rate = 100.0_kHz;

    /**
     * @brief Enables default comparison
     *
     */
    bool operator<=>(settings const&) const = default;
  };

  /**
   * @brief Configure i2c to match the settings supplied
   *
   * @param p_settings - settings to apply to i2c driver
   * @throws hal::operation_not_supported - if the settings could not be
   * achieved.
   */
  void configure(settings const& p_settings)
  {
    driver_configure(p_settings);
  }

  /**
   * @brief perform an i2c transaction with another device on the bus. The type
   * of transaction depends on values of input parameters. This function will
   * block until the entire transfer is finished.
   *
   * Performing Write, Read and Write-Then-Read transactions depends on which
   * span for data_out and data_in are set to null.
   *
   * - For write transactions, pass p_data_in as an empty span
   * `std::span<hal::byte>{}` and pass a buffer to p_data_out.
   *
   * - For read transactions, pass p_data_out as an empty span
   * `std::span<hal::byte const>{}` and pass a buffer to p_data_in.
   *
   * - For write-then-read transactions, pass a buffer for both p_data_in
   *   p_data_out.
   *
   * - If both p_data_in and p_data_out are empty, simply do nothing and return
   *   success.
   *
   * In the event of arbitration loss, this function will wait for the bus to
   * become free and try again. Arbitration loss means that during the address
   * phase of a transaction 1 or more i2c bus controllers attempted to perform
   * an transaction and one of the i2c bus controllers, that isn't this one won
   * out.
   *
   * @param p_address 7-bit address of the device you want to communicate with.
   * To perform a transaction with a 10-bit address, this parameter must be the
   * address upper byte of the 10-bit address OR'd with 0b1111'0000 (the 10-bit
   * address indicator). The lower byte of the address must be contained in the
   * first byte of the p_data_out span.
   * @param p_data_out data to be written to the addressed device. Set to
   * nullptr with length zero in order to skip writing.
   * @param p_data_in buffer to store read data from the addressed device. Set
   * to nullptr with length 0 in order to skip reading.
   * @throws hal::no_such_device - indicates that no devices on
   * the bus acknowledge the address in this transaction, which could mean that
   * the device is not connected to the bus, is not powered, not available to
   * respond, broken or many other possible outcomes.
   * @throws hal::io_error - indicates that the i2c lines were put into an
   * invalid state during the transaction due to interference, misconfiguration,
   * hardware fault, malfunctioning i2c peripheral or possibly something else.
   * This tends to present a hardware issue and is usually not recoverable.
   */
  void transaction(hal::byte p_address,
                   std::span<hal::byte const> p_data_out,
                   std::span<hal::byte> p_data_in)
  {
    driver_transaction(p_address, p_data_out, p_data_in);
  }

  /**
   * @deprecated Prefer to use the i2c API that does not use timeouts. Timeout
   * functions get in the way of an efficient usage of DMA, is a viral
   * performance hit, and is only for the rare situation where a device on the
   * bus may perform clock stretching for which there are few devices that
   * support this. The deprecated attribute is not used yet, as the transition
   * is still ongoing.
   *
   * @brief perform an i2c transaction with another device on the bus. The type
   * of transaction depends on values of input parameters. This function will
   * block until the entire transfer is finished.
   *
   *
   * @param p_address 7-bit address of the device you want to communicate with.
   * To perform a transaction with a 10-bit address, this parameter must be the
   * address upper byte of the 10-bit address OR'd with 0b1111'0000 (the 10-bit
   * address indicator). The lower byte of the address must be contained in the
   * first byte of the p_data_out span.
   * @param p_data_out data to be written to the addressed device. Set to
   * nullptr with length zero in order to skip writing.
   * @param p_data_in buffer to store read data from the addressed device. Set
   * to nullptr with length 0 in order to skip reading.
   * @param p_timeout callable which notifies the i2c driver that it has run out
   * of time to perform the transaction and must stop and return control to the
   * caller.
   * @throws hal::timed_out from p_timeout if the transaction exceeds its
   * deadline.
   * @throws hal::no_such_device - indicates that no devices on
   * the bus acknowledge the address in this transaction, which could mean that
   * the device is not connected to the bus, is not powered, not available to
   * respond, broken or many other possible outcomes.
   * @throws hal::io_error - indicates that the i2c lines were put into an
   * invalid state during the transaction due to interference, misconfiguration,
   * hardware fault, malfunctioning i2c peripheral or possibly something else.
   * This tends to present a hardware issue and is usually not recoverable.
   */
  void transaction(hal::byte p_address,
                   std::span<hal::byte const> p_data_out,
                   std::span<hal::byte> p_data_in,
                   hal::function_ref<hal::timeout_function> p_timeout)
  {
    driver_transaction(p_address, p_data_out, p_data_in, p_timeout);
  }

  virtual ~i2c() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;

  /// Implementors of this virtual API should simply call the concrete class's
  /// implementation of driver_transaction() without the p_timeout parameter and
  /// drop the p_timeout parameter.
  virtual void driver_transaction(
    hal::byte p_address,
    std::span<hal::byte const> p_data_out,
    std::span<hal::byte> p_data_in,
    hal::function_ref<hal::timeout_function> p_timeout) = 0;

  /// The default implementation of this calls the original driver_transaction
  /// with a never timeout parameter. This is to make this API call backwards
  /// compatible.
  virtual void driver_transaction(hal::byte p_address,
                                  std::span<hal::byte const> p_data_out,
                                  std::span<hal::byte> p_data_in)
  {
    // NOLINTNEXTLINE
    transaction(p_address, p_data_out, p_data_in, hal::never_timeout());
  }
};

namespace v5 {
/**
 * @brief Inter-integrated Circuit (I2C) hardware abstract interface.
 *
 * Also known as Two Wire Interface (TWI) communication protocol. This is a very
 * commonly used protocol for communication with sensors and peripheral devices
 * because it only requires two connections SDA (data signal) and SCL (clock
 * signal). This is possible because the protocol for I2C is addressable.
 *
 * Some devices can utilize clock stretching as a means to pause the i2c
 * controller until the device is ready to respond. To ensure operations with
 * i2c are deterministic and reliably it is advised to NEVER use a clock
 * stretching device in your application.
 *
 */
class i2c
{
public:
  /**
   * @brief Get the clock rate of the i2c bus
   *
   * This is used by device drivers sharing this resource to determine if the
   * clock rate is above what the device can support. If that is the case, the
   * device should throw the exception hal::operation_not_supported to signal
   * that the i2c is not supported for this device.
   *
   * NOTE: if the i2c clock rate is too high for a device on the bus it is
   * likely that the i2c may experience and throw hal::io_errors on any
   * transaction due to devices misinterpreting the bus signals.
   *
   * @returns u32 - returns the i2c device's clock rate
   */
  u32 clock_rate()
  {
    return driver_clock_rate();
  }

  /**
   * @brief Perform an i2c transaction with another device on the bus. The type
   * of transaction depends on values of input parameters. This function will
   * block until the entire transfer is finished.
   *
   * Performing Write, Read and Write-Then-Read transactions depends on which
   * span for data_out and data_in are set to null.
   *
   * - For write transactions, pass p_data_in as an empty span
   * `std::span<hal::byte>{}` and pass a buffer to p_data_out.
   *
   * - For read transactions, pass p_data_out as an empty span
   * `std::span<hal::byte const>{}` and pass a buffer to p_data_in.
   *
   * - For write-then-read transactions, pass a buffer for both p_data_in
   *   p_data_out.
   *
   * - If both p_data_in and p_data_out are empty, simply do nothing and return
   *   success.
   *
   * In the event of arbitration loss, this function will wait for the bus to
   * become free and try again. Arbitration loss means that during the address
   * phase of a transaction 1 or more i2c bus controllers attempted to perform
   * an transaction and one of the i2c bus controllers, that isn't this one won
   * out.
   *
   * @param p_address 7-bit address of the device you want to communicate with.
   * To perform a transaction with a 10-bit address, this parameter must be the
   * address upper byte of the 10-bit address OR'd with 0b1111'0000 (the 10-bit
   * address indicator). The lower byte of the address must be contained in the
   * first byte of the p_data_out span.
   * @param p_data_out data to be written to the addressed device. Set this to a
   * span with `size() == 0` in order to skip writing.
   * @param p_data_in buffer to store read data from the addressed device. Set
   * to nullptr with length 0 in order to skip reading.
   * @throws hal::no_such_device - indicates that no devices on
   * the bus acknowledge the address in this transaction, which could mean that
   * the device is not connected to the bus, is not powered, not available to
   * respond, broken or many other possible outcomes.
   * @throws hal::io_error - indicates that the i2c lines were put into an
   * invalid state during the transaction due to interference, misconfiguration,
   * hardware fault, malfunctioning i2c peripheral or possibly something else.
   * This tends to present a hardware issue and is usually not recoverable.
   */
  void transaction(hal::byte p_address,
                   std::span<hal::byte const> p_data_out,
                   std::span<hal::byte> p_data_in)
  {
    driver_transaction(p_address, p_data_out, p_data_in);
  }

  virtual ~i2c() = default;

private:
  virtual u32 driver_clock_rate() = 0;
  virtual void driver_transaction(hal::byte p_address,
                                  std::span<hal::byte const> p_data_out,
                                  std::span<hal::byte> p_data_in) = 0;
};
}  // namespace v5
}  // namespace hal
