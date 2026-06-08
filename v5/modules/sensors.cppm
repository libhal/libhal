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
// See the License for the specific language governing permissions and
// limitations under the License.

export module hal:sensors;

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
   * @return async::future<amperes> - measured current in amps
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
   * @return async::future<volts> - measured voltage in volts
   */
  [[nodiscard]] async::future<volts> read(async::context& p_context)
  {
    return driver_read(p_context);
  }

  virtual ~volt_sensor() = default;

private:
  virtual async::future<volts> driver_read(async::context& p_context) = 0;
};

/**
 * @brief Linear distance hardware abstraction interface
 *
 * Examples of distance encoder are:
 *
 *    - Linear Potentiometers
 *    - LIDAR or TOF (time of flight) sensor
 *    - Ultrasonic range finder
 *    - Infrared Distance Sensors
 *    - Linear Quadrature Encoders
 *    - Linear Incremental Encoders
 *    - Linear Absolute Encoders
 *    - Linear Magnetic Encoders
 *
 * Distance sensors can be relative or absolute. Relative position means that
 * the sensor can only see changes in rotation from where measurement started.
 * In other words, at application start, relative encoders will start at 0.
 * Absolute encoders know their position at all times. At application start, the
 * absolute encoder will be able to determine its exact orientation relative to
 * a frame of reference when read.
 *
 * Examples of relative rotation sensors are:
 *
 *    - Quadrature Encoders
 *    - Incremental Encoders
 *
 * Examples of absolute rotation sensors are:
 *
 *    - Potentiometers
 *    - Absolute Encoders
 *    - Rotary Magnetic Encoders
 *    - IMUs
 *
 * Distance sensors can also be finite or infinite. Finite meaning that the
 * angle that can be reported is a fixed amount for the device. Infinite means
 * that the encoder can continue rotating and adding more to its angle reading
 * forever. Infinite rotation sensors tend to not have a physical stop that
 * limits how much they can be rotated.
 *
 * Examples of finite rotation sensors are:
 *
 *    - Potentiometers
 *    - Absolute Encoders
 *    - IMUs
 *
 * Examples of infinite rotation sensors are:
 *
 *    - Rotary Magnetic Encoders
 *    - Quadrature Encoders
 *    - Incremental Encoders
 *
 * This interface does not provide a means to determine these attributes of a
 * rotation sensor as this is an application architecture decision. Drivers that
 * implement this interface should document what kind of distance sensor it is
 * such that a developer can determine its applicability to their application.
 * The context of which sensor ought to be used for an application is solely
 * known at architecture definition time and software should not be expected to
 * at runtime, if the right type of rotation sensor was passed into the object.
 */
export class distance_sensor
{
public:
  /**
   * @brief Read the current distance measured by the device
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<meters> - measured distance in meters
   */
  [[nodiscard]] async::future<meters> read(async::context& p_context)
  {
    return driver_read(p_context);
  }

  virtual ~distance_sensor() = default;

private:
  virtual async::future<meters> driver_read(async::context& p_context) = 0;
};

/**
 * @brief angular velocity sensor hardware abstraction interface
 *
 */
export class angular_velocity_sensor
{
public:
  /**
   * @brief Reads the most up to date angular velocity from the sensor
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<angular_velocity> - angular velocity measured in
   * degrees / second
   */
  [[nodiscard]] async::future<angular_velocity> read(async::context& p_context)
  {
    return driver_read(p_context);
  }

  virtual ~angular_velocity_sensor() = default;

private:
  virtual async::future<angular_velocity> driver_read(
    async::context& p_context) = 0;
};

/**
 * @brief Rotation measuring hardware abstraction interface
 *
 * Examples of rotary encoder are:
 *
 *    - Rotary Potentiometers
 *    - Quadrature Encoders
 *    - Incremental Encoders
 *    - Absolute Encoders
 *    - Rotary Magnetic Encoders
 *    - Inertial Measurement Unit or IMU
 *
 * Rotation sensors can be relative or absolute. Relative position means that
 * the sensor can only see changes in rotation from where measurement started.
 * In other words, at application start, relative encoders will start at 0.
 * Absolute encoders know their position at all times. At application start, the
 * absolute encoder will be able to determine its exact orientation relative to
 * a frame of reference when read.
 *
 * Examples of relative rotation sensors are:
 *
 *    - Quadrature Encoders
 *    - Incremental Encoders
 *
 * Examples of absolute rotation sensors are:
 *
 *    - Rotary Potentiometers
 *    - Absolute Encoders
 *    - Rotary Magnetic Encoders
 *    - IMUs
 *
 * Rotation sensors can also be finite or infinite. Finite meaning that the
 * angle that can be reported is a fixed amount for the device. Infinite means
 * that the encoder can continue rotating and adding more to its angle reading
 * forever. Infinite rotation sensors tend to not have a physical stop that
 * limits how much they can be rotated.
 *
 * Examples of finite rotation sensors are:
 *
 *    - Rotary Potentiometers
 *    - Absolute Encoders
 *    - IMUs
 *
 * Examples of infinite rotation sensors are:
 *
 *    - Rotary Magnetic Encoders
 *    - Quadrature Encoders
 *    - Incremental Encoders
 *
 * This interface does not provide a means to determine these attributes of a
 * rotation sensor as this is an application architecture decision. Drivers that
 * implement this interface should document what kind of rotary sensor it is
 * such that a developer can determine its applicability to their application.
 * The context of which sensor ought to be used for an application is solely
 * known at architecture definition time and software should not be expected to
 * at runtime, if the right type of rotation sensor was passed into the object.
 */
export class rotation_sensor
{
public:
  /**
   * @brief Read the current angle measured by the device
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<degrees> - measured rotation angle
   */
  [[nodiscard]] async::future<revolutions> read(async::context& p_context)
  {
    return driver_read(p_context);
  }

  virtual ~rotation_sensor() = default;

private:
  virtual async::future<revolutions> driver_read(async::context& p_context) = 0;
};

/**
 * @brief Acceleration sensing hardware abstraction interface.
 */
export class accelerometer
{
public:
  /**
   * @brief Result from reading the accelerometer.
   *
   */
  struct read_t
  {
    /**
     * @brief Acceleration in the X axis, relative to the device's reference
     * frame.
     *
     */
    acceleration x;
    /**
     * @brief Acceleration in the Y axis, relative to the device's reference
     * frame.
     *
     */
    acceleration y;
    /**
     * @brief Acceleration in the Z axis, relative to the device's reference
     * frame.
     *
     */
    acceleration z;
  };

  /**
   * @brief Read the latest acceleration sensed by the device
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<read_t> - measured acceleration data
   */
  [[nodiscard]] async::future<read_t> read(async::context& p_context)
  {
    return driver_read(p_context);
  }

  virtual ~accelerometer() = default;

private:
  virtual async::future<read_t> driver_read(async::context& p_context) = 0;
};

/**
 * @brief Magnetic field strength sensing hardware abstraction interface.
 *
 * Magnetometers are usually used for determining the strength of a magnetic
 * field, or calculating compass headings. If the device that the magnetometer
 * is mounted on, tends to move, or change its own orientation, then it is
 * helpful to use an accelerometer or tilt sensor in order to determine
 * appropriate heading for compass calculations.
 */
export class magnetometer
{
public:
  /**
   * @brief Result from reading the magnetometer.
   *
   */
  struct read_t
  {
    /**
     * @brief Magnetic field strength in the X axis, relative to the device's
     * reference frame.
     *
     */
    teslas x;

    /**
     * @brief Magnetic field strength in the Y axis, relative to the device's
     * reference frame.
     *
     */
    teslas y;

    /**
     * @brief Magnetic field strength in the Z axis, relative to the device's
     * reference frame.
     *
     */
    teslas z;
  };

  /**
   * @brief Read the latest magnetic field strength sensed by the device
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<read_t> - measured magnetic field strength data
   */
  [[nodiscard]] async::future<read_t> read(async::context& p_context)
  {
    return driver_read(p_context);
  }

  virtual ~magnetometer() = default;

private:
  virtual async::future<read_t> driver_read(async::context& p_context) = 0;
};

/**
 * @brief Angular velocity sensing hardware abstraction interface.
 */
export class gyroscope
{
public:
  /**
   * @brief Result from reading the gyroscope.
   *
   */
  struct read_t
  {
    /**
     * @brief Angular velocity in the X axis, relative to the device's reference
     * frame
     */
    angular_velocity x;
    /**
     * @brief Angular velocity in the Y axis, relative to the device's reference
     * frame.
     *
     */
    angular_velocity y;
    /**
     * @brief Angular velocity in the Z axis, relative to the device's reference
     * frame.
     *
     */
    angular_velocity z;
  };

  /**
   * @brief Read the latest angular velocity sensed by the device
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<read_t> - measured angular velocity data
   */
  [[nodiscard]] async::future<read_t> read(async::context& p_context)
  {
    return driver_read(p_context);
  }

  virtual ~gyroscope() = default;

private:
  virtual async::future<read_t> driver_read(async::context& p_context) = 0;
};

/**
 * @brief Interface for acquiring temperature samples from a device.
 *
 */
export class temperature_sensor
{
public:
  /**
   * @brief Read the current temperature measured by the device
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<kelvin> - measured temperature
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
