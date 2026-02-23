// Copyright 2024 - 2025 Khalil Estell and the libhal contributors
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

module;

#include <span>

export module hal:i2c;

export import :units;
import async_context;

namespace hal::inline v5 {
class i2c_mmap_target
{
  virtual usize size() = 0;
  virtual async::future<void> write(async::context& p_context,
                                    u32 p_address,
                                    std::span<u8> p_buffer) = 0;
  virtual async::future<std::span<u8>> read(async::context& p_context,
                                            u32 p_address,
                                            std::span<u8> p_buffer) = 0;
};
}  // namespace hal::inline v5
