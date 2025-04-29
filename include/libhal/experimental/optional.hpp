#pragma once

#include <optional>

namespace hal::inline v1 {
/**
 * @brief libhal's overload of std::optional to eliminate UB from common
 * accessor operators.
 *
 * @tparam T - optional object type
 */
template<typename T>
class optional : public std::optional<T>
{
public:
  // Inherit all constructors from std::optional
  using std::optional<T>::optional;

  // One version of value which works for everything
  template<class Self>
  constexpr auto& operator*(this Self&& p_self)
  {
    return std::forward<Self>(p_self).value();
  }

  // One version of value which works for everything
  template<class Self>
  constexpr auto* operator->(this Self&& p_self)
  {
    return &std::forward<Self>(p_self).value();
  }
};

/// Deduction guides to support CTAD
template<typename T>
optional(T) -> optional<T>;
}  // namespace hal::inline v1
