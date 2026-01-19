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
#include <coroutine>
#include <memory_resource>
#include <print>
#include <span>
#include <thread>
#include <variant>

import hal;
import async_context;

using namespace std::literals;

struct context : public async::basic_context
{
  std::array<async::uptr, 1024> m_stack_memory{};

  context()
  {
    initialize_stack_memory(m_stack_memory);
  }

  ~context() override = default;
};

class test_pwm : public hal::pwm16_channel
{
private:
  async::future<hal::hertz> driver_frequency(async::context&) final
  {
    using namespace mp_units::si::unit_symbols;
    return 10 * kHz;
  }
  async::future<void> driver_duty_cycle(async::context&,
                                        hal::u16 p_duty_cycle) final
  {
    std::println("duty cycle = {}/{}", p_duty_cycle, (1 << 16) - 1);
    return {};
  }
};

async::future<int> app_main(async::context& p_ctx,
                            mem::strong_ptr<hal::pwm16_channel> p_pwm)
{
  try {
    auto pwm_frequency_int = (co_await p_pwm->frequency(p_ctx))
                               .numerical_value_in(mp_units::si::hertz);
    std::println("PWM frequency = {}", pwm_frequency_int);
    co_await p_pwm->duty_cycle(p_ctx, 1 << 15);
    co_await p_pwm->duty_cycle(p_ctx, 1 << 14);
    co_await p_pwm->duty_cycle(p_ctx, 1 << 13);
    co_await p_pwm->duty_cycle(p_ctx, 1 << 12);
  } catch (hal::argument_out_of_domain const& p_errc) {
    std::println("Caught argument_out_of_domain error successfully!");
    std::println("    Object address: {}", p_errc.instance());
  } catch (...) {
    std::println("Unknown error!");
    co_return -1;
  }
  co_return 0;
}

int main()
{
  int status = 0;
  try {
    context context;
    auto pwm = mem::make_strong_ptr<test_pwm>(std::pmr::new_delete_resource());
    auto app = app_main(context, pwm);

    context.sync_wait([](async::sleep_duration p_duration) {
      std::this_thread::sleep_for(p_duration);
    });

    return app.value();
  } catch (...) {
    return -2;
  }
}
