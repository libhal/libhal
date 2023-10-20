// Copyright 2024 Khalil Estell
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

#include "units.hpp"

namespace hal {
/**
 * @brief current sensor hardware abstraction interface
 *
 */
class current_sensor
{
public:
  /**
   * @brief Reads the most up to date current from the sensor
   *
   * @return hal::ampere - amount of current, in ampere's, read from the current
   * sensor.
   */
  [[nodiscard]] hal::ampere read()
  {
    return driver_read();
  }

  virtual ~current_sensor() = default;

private:
  virtual hal::ampere driver_read() = 0;
};
}  // namespace hal
