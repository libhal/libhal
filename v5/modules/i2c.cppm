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
 *
 * # Stall timeout
 *
 * Every wait a transaction makes on the bus is limited by a stall timeout, so
 * that a stuck bus, for example a device holding SCL or SDA low, can't block
 * a transaction forever. The timeout limits each wait, never the whole
 * transaction, so a long transfer at a slow clock rate never times out on its
 * length alone. An implementation never gives up sooner than its timeout, but
 * may round it up to what its hardware can measure.
 *
 * Like the clock rate, the timeout can't be set through this interface, since
 * every device on the bus shares it. Platform drivers take it when they are
 * constructed, and document their default. The application chooses a timeout
 * longer than:
 *
 * - the longest any device on the bus stretches the clock;
 * - on a bus with more than one controller, the longest transaction any other
 *   controller makes, since a transaction waits for the bus to be free before
 *   it starts.
 *
 * A default of 100 ms is suggested. It is longer than the SMBus clock-low
 * timeout of 25 to 35 ms, and long enough for devices that stretch the clock
 * for tens of milliseconds, such as some humidity sensors while they measure.
 *
 * Some hardware can't detect a stall, or can't measure the timeout the
 * application asks for. Such an implementation documents what it does
 * instead, which may be to wait until the bus recovers. It is up to the
 * application to tolerate that, for example with a watchdog.
 *
 * A device driver shouldn't rely on its device stretching the clock for more
 * than 25 ms, the SMBus limit. If the device has a mode that doesn't stretch
 * the clock, such as starting a measurement and reading the result later, the
 * driver uses it. Otherwise, it documents the stall timeout it needs.
 *
 * # Arbitration loss
 *
 * On a bus with more than one controller, another controller can start a
 * transaction at the same time as this one. Both send their address and data
 * bit by bit until one sends a 1 while the other sends a 0, and the one
 * sending the 0 wins the bus. When the hardware reports that this controller
 * lost, `transaction()` throws `hal::arbitration_lost` at once. It doesn't
 * retry, so the application decides whether, when and how often to try again.
 *
 * On a bus with only one controller, nothing can win arbitration from it, so
 * a lost arbitration means a fault on the bus, such as a device driving SDA
 * out of turn after a reset. Handle it there as `hal::io_error`.
 *
 * Implementations whose hardware can't share a bus with another controller
 * never throw `hal::arbitration_lost`.
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
   * Every wait on the bus is limited by the implementation's stall timeout.
   * See "Stall timeout" in the class description.
   *
   * # Using a bus from more than one thread
   *
   * An implementation may run only one transaction at a time. If a
   * transaction starts while another on the same bus is still running, for
   * example from a second thread, the implementation may throw
   * `hal::device_or_resource_busy`. To use one bus from several threads or
   * contexts, wrap it in libhal-util's `hal::thread_safe_i2c`, which holds a
   * lock for the whole transaction so that callers wait instead.
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
   * Also thrown when the bus stays busy for longer than the stall timeout
   * before this call can send its start condition, for example because a
   * device or another controller holds SCL or SDA low.
   * @throws hal::timed_out - the bus made no progress for longer than the
   * stall timeout after this call sent its start condition, for example
   * because the device stretched the clock. The exception's `progress` is how
   * many bytes are known to have been transferred, counting `p_data_out`
   * first and then `p_data_in`. More may have been, so `progress` is a lower
   * bound. An implementation that can't tell when its start condition was
   * sent reports a stall at any point as `hal::timed_out`.
   * @throws hal::arbitration_lost - another controller won the bus during
   * this transaction. See "Arbitration loss" in the class description.
   * @throws hal::device_or_resource_busy - if another transaction on this bus
   * is still running. Wrap the bus in `hal::thread_safe_i2c` to make callers
   * wait instead.
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
