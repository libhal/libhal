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

// Single-subscriber callbacks through hal::subscription, using
// edge_triggered_interrupt as the example source. Misuse that Clang's lifetime
// safety analysis must catch is in tests/compile_checks/subscription_cases.cpp.

#include <type_traits>
#include <utility>

#include <boost/ut.hpp>

import hal;

namespace {

// Fake platform driver that follows the single-subscriber contract.
struct fake_pin final : hal::edge_triggered_interrupt
{
  hal::edge_triggered_callback* callback = nullptr;
  settings config{};

  // Like real hardware: only edges matching the configured trigger fire.
  void fire(bool p_state)
  {
    bool const wanted =
      config.trigger == hal::edge_trigger::both ||
      (config.trigger == hal::edge_trigger::rising && p_state) ||
      (config.trigger == hal::edge_trigger::falling && not p_state);
    if (callback && wanted) {
      callback->callback(p_state);
    }
  }

private:
  void driver_configure(settings const& p_settings) override
  {
    config = p_settings;
  }
  void driver_on_trigger(hal::edge_triggered_callback& p_callback) override
  {
    if (callback) {
      throw hal::subscription_busy{};
    }
    callback = &p_callback;
  }
  void driver_clear_trigger() noexcept override
  {
    callback = nullptr;
  }
};

// Composite that listens to a pin: state first, subscription last, so the
// subscription is destroyed before anything the callback reaches.
class button_counter : public hal::edge_triggered_callback
{
public:
  explicit button_counter(hal::ref<hal::edge_triggered_interrupt> p_pin
                          [[clang::lifetimebound]],
                          hal::edge_trigger p_edge = hal::edge_trigger::rising)
    : m_subscription(subscribe(*p_pin, p_edge, *this))
  {
  }

  [[nodiscard]] hal::u32 count() const
  {
    return m_count;
  }

private:
  static hal::subscription subscribe(hal::edge_triggered_interrupt& p_pin,
                                     hal::edge_trigger p_edge,
                                     button_counter& p_self)
  {
    p_pin.configure({ .trigger = p_edge });
    return p_pin.on_trigger(p_self);
  }

  void callback(bool) noexcept override
  {
    ++m_count;
  }

  hal::u32 m_count = 0;
  hal::subscription m_subscription;  // last
};

struct static_counter final : hal::edge_triggered_callback
{
  hal::u32 count = 0;
  void callback(bool) noexcept override
  {
    ++count;
  }
};

static_assert(sizeof(hal::subscription) == 2 * sizeof(void*));
static_assert(not std::is_copy_constructible_v<hal::subscription>);
static_assert(std::is_nothrow_move_constructible_v<hal::subscription>);

void subscription_test()
{
  using namespace boost::ut;

  "subscription delivers edges and unregisters when destroyed"_test = []() {
    // Setup
    fake_pin pin;
    hal::u32 count = 0;

    // Exercise
    {
      button_counter counter(pin);
      pin.fire(true);
      pin.fire(false);  // rising only
      count = counter.count();
    }

    // Verify
    expect(that % 1 == count);
    expect(nullptr == pin.callback);
  };

  "second subscriber throws, first keeps working"_test = []() {
    // Setup
    fake_pin pin;
    button_counter first(pin);
    bool threw = false;

    // Exercise
    try {
      button_counter second(pin);
    } catch (hal::subscription_busy const&) {
      threw = true;
    }
    pin.fire(true);

    // Verify
    expect(threw);
    expect(that % 1 == first.count());
  };

  "static_ref registration is permanent"_test = []() {
    // Setup
    static fake_pin pin;
    static static_counter counter;
    pin.on_trigger(hal::static_ref<hal::edge_triggered_callback>{ counter });

    // Exercise
    pin.fire(true);

    // Verify
    expect(that % 1 == counter.count);
    expect(&counter == pin.callback);
  };

  "moving a subscription transfers the registration"_test = []() {
    // Setup
    fake_pin pin;
    static_counter counter;

    // Exercise
    {
      auto original = pin.on_trigger(counter);
      {
        hal::subscription moved = std::move(original);
        expect(&counter == pin.callback);
      }  // moved ends the registration
      expect(nullptr == pin.callback);
    }  // original was moved from; destroying it must not unregister again

    // Verify
    expect(nullptr == pin.callback);
  };
}

}  // namespace

int main()
{
  subscription_test();
}
