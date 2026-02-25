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
  async::future<void> configure(async::context& p_context,
                                settings const& p_settings)
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
  async::future<void> transaction(async::context& p_context,
                                  hal::byte p_address,
                                  scatter_span<hal::byte const> p_data_out,
                                  scatter_span<hal::byte> p_data_in)
  {
    return driver_transaction(p_context, p_address, p_data_out, p_data_in);
  }

  virtual ~i2c() = default;

private:
  virtual async::future<void> driver_configure(async::context& p_context,
                                               settings const& p_settings) = 0;

  /// Implementors of this virtual API should simply call the concrete class's
  /// implementation of driver_transaction() without the p_timeout parameter and
  /// drop the p_timeout parameter.
  virtual async::future<void> driver_transaction(
    async::context& p_context,
    hal::byte p_address,
    scatter_span<hal::byte const> p_data_out,
    scatter_span<hal::byte> p_data_in) = 0;
};
/**
 * @brief I2C memory-mapped target (target) hardware abstraction.
 *
 * This interface enables a device to operate as an I2C target with a
 * memory-mapped register model. The I2C bus controller can read from and write
 * to the device's memory region using standard I2C transactions, similar to how
 * hardware peripherals expose configuration and data registers.
 *
 * Internally, implementations MUST use a double-buffered memory scheme to
 * eliminate race conditions between the I2C bus and the application. The
 * application and I2C driver always operate on separate buffers, with pointer
 * swaps occurring on I2C STOP conditions to commit changes atomically.
 *
 * The `write` function updates the memory region visible to the I2C bus
 * controller. After a successful write, any subsequent I2C read transaction
 * targeting the updated address range will observe the new data. For
 * multi-field updates that must appear atomically to the bus controller, all
 * fields should be written in a single `write` call.
 *
 * The `read` function retrieves the latest data written by the I2C bus
 * controller. The returned data reflects the state of the memory at the most
 * recent I2C STOP condition. If a transaction is currently in progress, the
 * read will suspend until the transaction completes and the committed buffer is
 * updated.
 *
 * The memory region size is fixed at construction time and determined by the
 * the driver. It is good practice to provide a means to the application to set
 * the size of the memory map region using an allocator of their choosing.
 * Addresses are zero-indexed offsets into the memory region. Writes or reads
 * that exceed the memory region boundary will return a byte count less than the
 * requested length.
 *
 */
export class i2c_mmap_target
{
public:
  /**
   * @brief Describes the type of I2C transaction that completed.
   *
   */
  enum struct operation : u8
  {
    /// I2C bus controller performed a read from this device
    read = 0,
    /// I2C bus controller performed a write to this device
    write,
  };

  /**
   * @brief Listener for completed I2C transactions on this target.
   *
   * Implementations of this class receive notifications after an I2C
   * transaction has completed (STOP condition detected). The notification
   * includes the type of operation, the starting address, and the number of
   * bytes transferred.
   *
   * The `do_notify` virtual function is called from ISR context.
   * Implementations must not throw exceptions, must not perform I/O operations,
   * and must return as quickly as possible. If the application only cares about
   * a subset of operations, the implementation should check `p_operation` and
   * return early for uninteresting events.
   *
   */
  class transaction_complete_listener
  {
  public:
    /**
     * @brief Notify the listener that an I2C transaction has completed.
     *
     * Called by the driver from an interrupt context after an I2C STOP
     * condition is detected. The listener receives the operation type, starting
     * address, and byte count of the completed transaction.
     *
     * @param p_operation - whether the bus controller performed a read or write
     * @param p_address - zero-indexed starting address of the transaction
     * @param p_length - number of bytes transferred in the transaction
     */
    void notify(operation p_operation, usize p_address, usize p_length) noexcept
    {
      return do_notify(p_operation, p_address, p_length);
    }

  private:
    /**
     * @brief Implementation of the transaction notification.
     *
     * Called from an interrupt context. Must not throw. Must not perform I/O.
     * Must return quickly.
     *
     * @param p_operation - whether the bus controller performed a read or write
     * @param p_address - zero-indexed starting address of the transaction
     * @param p_length - number of bytes transferred in the transaction
     */
    virtual void do_notify(operation p_operation,
                           usize p_address,
                           usize p_length) = 0;
  };

  /**
   * @brief Register a listener for completed I2C transactions.
   *
   * The listener will be notified from ISR context after each I2C STOP
   * condition. Only one listener may be registered at a time; calling this
   * function replaces any previously registered listener. The strong_ptr
   * ensures the listener remains alive for the lifetime of the driver.
   *
   * @param p_listener - the transaction complete listener to register
   */
  void on_transaction_complete(
    mem::strong_ptr<transaction_complete_listener> p_listener)
  {
    return driver_on_transaction_complete(p_listener);
  }

  /**
   * @brief Returns the size of the memory-mapped region in bytes.
   *
   * The size is fixed at construction time . Valid addresses for read and write
   * operations range from 0 to size() - 1.
   *
   * @return usize - the size of the memory region in bytes
   */
  usize size()
  {
    return driver_size();
  }

  /**
   * @brief Write data into the memory region visible to the I2C bus controller.
   *
   * Updates the memory-mapped region starting at `p_address` with the contents
   * of `p_data`. After this call returns, any subsequent I2C read transaction
   * targeting the written address range will observe the new data.
   *
   * If multiple fields must appear atomically to the I2C bus controller (e.g.
   * a multi-byte sensor reading), all fields must be provided in a single call
   * to `write`. Individual calls to `write` are not guaranteed to be visible
   * atomically with respect to each other.
   *
   * If the write extends beyond the end of the memory region, only the bytes
   * that fit within the region are written. The returned byte count reflects
   * the number of bytes actually written.
   *
   * @param p_context - async context for suspending if a buffer swap is pending
   * @param p_address - zero-indexed starting address to write to
   * @param p_data - data to write into the memory region
   * @return async::future<usize> - the number of bytes written
   */
  async::future<usize> write(async::context& p_context,
                             usize p_address,
                             scatter_span<hal::byte const> p_data)
  {
    return driver_write(p_context, p_address, p_data);
  }

  /**
   * @brief Read data from the memory region as last written by the I2C bus
   * controller.
   *
   * Returns the contents of the memory region starting at `p_address` as of
   * the most recently completed I2C write transaction (most recent STOP
   * condition). If an I2C transaction is currently in progress, this call will
   * suspend until the transaction completes and the committed buffer is
   * available.
   *
   * If the read extends beyond the end of the memory region, only the bytes
   * within the region are returned. The returned byte count reflects the number
   * of bytes actually read.
   *
   * @param p_context - async context for suspending until the bus is idle
   * @param p_address - zero-indexed starting address to read from
   * @param p_data - buffer to store the read data
   * @return async::future<usize> - the number of bytes read
   */
  async::future<usize> read(async::context& p_context,
                            usize p_address,
                            scatter_span<hal::byte> p_data)
  {
    return driver_read(p_context, p_address, p_data);
  }

  virtual ~i2c_mmap_target() = default;

private:
  virtual void driver_on_transaction_complete(
    mem::strong_ptr<transaction_complete_listener> p_listener) = 0;

  virtual usize driver_size() = 0;

  virtual async::future<usize> driver_write(
    async::context& p_context,
    usize p_address,
    scatter_span<hal::byte const> p_data) = 0;

  virtual async::future<usize> driver_read(async::context& p_context,
                                           usize p_address,
                                           scatter_span<hal::byte> p_data) = 0;
};
}  // namespace hal::inline v5
