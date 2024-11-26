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

#include <libhal/io_waiter.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
std::array call_count = { 0, 0, 0 };
}
boost::ut::suite<"polling io_waiter"> io_waiter_test = []() {
  using namespace boost::ut;
  // Setup
  hal::io_waiter& test_subject = hal::polling_io_waiter();
  call_count.fill(0);

  // Exercise
  auto stage1 = [&test_subject](io_waiter::on_wait_tag) {
    puts("Stage 1");
    call_count[0]++;
    auto stage2 = [&test_subject](io_waiter::on_wait_tag) {
      puts("Stage 2");
      call_count[1]++;
      auto stage3 = [&test_subject](io_waiter::on_wait_tag) {
        puts("Stage 3");
        call_count[2]++;
        // Turn off on_wait callback
        test_subject.on_wait(std::nullopt);
      };
      test_subject.on_wait(stage3);
      // Exercise: This wait call is a stand in for a wait called internally by
      // some driver.
      test_subject.wait();
    };
    test_subject.on_wait(stage2);
    // Exercise: This wait call is a stand in for a wait called internally by
    // some driver. Like for example a call to start streaming
    test_subject.wait();
  };

  puts("Set off stages 1st time");
  test_subject.on_wait(stage1);
  test_subject.wait();

  puts("\nSet off stages 2nd time");
  test_subject.on_wait(stage1);
  test_subject.wait();

  // Exercise: This wait call shouldn't do anything because the on_wait should
  //           be reset at this point.
  test_subject.wait();

  // Verify
  expect(that % 2 == call_count[0]);
  expect(that % 2 == call_count[1]);
  expect(that % 2 == call_count[2]);
};
}  // namespace hal
