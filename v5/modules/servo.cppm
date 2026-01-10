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

export module hal:servo;

export import :units;

export namespace hal {
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

export namespace hal::inline v5 {
/**
 * @brief Interface for the most basic of servos, devices that can be instructed
 * to move to a specific location and hold their position.
 *
 * This interface represents simple positional servos without feedback
 * capabilities. Basic servos are typically controlled via PWM signals with
 * position determined by pulse width.
 */
class basic_servo
{
public:
  /**
   * @brief Structure defining the minimum and maximum position range of the
   * servo
   */
  struct position_range_t
  {
    degrees min;  ///< Minimum position in degrees
    degrees max;  ///< Maximum position in degrees
  };

  virtual ~basic_servo() = default;

  /**
   * @brief Enable or disable the servo
   *
   * When disabled, the servo will go limp as if power was removed. The servo
   * may enter a low-power braking mode, but should not actively control
   * position.
   *
   * @param p_state - true to enable, false to disable
   */
  void enable(bool p_state)
  {
    return driver_enable(p_state);
  }

  /**
   * @brief Set the target position for the servo
   *
   * Moves the servo to the specified position and holds it there.
   * Position must be within the range returned by position_range().
   *
   * @param p_target_position - The position in degrees to move to
   */
  void position(degrees p_target_position)
  {
    return driver_position(p_target_position);
  }

  /**
   * @brief Get the valid position range for this servo
   *
   * @return The minimum and maximum position in degrees
   */
  [[nodiscard]] position_range_t position_range()
  {
    return driver_position_range();
  }

private:
  virtual void driver_enable(bool p_state) = 0;
  virtual void driver_position(degrees p_target_position) = 0;
  virtual position_range_t driver_position_range() = 0;
};

/**
 * @brief Interface for servos with position feedback capabilities
 *
 * This interface extends basic_servo to add methods for reading the current
 * position and determining if the servo is in motion.
 */
class feedback_servo : public basic_servo
{
public:
  /**
   * @brief Get the current position of the servo
   *
   * @return The current position in degrees
   */
  [[nodiscard]] degrees position()
  {
    return driver_get_position();
  }

  /**
   * @brief Check if the servo is currently moving
   *
   * @return true if the servo is in motion, false if stationary
   */
  [[nodiscard]] bool is_moving()
  {
    return driver_is_moving();
  }

private:
  virtual degrees driver_get_position() = 0;
  virtual bool driver_is_moving() = 0;
};

/**
 * @brief Interface for servos with variable velocity control
 *
 * Allows for controlling the speed at which the servo moves to position
 * targets. If hardware supports torque control, implementations should accept a
 * max_torque parameter in their constructor to bound the applied torque.
 */
class velocity_servo : public feedback_servo
{
public:
  /**
   * @brief Settings structure for configuring velocity parameters
   */
  struct settings
  {
    rpm velocity;  ///< Target velocity in RPM
  };

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

  /**
   * @brief Configure the velocity parameters for the servo
   *
   * Sets the velocity that will be used for subsequent position commands.
   *
   * @param p_settings - Structure containing velocity parameters
   * @throws hal::operation_not_supported - if the settings cannot be
   * accommodated by the servo. This occurs if the magnitude of the value is
   * greater than the value returned from range().
   */
  void configure(settings const& p_settings)
  {
    return driver_configure(p_settings);
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
   * @brief Get the valid velocity range for this servo
   *
   * @return The minimum and maximum velocities in RPM
   */
  [[nodiscard]] range_t velocity_range()
  {
    return driver_velocity_range();
  }

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual status_t driver_status() = 0;
  virtual range_t driver_velocity_range() = 0;
};

/**
 * @brief Interface for servos with variable torque control
 *
 * Allows for controlling the torque applied during position movements.
 * Implementations should accept a constant speed value during construction
 * if the hardware supports velocity control.
 */
class torque_servo : public feedback_servo
{
public:
  /**
   * @brief Settings structure for configuring torque parameters
   */
  struct settings
  {
    newton_meter torque;  ///< Target torque in newton meters
  };

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

  /**
   * @brief Configure the torque parameters for the servo
   *
   * Sets the torque limit that will be applied during position commands.
   *
   * @param p_settings - Structure containing torque parameters
   * @throws hal::operation_not_supported - if the settings cannot be
   * accommodated by the servo. This occurs if the magnitude of the value is
   * greater than the value returned from range().
   */
  void configure(settings const& p_settings)
  {
    return driver_configure(p_settings);
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
   * @brief Get the valid torque range for this servo
   *
   * @return The minimum and maximum torque values in newton meters
   */
  [[nodiscard]] range_t torque_range()
  {
    return driver_torque_range();
  }

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual status_t driver_status() = 0;
  virtual range_t driver_torque_range() = 0;
};

/**
 * @brief Interface for servos with both variable velocity and torque control
 *
 * Allows for simultaneous control of both velocity and torque parameters
 * for advanced servo applications.
 */
class veltor_servo : public feedback_servo
{
public:
  /**
   * @brief Combined range structure for velocity and torque
   */
  struct range_t
  {
    velocity_servo::range_t velocity;  ///< Velocity range in RPM
    torque_servo::range_t torque;      ///< Torque range in newton meters
  };

  /**
   * @brief Settings structure for configuring velocity and torque
   */
  struct settings
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

  /**
   * @brief Configure velocity and torque parameters
   *
   * Sets both velocity and torque values to be used for subsequent
   * position commands. Only the magnitude (absolute values) of the values are
   * taken.
   *
   * @param p_settings - Structure containing velocity and torque parameters
   * @throws hal::operation_not_supported - if the settings cannot be
   * accommodated by the servo. This occurs if the magnitude of the value is
   * greater than the value returned from range().
   */
  void configure(settings const& p_settings)
  {
    return driver_configure(p_settings);
  }

  /**
   * @brief Set the target velocity
   *
   * Directly sets just the velocity parameter while maintaining other settings.
   *
   * @param p_target_velocity - Target velocity in RPM
   */
  void velocity(rpm p_target_velocity)
  {
    return driver_velocity(p_target_velocity);
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

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual void driver_velocity(rpm p_target_velocity) = 0;
  virtual status_t driver_status() = 0;
  virtual range_t driver_range() = 0;
};
}  // namespace hal::inline v5
