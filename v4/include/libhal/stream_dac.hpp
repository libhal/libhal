#pragma once

#include <concepts>
#include <span>

#include "units.hpp"

namespace hal {
/**
 * @brief Hardware abstraction for a dac that accepts a stream of data
 *
 * This interface can be used for the following:
 *
 * 1. Audio output using unsigned PCM8 or unsigned PCM16 streams
 * 2. Arbitrary waveform generation
 *
 * The driver is likely to use the following to operate:
 *
 * 1. DMA: preferred choice as it removes the need for the CPU to handle the
 *    transfer.
 * 2. SPI (using DMA): preferred when communicating with an external dac
 * 3. Interrupts & Timers: Not recommended but can be used. The cons of this
 *    approach are a low maximum sample rate, sample rate accuracy due to
 *    interrupt & software latency, and it requires cycles from the CPU.
 *
 * Dac device and peripherals with a bit resolution that is not a multiple of
 * 8-bits must ensure that the dac data register is the size of the stream
 * container type and that the used bits are left-justified.
 *
 * Left Justified
 *
 *           8-bit DAC Register:
 *          +---+---+---+---+---+---+---+---+
 *          | D7| D6| D5| D4| D3| D2| D1| X |
 *          +---+---+---+---+---+---+---+---+
 *            ^                           ^
 *            |                           |
 *            Usable DAC Data             Unused Bit
 *            (7 bits)
 *
 * This allows 8-bit data to be inserted into a 7-bit DAC and only lose a single
 * bit of resolution while keeping the interface clean and efficient without the
 * need to perform pre-processing of the data stream. Usually, the unused LSB is
 * ignored by the DAC and thus assigning it a value has no negative effects.
 *
 * Right Justified
 *
 *           8-bit DAC Register:
 *          +---+---+---+---+---+---+---+---+
 *          | X | D7| D6| D5| D4| D3| D2| D1|
 *          +---+---+---+---+---+---+---+---+
 *           ^   ^
 *           |   |
 *        Unused  Usable DAC Data
 *           Bit    (7 bits)
 *
 * Right justified DACs are more complicated to work with because assigning a
 * 8-bit value to such a register will result in the MSB being trimmed off,
 * losing the most significant data. To resolve this, the data must be
 * preprocessed into an additional buffer where each value is shifted to the
 * right by 1, then assigned to the register.
 *
 * A worse scenario is that the unused bit is potentially a flag for the dac
 * peripheral. In this case, the data either needs to be preprocessed into a
 * local buffer with that flag set to the previous value to prevent destroying
 * it. And in some cases, this design simply doesn't make using DMA a reasonable
 * use case.
 *
 * @tparam data_t - container size for the sample data. For such things as PCM8
 * and PCM16, this would be std::uint8_t and std::uint16_t respectively.
 */
template<std::unsigned_integral data_t>
class stream_dac
{
public:
  struct samples
  {
    /**
     * Sample rate sets the update rate of the dac. For example, if the sample
     * rate is 44.1kHz (typical for consumer audio), then the samples will be
     * read from the data array and written to the dac output at that frequency.
     *
     */
    hal::hertz sample_rate;
    /**
     * Span of unsigned integer data to be written to the dac at the rate
     * specified by the sample rate field.
     *
     * If this is empty (empty() == true), then the write command simply
     * returns without updating anything.
     */
    std::span<data_t const> data;
  };

  /**
   * @brief Stream a sequence of unsigned PCM data to the dac
   *
   * This API is a blocking call. Implementations MUST follow the io_concurrency
   * model and accept an io_waiter object. Implementations must utilize
   * interrupts or DMA with an interrupt on transaction complete in order to
   * transfer the data without CPU intervention. When the transfer starts, the
   * io_waiter::wait() function should be called. When the transfer is finished,
   * io_waiter::resume() should be called.
   *
   * This api has a strong exception guarantee, in that, it will throw an
   * exception before it transmits any data to the dac.
   *
   * @param p_samples - sample data to be written to the dac output
   * @throws hal::argument_out_of_domain - when the sample rate is not possible
   * for the DAC. This can happen if:
   *     - The sample rate is above the input clock rate of the DAC making it
   *       impossible to produce.
   *     - The DAC could not reach a sample rate close enough to the desired
   *       sample rate. This can occur if the DAC's source clock and the sample
   *       rates are close and no divider cleanly reaches a reasonable
   *       sample rate for the device. For example, if the desired sample rate
   *       is 750kHz and the clock rate is 1MHz and the divider is an integer,
   *       you would either operate at 1MHz or 500kHz, which are very far away
   *       from the desired sample rate. The driver should, but is not required
   *       to, provide a means to control the allowed "error" between the
   *       desired sample rate and the achievable sample rate in order to tune
   *       which samples are rejected and when an exception is thrown.
   *     - The sample rate is too low and cannot be achieved by the DAC.
   */
  void write(samples const& p_samples)
  {
    driver_write(p_samples);
  }

  virtual ~stream_dac() = default;

private:
  virtual void driver_write(samples const& p_samples) = 0;
};

/**
 * @brief Shorthand for stream_dac<std::uint8_t>
 *
 */
using stream_dac_u8 = stream_dac<std::uint8_t>;

/**
 * @brief Shorthand for stream_dac<std::uint16_t>
 *
 */
using stream_dac_u16 = stream_dac<std::uint16_t>;
}  // namespace hal

namespace hal::v5 {
using hal::stream_dac;
using hal::stream_dac_u16;
using hal::stream_dac_u8;
}  // namespace hal::v5
