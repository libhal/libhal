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

#include <optional>

#include "functional.hpp"

namespace hal {
/**
 * @brief An interface for customizing the behavior of drivers when they must
 * wait for I/O to complete.
 *
 * All drivers that utilize DMA or interrupts to perform work while the CPU
 * idles, should accept an io_waiter object as an input parameter and call the
 * wait function when the CPU would normally spin in a polling loop for a
 * considerable amount of time. This driver allows applications to optimize the
 * amount of time the CPU spends doing meaningful work. Utilizing an io_waiter
 * allows users of a driver to optimize the time the driver spends waiting for
 * operations, specifically it can be used for context switching, putting the
 * device to sleep, and doing small amounts of work.
 *
 * An exception to this rule would be drivers like non-dma spi or uart where the
 * most efficient way to reach maximum throughput is to poll a register until
 * the individual byte has been transferred.
 *
 * This is a standard interface for libhal drivers that allows operating systems
 * and applications to customize the behavior of drivers when they wait for
 * their I/O to complete. For example, if a driver performs some operation over
 * DMA and must poll on a register or wait for an interrupt to fire before it
 * can move on from that transaction, then the driver ought to have accepted an
 * io_waiter from its constructor and called that object's `wait()` function.
 */
class io_waiter
{
public:
  /**
   * @brief Execute this function when your driver is waiting on something
   *
   * The wait function should do one of the following:
   *
   * 1. Block the current thread of execution, if an RTOS or OS is used.
   * 2. Put the system to sleep.
   * 3. Perform a small chunk of work.
   *
   * Care should be taken when using an io_waiter for performing work. The work
   * performed could exceed the time for a response or a transfer to complete.
   * In some scenarios, this delay is harmless. But in some timing-sensitive
   * situations, this could cause issues. Consider decoding MP3 data and
   * streaming it to a DAC. Technically, the wait function could be used to
   * prepare the next buffer for the DAC to stream, but if the decoding takes
   * too long, then the DAC will be starved of data, the voltage stays the same
   * until the decoding task is finished and a new stream of data is sent out.
   * That break in the audio will sound like a pop or click and will make the
   * audio choppy.
   *
   * If this API is used for doing chunks of work, then `resume()` can be an
   * empty function as there is nothing needed to resume the current thread of
   * execution.
   *
   * If this API is used for going to sleep, implement `resume()` if it is
   * necessary to wake from sleep.
   *
   * If this API is used for context switching, block the current task using the
   * lightest weight construct that the OS provides. That may be a semaphore, it
   * could be a task notification, a signal, or a mutex.
   *
   * To use the `wait` API in your driver, provide a member boolean variable
   * labelled something like `m_finished`. That flag will notify the driver code
   * that the transaction has finished. The wait() function may return before
   * the transaction is finished, so a loop is required.
   *
   * USAGE:
   *
   *      while (not m_finished) {
   *          m_waiter.wait();
   *      }
   *
   */
  void wait()
  {
    driver_wait();
  }

  /**
   * @brief Execute this function within an interrupt service routine to resume
   *        normal operation.
   *
   * The resume function should do one of the following:
   *
   * 1. Unblock the previously blocked thread of execution.
   * 2. Wake the device from sleep if necessary
   *
   * Care should be taken when unblocking a thread as it is likely that the
   * resume api is being called in an interrupt context. As such the resume
   * API MUST NOT THROW! The resume implementation should be as short as
   * possible to achieve the action of resuming the thread of execution. Using
   * resume to perform any lengthy work, calculations, or to control hardware is
   * strictly forbidden as this can cause potential race conditions and
   * starvation of the scheduler/application.
   *
   * The resume implementation should try as much as possible to ensure that no
   * exceptions can be thrown within any api called within it. This should be
   * avoided even if the terminate handler for the application is OS aware and
   * only terminates the threads in which an exception was thrown. Even in this
   * case, because resume can be called in an interrupt context it could have
   * been fired at any point during any task, which would improperly terminate
   * the wrong task.
   *
   */
  void resume() noexcept
  {
    driver_resume();
  }

  virtual ~io_waiter() = default;

private:
  virtual void driver_wait() = 0;
  virtual void driver_resume() noexcept = 0;
};

/**
 * @brief Returns a reference to a statically allocated polling io waiter
 *
 * Polling io waiter is a waiter that does nothing. It simply returns when wait
 * or resume are called, causing the driver to poll its ready flag until it is
 * completed. This should be used as a default io waiter type for drivers so
 * the user does not have to manually pass the polling io waiter in the code.
 * This waiter can be used in applications where polling is an acceptable option
 * during a driver's waiting period.
 *
 * @return io_waiter& - reference to statically allocated polling io waiter
 * object.
 */
inline io_waiter& polling_io_waiter()
{
  class polling_io_waiter_t : public hal::io_waiter
  {
    void driver_wait() final
    {
      // Do nothing and allow the outer loop of the driver to poll its ready
      // flag.
      return;
    }
    void driver_resume() noexcept final
    {
      // Since wait didn't do anything, neither does resume.
      return;
    }
  };

  static polling_io_waiter_t waiter;

  return waiter;
}
namespace v5 {
/**
 * @brief An interface for customizing the behavior of drivers when they must
 * wait for I/O to complete.
 *
 * All drivers that utilize DMA or interrupts to perform work while the CPU
 * idles, should accept an io_waiter object as an input parameter and call the
 * wait function when the CPU would normally spin in a polling loop for a
 * considerable amount of time. This driver allows applications to optimize the
 * amount of time the CPU spends doing meaningful work. Utilizing an io_waiter
 * allows users of a driver to optimize the time the driver spends waiting for
 * operations, specifically it can be used for context switching, putting the
 * device to sleep, and doing small amounts of work.
 *
 * An exception to this rule would be drivers like non-dma spi or uart where the
 * most efficient way to reach maximum throughput is to poll a register until
 * the individual byte has been transferred.
 *
 * This is a standard interface for libhal drivers that allows operating systems
 * and applications to customize the behavior of drivers when they wait for
 * their I/O to complete. For example, if a driver performs some operation over
 * DMA and must poll on a register or wait for an interrupt to fire before it
 * can move on from that transaction, then the driver ought to have accepted an
 * io_waiter from its constructor and called that object's `wait()` function.
 */
class io_waiter
{
public:
  /**
   * @brief Disambiguation tag object for io_waiter on wait callbacks
   *
   */
  struct on_wait_tag
  {};

  using on_wait_handler = hal::callback<void(on_wait_tag)>;

  /**
   * @brief Execute this function when your driver is waiting on something
   *
   * The wait function should do one of the following:
   *
   * 1. Block the current thread of execution, if an RTOS or OS is used.
   * 2. Put the system to sleep.
   * 3. Perform a small chunk of work.
   *
   * Care should be taken when using an io_waiter for performing work. The work
   * performed could exceed the time for a response or a transfer to complete.
   * In some scenarios, this delay is harmless. But in some timing-sensitive
   * situations, this could cause issues. Consider decoding MP3 data and
   * streaming it to a DAC. Technically, the wait function could be used to
   * prepare the next buffer for the DAC to stream, but if the decoding takes
   * too long, then the DAC will be starved of data, the voltage stays the same
   * until the decoding task is finished and a new stream of data is sent out.
   * That break in the audio will sound like a pop or click and will make the
   * audio choppy.
   *
   * If this API is used for doing chunks of work, then `resume()` can be an
   * empty function as there is nothing needed to resume the current thread of
   * execution.
   *
   * If this API is used for going to sleep, implement `resume()` if it is
   * necessary to wake from sleep.
   *
   * If this API is used for context switching, block the current task using the
   * lightest weight construct that the OS provides. That may be a semaphore, it
   * could be a task notification, a signal, or a mutex.
   *
   * To use the `wait` API in your driver, provide a member boolean variable
   * labelled something like `m_finished`. That flag will notify the driver code
   * that the transaction has finished. The wait() function may return before
   * the transaction is finished, so a loop is required.
   *
   * USAGE:
   *
   *      while (not m_finished) {
   *          m_waiter.wait();
   *      }
   *
   */
  void wait()
  {
    driver_wait();
  }

  /**
   * @brief Execute this function within an interrupt service routine to resume
   *        normal operation.
   *
   * The resume function should do one of the following:
   *
   * 1. Unblock the previously blocked thread of execution.
   * 2. Wake the device from sleep if necessary
   *
   * Care should be taken when unblocking a thread as it is likely that the
   * resume api is being called in an interrupt context. As such the resume
   * API MUST NOT THROW! The resume implementation should be as short as
   * possible to achieve the action of resuming the thread of execution. Using
   * resume to perform any lengthy work, calculations, or to control hardware is
   * strictly forbidden as this can cause potential race conditions and
   * starvation of the scheduler/application.
   *
   * The resume implementation should try as much as possible to ensure that no
   * exceptions can be thrown within any api called within it. This should be
   * avoided even if the terminate handler for the application is OS aware and
   * only terminates the threads in which an exception was thrown. Even in this
   * case, because resume can be called in an interrupt context it could have
   * been fired at any point during any task, which would improperly terminate
   * the wrong task.
   *
   */
  void resume() noexcept
  {
    driver_resume();
  }

  /**
   * @brief Inject a function to call before waiting on I/O
   *
   * This function allows application code to inject functionality into an I/O
   * waiter before performing a wait. This is useful for stacking multiple
   * waiting operations in a single thread OR throwing an exception out of an
   * io_waiter if a deadline violation has occurred.
   *
   * @param p_callback - A function to be called before waiting on I/O
   */
  void on_wait(std::optional<on_wait_handler>& p_callback)
  {
    driver_on_wait(p_callback);
  }

  virtual ~io_waiter() = default;

private:
  virtual void driver_wait() = 0;
  virtual void driver_resume() noexcept = 0;
  virtual void driver_on_wait(std::optional<on_wait_handler>& p_callback) = 0;
};

/**
 * @brief Returns a reference to a statically allocated polling io waiter
 *
 * Polling io waiter is a waiter that does nothing. It simply returns when wait
 * or resume are called, causing the driver to poll its ready flag until it is
 * completed. This should be used as a default io waiter type for drivers so
 * the user does not have to manually pass the polling io waiter in the code.
 * This waiter can be used in applications where polling is an acceptable option
 * during a driver's waiting period.
 *
 * @return io_waiter& - reference to statically allocated polling io waiter
 * object.
 */
inline io_waiter& polling_io_waiter()
{
  class polling_io_waiter_t : public hal::v5::io_waiter
  {
    void driver_wait() override
    {
      if (m_callback) {
        m_callback.value()(on_wait_tag{});
      }

      // Do nothing and allow the outer loop of the driver to poll its ready
      // flag.
      return;
    }

    void driver_resume() noexcept override
    {
      // Since wait didn't do anything, neither does resume.
      return;
    }

    void driver_on_wait(
      std::optional<hal::callback<void(on_wait_tag)>>& p_callback) override
    {
      m_callback = p_callback;
    }

    std::optional<hal::callback<void(on_wait_tag)>> m_callback;
  };

  static polling_io_waiter_t waiter;

  return waiter;
}
}  // namespace v5
}  // namespace hal
