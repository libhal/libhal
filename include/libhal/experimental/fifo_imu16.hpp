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

#include <array>
#include <cstdint>
#include <span>

#include "../units.hpp"

namespace hal {

struct accelerometer16
{
  std::int16_t x;
  std::int16_t y;
  std::int16_t z;
};

struct gyroscope16
{
  std::int16_t x;
  std::int16_t y;
  std::int16_t z;
};

struct imu16
{
  accelerometer16 accel;
  gyroscope16 gyro;
};

class fifo_imu16
{
public:
  /**
   * @brief Provides information needed to map the bytes of the read buffer to
   * the values of acceleration and gyroscope.
   *
   */
  struct data_map
  {
    struct accelerometer_t
    {
      /**
       * @brief Contains the locations of the bytes of acceleration in the
       * x-axis
       *
       * Index 0 first byte is low byte
       * Index 1 second byte is high byte
       */
      std::array<std::uint16_t, 2> x{};

      /**
       * @brief Contains the locations of the bytes of acceleration in the
       * y-axis
       *
       * Index 0 first byte is low byte
       * Index 1 second byte is high byte
       */
      std::array<std::uint16_t, 2> y{};

      /**
       * @brief Contains the locations of the bytes of acceleration in the
       * z-axis
       *
       * Index 0 first byte is low byte
       * Index 1 second byte is high byte
       */
      std::array<std::uint16_t, 2> z{};
    };

    struct gyroscope_t
    {
      /**
       * @brief Contains the locations of the bytes of angular acceleration in
       * the x-axis
       *
       * Index 0 first byte is low byte
       * Index 1 second byte is high byte
       */
      std::array<std::uint16_t, 2> x{};

      /**
       * @brief Contains the locations of the bytes of angular acceleration in
       * the y-axis
       *
       * Index 0 first byte is low byte
       * Index 1 second byte is high byte
       */
      std::array<std::uint16_t, 2> y{};

      /**
       * @brief Contains the locations of the bytes of angular acceleration in
       * the z-axis
       *
       * Index 0 first byte is low byte
       * Index 1 second byte is high byte
       */
      std::array<std::uint16_t, 2> z{};
    };

    accelerometer_t accelerometer{};
    gyroscope_t gyroscope{};

    /**
     * @brief The number of bytes to skip to reach the next set of samples
     *
     */
    std::uint16_t sample_offset = 0;
  };

  /**
   * @brief Returns the data map for this 16-bit fifo imu
   *
   * @return data_map - the data map for this device
   */
  data_map map()
  {
    return driver_map();
  }

  /**
   * @brief Read IMU fifo data into a buffer
   *
   * To decode the data within the buffer, use the data_map provided by the
   * `map()` API
   *
   * USAGE:
   *
   *      auto map = lis16dhtr.map();
   *
   *      std::array<std::byte, 128> buffer;
   *      auto data_available = lis16dhtr.read(buffer);
   *      if (data_available.empty()) { return; }
   *
   *      for (auto i = 0; i < data_available.size(); i += map.sample_offset) {
   *          auto acc_x_l = data_available[i + map.acceleration.x[0]];
   *          auto acc_x_h = data_available[i + map.acceleration.x[1]];
   *          auto acc_y_l = data_available[i + map.acceleration.y[0]];
   *          auto acc_y_h = data_available[i + map.acceleration.y[1]];
   *          auto acc_z_l = data_available[i + map.acceleration.z[0]];
   *          auto acc_z_h = data_available[i + map.acceleration.z[1]];
   *
   *          auto acc_x = acc_x_h << 8 | acc_x_l;
   *          auto acc_y = acc_y_h << 8 | acc_y_l;
   *          auto acc_z = acc_z_h << 8 | acc_z_l;
   *
   *          auto gyro_x_l = data_available[i + map.gyroscope.x[0]];
   *          auto gyro_x_h = data_available[i + map.gyroscope.x[1]];
   *          auto gyro_y_l = data_available[i + map.gyroscope.y[0]];
   *          auto gyro_y_h = data_available[i + map.gyroscope.y[1]];
   *          auto gyro_z_l = data_available[i + map.gyroscope.z[0]];
   *          auto gyro_z_h = data_available[i + map.gyroscope.z[1]];
   *
   *          auto gyro_x = gyro_x_h << 8 | gyro_x_l;
   *          auto gyro_y = gyro_y_h << 8 | gyro_y_l;
   *          auto gyro_z = gyro_z_h << 8 | gyro_z_l;
   *
   *          process(acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z);
   *      }
   *
   * @param p_buffer - a buffer of bytes to read data into
   * @return std::span<hal::byte> - is a subspan of p_buffer where the address
   * is either at the start of p_buffer or some offset from the start of
   * p_buffer, and `size()` returns the number of valid bytes within the buffer.
   * If `size()` is 0, then there are no samples available.
   */
  std::span<hal::byte> read(std::span<hal::byte> p_buffer)
  {
    return driver_read(p_buffer);
  }

  struct conversion_result
  {
    std::span<imu16> imu;
    std::span<hal::byte> remaining;
  };

  /**
   * @brief Convert raw imu data into imu16 data
   *
   * @param p_imu_buffer - set of imu16 data to fill with imu data
   * @param p_buffer - raw imu data in bytes from `read()`. Passing anything
   * other than raw data from this implementations `read()` will result in
   * undefined data in the `p_imu_buffer`. API. The buffer
   * @return conversion_result - results of the conversion.
   */
  conversion_result convert(std::span<imu16> p_imu_buffer,
                            std::span<hal::byte> p_raw_imu_data)
  {
    return driver_convert(p_imu_buffer, p_raw_imu_data);
  }

  virtual ~fifo_imu16() = default;

private:
  virtual data_map driver_map() = 0;
  virtual std::span<hal::byte> driver_read(std::span<hal::byte> p_buffer) = 0;
  virtual conversion_result driver_convert(
    std::span<imu16> p_imu_buffer,
    std::span<hal::byte> p_raw_imu_data) = 0;
};
}  // namespace hal
