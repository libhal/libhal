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

#include <memory_resource>

export module hal:memory;

import strong_ptr;

namespace hal::inline v5 {

export using allocator = std::pmr::polymorphic_allocator<>;

export template<class T>
using ptr = mem::strong_ptr<T>;

export template<class T>
using opt_ptr = mem::optional_ptr<T>;

export template<class T, class... Args>
ptr<T> allocate(allocator p_allocator, Args&&... p_args)
{
  return mem::make_strong_ptr<T>(p_allocator, p_args...);
}

export template<class T, class... Args, auto unique_key = []() {}>
mem::strong_ptr<T> static_allocate(Args... p_args)
{
  static T object(p_args...);
  return mem::strong_ptr<T>(mem::unsafe_assume_static_tag{}, object);
}

export template<typename Derived>
class enable_pimpl : public mem::enable_strong_from_this<enable_pimpl<Derived>>
{
  using destroy_fn_t = void(void*, allocator) noexcept;

protected:
  [[nodiscard]] auto& impl() noexcept
  {
    return *static_cast<typename Derived::impl*>(m_impl);
  }

  [[nodiscard]] auto const& impl() const noexcept
  {
    return *static_cast<typename Derived::impl const*>(m_impl);
  }

  template<typename... Args>
  void initialize_pimpl(allocator p_resource, Args&&... p_args)
  {
    using impl_type = typename Derived::impl;
    m_impl = p_resource.new_object<impl_type>(std::forward<Args>(p_args)...);
    m_destroy = [](void* p_address, allocator p_resource) noexcept {
      p_resource.delete_object(static_cast<impl_type*>(p_address));
    };
  }

  ~enable_pimpl() noexcept
  {
    if (m_impl != nullptr) {
      m_destroy(m_impl, this->strong_from_this().get_allocator());
    }
  }

private:
  friend Derived;
  struct private_key
  {
    friend Derived;

  private:
    private_key() = default;
  };
  enable_pimpl() = default;
  void* m_impl = nullptr;
  destroy_fn_t* m_destroy = nullptr;
};
}  // namespace hal::inline v5
