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

#include "units.hpp"

namespace hal {
/**
 * @brief Hardware abstraction for a closed loop position controlled rotational
 * actuator
 *
 */
class servo
{
public:
  /**
   * @brief Set the position of the servo's output shaft
   *
   * Position is the rotational position as a angle in degrees that the caller
   * wants the shaft to rotate to. The allowed range of positions is defined by
   * the servo itself. Many servos have intrinsic limits to their range.
   *
   * Developers must choose servos that fit the range for their applications.
   * Applications must clearly define the range that they require in order to
   * perform correctly.
   *
   * The velocity in which the servo shaft moves is not defined by this function
   * but is either intrinsic to the servo or a configuration of the servo.
   *
   * @param p_position - the position to move the servo shaft in degrees.
   * @throws hal::argument_out_of_domain - when position exceeds the range of
   * the servo. When this error occurs, the guaranteed behavior is that the
   * servo keeps its last set position.
   */
  void position(hal::degrees p_position)
  {
    driver_position(p_position);
  }

  virtual ~servo() = default;

private:
  virtual void driver_position(hal::degrees p_position) = 0;
};
}  // namespace hal

namespace hal::v5 {
/**
 * @brief Hardware abstraction for a closed loop position controlled rotational
 * actuator
 *
 */
class servo
{
public:
  /**
   * @brief Set the position of the servo's output shaft
   *
   * Position is the rotational position as a angle in degrees that the caller
   * wants the shaft to rotate to. The allowed range of positions is defined by
   * the servo itself. Many servos have intrinsic limits to their range.
   *
   * Developers must choose servos that fit the range for their applications.
   * Applications must clearly define the range that they require in order to
   * perform correctly.
   *
   * The velocity in which the servo shaft moves is not defined by this function
   * but is either intrinsic to the servo or a configuration of the servo.
   *
   * @param p_position - the position to move the servo shaft in degrees.
   * @throws hal::argument_out_of_domain - when position exceeds the range of
   * the servo. When this error occurs, the guaranteed behavior is that the
   * servo keeps its last set position.
   */
  void position(hal::degrees p_position)
  {
    driver_position(p_position);
  }

  virtual ~servo() = default;

private:
  virtual void driver_position(hal::degrees p_position) = 0;
};
}  // namespace hal::v5
