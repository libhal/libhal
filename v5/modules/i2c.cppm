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

export module hal:i2c;

export import scatter_span;

import :units;

namespace hal::inline v5 {
/**
 * @brief Settings for an I2C bus
 *
 * A platform's I2C driver takes these from the application when it is
 * constructed. They aren't part of `hal::i2c`, since every device on a bus
 * sees its clock: see `hal::i2c` for why only the application sets the rate.
 */
export struct i2c_settings
{
  /**
   * @brief The serial clock rate in hertz.
   *
   * Every device on the bus sees this rate, so it must be at or below the
   * slowest device's limit. A standard-mode (100 kHz) device can misread
   * faster traffic, even traffic addressed to another device, and corrupt
   * the bus. The default, 100 kHz, is the rate every I2C device supports.
   */
  hertz clock_rate = 100 * si::unit_symbols::kHz;

  /**
   * @brief Enables default comparison
   *
   */
  bool operator<=>(i2c_settings const&) const = default;
};

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
 * # Clock rate
 *
 * There is no way to change the clock rate through this interface. Every
 * device on a bus sees its clock, so the bus must run no faster than its
 * slowest device, and only the application, which knows every device wired
 * to the bus, can choose that rate. It must be chosen before the first
 * transaction: device drivers talk to their devices as soon as they are
 * created, for example to check an ID, and a device that hasn't been set up
 * yet still sees that traffic. Platform drivers take `hal::i2c_settings` when
 * they are constructed. A device driver that needs a slower bus documents its
 * limit for the application instead of changing the rate itself.
 */
export class i2c
{
public:
  /**
   * @brief perform an i2c transaction with another device on the bus. The type
   * of transaction depends on values of input parameters. This function will
   * block until the entire transfer is finished.
   *
   * # Performing Write/Read/Write-Then-Read Operations
   *
   * Performing Write, Read and Write-Then-Read transactions depends on which
   * span for data_out and data_in are set to null.
   *
   * - For write-only transactions, pass the content to be written to the
   * `p_data_out` parameter and provide an empty `p_data_in` parameters.
   *
   * - For read-only transactions, pass an empty `p_data_out` and pass a buffer
   * to p_data_in.
   *
   * - For write-then-read transactions, pass a buffer for both p_data_in
   *   p_data_out.
   *
   * - If both p_data_in and p_data_out are empty, simply do nothing and return
   *   success.
   *
   * # Blocking behavior
   *
   * This call blocks the calling context until the transaction is finished.
   * Implementations that wait on interrupts or DMA should block through a
   * `hal::notifier`, so the installed waiter provider can idle the CPU or run
   * other contexts in the meantime.
   *
   * Implementations don't have to make contexts take turns. One may instead
   * throw `hal::device_or_resource_busy` when a transaction starts while
   * another is in progress. To share a bus between contexts, wrap it in a
   * lock, such as libhal-util's `hal::locked_i2c`.
   *
   * Implementations must bound how long they wait on the bus, for example when
   * a device stretches the clock or holds SDA low indefinitely. That bound is a
   * property of the implementation, typically set at construction, and is not
   * a parameter of this API.
   *
   * # How Arbitration loss is handled
   *
   * In the event of arbitration loss, this function will wait for the bus to
   * become free and try again. Arbitration loss means that during the address
   * phase of a transaction 1 or more i2c bus controllers attempted to perform
   * an transaction and one of the i2c bus controllers, that isn't this one won
   * out. In this situation this call continues to block until the transaction
   * is complete.
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
   * @throws hal::timed_out - indicates that the bus did not complete the
   * transaction within the implementation's bound, for example because a
   * device stretched the clock for too long.
   * @throws hal::device_or_resource_busy - if the implementation doesn't make
   * contexts take turns and another transaction is in progress.
   */
  void transaction(hal::byte p_address,
                   mem::scatter_span<hal::byte const> p_data_out,
                   mem::scatter_span<hal::byte> p_data_in)
  {
    driver_transaction(p_address, p_data_out, p_data_in);
  }

protected:
  ~i2c() = default;

private:
  virtual void driver_transaction(hal::byte p_address,
                                  mem::scatter_span<hal::byte const> p_data_out,
                                  mem::scatter_span<hal::byte> p_data_in) = 0;
};
}  // namespace hal::inline v5
