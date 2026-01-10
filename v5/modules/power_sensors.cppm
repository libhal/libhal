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

export module hal:power_sensors;

export import :units;

namespace hal::inline v5 {
/**
 * @brief current sensor hardware abstraction interface
 *
 */
export class current_sensor
{
public:
  /**
   * @brief Reads the most up to date current from the sensor
   *
   * @return ampere - amount of current, in ampere's, read
   * from the current sensor.
   */
  [[nodiscard]] amperes read()
  {
    return driver_read();
  }

  virtual ~current_sensor() = default;

private:
  virtual amperes driver_read() = 0;
};

/**
 * @brief A voltage sensor hardware abstraction interface
 *
 */
export class volt_sensor
{
public:
  /**
   * @brief Reads the most up to date voltage measured from the sensor
   *
   * @return volts - amount of volts read from the voltage sensor.
   */
  [[nodiscard]] volts read()
  {
    return driver_read();
  }

  virtual ~volt_sensor() = default;

private:
  virtual volts driver_read() = 0;
};

}  // namespace hal::inline v5
