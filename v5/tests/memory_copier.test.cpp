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

#include <algorithm>
#include <array>
#include <span>

#include <boost/ut.hpp>

import hal;

namespace {
/// Copies with the CPU, one byte at a time, the way a software
/// implementation would.
class software_copier final : public hal::memory_copier
{
public:
  int copy_count = 0;
  int fill_count = 0;

private:
  void driver_copy(mem::scatter_span<hal::byte const> p_source,
                   mem::scatter_span<hal::byte> p_destination) override
  {
    copy_count++;
    auto source_chunk = p_source.begin();
    auto destination_chunk = p_destination.begin();
    std::size_t source_offset = 0;
    std::size_t destination_offset = 0;

    while (source_chunk != p_source.end() &&
           destination_chunk != p_destination.end()) {
      auto const from = *source_chunk;
      auto const to = *destination_chunk;
      if (source_offset == from.size()) {
        ++source_chunk;
        source_offset = 0;
        continue;
      }
      if (destination_offset == to.size()) {
        ++destination_chunk;
        destination_offset = 0;
        continue;
      }
      to[destination_offset++] = from[source_offset++];
    }
  }

  void driver_fill(mem::scatter_span<hal::byte> p_destination,
                   hal::byte p_value) override
  {
    fill_count++;
    for (auto const chunk : p_destination) {
      std::ranges::fill(chunk, p_value);
    }
  }
};

void memory_copier_test()
{
  using namespace boost::ut;

  "copy() copies the bytes through driver_copy()"_test = []() {
    // Setup
    software_copier copier;
    std::array<hal::byte, 4> const source{ 1, 2, 3, 4 };
    std::array<hal::byte, 4> destination{};

    // Exercise
    copier.copy({ std::span(source) }, { std::span(destination) });

    // Verify
    expect(that % 1 == copier.copy_count);
    expect(source == destination);
  };

  "copy() stops at the shorter of the two spans"_test = []() {
    // Setup
    software_copier copier;
    std::array<hal::byte, 4> const source{ 1, 2, 3, 4 };
    std::array<hal::byte, 6> destination{};

    // Exercise
    copier.copy({ std::span(source) }, { std::span(destination) });

    // Verify
    expect(destination == std::array<hal::byte, 6>{ 1, 2, 3, 4, 0, 0 });
  };

  "copy() carries chunk boundaries through to the driver"_test = []() {
    // Setup
    software_copier copier;
    std::array<hal::byte, 2> const first{ 1, 2 };
    std::array<hal::byte, 3> const second{ 3, 4, 5 };
    std::array<hal::byte, 4> head{};
    std::array<hal::byte, 1> tail{};

    // Exercise
    copier.copy({ std::span(first), std::span(second) },
                { std::span(head), std::span(tail) });

    // Verify
    expect(head == std::array<hal::byte, 4>{ 1, 2, 3, 4 });
    expect(that % 5 == tail[0]);
  };

  "fill() sets every byte through driver_fill()"_test = []() {
    // Setup
    software_copier copier;
    std::array<hal::byte, 3> first{};
    std::array<hal::byte, 2> second{};

    // Exercise
    copier.fill({ std::span(first), std::span(second) }, 0xAA);

    // Verify
    expect(that % 1 == copier.fill_count);
    expect(first == std::array<hal::byte, 3>{ 0xAA, 0xAA, 0xAA });
    expect(second == std::array<hal::byte, 2>{ 0xAA, 0xAA });
  };
}
}  // namespace

int main()
{
  memory_copier_test();
}
