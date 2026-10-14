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

// Compile-time checks for single-subscriber callbacks, using
// edge_triggered_interrupt as the example source. Each case is built as its
// own target with -DCASE_<name> (see compile_checks.cmake):
//   ERR_*   must be rejected by the compiler
//   WARN_*  must be reported by Clang (built with -Werror -Wlifetime-safety)
//   GAP_*   known misses: must compile cleanly, so a future fix shows up

#include <optional>

import hal;

struct fake_pin final : hal::edge_triggered_interrupt
{
  hal::edge_triggered_callback* callback = nullptr;

private:
  void driver_configure(settings const&) override
  {
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

class button_counter : public hal::edge_triggered_callback
{
public:
  explicit button_counter(hal::ref<hal::edge_triggered_interrupt> p_pin
                          [[clang::lifetimebound]])
    : m_subscription(p_pin->on_trigger(*this))
  {
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
  hal::u32 m_count = 0;
  hal::subscription m_subscription;  // last
};

struct counter final : hal::edge_triggered_callback
{
  void callback(bool) noexcept override
  {
  }
};

#if defined(CASE_ERR_STATIC_REF_TO_LOCAL_CALLBACK)
// Permanent registration needs a callback with static storage.
int main()
{
  static fake_pin pin;
  counter local;
  pin.on_trigger(hal::static_ref<hal::edge_triggered_callback>{ local });
}

#elif defined(CASE_WARN_DISCARDED_SUBSCRIPTION)
// A discarded subscription unregisters immediately.
int main()
{
  fake_pin pin;
  counter c;
  pin.on_trigger(c);
}

#elif defined(CASE_WARN_SUBSCRIPTION_OUTLIVES_CALLBACK)
hal::subscription f(fake_pin& p_pin)
{
  counter local;
  return p_pin.on_trigger(local);
}
int main()
{
  static fake_pin pin;
  auto subscription = f(pin);
}

#elif defined(CASE_WARN_SUBSCRIPTION_OUTLIVES_PIN)
hal::subscription f()
{
  static counter c;
  fake_pin local;
  return local.on_trigger(c);
}
int main()
{
  auto subscription = f();
}

#elif defined(CASE_WARN_RETURN_COUNTER_PAST_LOCAL_PIN)
button_counter f()
{
  fake_pin pin;
  return button_counter(pin);
}
int main()
{
  return static_cast<int>(f().count());
}

#elif defined(CASE_WARN_STATIC_COUNTER_ON_LOCAL_PIN)
void f()
{
  fake_pin pin;  // forgot static
  static button_counter c(pin);
}
int main()
{
  f();
}

#elif defined(CASE_GAP_OPTIONAL_OUTLIVES_CALLBACK)
// Optional subscription returned past the callback it registered.
std::optional<hal::subscription> f(fake_pin& p_pin)
{
  counter local;
  std::optional<hal::subscription> active;
  active.emplace(p_pin.on_trigger(local));
  return active;
}
int main()
{
  static fake_pin pin;
  auto active = f(pin);
}

#elif defined(CASE_GAP_OPTIONAL_DIRECT_INIT_OUTLIVES_CALLBACK)
std::optional<hal::subscription> f(fake_pin& p_pin)
{
  counter local;
  return std::optional<hal::subscription>(p_pin.on_trigger(local));
}
int main()
{
  static fake_pin pin;
  auto active = f(pin);
}

#elif defined(CASE_GAP_OPTIONAL_OUTLIVES_PIN)
std::optional<hal::subscription> f()
{
  static counter c;
  fake_pin local;
  std::optional<hal::subscription> active;
  active.emplace(local.on_trigger(c));
  return active;
}
int main()
{
  auto active = f();
}

#elif defined(CASE_GAP_STATIC_OPTIONAL_ON_LOCAL_CALLBACK)
void f()
{
  static fake_pin pin;
  static std::optional<hal::subscription> active;
  counter local;  // forgot static
  active.emplace(pin.on_trigger(local));
}
int main()
{
  f();
}

#else
#error "define one CASE_*"
#endif
