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
#include <type_traits>
#include <vector>

#include <boost/ut.hpp>

import hal;

namespace {

struct multi_buffer
{
  std::array<int, 5> arr = { 1, 2, 3, 4, 5 };
  std::vector<int> vec = { 10, 20, 30, 40 };
};

void make_scatter_array_test()
{
  using namespace boost::ut;

  "make_scatter_array - generic"_test = [] {
    multi_buffer buf;
    constexpr auto expected_length = 2;
    auto arr = hal::make_scatter_array<int const>(buf.arr, buf.vec);

    static_assert(
      std::is_same_v<decltype(arr),
                     std::array<std::span<int const>, expected_length>>,
      "make_scatter_array must return std::array<std::span<T const>, "
      "expected_length>");

    expect(that % expected_length == arr.size());
    expect(that % buf.arr.size() == arr[0].size());
    expect(that % buf.vec.size() == arr[1].size());
    expect(that % arr[0].data() == buf.arr.data());
    expect(that % arr[1].data() == buf.vec.data());
  };
}

void make_scatter_bytes_test()
{
  using namespace boost::ut;

  "make_scatter_bytes - const bytes"_test = [] {
    std::array<hal::byte, 4> a{ 0x1, 0x2, 0x3, 0x4 };
    std::vector<hal::byte> v{ 0x5, 0x6 };
    constexpr static std::array<hal::byte, 3> static_arr{ 0xA, 0xB, 0xC };

    auto arr = hal::make_scatter_bytes(a, v, static_arr);
    hal::scatter_span<hal::byte const> span(arr);

    static_assert(
      std::is_same_v<decltype(arr), std::array<std::span<hal::byte const>, 3>>,
      "make_scatter_bytes must return array of const byte spans");

    expect(that % 0x1 == arr[0][0]);
    expect(that % 0x4 == arr[0][3]);
    expect(that % 0x5 == arr[1][0]);
    expect(that % 0x6 == arr[1][1]);
    expect(that % 0xA == arr[2][0]);
    expect(that % 0xB == arr[2][1]);
    expect(that % 0xC == arr[2][2]);

    expect(that % 0x1 == span[0][0]);
    expect(that % 0x4 == span[0][3]);
    expect(that % 0x5 == span[1][0]);
    expect(that % 0x6 == span[1][1]);
    expect(that % 0xA == span[2][0]);
    expect(that % 0xB == span[2][1]);
    expect(that % 0xC == span[2][2]);

    using element_type = std::remove_reference_t<decltype(arr[0][0])>;
    static_assert(std::is_const_v<element_type>, "Element should be non-const");
  };
}

void make_writable_scatter_bytes_test()
{
  using namespace boost::ut;

  "make_writable_scatter_bytes - mutable bytes"_test = [] {
    std::array<hal::byte, 3> a{ 0x7, 0x8, 0x9 };
    std::vector<hal::byte> v{ 0xA, 0xB };

    auto arr = hal::make_writable_scatter_bytes(a, v);
    hal::scatter_span<hal::byte> span(arr);

    static_assert(
      std::is_same_v<decltype(arr), std::array<std::span<hal::byte>, 2>>,
      "make_writable_scatter_bytes must return array of mutable byte spans");

    arr[0][0] = 0xC;

    expect(that % 0xC == arr[0][0]);
    expect(that % 0xC == a[0]);
    expect(that % 0xC == span[0][0]);

    static_assert(not std::is_const_v<decltype(arr[0][0])>,
                  "Element should be non-const");
  };
}

void spanable_concepts_test()
{
  using namespace boost::ut;

  "compile time testing of spanable concepts"_test = [] {
    struct test_struct
    {};

    static_assert(hal::spanable<std::span<hal::byte>>);
    static_assert(hal::spanable<std::span<hal::byte const>>);
    static_assert(hal::spanable<std::vector<hal::byte>>);
    static_assert(hal::spanable<std::array<hal::byte, 10>>);
    static_assert(hal::spanable<std::span<test_struct>>);
    static_assert(hal::spanable<std::vector<int>>);
    static_assert(hal::spanable<std::array<float, 10>>);
    static_assert(not hal::spanable<test_struct>);
    static_assert(not hal::spanable<int>);
    static_assert(not hal::spanable<float>);
    static_assert(not hal::spanable<test_struct>);

    static_assert(hal::spanable_bytes<std::span<hal::byte>>);
    static_assert(hal::spanable_bytes<std::span<hal::byte const>>);
    static_assert(hal::spanable_bytes<std::vector<hal::byte>>);
    static_assert(hal::spanable_bytes<std::array<hal::byte, 10>>);
    static_assert(not hal::spanable_bytes<std::span<test_struct>>);
    static_assert(not hal::spanable_bytes<std::vector<int>>);
    static_assert(not hal::spanable_bytes<std::array<float, 10>>);
    static_assert(not hal::spanable_bytes<test_struct>);
    static_assert(not hal::spanable_bytes<int>);
    static_assert(not hal::spanable_bytes<float>);
    static_assert(not hal::spanable_bytes<test_struct>);

    static_assert(hal::spanable_writable_bytes<std::span<hal::byte>>);
    static_assert(not hal::spanable_writable_bytes<std::span<hal::byte const>>);
    static_assert(hal::spanable_writable_bytes<std::vector<hal::byte>>);
    static_assert(hal::spanable_writable_bytes<std::array<hal::byte, 10>>);
    static_assert(not hal::spanable_writable_bytes<std::span<test_struct>>);
    static_assert(not hal::spanable_writable_bytes<std::vector<int>>);
    static_assert(not hal::spanable_writable_bytes<std::array<float, 10>>);
    static_assert(not hal::spanable_writable_bytes<test_struct>);
    static_assert(not hal::spanable_writable_bytes<int>);
    static_assert(not hal::spanable_writable_bytes<float>);
    static_assert(not hal::spanable_writable_bytes<test_struct>);
  };
}

void scatter_span_equality_test()
{
  using namespace boost::ut;

  "scatter_span operator=="_test = []() {
    std::array<hal::byte, 4> a{ 1, 2, 3, 4 };
    std::array<hal::byte, 4> b{ 1, 2, 3, 4 };
    std::span<hal::byte const> a_span(a.data(), a.size());
    std::span<hal::byte const> b_span(b.data(), b.size());

    auto scatter_array_a = hal::make_scatter_bytes(a_span);
    auto scatter_array_b = hal::make_scatter_bytes(b_span);

    hal::scatter_span<hal::byte const> sa(scatter_array_a);
    hal::scatter_span<hal::byte const> sb(scatter_array_b);
    // Test equal spans with the same size
    expect(that % hal::operator==(sa, sb));

    // Test different spans with the same size
    a[3] = 0;
    expect(that % hal::operator!=(sa, sb));

    auto c_span = hal::make_scatter_bytes(std::span{ b.begin(), 3 });
    hal::scatter_span<hal::byte const> sc(c_span);

    // Test different sized scatter spans to make sure they're not equal
    // expect(that % (sa != sc));

    // Test empty scatter spans
    auto empty = hal::make_scatter_bytes();
    expect(that %
           hal::operator!=(
             sa, static_cast<hal::scatter_span<hal::byte const>>(empty)));

    // Advanced tests
    std::array<hal::u8, 4> e{ 1, 2, 3, 4 };
    std::array<hal::u8, 3> f{ 1, 2, 3 };

    std::array<hal::u8, 5> g{ 1, 2, 3, 4, 1 };
    std::array<hal::u8, 2> h{ 2, 3 };

    auto ef_scatter_span = hal::make_scatter_array<hal::u8>(e, f);
    hal::scatter_span<hal::u8> se(ef_scatter_span);
    auto gh_scatter_span = hal::make_scatter_array<hal::u8>(g, h);
    hal::scatter_span<hal::u8> sg(gh_scatter_span);

    expect(that % hal::operator==(se, sg));
  };
}

}  // namespace

int main()
{
  make_scatter_array_test();
  make_scatter_bytes_test();
  make_writable_scatter_bytes_test();
  spanable_concepts_test();
  scatter_span_equality_test();
}
