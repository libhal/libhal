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

#include <libhal/error.hpp>
#include <libhal/io_waiter.hpp>
#include <libhal/timer.hpp>
#include <libhal/units.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
std::array call_count = { 0, 0, 0 };

struct serial_io_tag
{};

template<typename... Callbacks>
void serial_io_wait(hal::io_waiter& p_waiter, Callbacks... callbacks)
{
  std::array<hal::callback<void(serial_io_tag)>, sizeof...(callbacks)>
    callback_array{ callbacks... };

  std::array<hal::io_waiter::on_wait_handler, callback_array.size()>
    final_callback_array{};

  struct gather
  {
    decltype(callback_array)& array;
    decltype(final_callback_array)& final_array;
    hal::io_waiter& waiter;
  };

  gather group = { .array = callback_array,
                   .final_array = final_callback_array,
                   .waiter = p_waiter };

  for (std::size_t i = 0; i < callback_array.size(); i++) {
    final_callback_array[i] = [&group, i](io_waiter::on_wait_tag) {
      if (i + 1 == group.array.size()) {
        group.waiter.on_wait(std::nullopt);
        return;
      }
      group.waiter.on_wait(group.final_array[i + 1]);
      group.array[i](serial_io_tag{});
      group.waiter.wait();
    };
  }

  p_waiter.on_wait(final_callback_array[0]);
  p_waiter.wait();
}

}  // namespace
boost::ut::suite<"polling io_waiter"> io_waiter_test = []() {
  using namespace boost::ut;
  // Setup
  hal::io_waiter& io_waiter = hal::polling_io_waiter();
  call_count.fill(0);

  // Exercise
  auto stage3 = [&io_waiter](io_waiter::on_wait_tag) {
    puts("Stage 3");
    call_count[2]++;
    // Turn off on_wait callback
    io_waiter.on_wait(std::nullopt);
  };

  auto stage2 = [&io_waiter, &stage3](io_waiter::on_wait_tag) {
    puts("Stage 2");
    call_count[1]++;

    io_waiter.on_wait(stage3);
    // Exercise: This wait call is a stand in for a wait called internally by
    // some driver.
    io_waiter.wait();
  };

  auto stage1 = [&io_waiter, &stage2](io_waiter::on_wait_tag) {
    puts("Stage 1");
    call_count[0]++;

    io_waiter.on_wait(stage2);
    // Exercise: This wait call is a stand in for a wait called internally by
    // some driver. Like for example a call to start streaming
    io_waiter.wait();  // ils9110.read();
  };

  puts("Set off stages 1st time");
  io_waiter.on_wait(stage1);
  io_waiter.wait();  // ils9110.read();

  puts("\nSet off stages 2nd time");
  io_waiter.on_wait(stage1);
  io_waiter.wait();

  // Exercise: This wait call shouldn't do anything because the on_wait should
  //           be reset at this point.
  io_waiter.wait();

  serial_io_wait(
    io_waiter,
    [](serial_io_tag) { puts("serial stage 1"); },
    [](serial_io_tag) { puts("serial stage 2"); },
    [](serial_io_tag) { puts("serial stage 3"); });

  // Verify
  expect(that % 2 == call_count[0]);
  expect(that % 2 == call_count[1]);
  expect(that % 2 == call_count[2]);
};  // namespace hal
}  // namespace hal
