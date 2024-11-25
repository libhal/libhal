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

#pragma once

#include <optional>
#include <span>

#include "../functional.hpp"
#include "../units.hpp"

namespace hal {
class i2c_target
{
public:
  struct transaction_handler_tag
  {};

  using handler = hal::callback<std::span<byte>(transaction_handler_tag,
                                                std::span<byte const> p_data_in,
                                                std::span<byte> p_data_out)>;

  void register_transaction_handler(u8 p_address,
                                    std::optional<handler> p_callback,
                                    std::span<byte const> p_data_in,
                                    std::span<byte> p_data_out)
  {
    driver_register_transaction_handler(
      p_address, p_callback, p_data_in, p_data_out);
  }

  virtual ~i2c_target() = default;

private:
  virtual void driver_register_transaction_handler(
    u8 p_address,
    std::optional<handler> p_callback,
    std::span<byte const> p_data_in,
    std::span<byte> p_data_out) = 0;
};
}  // namespace hal
