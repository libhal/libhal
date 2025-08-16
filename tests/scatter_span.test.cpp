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
// limitations under the License.

#include <cstring>

#include <array>
#include <type_traits>
#include <vector>

#include <libhal/scatter_span.hpp>

#include <boost/ut.hpp>

namespace hal::v5 {
namespace {
struct multi_buffer
{
  std::array<int, 5> arr = { 1, 2, 3, 4, 5 };
  std::vector<int> vec = { 10, 20, 30, 40 };
};
}  // namespace

boost::ut::suite scatter_api_test = []() {
  using namespace boost::ut;

  "make_scatter_array - generic"_test = [] {
    multi_buffer buf;
    constexpr auto expected_length = 2;
    auto arr = make_scatter_array<int const>(buf.arr, buf.vec);

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

  "make_scatter_bytes - const bytes"_test = [] {
    std::array<hal::byte, 4> a{ 0x1, 0x2, 0x3, 0x4 };
    std::vector<hal::byte> v{ 0x5, 0x6 };
    constexpr static std::array<hal::byte, 3> static_arr{ 0xA, 0xB, 0xC };

    auto arr = make_scatter_bytes(a, v, static_arr);
    auto span = scatter_span<hal::byte const>(arr);

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

  "make_writable_scatter_bytes - mutable bytes"_test = [] {
    std::array<hal::byte, 3> a{ 0x7, 0x8, 0x9 };
    std::vector<hal::byte> v{ 0xA, 0xB };

    auto arr = make_writable_scatter_bytes(a, v);
    auto span = scatter_span<hal::byte>(arr);

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

  "compile time testing of spanable concepts"_test = [] {
    struct test_struct
    {};

    static_assert(spanable<std::span<hal::byte>>);
    static_assert(spanable<std::span<hal::byte const>>);
    static_assert(spanable<std::vector<hal::byte>>);
    static_assert(spanable<std::array<hal::byte, 10>>);
    static_assert(spanable<std::span<test_struct>>);
    static_assert(spanable<std::vector<int>>);
    static_assert(spanable<std::array<float, 10>>);
    static_assert(not spanable<test_struct>);
    static_assert(not spanable<int>);
    static_assert(not spanable<float>);
    static_assert(not spanable<test_struct>);

    static_assert(spanable_bytes<std::span<hal::byte>>);
    static_assert(spanable_bytes<std::span<hal::byte const>>);
    static_assert(spanable_bytes<std::vector<hal::byte>>);
    static_assert(spanable_bytes<std::array<hal::byte, 10>>);
    static_assert(not spanable_bytes<std::span<test_struct>>);
    static_assert(not spanable_bytes<std::vector<int>>);
    static_assert(not spanable_bytes<std::array<float, 10>>);
    static_assert(not spanable_bytes<test_struct>);
    static_assert(not spanable_bytes<int>);
    static_assert(not spanable_bytes<float>);
    static_assert(not spanable_bytes<test_struct>);

    static_assert(spanable_writable_bytes<std::span<hal::byte>>);
    static_assert(not spanable_writable_bytes<std::span<hal::byte const>>);
    static_assert(spanable_writable_bytes<std::vector<hal::byte>>);
    static_assert(spanable_writable_bytes<std::array<hal::byte, 10>>);
    static_assert(not spanable_writable_bytes<std::span<test_struct>>);
    static_assert(not spanable_writable_bytes<std::vector<int>>);
    static_assert(not spanable_writable_bytes<std::array<float, 10>>);
    static_assert(not spanable_writable_bytes<test_struct>);
    static_assert(not spanable_writable_bytes<int>);
    static_assert(not spanable_writable_bytes<float>);
    static_assert(not spanable_writable_bytes<test_struct>);
  };
};
}  // namespace hal::v5
