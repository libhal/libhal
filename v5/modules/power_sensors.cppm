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

export import async_context;
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
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<amperes> - amount of current, in amperes, read from
   * the current sensor.
   */
  [[nodiscard]] async::future<amperes> read(async::context& p_context)
  {
    return driver_read(p_context);
  }

  virtual ~current_sensor() = default;

private:
  virtual async::future<amperes> driver_read(async::context& p_context) = 0;
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
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<volts> - amount of volts read from the voltage
   * sensor.
   */
  [[nodiscard]] async::future<volts> read(async::context& p_context)
  {
    return driver_read(p_context);
  }

  virtual ~volt_sensor() = default;

private:
  virtual async::future<volts> driver_read(async::context& p_context) = 0;
};

}  // namespace hal::inline v5
