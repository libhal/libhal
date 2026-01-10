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

export module hal:temperature_sensor;

export import mp_units;

using namespace mp_units;

export namespace hal::inline v5 {
/**
 * @brief Interface for acquiring temperature samples from a device.
 *
 */
class temperature_sensor
{
public:
  /**
   * @brief Read the current temperature measured by the device
   *
   * @return celsius - Measured temperature
   */
  [[nodiscard]] quantity<si::kelvin, float> read()
  {
    return driver_read();
  }

  virtual ~temperature_sensor() = default;

private:
  virtual quantity<si::kelvin, float> driver_read() = 0;
};
}  // namespace hal::inline v5
