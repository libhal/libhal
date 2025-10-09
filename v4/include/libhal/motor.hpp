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
class open_loop_motor
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

  virtual ~open_loop_motor() = default;

private:
  virtual void driver_power(i16 p_power) = 0;
};

/**
 * @brief Hardware abstraction for a motor with closed-loop velocity control
 *
 * Represents motors with velocity feedback, allowing precise speed control
 * regardless of load variations. Supports configuration of target velocity,
 * status monitoring, and movement detection.
 */
class velocity_motor
{
public:
  /**
   * @brief Structure containing current velocity status
   */
  struct status_t
  {
    rpm velocity;  ///< Current velocity in RPM
  };

  /**
   * @brief Structure defining velocity limits
   */
  struct range_t
  {
    rpm max;  ///< Maximum velocity in RPM
  };

  virtual ~velocity_motor() = default;

  /**
   * @brief Enable or disable the motor
   *
   * When disabled, the motor will stop applying power. The motor may enter a
   * low-power braking mode, but should not actively control velocity.
   *
   * @param p_state - true to enable, false to disable
   */
  void enable(bool p_state)
  {
    return driver_enable(p_state);
  }

  /**
   * @brief Command the motor to move at a specific velocity
   *
   * Sets the target velocity for the motor. The value's sign controls the
   * direction and its magnitude is the velocity in RPM. The direction a
   * positive or negative RPM is not defined by the interface but by the
   * construction the motor is apart of. This information must be passed to
   * drivers along with this interface in order to decern directionality, if
   * that is necessary for the application.
   *
   * @param p_velocity - Target velocity in RPM
   * @throws hal::operation_not_supported - if the value exceeds the
   * velocity range defined in velocity_range().
   */
  void drive(rpm p_velocity)
  {
    return driver_drive(p_velocity);
  }

  /**
   * @brief Get the current velocity status
   *
   * @return Structure containing current velocity information
   */
  [[nodiscard]] status_t status()
  {
    return driver_status();
  }

  /**
   * @brief Get the valid velocity range for this motor
   *
   * @return The minimum and maximum velocities in RPM
   */
  [[nodiscard]] range_t velocity_range()
  {
    return driver_velocity_range();
  }

private:
  virtual void driver_enable(bool p_state) = 0;
  virtual void driver_drive(rpm p_velocity) = 0;
  virtual status_t driver_status() = 0;
  virtual range_t driver_velocity_range() = 0;
};

/**
 * @brief Hardware abstraction for a motor with closed-loop torque control
 *
 * Represents motors with torque feedback, allowing precise force control
 * regardless of position or velocity. The implementation may use current
 * sensing to estimate and control torque.
 */
class torque_motor
{
public:
  /**
   * @brief Structure containing current torque status
   */
  struct status_t
  {
    newton_meter torque;  ///< Current torque in newton meters
  };

  /**
   * @brief Structure defining torque limits
   */
  struct range_t
  {
    newton_meter max;  ///< Maximum torque in newton meters
  };

  virtual ~torque_motor() = default;

  /**
   * @brief Enable or disable the motor
   *
   * When disabled, the motor will stop applying power. The motor
   * may enter a low-power state, but should not actively control torque.
   *
   * @param p_state - true to enable, false to disable
   */
  void enable(bool p_state)
  {
    return driver_enable(p_state);
  }

  /**
   * @brief Apply a specific torque
   *
   * Sets the target torque for the motor to exert. Only the magnitude
   * (absolute value) of the torque is considered.
   *
   * @param p_torque - Target torque in newton meters. Setting this value to
   * zero will stop the motor from moving.
   * @throws hal::operation_not_supported - if the value exceeds the
   * torque range defined in torque_range().
   */
  void exert(newton_meter p_torque)
  {
    return driver_exert(p_torque);
  }

  /**
   * @brief Get the current torque status
   *
   * @return Structure containing current torque information
   */
  [[nodiscard]] status_t status()
  {
    return driver_status();
  }

  /**
   * @brief Get the valid torque range for this motor
   *
   * @return The minimum and maximum torque values in newton meters
   */
  [[nodiscard]] range_t torque_range()
  {
    return driver_torque_range();
  }

private:
  virtual void driver_enable(bool p_state) = 0;
  virtual void driver_exert(newton_meter p_torque) = 0;
  virtual status_t driver_status() = 0;
  virtual range_t driver_torque_range() = 0;
};

/**
 * @brief Hardware abstraction for a motor with both velocity and torque control
 *
 * Represents advanced motors that can control both velocity and torque
 * simultaneously, suitable for sophisticated motion control applications.
 */
class veltor_motor
{
public:
  /**
   * @brief Combined range structure for velocity and torque
   */
  struct range_t
  {
    velocity_motor::range_t velocity;  ///< Velocity range in RPM
    torque_motor::range_t torque;      ///< Torque range in newton meters
  };

  /**
   * @brief Settings structure for velocity and torque control
   */
  struct control_t
  {
    newton_meter torque;  ///< Target torque in newton meters
    rpm velocity;         ///< Target velocity in RPM
  };

  /**
   * @brief Structure containing current velocity and torque status
   */
  struct status_t
  {
    newton_meter torque;  ///< Current torque in newton meters
    rpm velocity;         ///< Current velocity in RPM
  };

  virtual ~veltor_motor() = default;

  /**
   * @brief Enable or disable the motor
   *
   * When disabled, the motor will stop applying power. The motor
   * may enter a low-power state, but should not actively control
   * velocity or torque.
   *
   * @param p_state - true to enable, false to disable
   */
  void enable(bool p_state)
  {
    return driver_enable(p_state);
  }

  /**
   * @brief Control velocity and torque parameters simultaneously
   *
   * Sets both velocity and torque values for the motor. Only the magnitude
   * (absolute values) of the values are taken.
   *
   * @param p_control - Structure containing velocity and torque parameters. If
   * either value within control is 0, then the motor operates as if
   * `enable(false)` was called.
   * @throws hal::operation_not_supported - if the settings cannot be
   * accommodated by the motor. This occurs if the magnitude of the value is
   * greater than the value returned from range().
   */
  void control(control_t const& p_control)
  {
    return driver_control(p_control);
  }

  /**
   * @brief Get current velocity and torque status
   *
   * @return Structure containing current velocity and torque information
   */
  [[nodiscard]] status_t status()
  {
    return driver_status();
  }

  /**
   * @brief Get the valid ranges for velocity and torque
   *
   * @return Structure containing velocity and torque range information
   */
  [[nodiscard]] range_t range()
  {
    return driver_range();
  }

  /**
   * @brief Check if the motor is currently moving
   *
   * @return true if the motor is in motion, false if stationary
   */
  [[nodiscard]] bool is_moving()
  {
    return driver_is_moving();
  }

private:
  virtual void driver_enable(bool p_state) = 0;
  virtual void driver_control(control_t const& p_control) = 0;
  virtual status_t driver_status() = 0;
  virtual range_t driver_range() = 0;
  virtual bool driver_is_moving() = 0;
};
}  // namespace hal::v5

namespace hal {
using v5::open_loop_motor;
using v5::torque_motor;
using v5::velocity_motor;
using v5::veltor_motor;
}  // namespace hal
