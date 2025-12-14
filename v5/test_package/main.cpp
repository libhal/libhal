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

#include <chrono>
#include <print>
#include <span>
#include <variant>

import hal;

using namespace hal::literals;

class test_scheduler : public async::scheduler
{
private:
  void do_schedule(
    async::context&,
    async::blocked_by,
    std::variant<std::chrono::nanoseconds, async::context*>) override
  {
  }
};

class test_pwm : public hal::pwm16_channel
{
private:
  async::future<hal::hertz> driver_frequency(async::context&) final
  {
    return 10.0_kHz;
  }
  async::future<void> driver_duty_cycle(async::context&,
                                        hal::u16 p_duty_cycle) final
  {
    std::println("duty cycle = {}/{}", p_duty_cycle, (1 << 16) - 1);
    return {};
  }
};

int main()
{
  int status = 0;
  std::array<async::byte, 512> stack_memory{};
  auto stack_span = std::span{ stack_memory };
  auto scheduler =
    mem::make_strong_ptr<test_scheduler>(std::pmr::new_delete_resource());
  auto stack = mem::make_strong_ptr<std::span<async::byte>>(
    std::pmr::new_delete_resource(), stack_span);
  async::context context(scheduler, stack);

  test_pwm pwm;

  try {
    std::println("PWM frequency = {}", pwm.frequency(context).sync_wait());
    pwm.duty_cycle(context, 1 << 15).sync_wait();
    pwm.duty_cycle(context, 1 << 14).sync_wait();
    pwm.duty_cycle(context, 1 << 13).sync_wait();
    pwm.duty_cycle(context, 1 << 12).sync_wait();
  } catch (hal::argument_out_of_domain const& p_errc) {
    std::println("Caught argument_out_of_domain error successfully!");
    std::println("    Object address: {}", p_errc.instance());
  } catch (...) {
    std::println("Unknown error!");
    status = -1;
  }

  return status;
}
