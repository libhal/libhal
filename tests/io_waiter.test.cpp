#include <libhal/io_waiter.hpp>

#include <boost/ut.hpp>

namespace hal {
void io_waiter_test()
{
  using namespace boost::ut;
  "spin lock waiter test"_test = []() {
    // Setup
    hal::io_waiter& test_subject = hal::polling_io_waiter();
    // Exercise
    // Verify
    test_subject.wait();
    test_subject.resume();
  };
}
}  // namespace hal
