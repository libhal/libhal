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

#include <optional>
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
      throw hal::subscription_busy(this);
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

// The same composite, subscribing as the last statement of its constructor
// body instead of in the member initializer list.
class late_button_counter : public hal::edge_triggered_callback
{
public:
  explicit late_button_counter(hal::ref<hal::edge_triggered_interrupt> p_pin
                               [[clang::lifetimebound]])
  {
    p_pin->configure({ .trigger = hal::edge_trigger::rising });
    m_count = 0;  // anything the callback relies on is ready
    m_subscription = p_pin->on_trigger(*this);  // subscribe last
  }

  [[nodiscard]] hal::u32 count() const
  {
    return m_count;
  }

private:
  void callback(bool) noexcept override
  {
    ++m_count;
  }

  hal::u32 m_count = 1;
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

// The default way to change behavior: the callback object switches on its own
// state. Changing the mode is a plain field write, safe even from an ISR.
struct mode_counter final : hal::edge_triggered_callback
{
  enum class mode : hal::u8
  {
    count_rising,
    count_falling,
  };

  mode active = mode::count_rising;
  hal::u32 rising = 0;
  hal::u32 falling = 0;

  void callback(bool p_state) noexcept override
  {
    if (active == mode::count_rising && p_state) {
      ++rising;
    } else if (active == mode::count_falling && not p_state) {
      ++falling;
    }
  }
};

static_assert(sizeof(hal::subscription) == 2 * sizeof(void*));
static_assert(not std::is_copy_constructible_v<hal::subscription>);
static_assert(std::is_nothrow_move_constructible_v<hal::subscription>);
static_assert(std::is_nothrow_default_constructible_v<hal::subscription>);
static_assert(std::is_nothrow_move_assignable_v<hal::subscription>);
static_assert(not std::is_copy_assignable_v<hal::subscription>);

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

  "callback changes behavior through its own state"_test = []() {
    // Setup
    fake_pin pin;
    pin.configure({ .trigger = hal::edge_trigger::both });
    mode_counter counter;
    auto subscription = pin.on_trigger(counter);

    // Exercise
    pin.fire(true);
    pin.fire(false);
    counter.active = mode_counter::mode::count_falling;
    pin.fire(true);
    pin.fire(false);

    // Verify
    expect(that % 1 == counter.rising);
    expect(that % 1 == counter.falling);
    expect(&counter == pin.callback);  // never re-registered
  };

  "optional subscription swaps callbacks with reset then emplace"_test = []() {
    // Setup
    fake_pin pin;
    static_counter first;
    static_counter second;
    std::optional<hal::subscription> active;

    // Exercise
    active.emplace(pin.on_trigger(first));
    pin.fire(true);
    active.reset();  // unregisters first
    bool const empty_after_reset = (nullptr == pin.callback);
    pin.fire(true);  // nobody registered
    active.emplace(pin.on_trigger(second));
    pin.fire(true);
    active.reset();

    // Verify
    expect(empty_after_reset);
    expect(that % 1 == first.count);
    expect(that % 1 == second.count);
    expect(nullptr == pin.callback);
  };

  "optional emplace without reset throws and keeps the old callback"_test =
    []() {
      // Setup
      fake_pin pin;
      static_counter first;
      static_counter second;
      std::optional<hal::subscription> active;
      active.emplace(pin.on_trigger(first));
      bool threw = false;

      // Exercise
      // on_trigger(second) is evaluated before emplace() destroys the old
      // subscription, so the slot is still taken. reset() first.
      try {
        active.emplace(pin.on_trigger(second));
      } catch (hal::subscription_busy const&) {
        threw = true;
      }
      pin.fire(true);

      // Verify
      expect(threw);
      expect(active.has_value());
      expect(&first == pin.callback);
      expect(that % 1 == first.count);
      expect(that % 0 == second.count);
    };

  "a default constructed subscription is empty"_test = []() {
    // Setup
    fake_pin pin;

    // Exercise
    {
      hal::subscription empty;
      expect(not empty);
    }

    // Verify
    expect(nullptr == pin.callback);
  };

  "a moved from subscription is empty"_test = []() {
    // Setup
    fake_pin pin;
    static_counter counter;
    auto first = pin.on_trigger(counter);

    // Exercise
    auto second = std::move(first);

    // Verify
    expect(not first);  // NOLINT(bugprone-use-after-move): checking it
    expect(static_cast<bool>(second));
    expect(&counter == pin.callback);
  };

  "reset() unregisters and leaves the subscription empty"_test = []() {
    // Setup
    fake_pin pin;
    static_counter counter;
    auto active = pin.on_trigger(counter);

    // Exercise
    active.reset();
    pin.fire(true);
    active.reset();  // resetting an empty subscription does nothing

    // Verify
    expect(not active);
    expect(nullptr == pin.callback);
    expect(that % 0 == counter.count);
  };

  "assignment swaps callbacks after reset()"_test = []() {
    // Setup
    fake_pin pin;
    static_counter first;
    static_counter second;
    hal::subscription active;

    // Exercise
    active = pin.on_trigger(first);
    pin.fire(true);
    active.reset();
    active = pin.on_trigger(second);
    pin.fire(true);

    // Verify
    expect(that % 1 == first.count);
    expect(that % 1 == second.count);
    expect(&second == pin.callback);
  };

  "assigning to the same source without reset() throws"_test = []() {
    // Setup
    fake_pin pin;
    static_counter first;
    static_counter second;
    auto active = pin.on_trigger(first);
    bool threw = false;

    // Exercise
    // on_trigger(second) is evaluated before the assignment ends the old
    // subscription, so the slot is still taken. reset() first.
    try {
      active = pin.on_trigger(second);
    } catch (hal::subscription_busy const&) {
      threw = true;
    }
    pin.fire(true);

    // Verify
    expect(threw);
    expect(static_cast<bool>(active));
    expect(&first == pin.callback);
    expect(that % 1 == first.count);
  };

  "assigning from another source unregisters the old one"_test = []() {
    // Setup
    fake_pin old_pin;
    fake_pin new_pin;
    static_counter first;
    static_counter second;
    auto active = old_pin.on_trigger(first);

    // Exercise
    active = new_pin.on_trigger(second);

    // Verify
    expect(nullptr == old_pin.callback);
    expect(&second == new_pin.callback);
  };

  "self move assignment keeps the registration"_test = []() {
    // Setup
    fake_pin pin;
    static_counter counter;
    auto active = pin.on_trigger(counter);
    auto& alias = active;

    // Exercise
    active = std::move(alias);

    // Verify
    expect(static_cast<bool>(active));
    expect(&counter == pin.callback);
  };

  "a composite can subscribe at the end of its constructor body"_test = []() {
    // Setup
    fake_pin pin;

    // Exercise
    late_button_counter counter(pin);
    pin.fire(true);
    pin.fire(true);

    // Verify
    expect(that % 2 == counter.count());
  };
}

}  // namespace

int main()
{
  subscription_test();
}
