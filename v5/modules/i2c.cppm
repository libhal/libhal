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

export import strong_ptr;
export import scatter_span;

export import :units;

namespace hal::inline v5 {
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
export class i2c
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
    hertz clock_rate = 100 * si::unit_symbols::kHz;

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
   * other contexts in the meantime. This interface is a shared resource:
   * implementations shared between contexts must serialize transactions.
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
  virtual void driver_configure(settings const& p_settings) = 0;

  virtual void driver_transaction(hal::byte p_address,
                                  mem::scatter_span<hal::byte const> p_data_out,
                                  mem::scatter_span<hal::byte> p_data_in) = 0;
};
}  // namespace hal::inline v5
