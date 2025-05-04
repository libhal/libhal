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

#include <algorithm>

#include "units.hpp"

namespace hal {
/**
 * @brief Hardware abstraction for an open loop rotational actuator
 *
 * The motor interface can represent a variety of things such as:
 *
 *   - A driver for motor controller IC like the DRV8801
 *   - A driver for a motor with integrated controller & serial interface
 *   - A unidirectional motor controlled by a single transistor
 *   - A servo with open loop motor control
 *
 */
class motor
{
public:
  /**
   * @brief Apply power to the motor
   *
   * Power is a percentage and thus cannot be used as a way to gauge how fast
   * the motor is moving. In general applying more power means to increase speed
   * and/or torque to the motor.
   *
   * - 0% power would mean that no power is being applied to the motor. In this
   *   situation an unloaded motor will not move. 0% power does not guarantee
   *   that the motor will hold its position. These specifics depend greatly on
   *   the type of motor used and careful selection of motor and motor driver
   *   are important for applications using this interface.
   *
   * - 100% power means that the maximum available of power is being applied to
   *   the motor. As an example, if the max voltage of a DC brushed motor's
   *   power supply is 12V, then 12V would be supplied to this motor.
   *
   * - 50% power would mean that half of the available power is being applied to
   *   the motor. Using the same example, in this case 6V would be applied to
   *   the motor either as a DC constant voltage or via PWM at 50% duty cycle.
   *
   * - Negative values will cause the motor to move in the opposite
   *   direction as positive values. In the event that motor driver can *
   *   only go in one direction, this function should clamp the power applied to
   *   0%.
   *
   * @param p_power - Percentage of power to apply to the motor from -1.0f to
   * +1.0f, -100% to 100%, respectively.
   */
  void power(float p_power)
  {
    auto clamped_power = std::clamp(p_power, -1.0f, +1.0f);
    return driver_power(clamped_power);
  }

  virtual ~motor() = default;

private:
  virtual void driver_power(float p_power) = 0;
};
}  // namespace hal

namespace hal::v5 {
/**
 * @brief Hardware abstraction for an open loop rotational actuator
 *
 * The motor interface can represent a variety of things such as:
 *
 *   - A driver for motor controller IC like the DRV8801
 *   - A driver for a motor with integrated controller & serial interface
 *   - A unidirectional motor controlled by a single transistor
 *   - A servo with open loop motor control
 *
 */
class motor
{
public:
  /**
   * @brief Apply power to the motor
   *
   * Power is a percentage and thus cannot be used as a way to gauge how fast
   * the motor is moving. In general applying more power means to increase speed
   * and/or torque to the motor.
   *
   * - 0% power would mean that no power is being applied to the motor. In this
   *   situation an unloaded motor will not move. 0% power does not guarantee
   *   that the motor will hold its position. These specifics depend greatly on
   *   the type of motor used and careful selection of motor and motor driver
   *   are important for applications using this interface.
   *
   * - 100% power means that the maximum available of power is being applied to
   *   the motor. As an example, if the max voltage of a DC brushed motor's
   *   power supply is 12V, then 12V would be supplied to this motor.
   *
   * - 50% power would mean that half of the available power is being applied to
   *   the motor. Using the same example, in this case 6V would be applied to
   *   the motor either as a DC constant voltage or via PWM at 50% duty cycle.
   *
   * - Negative values will cause the motor to move in the opposite
   *   direction as positive values. In the event that motor driver can *
   *   only go in one direction, this function should clamp the power applied to
   *   0%.
   *
   * @param p_power - Percentage of power to apply to the motor from -32'768 to
   * 32'767, -100% to 100%, respectively.
   */
  void power(i16 p_power)
  {
    return driver_power(p_power);
  }

  virtual ~motor() = default;

private:
  virtual void driver_power(i16 p_power) = 0;
};

class open_loop_motor
{
public:
  virtual void power(float target_position) = 0;

  virtual ~open_loop_motor() = default;
};

class basic_motor
{
public:
  virtual void enable(bool state) = 0;

  // Move motor rotor to this position
  virtual void power(float target_position) = 0;

  // Get the configured position range
  virtual std::pair<float, float> position_range() const = 0;

  virtual ~basic_motor() = default;
};

class feedback_motor : public basic_motor
{
public:
  // Position feedback
  virtual float position() const = 0;
};

class velocity_motor : public feedback_motor
{
public:
  // Sets max velocity for the next position() call.
  // Units TBD
  virtual void velocity(float target_velocity) = 0;

  // Get the current velocity (units TBD)
  virtual float velocity() const = 0;

  // Get configured velocity constraints
  virtual std::pair<float, float> velocity_range() const = 0;
};

class torque_motor : public feedback_motor
{
public:
  // Sets max the torque for the next position call.
  // Units TBD
  virtual void torque(float target_torque) = 0;
  virtual float torque() const = 0;
  virtual std::pair<float, float> torque_range() const = 0;
};

// Veltor means VELocity and TORque. This interface
// represents a motor with both velocity and torque
// control.
class veltor_motor : public feedback_motor
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
