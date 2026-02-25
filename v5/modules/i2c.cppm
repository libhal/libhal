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

#include <span>

export module hal:i2c;

export import async_context;
export import strong_ptr;

export import :units;
export import :scatter_span;

using namespace mp_units;
using namespace mp_units::si::unit_symbols;

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
    hertz clock_rate = 100 * kHz;

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
  async::task configure(async::context& p_context, settings const& p_settings)
  {
    return driver_configure(p_context, p_settings);
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
  async::task transaction(async::context& p_context,
                          hal::byte p_address,
                          scatter_span<hal::byte const> p_data_out,
                          scatter_span<hal::byte> p_data_in)
  {
    return driver_transaction(p_context, p_address, p_data_out, p_data_in);
  }

  virtual ~i2c() = default;

private:
  virtual async::task driver_configure(async::context& p_context,
                                       settings const& p_settings) = 0;

  /// Implementors of this virtual API should simply call the concrete class's
  /// implementation of driver_transaction() without the p_timeout parameter and
  /// drop the p_timeout parameter.
  virtual async::task driver_transaction(
    async::context& p_context,
    hal::byte p_address,
    scatter_span<hal::byte const> p_data_out,
    scatter_span<hal::byte> p_data_in) = 0;
};

export class i2c_mmap_target
{
public:
  enum struct operation : u8
  {
    read = 0,
    write,
  };

  class update_handler
  {
  public:
    void callback(operation p_operation,
                  usize p_address,
                  usize p_length) noexcept
    {
      return do_callback(p_operation, p_address, p_length);
    }

  private:
    virtual void do_callback(operation p_operation,
                             usize p_address,
                             usize p_length) = 0;
  };

  void on_update(mem::strong_ptr<update_handler> p_update_handler)
  {
    return driver_on_update(p_update_handler);
  }

  usize size()
  {
    return driver_size();
  }

  async::future<usize> write(async::context& p_context,
                             usize p_address,
                             scatter_span<hal::byte const> p_data)
  {
    return driver_write(p_context, p_address, p_data);
  }

  async::future<usize> read(async::context& p_context,
                            usize p_address,
                            scatter_span<hal::byte> p_data)
  {
    return driver_read(p_context, p_address, p_data);
  }

  virtual ~i2c_mmap_target() = default;

private:
  virtual usize driver_size() = 0;
  virtual async::future<usize> driver_write(
    async::context& p_context,
    usize p_address,
    scatter_span<hal::byte const> p_data) = 0;
  virtual async::future<usize> driver_read(async::context& p_context,
                                           usize p_address,
                                           scatter_span<hal::byte> p_data) = 0;
  virtual void driver_on_update(
    mem::strong_ptr<update_handler> p_update_handler) = 0;
};
}  // namespace hal::inline v5
