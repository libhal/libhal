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

#include <print>

import hal;

using namespace hal::literals;

class test_pwm : public hal::pwm16_channel
{
private:
  hal::hertz driver_frequency() final
  {
    return 10.0_kHz;
  }
  void driver_duty_cycle(hal::u16 p_duty_cycle) final
  {
    std::println("duty cycle = {}/{}", p_duty_cycle, (1 << 16) - 1);
  }
};

int main()
{
  int status = 0;
  test_pwm pwm;

  try {
    std::println("PWM frequency = {}", pwm.frequency());
    pwm.duty_cycle(1 << 15);
    pwm.duty_cycle(1 << 14);
    pwm.duty_cycle(1 << 13);
    pwm.duty_cycle(1 << 12);
  } catch (hal::argument_out_of_domain const& p_errc) {
    std::println("Caught argument_out_of_domain error successfully!");
    std::println("    Object address: {}", p_errc.instance());
  } catch (...) {
    std::println("Unknown error!");
    status = -1;
  }

  return status;
}
