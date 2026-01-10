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

export module hal:analog;

export import :units;
export import async_context;

namespace hal::inline v5 {
/**
 * @brief 16-bit Analog to Digital Converter (ADC) hardware abstraction
 * interface.
 *
 * Use this interface for devices and peripherals that can convert analog
 * voltage signals into a digital number.
 *
 * ADC peripheral only know the proportion of a voltage signal relative to a Vss
 * (negative reference) and a Vcc (positive reference) and thus cannot describe
 * the voltage directly.
 *
 * This interface is meant for ADCs of 16-bits and below. Most common ADCs fall
 * into this category.
 */
export class adc16
{
public:
  /**
   * @brief Sample the analog to digital converter and return the result
   *
   * Is guaranteed by the implementing driver to be between 0 and 65535
   * (0xFFFF). The value representing the voltage measured by the ADC from Vss
   * (negative reference) to Vcc (positive reference).
   *
   * For example, if Vss is 0V (gnd) and Vcc is 5V then a value of 32767
   * (0x7FFF) (half of 65535) would mean a measured voltage of 2.5V.
   *
   * In the case where there ADC's resolution is below 16-bits, the
   * implementation is required to perform bit upscaling via bit duplications.
   *
   * For example consider an 8-bit ADC returning a sample of 0x5A. This would be
   * upscaled to 0x5A5A by shifting the value to the MSB of the u16, then
   * duplicating the bits to lower 8-bits. This can be done with any bit width
   * ADC.
   *
   * For a 10-bit ADC the bit pattern in the u16 value would look like the
   * following if we denote each bit of the 10-bit ADC with aX where X is the
   * sampled bit's value.
   *
   * ```
   * u16 content = [ a9 a8 a7 a6 a5 a4 a3 a2 a1 a0 | a9 a8 a7 a6 a5 a4 ]
   *        bits =   15 14 13 12 11 10  9  8  7  6 |  5  4  3  2  1  0
   * ```
   *
   * Using this upscaling method is fast, reduces proportionality distortion and
   * ensures that an ADC sample of 0 results in a u16 value of zero and a
   * sample of all 1s, the maximum adc sample value, will return as 0xFFFF (the
   * maximum value for a u16 integer).
   *
   * @param p_context - async context for the operation
   * @return async::future<u16> - the sampled adc value upscaled to u16
   */
  async::future<u16> read(async::context& p_context)
  {
    return driver_read(p_context);
  }

  virtual ~adc16() = default;

private:
  virtual async::future<u16> driver_read(async::context& p_context) = 0;
};

/**
 * @brief 24-bit Analog to Digital Converter (ADC) hardware abstraction
 * interface.
 *
 * Use this interface for devices and peripherals that can convert analog
 * voltage signals into a digital number.
 *
 * ADC peripheral only know the proportion of a voltage signal relative to a Vss
 * (negative reference) and a Vcc (positive reference) and thus cannot describe
 * the voltage directly.
 *
 * This interface is meant for ADCs with precision of 17-bits to 24-bits. This
 * is interface is meant for high precision ADCs
 *
 */
export class adc24
{
public:
  /**
   * @brief Sample the analog to digital converter and return the result
   *
   * Is guaranteed by the implementing driver to be between 0 and 16777215.
   * The value representing the voltage measured by the ADC from Vss (negative
   * reference) to Vcc (positive reference).
   *
   * For example, if Vss is 0V (gnd) and Vcc is 5V then a value of 8388607 (half
   * of 16777215) would mean a measured voltage of 2.5V.
   *
   * See `hal::adc16` for details about how ADCs with precision below 24 are
   * upscaled to match the necessary precision.
   *
   * @param p_context - async context for the operation
   * @return async::future<u32> - the sampled adc value upscaled to 24-bits
   */
  async::future<u32> read(async::context& p_context)
  {
    return driver_read(p_context);
  }

  virtual ~adc24() = default;

private:
  virtual async::future<u32> driver_read(async::context& p_context) = 0;
};
/**
 * @brief 16-bit Digital to Analog Converter (DAC) hardware abstraction
 * interface.
 *
 * Use this interface for devices and peripherals that can create arbitrary
 * analog voltages between a defined Vss (negative reference) and Vcc (positive
 * reference) voltage.
 *
 * This interface can represent DAC's with precision 16-bits and below. See the
 * `hal::dac16::write()` API for more details.
 */
export class dac16
{
public:
  /**
   * @brief Set the output voltage of the DAC.
   *
   * The input value `p_percentage` is a 16-bit unsigned number from 0 (0x0000)
   * to 65535 (0xFFFF).
   *
   * The floating point value is linearly proportional to the output voltage
   * relative to the Vss and Vcc such that if Vss is 0V (gnd) and Vcc is 5V
   * then:
   *
   *   - 0.000V (0.000%) is 65535 * 0.000 = 0
   *   - 1.250V (25.00%) is 65535 * 0.250 = 16383
   *   - 2.225V (44.50%) is 65535 * 0.445 = 29163
   *   - 5.000V (100.0%) is 65535 * 1.000 = 65535
   *
   * For drivers where the underlying hardware has a DAC precision below
   * 16-bits, the behavior of this API is to set the DAC output register value
   * to the most significant bits of the u16 value. This preserves the most
   * significant information about the intended dac percentage value.
   *
   * @param p_context - async context for the operation
   * @param p_percentage - value from 0 (0x0000) to 65535 (0xFFFF) representing
   * the proportion of the output voltage from the Vss to Vcc.
   */
  async::future<void> write(async::context& p_context, u16 p_percentage)
  {
    return driver_write(p_context, p_percentage);
  }

  virtual ~dac16() = default;

private:
  virtual async::future<void> driver_write(async::context& p_context,
                                           u16 p_percentage) = 0;
};

// NOTE: If a dac24, for dac precisions between 17 and 24, is desired by
// developers, we can add that interface in. Not adding it now as to not add
// more unused interfaces.
}  // namespace hal::inline v5
