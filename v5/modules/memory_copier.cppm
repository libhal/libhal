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

export module hal:memory_copier;

export import scatter_span;

import :units;

namespace hal::inline v5 {
/**
 * @brief Copies and fills memory, typically with a DMA controller
 *
 * Use this to move large blocks of memory, such as a frame buffer or a block
 * of storage, without the CPU copying each byte. Implementations backed by
 * hardware usually sleep the calling context on its waiter while the copy
 * runs, so other contexts can use the CPU. An implementation that copies with
 * the CPU, using `std::memcpy`, is also valid, for platforms without copy
 * hardware and for tests.
 *
 * Only memory to memory transfers are covered. Moving data between memory and
 * a peripheral needs that peripheral's data register and request line, which
 * only the platform's own drivers know, so those transfers stay in platform
 * libraries.
 */
export class memory_copier
{
public:
  /**
   * @brief Copy bytes from one block of memory to another
   *
   * Copies `min(p_source.length(), p_destination.length())` bytes, in order,
   * across the chunks of both scatter spans. Blocks the calling context until
   * the copy is done.
   *
   * If the source and destination overlap, the contents of the destination
   * are unspecified afterwards.
   *
   * @param p_source - the bytes to copy
   * @param p_destination - where the bytes go
   * @throws hal::io_error - the copy hardware reported a transfer error
   */
  void copy(mem::scatter_span<byte const> p_source,
            mem::scatter_span<byte> p_destination)
  {
    driver_copy(p_source, p_destination);
  }

  /**
   * @brief Set every byte of a block of memory to one value
   *
   * Blocks the calling context until every byte is written.
   *
   * @param p_destination - the bytes to set
   * @param p_value - the value to set them to
   * @throws hal::io_error - the copy hardware reported a transfer error
   */
  void fill(mem::scatter_span<byte> p_destination, byte p_value)
  {
    driver_fill(p_destination, p_value);
  }

protected:
  ~memory_copier() = default;

private:
  virtual void driver_copy(mem::scatter_span<byte const> p_source,
                           mem::scatter_span<byte> p_destination) = 0;
  virtual void driver_fill(mem::scatter_span<byte> p_destination,
                           byte p_value) = 0;
};
}  // namespace hal::inline v5
