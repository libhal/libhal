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

module;

#include <exception>
#include <functional>
#include <system_error>
#include <type_traits>
#include <utility>

export module hal:subscription;

import :error;

namespace hal::inline v5 {

/**
 * @brief Ends a callback registration when destroyed
 *
 * Move-only, two pointers. Every single-subscriber callback API returns this
 * type, so a composite listening to several sources stores plain
 * `hal::subscription` members. Declare them after the callbacks and state
 * they reach, so they are destroyed first.
 *
 * A subscription can be empty: default constructed, moved from, or reset().
 * An empty subscription is registered with nothing. That lets a member be
 * assigned in the constructor body, where the callback can safely start
 * running once everything it uses is set up:
 *
 * ```cpp
 * motor_controller(hal::can_interrupt& p_interrupt [[clang::lifetimebound]])
 * {
 *   configure();  // whatever the callback relies on
 *   m_subscription = p_interrupt.on_receive(*this, filter);  // subscribe last
 * }
 * ```
 *
 * To subscribe to the same source again, reset() first. Assignment evaluates
 * its right side first, which subscribes while the old subscription still
 * holds the source's only slot and throws hal::subscription_busy.
 *
 * Never assign, reset() or destroy a subscription from an interrupt service
 * routine: each of them unregisters a callback.
 *
 * A source creates one with make<Unsubscribe>(source), where Unsubscribe is a
 * constant callable (member function pointer or captureless lambda) invoked
 * as Unsubscribe(source). It must be noexcept: it runs from a destructor.
 */
export class [[nodiscard(
  "destroying a subscription unregisters its callback, so a discarded one "
  "unregisters it immediately; store it for as long as the callback should "
  "run")]] [[gsl::Pointer]] subscription
{
public:
  /**
   * @brief Create a subscription that calls Unsubscribe(p_source) when it
   * ends
   *
   * @tparam Unsubscribe - noexcept callable invoked as Unsubscribe(source)
   * @param p_source - the callback source this subscription belongs to
   * @return subscription - the new registration handle
   */
  template<auto Unsubscribe, class Source>
  static subscription make(Source& p_source [[clang::lifetimebound]]) noexcept
  {
    static_assert(
      std::is_nothrow_invocable_v<decltype(Unsubscribe), Source&>,
      "Unsubscribe must be callable as Unsubscribe(source) noexcept");
    return subscription(&p_source, [](void* p_src) noexcept {
      std::invoke(Unsubscribe, *static_cast<Source*>(p_src));
    });
  }

  /// @brief Create an empty subscription, registered with nothing
  constexpr subscription() noexcept = default;

  /**
   * @brief Take over another subscription
   *
   * @param p_other - subscription that is left empty
   */
  subscription(subscription&& p_other) noexcept
    : m_source(std::exchange(p_other.m_source, nullptr))
    , m_unsubscribe(std::exchange(p_other.m_unsubscribe, nullptr))
  {
  }

  /**
   * @brief End this subscription, then take over another one
   *
   * @param p_other - subscription that is left empty
   * @return subscription& - this subscription
   */
  subscription& operator=(subscription&& p_other) noexcept
  {
    if (this != &p_other) {
      reset();
      m_source = std::exchange(p_other.m_source, nullptr);
      m_unsubscribe = std::exchange(p_other.m_unsubscribe, nullptr);
    }
    return *this;
  }

  /// Unregister the callback from its source
  ~subscription()
  {
    reset();
  }

  /**
   * @brief Unregister the callback from its source, leaving this subscription
   * empty
   *
   * Clang's lifetime analysis doesn't see reset() end what the subscription
   * points at. Where that matters, such as returning a subscription that was
   * reset, assign an empty one instead: `active = {};`.
   */
  [[clang::reinitializes]] void reset() noexcept
  {
    auto* const unsubscribe = std::exchange(m_unsubscribe, nullptr);
    auto* const source = std::exchange(m_source, nullptr);
    if (unsubscribe) {
      unsubscribe(source);
    }
  }

  /// @return true - if this subscription is registered with a source
  [[nodiscard]] explicit operator bool() const noexcept
  {
    return m_unsubscribe != nullptr;
  }

private:
  using unsubscribe_fn = void (*)(void*) noexcept;

  subscription(void* p_source, unsubscribe_fn p_fn) noexcept
    : m_source(p_source)
    , m_unsubscribe(p_fn)
  {
  }

  void* m_source = nullptr;
  unsubscribe_fn m_unsubscribe = nullptr;
};

/// @brief Thrown when subscribing to a source that already has a subscriber.
export struct subscription_busy : public exception
{
  /**
   * @brief Construct a new subscription_busy exception
   *
   * @param p_instance - the driver that already has a subscriber
   */
  explicit subscription_busy(void const* p_instance)
    : exception(std::errc::device_or_resource_busy, p_instance)
  {
  }

  /// @return char const* - "hal::subscription_busy"
  [[nodiscard]] char const* what() const noexcept final
  {
    return "hal::subscription_busy";
  }
};

}  // namespace hal::inline v5
