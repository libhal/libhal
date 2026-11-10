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

module;

#include <chrono>

export module hal:i2c;

export import scatter_span;

import :units;

namespace hal::inline v5 {
/**
 * @brief Settings for an I2C bus
 *
 * A `hal::i2c` implementation takes these from the application when it is
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
   * @brief How much longer than twice its own length a transaction may take
   *
   * It covers the time devices hold SCL low to stretch the clock, and the time
   * the transaction waits on the calling context, for example while an
   * interrupt runs. See "Timeout" in `hal::i2c`. The default, 1 ms, suits
   * devices that stretch the clock briefly or not at all, on a bus used from a
   * context that isn't interrupted for long.
   */
  time_duration slack = std::chrono::milliseconds(1);

  /**
   * @brief Enables default equality comparison
   *
   */
  bool operator==(i2c_settings const&) const = default;
};

/**
 * @brief Inter-integrated Circuit (I2C) hardware abstract interface.
 *
 * Also known as Two Wire Interface (TWI). It is widely used for sensors and
 * other peripherals, since many devices can share one bus of two wires, SDA
 * (data) and SCL (clock), each at its own address.
 *
 * A device can stretch the clock, holding SCL low to pause the transaction
 * until it is ready to respond. Stretching counts toward the transaction's
 * time limit, described below. While a device stretches the clock, it has the
 * bus to itself, and no other device can be serviced. Prefer devices and modes
 * that don't stretch the clock, such as polling a status flag, which leaves
 * the bus free for other devices between polls.
 *
 * # Clock rate
 *
 * An implementation takes the clock rate in `hal::i2c_settings` when it is
 * constructed, and may offer its own function to change it later. There is no
 * way to change it through this interface. Every device on a bus sees its
 * clock, so the bus must run no faster than its slowest device. Only the
 * application knows every device wired to the bus, so only it can choose that
 * rate. It must be set before the first transaction. Device drivers talk to
 * their devices as soon as they are created, for example to check a device ID.
 * Every device on the bus sees that traffic, even one whose driver hasn't been
 * created yet. A device driver that needs a slower bus documents its limit for
 * the application instead of changing the rate itself.
 *
 * # Timeout
 *
 * A transaction may take twice its own length at the clock rate, plus the
 * slack in `hal::i2c_settings`. If it hasn't finished by then, counting from
 * when `transaction()` was called, it throws `hal::timed_out`. This keeps a
 * stuck bus, for example a device holding SCL low, from blocking the caller
 * forever.
 *
 * Its own length is 9 clock cycles for every byte, address bytes included,
 * plus one for each start and stop condition. A write-then-read sends the
 * address twice and has a repeated start. Writing 2 bytes, for example, is 3
 * bytes of 9 cycles plus a start and a stop: 29 cycles, or 0.29 ms at 100 kHz.
 * Doubling the length allows for a bus that runs slower than its clock rate,
 * for example because a device stretches the clock a little on every byte. The
 * limit grows with the transaction, so a long transfer at a slow clock rate
 * never times out on its length alone.
 *
 * The slack covers the rest:
 *
 * - Clock stretching: how long a device holds SCL low during a transaction,
 *   beyond a little on each byte. For example, the Sensirion SHT21 in
 *   hold-master mode holds SCL low while it measures, which takes up to 85 ms.
 * - Waiting on the calling context: if an interrupt or another thread runs in
 *   the middle of a transaction, the bus waits for it. On a bus used from a
 *   thread that can be preempted, the slack must be longer than the longest
 *   preemption.
 *
 * The default slack, 1 ms, suits devices that stretch the clock briefly or not
 * at all. A device driver whose device stretches the clock for longer declares
 * how long, so the application can add it to the slack. libhal-util's
 * `hal::i2c_bus_slack()` does this for every device on a bus.
 *
 * An implementation never gives up sooner than this limit, but may round it up
 * to what it can measure. Like the clock rate, the slack can't be changed
 * through this interface, since every device on the bus shares it.
 * Implementations take it in `hal::i2c_settings` when they are constructed.
 *
 * Some hardware can't measure time. Such an implementation documents what it
 * does instead, which may be to wait until the bus recovers. It is up to the
 * application to tolerate that, for example with a watchdog.
 *
 * A device driver should avoid stretching the clock for long. If the device
 * has a mode that doesn't stretch the clock, such as starting a measurement
 * and reading the result later, the driver uses it.
 *
 * # A busy bus or lost arbitration
 *
 * `transaction()` throws `hal::resource_unavailable_try_again` in the following
 * circumstances:
 *
 * - The bus is busy when the transaction starts.
 * - The start condition hasn't gone out within the transaction's time limit.
 * - Another controller wins arbitration. When two controllers start at the
 *   same time, the first to send a 1 while the other sends a 0 loses the bus.
 *
 * None of these is an error by itself. Usually another controller has the bus,
 * but a device holding a line low looks the same, and only time tells them
 * apart. Retrying is left to the application, a device driver using the i2c
 * interface, or a wrapper around the bus, which decides how often to try again
 * and how long the bus can stay taken before reporting a fault as
 * `hal::io_error`. On a bus with only one controller, nothing else should take
 * the bus, so the application can treat the exception as a fault at once.
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
   *   `p_data_out` parameter and provide an empty `p_data_in` parameters.
   * - For read-only transactions, pass an empty `p_data_out` and pass a buffer
   *   to p_data_in.
   * - For write-then-read transactions, pass a buffer for both p_data_in
   *   p_data_out.
   * - If both p_data_in and p_data_out are empty, simply do nothing and return.
   *
   * # Blocking behavior
   *
   * This call blocks the calling context until the transaction is finished.
   * Implementations that wait on interrupts or DMA should block through a
   * `hal::notifier`, so the installed waiter provider can idle the CPU or run
   * other contexts in the meantime.
   *
   * The transaction is limited in time. See "Timeout" in the class
   * description.
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
   * There is no 10-bit mode. A device driver may send a 10-bit address by
   * passing `0b111'1000 | (address >> 8)` here, with the address's low byte as
   * the first byte of `p_data_out`. Not all hardware allows this.
   * @param p_data_out data to be written to the addressed device. Set to
   * nullptr with length zero in order to skip writing.
   * @param p_data_in buffer to store read data from the addressed device. Set
   * to nullptr with length 0 in order to skip reading.
   * @throws hal::no_such_device - indicates that no devices on
   * the bus acknowledge the address in this transaction, which could mean that
   * the device is not connected to the bus, is not powered, not available to
   * respond, broken or many other possible outcomes.
   * @throws hal::broken_pipe - the device acknowledged its address, then didn't
   * acknowledge a byte of `p_data_out`, or the read address of a
   * write-then-read. This can occur if the device is busy, does not understand
   * what it was sent, or may not be able to take any more data. The transaction
   * stops, and nothing more is sent. The exception's `progress` field is how
   * many bytes of `p_data_out` the device acknowledged. With a 10-bit address,
   * a `progress` of 0 means the device didn't acknowledge the address's low
   * byte.
   * @throws hal::io_error - indicates that the i2c lines were put into an
   * invalid state during the transaction due to interference, misconfiguration,
   * hardware fault, malfunctioning i2c peripheral or possibly something else.
   * This tends to present a hardware issue and may not be recoverable.
   * Implementations throw it only when they detect a bus error, such as a
   * misplaced start or stop condition. A wrapper around the bus may also throw
   * it for a bus that stays taken for too long.
   * @throws hal::timed_out - this call sent its start condition, then didn't
   * finish within its time limit, for example because the device stretched
   * the clock for longer than the slack. See "Timeout" in the class
   * description. The exception's `progress` field is how many bytes are known
   * to have been transferred, counting `p_data_out` first and then
   * `p_data_in`. More may have been, so `progress` is a lower bound. An
   * implementation that can't tell when its start condition was sent reports
   * running out of time at any point as `hal::timed_out`.
   * @throws hal::resource_unavailable_try_again - the bus was busy when this
   * call started, its start condition didn't go out within the time limit, or
   * another controller won the bus during it. See "A busy bus or lost
   * arbitration" in the class description.
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
