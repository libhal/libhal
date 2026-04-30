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

#include <array>
#include <chrono>
#include <coroutine>
#include <memory_resource>

#include <boost/ut.hpp>

import hal;
import async_context;

using namespace mp_units;
using namespace mp_units::si::unit_symbols;

namespace {

// =============================================================================
// current_sensor
// =============================================================================

class test_current_sensor : public hal::current_sensor
{
public:
  hal::amperes returned_value = 1.5f * A;
  ~test_current_sensor() override = default;

private:
  async::future<hal::amperes> driver_read(async::context&) override
  {
    return returned_value;
  }
};

void current_sensor_test() noexcept
{
  using namespace boost::ut;

  "current_sensor::read() returns value from driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_current_sensor test;
    test.returned_value = 2.5f * A;

    // Exercise
    auto result = test.read(ctx);

    // Verify
    expect(result.has_value());
    expect((2.5f * A) == result.value());
  };
}

// =============================================================================
// volt_sensor
// =============================================================================

class test_volt_sensor : public hal::volt_sensor
{
public:
  hal::volts returned_value = 3.3f * V;
  ~test_volt_sensor() override = default;

private:
  async::future<hal::volts> driver_read(async::context&) override
  {
    return returned_value;
  }
};

void volt_sensor_test() noexcept
{
  using namespace boost::ut;

  "volt_sensor::read() returns value from driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_volt_sensor test;
    test.returned_value = 5.0f * V;

    // Exercise
    auto result = test.read(ctx);

    // Verify
    expect(result.has_value());
    expect((5.0f * V) == result.value());
  };
}

// =============================================================================
// distance_sensor
// =============================================================================

class test_distance_sensor : public hal::distance_sensor
{
public:
  hal::meters returned_value = 1.0f * m;
  ~test_distance_sensor() override = default;

private:
  async::future<hal::meters> driver_read(async::context&) override
  {
    return returned_value;
  }
};

void distance_sensor_test() noexcept
{
  using namespace boost::ut;

  "distance_sensor::read() returns value from driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_distance_sensor test;
    test.returned_value = 0.5f * m;

    // Exercise
    auto result = test.read(ctx);

    // Verify
    expect(result.has_value());
    expect((0.5f * m) == result.value());
  };
}

// =============================================================================
// angular_velocity_sensor
// =============================================================================

class test_angular_velocity_sensor : public hal::angular_velocity_sensor
{
public:
  hal::angular_velocity returned_value = 90.0f * (deg / s);
  ~test_angular_velocity_sensor() override = default;

private:
  async::future<hal::angular_velocity> driver_read(async::context&) override
  {
    return returned_value;
  }
};

void angular_velocity_sensor_test() noexcept
{
  using namespace boost::ut;

  "angular_velocity_sensor::read() returns value from driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_angular_velocity_sensor test;
    test.returned_value = 180.0f * (deg / s);

    // Exercise
    auto result = test.read(ctx);

    // Verify
    expect(result.has_value());
    expect((180.0f * (deg / s)) == result.value());
  };
}

// =============================================================================
// rotation_sensor
// =============================================================================

class test_rotation_sensor : public hal::rotation_sensor
{
public:
  hal::degrees returned_value = 45.0f * deg;
  ~test_rotation_sensor() override = default;

private:
  async::future<hal::degrees> driver_read(async::context&) override
  {
    return returned_value;
  }
};

void rotation_sensor_test() noexcept
{
  using namespace boost::ut;

  "rotation_sensor::read() returns value from driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_rotation_sensor test;
    test.returned_value = 270.0f * deg;

    // Exercise
    auto result = test.read(ctx);

    // Verify
    expect(result.has_value());
    expect((270.0f * deg) == result.value());
  };
}

// =============================================================================
// accelerometer
// =============================================================================

class test_accelerometer : public hal::accelerometer
{
public:
  hal::accelerometer::read_t returned_value{
    .x = 0.0f * (m / pow<2>(s)),
    .y = 0.0f * (m / pow<2>(s)),
    .z = 9.81f * (m / pow<2>(s)),
  };
  ~test_accelerometer() override = default;

private:
  async::future<hal::accelerometer::read_t> driver_read(
    async::context&) override
  {
    return returned_value;
  }
};

void accelerometer_test() noexcept
{
  using namespace boost::ut;

  "accelerometer::read() returns x, y, z values from driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_accelerometer test;
    test.returned_value = {
      .x = 1.0f * (m / pow<2>(s)),
      .y = 2.0f * (m / pow<2>(s)),
      .z = 3.0f * (m / pow<2>(s)),
    };

    // Exercise
    auto result = test.read(ctx);

    // Verify
    expect(result.has_value());
    expect((1.0f * (m / pow<2>(s))) == result.value().x);
    expect((2.0f * (m / pow<2>(s))) == result.value().y);
    expect((3.0f * (m / pow<2>(s))) == result.value().z);
  };
}

// =============================================================================
// magnetometer
// =============================================================================

class test_magnetometer : public hal::magnetometer
{
public:
  hal::magnetometer::read_t returned_value{
    .x = 0.0f * T,
    .y = 0.0f * T,
    .z = 0.0f * T,
  };
  ~test_magnetometer() override = default;

private:
  async::future<hal::magnetometer::read_t> driver_read(async::context&) override
  {
    return returned_value;
  }
};

void magnetometer_test() noexcept
{
  using namespace boost::ut;

  "magnetometer::read() returns x, y, z values from driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_magnetometer test;
    test.returned_value = {
      .x = 0.1f * T,
      .y = 0.2f * T,
      .z = 0.3f * T,
    };

    // Exercise
    auto result = test.read(ctx);

    // Verify
    expect(result.has_value());
    expect((0.1f * T) == result.value().x);
    expect((0.2f * T) == result.value().y);
    expect((0.3f * T) == result.value().z);
  };
}

// =============================================================================
// gyroscope
// =============================================================================

class test_gyroscope : public hal::gyroscope
{
public:
  hal::gyroscope::read_t returned_value{
    .x = 0.0f * (deg / s),
    .y = 0.0f * (deg / s),
    .z = 0.0f * (deg / s),
  };
  ~test_gyroscope() override = default;

private:
  async::future<hal::gyroscope::read_t> driver_read(async::context&) override
  {
    return returned_value;
  }
};

void gyroscope_test() noexcept
{
  using namespace boost::ut;

  "gyroscope::read() returns x, y, z values from driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_gyroscope test;
    test.returned_value = {
      .x = 10.0f * (deg / s),
      .y = 20.0f * (deg / s),
      .z = 30.0f * (deg / s),
    };

    // Exercise
    auto result = test.read(ctx);

    // Verify
    expect(result.has_value());
    expect((10.0f * (deg / s)) == result.value().x);
    expect((20.0f * (deg / s)) == result.value().y);
    expect((30.0f * (deg / s)) == result.value().z);
  };
}

// =============================================================================
// temperature_sensor
// =============================================================================

class test_temperature_sensor : public hal::temperature_sensor
{
public:
  hal::kelvin returned_value = delta<K>(298.15f);
  ~test_temperature_sensor() override = default;

private:
  async::future<hal::kelvin> driver_read(async::context&) override
  {
    return returned_value;
  }
};

void temperature_sensor_test() noexcept
{
  using namespace boost::ut;

  "temperature_sensor::read() returns value from driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_temperature_sensor test;
    test.returned_value = delta<K>(373.15f);

    // Exercise
    auto result = test.read(ctx);

    // Verify
    expect(result.has_value());
    expect(delta<K>(373.15f) == result.value());
  };
}

}  // namespace

int main()
{
  current_sensor_test();
  volt_sensor_test();
  distance_sensor_test();
  angular_velocity_sensor_test();
  rotation_sensor_test();
  accelerometer_test();
  magnetometer_test();
  gyroscope_test();
  temperature_sensor_test();
}
