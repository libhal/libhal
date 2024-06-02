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
 * 1. Audio output using PCM8 or PCM16 streams
 * 2. Arbitrary waveform generation
 * 3. Single wire data protocols (ex: neopixel LEDs)
 *
 * The driver is likely to use the following to operate:
 *
 * 1. DMA: preferred choice as it removes the need for the CPU to handle the
 *    transfer.
 * 2. SPI (using DMA): preferred when communicating with an external dac
 * 3. Interrupts & Timers: Not recommended but can be used. The CONs of this
 *    approach is a low maximum sample rate, sample rate accuracy due to
 *    interrupt & software latency, and requires cycles from the CPU.
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
 * This allows 8-bit data to be inserted into a 7-bit dac and only lose a single
 * bit of resolution while keeping the interface clean and efficient without the
 * need to perform pre-processing of the data stream.
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
 *
 * hal::stream_dac<std::uint8_t> should only support dacs with
 *
 * @tparam data_t - container size for the sample data. For such things as PCM8
 * and PCM16 this would be std::uint8_t and std::uint16_t respectfully.
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
    std::span<data_t> data;
  };

  /**
   * @brief Write a sequence of data to the dac
   *
   * This API is blocking call. The expectation is that implementors
   *
   * @param p_samples - sample data to be written to the dac output
   * @throws hal::argument_out_of_domain - when the sample rate is not possible
   * for the driver. This can happen if:
   *     - The sample rate is above the input clock rate of the dac making it
   *       impossible to produce.
   *     - The dac could not reach a sample rate close enough to the desired
   *       sample rate. This can occur if the dac's source clock and the sample
   *       rates are close and as such no divider cleanly reaches a reasonable
   *       sample rate for the device. For example, if the desired sample rate
   *       is 750kHz and the clock rate is 1MHz and divider is an integer. You
   *       would either operate at 1MHz or 500kHz which are very far away from
   *       the desired sample rate. The driver should, but is not required, to
   *       provide a means to control the allowed "error" between the desired
   *       sample rate and the achievable sample rate in order tune what
   *       samples are rejected and an exception thrown.
   *     - Sample rate is too low and cannot be achieved by the dac.
   */
  void write(samples p_samples)
  {
    driver_write(p_samples);
  }

private:
  virtual void driver_write(samples p_samples) = 0;
};
}  // namespace hal