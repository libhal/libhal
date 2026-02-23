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

#include <array>

#include <libhal/error.hpp>
#include <libhal/scatter_span.hpp>
#include <libhal/usb.hpp>

#include <boost/ut.hpp>

namespace hal::usb {

namespace {

class mock_usb_endpoint : public usb::endpoint
{
public:
  usb::endpoint_info m_info{};
  bool m_stall_called{ false };
  bool m_should_stall{ false };
  bool m_reset_called{ false };

protected:
  [[nodiscard]] usb::endpoint_info driver_info() const override
  {
    return m_info;
  }

  void driver_stall(bool p_should_stall) override
  {
    m_stall_called = true;
    m_should_stall = p_should_stall;
  }

  void driver_reset() override
  {
    m_reset_called = true;
  }
};

// Mock implementation for usb_control_endpoint
class mock_usb_control_endpoint : public control_endpoint
{
public:
  mock_usb_endpoint m_endpoint;
  bool m_should_connect{ false };
  u8 m_address{ 0 };
  scatter_span<byte const> m_write_data{};
  scatter_span<byte> m_read_buffer{};
  usize m_read_result{ 0 };
  callback<void(on_receive_tag)> m_receive_callback{};
  bool m_on_receive_called{ false };

private:
  [[nodiscard]] endpoint_info driver_info() const override
  {
    return m_endpoint.info();
  }

  void driver_stall(bool p_should_stall) override
  {
    m_endpoint.stall(p_should_stall);
  }

  void driver_connect(bool p_should_connect) override
  {
    m_should_connect = p_should_connect;
  }

  void driver_set_address(u8 p_address) override
  {
    m_address = p_address;
  }

  void driver_write(scatter_span<byte const> p_data) override
  {
    m_write_data = p_data;
  }

  usize driver_read(scatter_span<byte> p_buffer) override
  {
    m_read_buffer = p_buffer;
    return m_read_result;
  }

  void driver_on_receive(
    callback<void(on_receive_tag)> const& p_callback) override
  {
    m_receive_callback = p_callback;
    m_on_receive_called = true;
  }

  void driver_reset() override
  {
    m_endpoint.reset();
  }
};

// Mock implementation for usb_in_endpoint
class mock_usb_in_endpoint : public in_endpoint
{
public:
  mock_usb_endpoint m_endpoint;
  scatter_span<byte const> m_write_data{};

private:
  [[nodiscard]] endpoint_info driver_info() const override
  {
    return m_endpoint.info();
  }

  void driver_stall(bool p_should_stall) override
  {
    m_endpoint.stall(p_should_stall);
  }
  void driver_write(scatter_span<byte const> p_data) override
  {
    m_write_data = p_data;
  }

  void driver_reset() override
  {
    m_endpoint.reset();
  }
};

// Mock implementation for usb_out_endpoint
class mock_usb_out_endpoint : public out_endpoint
{
public:
  mock_usb_endpoint m_endpoint;
  callback<void(on_receive_tag)> m_receive_callback{};
  bool m_on_receive_called{ false };
  scatter_span<byte> m_read_buffer{};
  usize m_read_result{ 0 };

private:
  [[nodiscard]] endpoint_info driver_info() const override
  {
    return m_endpoint.info();
  }

  void driver_stall(bool p_should_stall) override
  {
    m_endpoint.stall(p_should_stall);
  }

  void driver_on_receive(
    callback<void(on_receive_tag)> const& p_callback) override
  {
    m_receive_callback = p_callback;
    m_on_receive_called = true;
  }

  usize driver_read(scatter_span<byte> p_buffer) override
  {
    m_read_buffer = p_buffer;
    return m_read_result;
  }

  void driver_reset() override
  {
    m_endpoint.reset();
  }
};

struct mock_usb_interrupt_in_endpoint : public mock_usb_in_endpoint
{};

struct mock_usb_bulk_in_endpoint : public mock_usb_in_endpoint
{};

struct mock_usb_interrupt_out_endpoint : public mock_usb_out_endpoint
{};

struct mock_usb_bulk_out_endpoint : public mock_usb_out_endpoint
{};
}  // namespace
// Test for usb_endpoint_info methods
boost::ut::suite<"usb_endpoint_info_test"> endpoint_info_test = []() {
  using namespace boost::ut;

  "endpoint info in_direction test"_test = []() {
    endpoint_info info{};

    // Test IN endpoint (bit 7 set)
    info.number = 0x81;  // Endpoint 1, IN
    expect(that % true == info.in_direction());

    // Test OUT endpoint (bit 7 clear)
    info.number = 0x02;  // Endpoint 2, OUT
    expect(that % false == info.in_direction());
  };

  "endpoint info logical_number test"_test = []() {
    endpoint_info info{};

    // Test with various endpoint numbers
    info.number = 0x81;  // Endpoint 1, IN
    expect(that % 1 == info.logical_number());

    info.number = 0x05;  // Endpoint 5, OUT
    expect(that % 5 == info.logical_number());

    info.number = 0x8F;  // Endpoint 15, IN
    expect(that % 15 == info.logical_number());
  };
};

// Test for endpoint interface
boost::ut::suite<"endpoint_test"> endpoint_test = []() {
  using namespace boost::ut;

  "endpoint info test"_test = []() {
    mock_usb_endpoint endpoint;
    endpoint.m_info.size = 64;
    endpoint.m_info.number = 0x81;
    endpoint.m_info.stalled = true;

    auto info = endpoint.info();
    expect(that % 64 == info.size);
    expect(that % 0x81 == info.number);
    expect(that % true == info.stalled);
    expect(that % true == info.in_direction());
    expect(that % 1 == info.logical_number());
  };

  "usb_endpoint stall test"_test = []() {
    mock_usb_endpoint endpoint;

    endpoint.stall(true);
    expect(that % true == endpoint.m_stall_called);
    expect(that % true == endpoint.m_should_stall);

    endpoint.stall(false);
    expect(that % true == endpoint.m_stall_called);
    expect(that % false == endpoint.m_should_stall);
  };

  "usb_endpoint reset test"_test = []() {
    mock_usb_endpoint endpoint;

    expect(that % not endpoint.m_reset_called);
    endpoint.reset();
    expect(that % endpoint.m_reset_called);
  };
};

// Test for usb_control_endpoint interface
boost::ut::suite<"usb_control_endpoint_test"> control_endpoint_test = []() {
  using namespace boost::ut;

  "usb_control_endpoint connect test"_test = []() {
    mock_usb_control_endpoint endpoint;

    endpoint.connect(true);
    expect(that % true == endpoint.m_should_connect);

    endpoint.connect(false);
    expect(that % false == endpoint.m_should_connect);
  };

  "usb_control_endpoint set_address test"_test = []() {
    mock_usb_control_endpoint endpoint;

    endpoint.set_address(5);
    expect(that % 5 == endpoint.m_address);
  };

  "usb_control_endpoint write test"_test = []() {
    mock_usb_control_endpoint endpoint;

    std::array<byte, 3> data1 = { 1, 2, 3 };
    std::array<byte, 2> data2 = { 4, 5 };

    auto const scatter_span = make_scatter_bytes(data1, data2);
    endpoint.write(scatter_span);

    expect(that % data1.data() == endpoint.m_write_data[0].data());
    expect(that % data1.size() == endpoint.m_write_data[0].size());

    expect(that % data2.data() == endpoint.m_write_data[1].data());
    expect(that % data2.size() == endpoint.m_write_data[1].size());
  };

  "usb_control_endpoint read test"_test = []() {
    mock_usb_control_endpoint endpoint;

    std::array<byte, 3> buffer1 = { 0 };
    std::array<byte, 2> buffer2 = { 0 };

    // Setup: Set the response length
    endpoint.m_read_result = 5;

    // Exercise
    auto const scatter_array = make_writable_scatter_bytes(buffer1, buffer2);
    auto result = endpoint.read(scatter_array);

    // Verify
    expect(that % 5 == result);
    expect(that % buffer1.data() == endpoint.m_read_buffer[0].data());
    expect(that % buffer1.size() == endpoint.m_read_buffer[0].size());

    expect(that % buffer2.data() == endpoint.m_read_buffer[1].data());
    expect(that % buffer2.size() == endpoint.m_read_buffer[1].size());
  };

  "usb_control_endpoint on_receive test"_test = []() {
    mock_usb_control_endpoint endpoint;
    bool callback_called = false;

    auto callback = [&callback_called](control_endpoint::on_receive_tag) {
      callback_called = true;
    };

    endpoint.on_receive(callback);
    expect(that % true == endpoint.m_on_receive_called);

    // Manually call the stored callback to verify it works
    endpoint.m_receive_callback(control_endpoint::on_receive_tag{});
    expect(that % true == callback_called);
  };

  "usb_control_endpoint info test"_test = []() {
    mock_usb_control_endpoint endpoint;
    endpoint.m_endpoint.m_info.size = 64;
    endpoint.m_endpoint.m_info.number = 0x81;
    endpoint.m_endpoint.m_info.stalled = true;

    auto info = endpoint.info();
    expect(that % 64 == info.size);
    expect(that % 0x81 == info.number);
    expect(that % true == info.stalled);
    expect(that % true == info.in_direction());
    expect(that % 1 == info.logical_number());
  };

  "usb_control_endpoint stall test"_test = []() {
    mock_usb_control_endpoint endpoint;

    endpoint.stall(true);
    expect(that % true == endpoint.m_endpoint.m_stall_called);
    expect(that % true == endpoint.m_endpoint.m_should_stall);

    endpoint.stall(false);
    expect(that % true == endpoint.m_endpoint.m_stall_called);
    expect(that % false == endpoint.m_endpoint.m_should_stall);
  };

  "mock_usb_control_endpoint reset test"_test = []() {
    mock_usb_control_endpoint endpoint;

    expect(that % not endpoint.m_endpoint.m_reset_called);
    endpoint.reset();
    expect(that % endpoint.m_endpoint.m_reset_called);
  };
};

// Test for usb_in_endpoint interface
boost::ut::suite<"usb_in_endpoint_test"> in_endpoint_test = []() {
  using namespace boost::ut;

  "usb_in_endpoint write test"_test = []() {
    mock_usb_in_endpoint endpoint;
    std::array<byte, 3> data0 = { 1, 2, 3 };
    std::array<byte, 2> data1 = { 4, 5 };

    auto const array_of_byte_spans = make_scatter_bytes(data0, data1);

    endpoint.write(array_of_byte_spans);
    expect(that % array_of_byte_spans.data() == endpoint.m_write_data.data());
    expect(that % array_of_byte_spans.size() == endpoint.m_write_data.size());

    expect(that % data0.data() == endpoint.m_write_data[0].data());
    expect(that % data0.size() == endpoint.m_write_data[0].size());

    expect(that % data1.data() == endpoint.m_write_data[1].data());
    expect(that % data1.size() == endpoint.m_write_data[1].size());
  };

  "mock_usb_in_endpoint reset test"_test = []() {
    mock_usb_in_endpoint endpoint;

    expect(that % not endpoint.m_endpoint.m_reset_called);
    endpoint.reset();
    expect(that % endpoint.m_endpoint.m_reset_called);
  };
};

// Test for usb_out_endpoint interface
boost::ut::suite<"usb_out_endpoint_test"> out_endpoint_test = []() {
  using namespace boost::ut;

  "usb_out_endpoint on_receive test"_test = []() {
    mock_usb_out_endpoint endpoint;
    bool callback_called = false;

    auto callback = [&callback_called](out_endpoint::on_receive_tag) {
      callback_called = true;
    };

    endpoint.on_receive(callback);
    expect(that % true == endpoint.m_on_receive_called);

    // Manually call the stored callback to verify it works
    endpoint.m_receive_callback(out_endpoint::on_receive_tag{});
    expect(that % true == callback_called);
  };

  "usb_out_endpoint read test"_test = []() {
    mock_usb_out_endpoint endpoint;
    std::array<byte, 3> buffer1 = { 0 };
    std::array<byte, 8> buffer2 = { 0 };

    // Setup: Set the response length
    endpoint.m_read_result = 7;

    // Exercise
    auto const scatter_array = make_writable_scatter_bytes(buffer1, buffer2);
    auto result = endpoint.read(scatter_array);

    // Verify
    expect(that % 7 == result);
    expect(that % buffer1.data() == endpoint.m_read_buffer[0].data());
    expect(that % buffer1.size() == endpoint.m_read_buffer[0].size());

    expect(that % buffer2.data() == endpoint.m_read_buffer[1].data());
    expect(that % buffer2.size() == endpoint.m_read_buffer[1].size());
  };

  "usb_out_endpoint reset test"_test = []() {
    mock_usb_out_endpoint endpoint;

    expect(that % not endpoint.m_endpoint.m_reset_called);
    endpoint.reset();
    expect(that % endpoint.m_endpoint.m_reset_called);
  };
};

// Test for specific endpoint types
boost::ut::suite<"usb_specific_endpoint_test"> specific_endpoint_test = []() {
  using namespace boost::ut;

  "usb_interrupt_in_endpoint test"_test = []() {
    // Test that it inherits from usb_in_endpoint
    static_assert(std::is_base_of_v<in_endpoint, interrupt_in_endpoint>);
    mock_usb_interrupt_in_endpoint endpoint;

    endpoint.m_endpoint.m_info.size = 64;
    endpoint.m_endpoint.m_info.number = 0x81;
    endpoint.m_endpoint.m_info.stalled = true;

    auto info = endpoint.info();
    expect(that % 64 == info.size);
    expect(that % 0x81 == info.number);
    expect(that % true == info.stalled);
    expect(that % true == info.in_direction());
    expect(that % 1 == info.logical_number());

    endpoint.stall(true);
    expect(that % true == endpoint.m_endpoint.m_stall_called);
    expect(that % true == endpoint.m_endpoint.m_should_stall);

    endpoint.stall(false);
    expect(that % true == endpoint.m_endpoint.m_stall_called);
    expect(that % false == endpoint.m_endpoint.m_should_stall);

    expect(that % not endpoint.m_endpoint.m_reset_called);
    endpoint.reset();
    expect(that % endpoint.m_endpoint.m_reset_called);

    std::array<byte, 3> data0 = { 1, 2, 3 };
    std::array<byte, 2> data1 = { 4, 5 };

    auto const array_of_byte_spans = make_scatter_bytes(data0, data1);

    endpoint.write(array_of_byte_spans);
    expect(that % array_of_byte_spans.data() == endpoint.m_write_data.data());
    expect(that % array_of_byte_spans.size() == endpoint.m_write_data.size());

    expect(that % data0.data() == endpoint.m_write_data[0].data());
    expect(that % data0.size() == endpoint.m_write_data[0].size());

    expect(that % data1.data() == endpoint.m_write_data[1].data());
    expect(that % data1.size() == endpoint.m_write_data[1].size());
  };

  "usb_interrupt_out_endpoint test"_test = []() {
    // Test that it inherits from usb_out_endpoint
    static_assert(std::is_base_of_v<out_endpoint, interrupt_out_endpoint>);

    mock_usb_interrupt_out_endpoint endpoint;

    endpoint.m_endpoint.m_info.size = 32;
    endpoint.m_endpoint.m_info.number = 0x02;
    endpoint.m_endpoint.m_info.stalled = true;

    auto info = endpoint.info();
    expect(that % 32 == info.size);
    expect(that % 0x02 == info.number);
    expect(that % true == info.stalled);
    expect(that % false == info.in_direction());
    expect(that % int{ 2 } == int{ info.logical_number() });

    endpoint.stall(true);
    expect(that % true == endpoint.m_endpoint.m_stall_called);
    expect(that % true == endpoint.m_endpoint.m_should_stall);

    endpoint.stall(false);
    expect(that % true == endpoint.m_endpoint.m_stall_called);
    expect(that % false == endpoint.m_endpoint.m_should_stall);

    expect(that % not endpoint.m_endpoint.m_reset_called);
    endpoint.reset();
    expect(that % endpoint.m_endpoint.m_reset_called);

    bool callback_called = false;

    auto callback = [&callback_called](out_endpoint::on_receive_tag) {
      callback_called = true;
    };

    endpoint.on_receive(callback);
    expect(that % true == endpoint.m_on_receive_called);

    // Manually call the stored callback to verify it works
    endpoint.m_receive_callback(out_endpoint::on_receive_tag{});
    expect(that % true == callback_called);

    std::array<byte, 3> buffer1 = { 0 };
    std::array<byte, 8> buffer2 = { 0 };

    // Setup: Set the response length
    endpoint.m_read_result = 7;

    // Exercise
    auto const scatter_array = make_writable_scatter_bytes(buffer1, buffer2);
    auto result = endpoint.read(scatter_array);

    // Verify
    expect(that % 7 == result);
    expect(that % buffer1.data() == endpoint.m_read_buffer[0].data());
    expect(that % buffer1.size() == endpoint.m_read_buffer[0].size());

    expect(that % buffer2.data() == endpoint.m_read_buffer[1].data());
    expect(that % buffer2.size() == endpoint.m_read_buffer[1].size());
  };

  "usb_bulk_in_endpoint test"_test = []() {
    // Test that it inherits from usb_in_endpoint
    static_assert(std::is_base_of_v<in_endpoint, bulk_in_endpoint>);

    mock_usb_bulk_in_endpoint endpoint;

    endpoint.m_endpoint.m_info.size = 64;
    endpoint.m_endpoint.m_info.number = 0x81;
    endpoint.m_endpoint.m_info.stalled = true;

    auto info = endpoint.info();
    expect(that % 64 == info.size);
    expect(that % 0x81 == info.number);
    expect(that % true == info.stalled);
    expect(that % true == info.in_direction());
    expect(that % 1 == info.logical_number());

    endpoint.stall(true);
    expect(that % true == endpoint.m_endpoint.m_stall_called);
    expect(that % true == endpoint.m_endpoint.m_should_stall);

    endpoint.stall(false);
    expect(that % true == endpoint.m_endpoint.m_stall_called);
    expect(that % false == endpoint.m_endpoint.m_should_stall);

    expect(that % not endpoint.m_endpoint.m_reset_called);
    endpoint.reset();
    expect(that % endpoint.m_endpoint.m_reset_called);

    std::array<byte, 3> data0 = { 1, 2, 3 };
    std::array<byte, 2> data1 = { 4, 5 };

    auto const array_of_byte_spans = make_scatter_bytes(data0, data1);

    endpoint.write(array_of_byte_spans);
    expect(that % array_of_byte_spans.data() == endpoint.m_write_data.data());
    expect(that % array_of_byte_spans.size() == endpoint.m_write_data.size());

    expect(that % data0.data() == endpoint.m_write_data[0].data());
    expect(that % data0.size() == endpoint.m_write_data[0].size());

    expect(that % data1.data() == endpoint.m_write_data[1].data());
    expect(that % data1.size() == endpoint.m_write_data[1].size());
  };

  "usb_bulk_out_endpoint test"_test = []() {
    // Test that it inherits from usb_out_endpoint
    static_assert(std::is_base_of_v<out_endpoint, bulk_out_endpoint>);

    mock_usb_bulk_out_endpoint endpoint;

    endpoint.m_endpoint.m_info.size = 64;
    endpoint.m_endpoint.m_info.number = 0x81;
    endpoint.m_endpoint.m_info.stalled = true;

    auto info = endpoint.info();
    expect(that % 64 == info.size);
    expect(that % 0x81 == info.number);
    expect(that % true == info.stalled);
    expect(that % true == info.in_direction());
    expect(that % 1 == info.logical_number());

    endpoint.stall(true);
    expect(that % true == endpoint.m_endpoint.m_stall_called);
    expect(that % true == endpoint.m_endpoint.m_should_stall);

    endpoint.stall(false);
    expect(that % true == endpoint.m_endpoint.m_stall_called);
    expect(that % false == endpoint.m_endpoint.m_should_stall);

    expect(that % not endpoint.m_endpoint.m_reset_called);
    endpoint.reset();
    expect(that % endpoint.m_endpoint.m_reset_called);

    bool callback_called = false;

    auto callback = [&callback_called](out_endpoint::on_receive_tag) {
      callback_called = true;
    };

    endpoint.on_receive(callback);
    expect(that % true == endpoint.m_on_receive_called);

    // Manually call the stored callback to verify it works
    endpoint.m_receive_callback(out_endpoint::on_receive_tag{});
    expect(that % true == callback_called);

    std::array<byte, 3> buffer1 = { 0 };
    std::array<byte, 8> buffer2 = { 0 };

    // Setup: Set the response length
    endpoint.m_read_result = 7;

    // Exercise
    auto const scatter_array = make_writable_scatter_bytes(buffer1, buffer2);
    auto result = endpoint.read(scatter_array);

    // Verify
    expect(that % 7 == result);
    expect(that % buffer1.data() == endpoint.m_read_buffer[0].data());
    expect(that % buffer1.size() == endpoint.m_read_buffer[0].size());

    expect(that % buffer2.data() == endpoint.m_read_buffer[1].data());
    expect(that % buffer2.size() == endpoint.m_read_buffer[1].size());
  };
};
}  // namespace hal::usb

namespace hal::usb {

namespace {

template<typename T>
size_t scatter_span_size(scatter_span<T> ss)
{
  size_t res = 0;
  for (auto const& s : ss) {
    res += s.size();
  }

  return res;
}

struct mock_endpoint_io : public endpoint_io
{
  usize driver_read(scatter_span<byte> p_buffer) override
  {
    read_called = true;
    return scatter_span_size(p_buffer);
  }

  usize driver_write(scatter_span<byte const> p_buffer) override
  {
    write_called = true;
    return scatter_span_size(p_buffer);
  }

  bool read_called = false;
  bool write_called = false;
};

constexpr u8 iface_desc_length = 9;
constexpr u8 iface_desc_type = 0x4;

struct mock : public interface
{
  constexpr static std::array<u8, 9> expected_descriptor = {
    iface_desc_length,
    iface_desc_type,
    0,  // interface_number
    0,  // alternate_setting
    1,  //  num_endpoints
    2,  //  interface_class
    3,  // interface_sub_class
    4,  // interface_protocol
    1   // i_interface
  };

  [[nodiscard]] descriptor_count driver_write_descriptors(
    descriptor_start p_start,
    endpoint_io& p_ep_req) override
  {
    write_descriptors_start = p_start;
    auto payload = make_scatter_bytes(expected_descriptor);
    p_ep_req.write(payload);

    return { .interface = 1, .string = 1 };
  }

  [[nodiscard]] bool driver_write_string_descriptor(
    u8 p_index,
    endpoint_io& p_ep_req) override
  {
    write_string_descriptor_string_index = p_index;
    auto data = std::to_array<u8>({ 0, 0x01 });
    auto payload = make_scatter_bytes(data);
    auto bytes_written = p_ep_req.write(payload);
    return bytes_written == data.size();
  }

  bool driver_handle_request(setup_packet const& p_setup,
                             endpoint_io& p_ep_req) override
  {
    handle_request_setup = p_setup;
    auto data = std::to_array<u8>({ 0xAA, 0xBB });
    auto payload = make_writable_scatter_bytes(data);
    auto bytes_read = p_ep_req.read(payload);

    return bytes_read == data.size();
  }

  descriptor_start write_descriptors_start{};
  setup_packet handle_request_setup{};
  u8 write_string_descriptor_string_index{};
};
}  // namespace

// Test the usb highlevel interface (interface descriptor level)
boost::ut::suite<"usb_iterface_req_bitmap_test"> req_bitmap_test = []() {
  using namespace boost::ut;

  "req bitmap construction from byte test"_test = []() {
    u8 raw_byte = 0b10000001;
    std::array<byte, 8> pkt;
    pkt[0] = raw_byte;
    setup_packet bm(pkt);

    // Test recipient
    expect(setup_packet::request_recipient::interface == bm.get_recipient());

    // Test type
    expect(setup_packet::request_type::standard == bm.get_type());

    // Test direction
    expect(that % true == bm.is_device_to_host());
  };
};

// Test the usb highlevel interface (interface descriptor level)
boost::ut::suite<"usb_interface_test"> usb_interface_test = []() {
  using namespace boost::ut;
  using descriptor_count = interface::descriptor_count;

  "interface::write_descriptor"_test = []() {
    mock iface;
    mock_endpoint_io eio;

    auto delta = iface.write_descriptors({ .interface = 0, .string = 1 }, eio);

    expect(descriptor_count{ 1, 1 } == delta);
    expect(that % eio.write_called);
  };

  "interface::write_string_descriptor"_test = []() mutable {
    mock iface;
    mock_endpoint_io eio;
    auto success = iface.write_string_descriptor(1, eio);

    expect(that % 1 == iface.write_string_descriptor_string_index);
    expect(that % eio.write_called);
    expect(that % success);
  };

  "interface::handle_request"_test = []() mutable {
    mock iface;
    mock_endpoint_io eio;
    std::array<byte, 8> command_bytes{ 0x80, 0x01, 0x03, 0x02,
                                       0x05, 0x04, 0x07, 0x06 };

    setup_packet command{ command_bytes };

    auto actual_res = iface.handle_request(command, eio);

    expect(that % actual_res);
    expect(that % eio.read_called);
    expect(command == iface.handle_request_setup);
  };
};
}  // namespace hal::usb
