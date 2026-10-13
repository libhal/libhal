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
#include <type_traits>
#include <utility>

export module hal:subscription;

namespace hal::inline v5 {

/**
 * @brief Ends a callback registration when destroyed
 *
 * Move-only, two pointers. Every single-subscriber callback API returns this
 * type, so a composite listening to several sources stores plain
 * `hal::subscription` members. Declare them after the callbacks and state
 * they reach, so they are destroyed first.
 *
 * A source creates one with make<Unsubscribe>(source), where Unsubscribe is a
 * constant callable (member function pointer or captureless lambda) invoked
 * as Unsubscribe(source). It must be noexcept: it runs from a destructor.
 */
export class [[nodiscard]] [[gsl::Pointer]] subscription
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
  [[nodiscard]] static subscription make(Source& p_source
                                         [[clang::lifetimebound]]) noexcept
  {
    static_assert(
      std::is_nothrow_invocable_v<decltype(Unsubscribe), Source&>,
      "Unsubscribe must be callable as Unsubscribe(source) noexcept");
    return subscription(&p_source, [](void* p_src) noexcept {
      std::invoke(Unsubscribe, *static_cast<Source*>(p_src));
    });
  }

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
  subscription& operator=(subscription&&) = delete;

  /// Unregister the callback from its source
  ~subscription()
  {
    if (m_unsubscribe) {
      m_unsubscribe(m_source);
    }
  }

private:
  using unsubscribe_fn = void (*)(void*) noexcept;

  subscription(void* p_source, unsubscribe_fn p_fn) noexcept
    : m_source(p_source)
    , m_unsubscribe(p_fn)
  {
  }

  void* m_source;
  unsubscribe_fn m_unsubscribe;
};

/// @brief Thrown when subscribing to a source that already has a subscriber.
export struct subscription_busy : std::exception
{
  /// @return char const* - description of the error
  [[nodiscard]] char const* what() const noexcept override
  {
    return "hal: source already has a subscriber";
  }
};

}  // namespace hal::inline v5
