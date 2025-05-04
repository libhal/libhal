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
class basic_servo
{
public:
  struct position_range_t
  {
    degrees min;
    degrees max;
  };

  virtual ~basic_servo() = default;

  void enable(bool p_state)
  {
    return driver_enable(p_state);
  }
  void position(degrees p_target_position)
  {
    return driver_position(p_target_position);
  }
  [[nodiscard]] position_range_t position_range()
  {
    return driver_position_range();
  }

private:
  virtual void driver_enable(bool p_state) = 0;
  virtual void driver_position(degrees p_target_position) = 0;
  virtual position_range_t driver_position_range() = 0;
};

class feedback_servo : public basic_servo
{
public:
private:
  virtual degrees position() = 0;
};

class velocity_servo : public feedback_servo
{
public:
  struct range_t
  {
    rpm min;
    rpm max;
  };

  virtual void velocity(float p_target_velocity)
  {
    return driver_velocity(p_target_velocity);
  }
  virtual rpm velocity()
  {
    return driver_velocity();
  }
  virtual range_t range()
  {
    return driver_range();
  }

private:
  virtual void driver_velocity(float target_velocity) = 0;
  virtual rpm driver_velocity() = 0;
  virtual range_t driver_range() = 0;
};

class torque_servo : public feedback_servo
{
public:
  // Sets max the torque for the next position call.
  // Units TBD
  virtual void torque(float target_torque) = 0;
  virtual float torque() const = 0;
  virtual std::pair<float, float> torque_range() const = 0;
};

// Veltor means VELocity and TORque. This interface
// represents a servo with both velocity and torque
// control.
class veltor_servo : public feedback_servo
{
public:
  struct range_t
  {
    float torque_min;
    float torque_max;
    float velocity_min;
    float velocity_max;
  };
  struct veltor_t
  {
    float torque;
    float velocity;
  };
  virtual void torque(float p_target_torque) = 0;
  virtual void velocity(float p_target_velocity) = 0;
  virtual veltor_t veltor() const = 0;
  virtual range_t range() const = 0;
};
}  // namespace hal::v5
