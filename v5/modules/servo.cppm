// Copyright 2026 Khalil Estell and the libhal contributors
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

export module hal:servo;

export import async_context;
export import :units;

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
    revolutions min;  ///< Minimum position in degrees
    revolutions max;  ///< Maximum position in degrees
  };

  virtual ~basic_servo() = default;

  /**
   * @brief Enable or disable the servo
   *
   * When disabled, the servo will go limp as if power was removed. The servo
   * may enter a low-power braking mode, but should not actively control
   * position.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_state - true to enable, false to disable
   */
  [[nodiscard]] async::future<void> enable(async::context& p_context,
                                           bool p_state)
  {
    return driver_enable(p_context, p_state);
  }

  /**
   * @brief Set the target position for the servo
   *
   * Moves the servo to the specified position and holds it there.
   * Position must be within the range returned by position_range().
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_target_position - The position in degrees to move to
   */
  [[nodiscard]] async::future<void> position(async::context& p_context,
                                             revolutions p_target_position)
  {
    return driver_position(p_context, p_target_position);
  }

  /**
   * @brief Get the valid position range for this servo
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<position_range_t> - The minimum and maximum position
   * in degrees
   */
  [[nodiscard]] async::future<position_range_t> position_range(
    async::context& p_context)
  {
    return driver_position_range(p_context);
  }

private:
  virtual async::future<void> driver_enable(async::context& p_context,
                                            bool p_state) = 0;
  virtual async::future<void> driver_position(
    async::context& p_context,
    revolutions p_target_position) = 0;
  virtual async::future<position_range_t> driver_position_range(
    async::context& p_context) = 0;
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
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<degrees> - The current position in degrees
   */
  [[nodiscard]] async::future<revolutions> position(async::context& p_context)
  {
    return driver_get_position(p_context);
  }

  /**
   * @brief Check if the servo is currently moving
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<bool> - true if the servo is in motion, false if
   * stationary
   */
  [[nodiscard]] async::future<bool> is_moving(async::context& p_context)
  {
    return driver_is_moving(p_context);
  }

private:
  virtual async::future<revolutions> driver_get_position(
    async::context& p_context) = 0;
  virtual async::future<bool> driver_is_moving(async::context& p_context) = 0;
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
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_settings - Structure containing velocity parameters
   * @throws hal::operation_not_supported - if the settings cannot be
   * accommodated by the servo. This occurs if the magnitude of the value is
   * greater than the value returned from range().
   */
  [[nodiscard]] async::future<void> configure(async::context& p_context,
                                              settings const& p_settings)
  {
    return driver_configure(p_context, p_settings);
  }

  /**
   * @brief Get the current velocity status
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<status_t> - Structure containing current velocity
   * information
   */
  [[nodiscard]] async::future<status_t> status(async::context& p_context)
  {
    return driver_status(p_context);
  }

  /**
   * @brief Get the valid velocity range for this servo
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<range_t> - The minimum and maximum velocities in RPM
   */
  [[nodiscard]] async::future<range_t> velocity_range(async::context& p_context)
  {
    return driver_velocity_range(p_context);
  }

private:
  virtual async::future<void> driver_configure(async::context& p_context,
                                               settings const& p_settings) = 0;
  virtual async::future<status_t> driver_status(async::context& p_context) = 0;
  virtual async::future<range_t> driver_velocity_range(
    async::context& p_context) = 0;
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
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_settings - Structure containing torque parameters
   * @throws hal::operation_not_supported - if the settings cannot be
   * accommodated by the servo. This occurs if the magnitude of the value is
   * greater than the value returned from range().
   */
  [[nodiscard]] async::future<void> configure(async::context& p_context,
                                              settings const& p_settings)
  {
    return driver_configure(p_context, p_settings);
  }

  /**
   * @brief Get the current torque status
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<status_t> - Structure containing current torque
   * information
   */
  [[nodiscard]] async::future<status_t> status(async::context& p_context)
  {
    return driver_status(p_context);
  }

  /**
   * @brief Get the valid torque range for this servo
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<range_t> - The minimum and maximum torque values in
   * newton meters
   */
  [[nodiscard]] async::future<range_t> torque_range(async::context& p_context)
  {
    return driver_torque_range(p_context);
  }

private:
  virtual async::future<void> driver_configure(async::context& p_context,
                                               settings const& p_settings) = 0;
  virtual async::future<status_t> driver_status(async::context& p_context) = 0;
  virtual async::future<range_t> driver_torque_range(
    async::context& p_context) = 0;
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
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_settings - Structure containing velocity and torque parameters
   * @throws hal::operation_not_supported - if the settings cannot be
   * accommodated by the servo. This occurs if the magnitude of the value is
   * greater than the value returned from range().
   */
  [[nodiscard]] async::future<void> configure(async::context& p_context,
                                              settings const& p_settings)
  {
    return driver_configure(p_context, p_settings);
  }

  /**
   * @brief Set the target velocity
   *
   * Directly sets just the velocity parameter while maintaining other settings.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_target_velocity - Target velocity in RPM
   */
  [[nodiscard]] async::future<void> velocity(async::context& p_context,
                                             rpm p_target_velocity)
  {
    return driver_velocity(p_context, p_target_velocity);
  }

  /**
   * @brief Get current velocity and torque status
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<status_t> - Structure containing current velocity
   * and torque information
   */
  [[nodiscard]] async::future<status_t> status(async::context& p_context)
  {
    return driver_status(p_context);
  }

  /**
   * @brief Get the valid ranges for velocity and torque
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<range_t> - Structure containing velocity and torque
   * range information
   */
  [[nodiscard]] async::future<range_t> range(async::context& p_context)
  {
    return driver_range(p_context);
  }

private:
  virtual async::future<void> driver_configure(async::context& p_context,
                                               settings const& p_settings) = 0;
  virtual async::future<void> driver_velocity(async::context& p_context,
                                              rpm p_target_velocity) = 0;
  virtual async::future<status_t> driver_status(async::context& p_context) = 0;
  virtual async::future<range_t> driver_range(async::context& p_context) = 0;
};
}  // namespace hal::inline v5
