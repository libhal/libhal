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

export module hal:temperature_sensor;

export import async_context;
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
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<kelvin> - Measured temperature
   */
  [[nodiscard]] async::future<kelvin> read(async::context& p_context)
  {
    return driver_read(p_context);
  }

  virtual ~temperature_sensor() = default;

private:
  virtual async::future<kelvin> driver_read(async::context& p_context) = 0;
};
}  // namespace hal::inline v5
