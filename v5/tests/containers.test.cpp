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

#include <array>
#include <span>
#include <vector>

#include <boost/ut.hpp>

import hal;

namespace {

void circular_span_construction_test()
{
  using namespace boost::ut;

  "circular_span constructs from std::span"_test = [] {
    std::array<int, 4> buf{ 1, 2, 3, 4 };
    std::span<int> s(buf);
    hal::circular_span<int> cs(s);
    expect(that % 4 == cs.size());
  };

  "circular_span constructs from std::array directly"_test = [] {
    std::array<int, 3> buf{ 10, 20, 30 };
    hal::circular_span<int> cs(buf);
    expect(that % 3 == cs.size());
  };

  "circular_span<const T> constructs from std::array<T>"_test = [] {
    std::array<hal::byte, 4> buf{
      hal::byte{ 0x01 }, hal::byte{ 0x02 }, hal::byte{ 0x03 }, hal::byte{ 0x04 }
    };
    hal::circular_span<hal::byte const> cs(buf);
    expect(that % 4 == cs.size());
  };

  "circular_span constructs from std::vector"_test = [] {
    std::vector<int> v{ 5, 6, 7 };
    hal::circular_span<int> cs(v);
    expect(that % 3 == cs.size());
  };
}

void circular_span_access_test()
{
  using namespace boost::ut;

  "operator[] returns correct element within bounds"_test = [] {
    std::array<int, 4> buf{ 10, 20, 30, 40 };
    hal::circular_span<int> cs(buf);

    expect(that % 10 == cs[0]);
    expect(that % 20 == cs[1]);
    expect(that % 30 == cs[2]);
    expect(that % 40 == cs[3]);
  };

  "operator[] wraps at exactly size"_test = [] {
    std::array<int, 4> buf{ 10, 20, 30, 40 };
    hal::circular_span<int> cs(buf);

    expect(that % 10 == cs[4]);
    expect(that % 20 == cs[5]);
    expect(that % 30 == cs[6]);
    expect(that % 40 == cs[7]);
  };

  "operator[] wraps multiple times"_test = [] {
    std::array<int, 3> buf{ 1, 2, 3 };
    hal::circular_span<int> cs(buf);

    expect(that % 1 == cs[0]);
    expect(that % 1 == cs[3]);
    expect(that % 1 == cs[6]);
    expect(that % 2 == cs[7]);
  };

  "operator[] allows mutation through non-const circular_span"_test = [] {
    std::array<int, 4> buf{ 1, 2, 3, 4 };
    hal::circular_span<int> cs(buf);

    cs[0] = 99;
    expect(that % 99 == buf[0]);
    expect(that % 99 == cs[4]);
  };

  "operator[] on const element type is read-only"_test = [] {
    std::array<int, 2> buf{ 7, 8 };
    hal::circular_span<int const> cs(buf);

    expect(that % 7 == cs[0]);
    expect(that % 8 == cs[1]);
    expect(that % 7 == cs[2]);
  };
}

void circular_span_size_test()
{
  using namespace boost::ut;

  "size() returns underlying span size"_test = [] {
    std::array<int, 5> buf{};
    hal::circular_span<int> cs(buf);
    expect(that % 5 == cs.size());
  };

  "size() returns 1 for single-element span"_test = [] {
    std::array<int, 1> buf{ 42 };
    hal::circular_span<int> cs(buf);
    expect(that % 1 == cs.size());
    expect(that % 42 == cs[0]);
    expect(that % 42 == cs[1]);
  };
}

void circular_span_span_accessor_test()
{
  using namespace boost::ut;

  "span() returns the underlying span"_test = [] {
    std::array<int, 3> buf{ 11, 22, 33 };
    hal::circular_span<int> cs(buf);
    auto s = cs.span();

    expect(that % static_cast<void*>(buf.data()) ==
           static_cast<void*>(s.data()));
    expect(that % static_cast<void*>(buf.data()) ==
           static_cast<void*>(s.data()));
    expect(that % 3 == s.size());
  };
}

void span_constructible_concept_test()
{
  using namespace boost::ut;

  "span_constructible concept compile-time checks"_test = [] {
    static_assert(hal::span_constructible<std::array<int, 4>&, int>);
    static_assert(hal::span_constructible<std::vector<int>&, int>);
    static_assert(hal::span_constructible<std::span<int>, int>);
    static_assert(
      hal::span_constructible<std::array<hal::byte, 4>, hal::byte const>);
    static_assert(not hal::span_constructible<int, int>);
    static_assert(not hal::span_constructible<float, int>);
  };
}

}  // namespace

int main()
{
  circular_span_construction_test();
  circular_span_access_test();
  circular_span_size_test();
  circular_span_span_accessor_test();
  span_constructible_concept_test();
}
