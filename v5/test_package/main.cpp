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

#include <chrono>
#include <print>

import hal;

using namespace std::literals;

class simulated_steady_clock : public hal::steady_clock
{
private:
  hal::hertz driver_frequency() final
  {
    using namespace mp_units::si::unit_symbols;
    return 1 * MHz;
  }

  hal::u64 driver_uptime() final
  {
    return m_uptime++;
  }

  hal::u64 m_uptime = 0;
};

class test_pwm : public hal::pwm16_channel
{
private:
  hal::hertz driver_frequency() final
  {
    using namespace mp_units::si::unit_symbols;
    return 10 * kHz;
  }

  void driver_duty_cycle(hal::u16 p_duty_cycle) final
  {
    std::println("duty cycle = {}/{}", p_duty_cycle, (1 << 16) - 1);
  }
};

int main()
{
  try {
    test_pwm pwm;
    simulated_steady_clock clock;

    auto const pwm_frequency_int =
      pwm.frequency().numerical_value_in(mp_units::si::hertz);
    std::println("PWM frequency = {}", pwm_frequency_int);
    pwm.duty_cycle(1 << 15);
    pwm.duty_cycle(1 << 14);
    pwm.duty_cycle(1 << 13);
    pwm.duty_cycle(1 << 12);

    auto const clock_frequency_int =
      clock.frequency().numerical_value_in(mp_units::si::hertz);
    std::println("Steady clock frequency = {}", clock_frequency_int);
    auto const uptime_before = clock.uptime();
    hal::busy_wait_without_yielding_for(clock, 5us);
    auto const uptime_after = clock.uptime();
    std::println("Steady clock uptime = {} -> {}", uptime_before, uptime_after);
  } catch (hal::argument_out_of_domain const& p_errc) {
    std::println("Caught argument_out_of_domain error successfully!");
    std::println("    Object address: {}", p_errc.instance());
  } catch (...) {
    std::println("Unknown error!");
    return -1;
  }
  return 0;
}
