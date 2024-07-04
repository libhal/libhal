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

#include <cstdio>

#include <libhal/error.hpp>
#include <libhal/pwm.hpp>

using namespace hal::literals;

class test_pwm : public hal::pwm
{
private:
  void driver_frequency(hal::hertz p_frequency) final
  {
    if (p_frequency > 1.0_MHz) {
      hal::safe_throw(hal::argument_out_of_domain(this));
    }
    std::printf("frequency = %f Hz\n", p_frequency);
  }
  void driver_duty_cycle(float p_position) final
  {
    std::printf("duty cycle = %f %%\n", p_position);
  }
};

int main()
{
  int status = 0;
  test_pwm pwm;

  try {
    pwm.frequency(10.0_kHz);
    pwm.duty_cycle(0.25);
    pwm.duty_cycle(0.50);
    pwm.duty_cycle(-0.25);
    pwm.duty_cycle(-1.0);
    pwm.frequency(10.0_MHz);
  } catch (hal::argument_out_of_domain const& p_errc) {
    std::printf("Caught argument_out_of_domain error successfully!\n");
    std::printf("    Object address: %p\n", p_errc.instance());
  } catch (...) {
    std::printf("Unknown error!\n");
    status = -1;
  }

  return status;
}
