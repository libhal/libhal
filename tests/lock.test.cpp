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

#include <libhal/lock.hpp>

#include <mutex>

#include <libhal/error.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
struct timed_lock_impl : public hal::timed_lock
{
  // Publicly available for inspection and modification
  bool lock_acquired = false;
  hal::time_duration duration{ 0 };
  bool allow_lock_to_be_acquired = true;

private:
  void os_lock() override
  {
    lock_acquired = true;
  }
  void os_unlock() override
  {
    lock_acquired = false;
  }
  bool os_try_lock() override
  {
    if (allow_lock_to_be_acquired) {
      lock_acquired = true;
    } else {
      lock_acquired = false;
    }
    return lock_acquired;
  }
  bool os_try_lock_for(hal::time_duration p_duration) override
  {
    duration = p_duration;
    if (allow_lock_to_be_acquired) {
      lock_acquired = true;
    } else {
      lock_acquired = false;
    }
    return lock_acquired;
  }
};
}  // namespace

void lock_test()
{
  using namespace boost::ut;

  "lock"_test = []() {
    // Setup
    timed_lock_impl test_subject;
    hal::timed_lock& timed_lock_ref = test_subject;
    hal::pollable_lock& pollable_lock_ref = test_subject;
    hal::basic_lock& basic_lock_ref = test_subject;
    test_subject.lock_acquired = false;

    /* Exercise */ {
      test_subject.lock();
      // Verify
      expect(that % test_subject.lock_acquired);
    }

    test_subject.lock_acquired = false;

    /* Exercise */ {
      timed_lock_ref.lock();
      // Verify
      expect(that % test_subject.lock_acquired);
    }

    /* Exercise */ {
      pollable_lock_ref.lock();
      // Verify
      expect(that % test_subject.lock_acquired);
    }

    test_subject.lock_acquired = false;

    /* Exercise */ {
      basic_lock_ref.lock();
      // Verify
      expect(that % test_subject.lock_acquired);
    }
  };

  "unlock"_test = []() {
    // Setup
    timed_lock_impl test_subject;
    hal::timed_lock& timed_lock_ref = test_subject;
    hal::pollable_lock& pollable_lock_ref = test_subject;
    hal::basic_lock& basic_lock_ref = test_subject;
    test_subject.lock_acquired = true;

    /* Exercise */ {
      test_subject.unlock();
      // Verify
      expect(that % not test_subject.lock_acquired);
    }

    test_subject.lock_acquired = true;

    /* Exercise */ {
      timed_lock_ref.unlock();
      // Verify
      expect(that % not test_subject.lock_acquired);
    }

    /* Exercise */ {
      pollable_lock_ref.unlock();
      // Verify
      expect(that % not test_subject.lock_acquired);
    }

    test_subject.lock_acquired = true;

    /* Exercise */ {
      basic_lock_ref.unlock();
      // Verify
      expect(that % not test_subject.lock_acquired);
    }
  };

  "try_lock"_test = []() {
    // Setup
    timed_lock_impl test_subject;
    hal::timed_lock& timed_lock_ref = test_subject;
    hal::pollable_lock& pollable_lock_ref = test_subject;
    test_subject.lock_acquired = false;
    test_subject.allow_lock_to_be_acquired = true;

    /* Exercise */ {
      auto got_lock = test_subject.try_lock();
      // Verify
      expect(that % test_subject.lock_acquired);
      expect(that % got_lock);
    }

    test_subject.lock_acquired = false;

    /* Exercise */ {
      auto got_lock = timed_lock_ref.try_lock();
      // Verify
      expect(that % test_subject.lock_acquired);
      expect(that % got_lock);
    }

    /* Exercise */ {
      auto got_lock = pollable_lock_ref.try_lock();
      // Verify
      expect(that % test_subject.lock_acquired);
      expect(that % got_lock);
    }

    test_subject.lock_acquired = false;
    test_subject.allow_lock_to_be_acquired = false;

    /* Exercise */ {
      auto got_lock = test_subject.try_lock();
      // Verify
      expect(that % not test_subject.lock_acquired);
      expect(that % not got_lock);
    }

    /* Exercise */ {
      auto got_lock = timed_lock_ref.try_lock();
      // Verify
      expect(that % not test_subject.lock_acquired);
      expect(that % not got_lock);
    }

    /* Exercise */ {
      auto got_lock = pollable_lock_ref.try_lock();
      // Verify
      expect(that % not test_subject.lock_acquired);
      expect(that % not got_lock);
    }
  };

  "try_lock_for"_test = []() {
    // Setup
    using namespace std::chrono_literals;
    timed_lock_impl test_subject;
    hal::timed_lock& timed_lock_ref = test_subject;
    test_subject.lock_acquired = false;
    test_subject.allow_lock_to_be_acquired = true;

    /* Exercise */ {
      auto got_lock = test_subject.try_lock_for(5ms);
      // Verify
      expect(that % test_subject.lock_acquired);
      expect(that % got_lock);
      expect(5ms == test_subject.duration);
    }

    test_subject.lock_acquired = false;

    /* Exercise */ {
      auto got_lock = timed_lock_ref.try_lock_for(10ms);
      // Verify
      expect(that % test_subject.lock_acquired);
      expect(that % got_lock);
      expect(10ms == test_subject.duration);
    }

    test_subject.lock_acquired = false;
    test_subject.allow_lock_to_be_acquired = false;

    /* Exercise */ {
      auto got_lock = test_subject.try_lock_for(15ms);
      // Verify
      expect(that % not test_subject.lock_acquired);
      expect(that % not got_lock);
      expect(15ms == test_subject.duration);
    }

    /* Exercise */ {
      auto got_lock = timed_lock_ref.try_lock_for(20ms);
      // Verify
      expect(that % not test_subject.lock_acquired);
      expect(that % not got_lock);
      expect(20ms == test_subject.duration);
    }
  };

  "Usage w/ std::lock_guard<hal::basic_lock>"_test = []() {
    // Setup
    timed_lock_impl test_subject;
    hal::basic_lock& basic_lock_ref = test_subject;
    hal::pollable_lock& pollable_lock_ref = test_subject;
    hal::timed_lock& timed_lock_ref = test_subject;
    test_subject.lock_acquired = false;

    /* Exercise */ {
      expect(that % not test_subject.lock_acquired);
      std::lock_guard my_lock(test_subject);
      // Verify
      expect(that % test_subject.lock_acquired);
    }
    expect(that % not test_subject.lock_acquired);

    test_subject.lock_acquired = false;

    /* Exercise */ {
      expect(that % not test_subject.lock_acquired);
      std::lock_guard my_lock(basic_lock_ref);
      // Verify
      expect(that % test_subject.lock_acquired);
    }
    expect(that % not test_subject.lock_acquired);

    test_subject.lock_acquired = false;

    /* Exercise */ {
      expect(that % not test_subject.lock_acquired);
      std::lock_guard my_lock(pollable_lock_ref);
      // Verify
      expect(that % test_subject.lock_acquired);
    }
    expect(that % not test_subject.lock_acquired);

    test_subject.lock_acquired = false;

    /* Exercise */ {
      expect(that % not test_subject.lock_acquired);
      std::lock_guard my_lock(timed_lock_ref);
      // Verify
      expect(that % test_subject.lock_acquired);
    }
    expect(that % not test_subject.lock_acquired);
  };
};
}  // namespace hal
