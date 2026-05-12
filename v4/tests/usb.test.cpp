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
#include <optional>

#include <libhal/error.hpp>
#include <libhal/scatter_span.hpp>
#include <libhal/usb.hpp>

#include <boost/ut.hpp>

#if defined(__clang__)
#define SUPPRESS_DEPRECATED_START                                              \
  _Pragma("clang diagnostic push")                                             \
    _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#define SUPPRESS_DEPRECATED_END _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define SUPPRESS_DEPRECATED_START                                              \
  _Pragma("GCC diagnostic push")                                               \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define SUPPRESS_DEPRECATED_END _Pragma("GCC diagnostic pop")
#else
#define SUPPRESS_DEPRECATED_START
#define SUPPRESS_DEPRECATED_END
#endif

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
  using bus_event = v5::usb::bus_event;
  mock_usb_endpoint m_endpoint;
  bool m_should_connect{ false };
  u8 m_address{ 0 };
  scatter_span<byte const> m_write_data{};
  scatter_span<byte> m_read_buffer{};
  usize m_read_result{ 0 };
  callback<void(on_receive_tag)> m_receive_callback{};
  bool m_on_receive_called{ false };
  std::optional<bool> m_has_setup_state{};
  callback<void(bus_event)> m_host_event_callback{};
  bool m_on_bus_event_called{ false };
  bool m_remote_wakeup_enabled{ false };
  bool m_acknowledge_sleep_accept{ false };
  bool m_acknowledge_sleep_called{ false };
  v5::usb::lpm_support m_supports_lpm_result{};

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

  std::optional<bool> driver_has_setup() const noexcept override
  {
    return m_has_setup_state;
  }

  void driver_on_bus_event(
    hal::callback<void(bus_event)> const& p_callback) override
  {
    m_host_event_callback = p_callback;
    m_on_bus_event_called = true;
  }

  void driver_remote_wakeup_enable(bool p_enabled) override
  {
    m_remote_wakeup_enabled = p_enabled;
  }

  bool driver_remote_wakeup_granted() override
  {
    return m_remote_wakeup_enabled;
  }

  void driver_acknowledge_sleep(bool p_accept) override
  {
    m_acknowledge_sleep_accept = p_accept;
    m_acknowledge_sleep_called = true;
  }

  v5::usb::lpm_support driver_supports_lpm() override
  {
    return m_supports_lpm_result;
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

  SUPPRESS_DEPRECATED_START

  "mock_usb_control_endpoint has_setup test"_test = []() {
    mock_usb_control_endpoint endpoint;
    endpoint.m_has_setup_state = std::nullopt;
    auto const val1 = endpoint.has_setup();
    expect(std::nullopt == val1);

    endpoint.m_has_setup_state = true;
    auto const val2 = endpoint.has_setup();
    expect(that % true == *val2);

    endpoint.m_has_setup_state = false;
    auto const val3 = endpoint.has_setup();
    expect(that % false == *val3);
  };

  SUPPRESS_DEPRECATED_END

  "mock_usb_control_endpoint default has_setup test"_test = []() {
    struct test_ctrl_ep : public hal::usb::control_endpoint
    {
      void driver_connect(bool) override
      {
      }
      void driver_set_address(u8) override
      {
      }
      void driver_write(scatter_span<byte const>) override
      {
      }
      usize driver_read(scatter_span<byte>) override
      {
        return 0;
      }
      void driver_on_receive(callback<void(on_receive_tag)> const&) override
      {
      }
      [[nodiscard]] endpoint_info driver_info() const override
      {
        return {};
      }
      void driver_stall(bool) override
      {
      }
      void driver_reset() override
      {
      }
      // driver_has_setup skipped!
    } endpoint;

    SUPPRESS_DEPRECATED_START

    auto const val1 = endpoint.has_setup();

    SUPPRESS_DEPRECATED_END

    expect(std::nullopt == val1);
  };

  "mock_usb_control_endpoint on_bus_event test"_test = []() {
    using bus_event = hal::v5::usb::bus_event;
    mock_usb_control_endpoint endpoint;
    bus_event received_event{};

    auto cb = [&received_event](bus_event e) { received_event = e; };
    endpoint.on_bus_event(cb);

    expect(that % endpoint.m_on_bus_event_called);

    endpoint.m_host_event_callback(bus_event::reset);
    expect(bus_event::reset == received_event);

    endpoint.m_host_event_callback(bus_event::suspend);
    expect(bus_event::suspend == received_event);
  };

  "mock_usb_control_endpoint remote_wakeup_enabled test"_test = []() {
    mock_usb_control_endpoint endpoint;

    endpoint.remote_wakeup_enable(true);
    expect(that % true == endpoint.m_remote_wakeup_enabled);

    endpoint.remote_wakeup_enable(false);
    expect(that % false == endpoint.m_remote_wakeup_enabled);
  };

  "mock_usb_control_endpoint remote_wakeup_granted test"_test = []() {
    mock_usb_control_endpoint endpoint;

    endpoint.remote_wakeup_enable(true);
    expect(that % true == endpoint.remote_wakeup_granted());

    endpoint.remote_wakeup_enable(false);
    expect(that % false == endpoint.remote_wakeup_granted());
  };

  "control_endpoint default remote_wakeup_granted returns false"_test = []() {
    struct test_ctrl_ep : public hal::usb::control_endpoint
    {
      void driver_connect(bool) override
      {
      }
      void driver_set_address(u8) override
      {
      }
      void driver_write(scatter_span<byte const>) override
      {
      }
      usize driver_read(scatter_span<byte>) override
      {
        return 0;
      }
      void driver_on_receive(callback<void(on_receive_tag)> const&) override
      {
      }
      [[nodiscard]] endpoint_info driver_info() const override
      {
        return {};
      }
      void driver_stall(bool) override
      {
      }
      void driver_reset() override
      {
      }
    } endpoint;

    expect(that % false == endpoint.remote_wakeup_granted());
  };

  "mock_usb_control_endpoint acknowledge_sleep test"_test = []() {
    mock_usb_control_endpoint endpoint;

    endpoint.acknowledge_sleep(true);
    expect(that % endpoint.m_acknowledge_sleep_called);
    expect(that % true == endpoint.m_acknowledge_sleep_accept);

    endpoint.acknowledge_sleep(false);
    expect(that % false == endpoint.m_acknowledge_sleep_accept);
  };

  "mock_usb_control_endpoint supports_lpm test"_test = []() {
    mock_usb_control_endpoint endpoint;
    auto constexpr expected1 = hal::v5::usb::lpm_support()
                                 .remote_wakeup_supported(true)
                                 .l1_supported(true)
                                 .u1_supported(true);

    endpoint.m_supports_lpm_result = expected1;
    expect(expected1 == endpoint.supports_lpm());

    auto constexpr expected2 = hal::v5::usb::lpm_support()
                                 .l1_supported(true)
                                 .u1_supported(true)
                                 .u2_supported(true);
    endpoint.m_supports_lpm_result = expected2;
    expect(expected2 == endpoint.supports_lpm());
  };

  "control_endpoint default supports_lpm returns false"_test = []() {
    struct test_ctrl_ep : public hal::usb::control_endpoint
    {
      void driver_connect(bool) override
      {
      }
      void driver_set_address(u8) override
      {
      }
      void driver_write(scatter_span<byte const>) override
      {
      }
      usize driver_read(scatter_span<byte>) override
      {
        return 0;
      }
      void driver_on_receive(callback<void(on_receive_tag)> const&) override
      {
      }
      [[nodiscard]] endpoint_info driver_info() const override
      {
        return {};
      }
      void driver_stall(bool) override
      {
      }
      void driver_reset() override
      {
      }
    } endpoint;

    expect(v5::usb::lpm_support{} == endpoint.supports_lpm());
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

boost::ut::suite<"lpm_support_test"> lpm_support_test = []() {
  using namespace boost::ut;
  using hal::v5::usb::lpm_support;

  "lpm_support default is all zeros"_test = []() {
    lpm_support s{};
    expect(that % false == s.remote_wakeup_supported());
    expect(that % false == s.l1_supported());
    expect(that % false == s.u1_supported());
    expect(that % false == s.u2_supported());
    expect(that % s.none());
    expect(that % not s.any());
  };

  "lpm_support setters enable individual bits"_test = []() {
    lpm_support s{};

    s.remote_wakeup_supported(true);
    expect(that % s.remote_wakeup_supported());
    expect(that % not s.l1_supported());
    expect(that % not s.u1_supported());
    expect(that % not s.u2_supported());
    expect(that % s.any());
    expect(that % not s.none());

    s.remote_wakeup_supported(false);
    expect(that % not s.remote_wakeup_supported());

    s.l1_supported(true);
    expect(that % s.l1_supported());
    s.l1_supported(false);
    expect(that % not s.l1_supported());

    s.u1_supported(true);
    expect(that % s.u1_supported());
    s.u1_supported(false);
    expect(that % not s.u1_supported());

    s.u2_supported(true);
    expect(that % s.u2_supported());
    s.u2_supported(false);
    expect(that % not s.u2_supported());
  };

  "lpm_support method chaining"_test = []() {
    auto s = lpm_support{}
               .remote_wakeup_supported(true)
               .l1_supported(true)
               .u1_supported(true)
               .u2_supported(true);

    expect(that % s.remote_wakeup_supported());
    expect(that % s.l1_supported());
    expect(that % s.u1_supported());
    expect(that % s.u2_supported());
    expect(that % s.any());
    expect(that % not s.none());
  };

  "lpm_support set() directly"_test = []() {
    lpm_support s{};
    s.set(lpm_support::l1_mask, true);
    expect(that % s.l1_supported());
    expect(that % not s.u1_supported());

    s.set(lpm_support::l1_mask, false);
    expect(that % not s.l1_supported());
  };

  "lpm_support operator<=>"_test = []() {
    auto a = lpm_support{}.l1_supported(true);
    auto b = lpm_support{}.l1_supported(true);
    auto c = lpm_support{}.u1_supported(true);

    expect(a == b);
    expect(a != c);
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
  using host_event = hal::v5::usb::host_event;

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

  void driver_handle_host_event(host_event p_event) override
  {
    last_host_event = p_event;
    handle_host_event_called = true;
  }

  descriptor_start write_descriptors_start{};
  setup_packet handle_request_setup{};
  u8 write_string_descriptor_string_index{};
  host_event last_host_event{};
  bool handle_host_event_called{ false };
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

boost::ut::suite<"setup_packet_test"> setup_packet_test = []() {
  using namespace boost::ut;

  "setup_packet default constructor"_test = []() {
    setup_packet pkt{};
    expect(that % 0 == pkt.bm_request_type());
    expect(that % 0 == pkt.request());
    expect(that % 0 == pkt.value());
    expect(that % 0 == pkt.index());
    expect(that % 0 == pkt.length());
  };

  "setup_packet construction from args"_test = []() {
    setup_packet pkt(setup_packet::args{
      .device_to_host = true,
      .type = setup_packet::request_type::class_t,
      .recipient = setup_packet::request_recipient::interface,
      .request = 0x09,
      .value = 0x0301,
      .index = 0x0002,
      .length = 0x0008,
    });

    expect(that % true == pkt.is_device_to_host());
    expect(setup_packet::request_type::class_t == pkt.get_type());
    expect(setup_packet::request_recipient::interface == pkt.get_recipient());
    expect(that % u8{ 0x09 } == pkt.request());
    expect(that % u16{ 0x0301 } == pkt.value());
    expect(that % u16{ 0x0002 } == pkt.index());
    expect(that % u16{ 0x0008 } == pkt.length());
  };

  "setup_packet bm_request_type and request fields"_test = []() {
    // device-to-host | vendor | endpoint recipient
    std::array<byte, 8> raw{
      byte{ 0b11000010 }, byte{ 0x42 }, 0, 0, 0, 0, 0, 0
    };
    setup_packet pkt(raw);

    expect(that % byte{ 0b11000010 } == byte{ pkt.bm_request_type() });
    expect(that % byte{ 0x42 } == byte{ pkt.request() });
    expect(setup_packet::request_type::vendor == pkt.get_type());
    expect(setup_packet::request_recipient::endpoint == pkt.get_recipient());
    expect(that % true == pkt.is_device_to_host());
  };

  "setup_packet value getter and setter"_test = []() {
    setup_packet pkt{};
    pkt.value(0x1234);
    expect(that % u16{ 0x1234 } == pkt.value());

    auto vb = pkt.value_bytes();
    expect(that % 2 == vb.size());
    expect(that % byte{ 0x34 } == vb[0]);
    expect(that % byte{ 0x12 } == vb[1]);
  };

  "setup_packet index getter and setter"_test = []() {
    setup_packet pkt{};
    pkt.index(0xABCD);
    expect(that % u16{ 0xABCD } == pkt.index());

    auto ib = pkt.index_bytes();
    expect(that % 2 == ib.size());
    expect(that % byte{ 0xCD } == ib[0]);
    expect(that % byte{ 0xAB } == ib[1]);
  };

  "setup_packet length getter and setter"_test = []() {
    setup_packet pkt{};
    pkt.length(0x0040);
    expect(that % u16{ 0x0040 } == pkt.length());

    auto lb = pkt.length_bytes();
    expect(that % 2 == lb.size());
    expect(that % byte{ 0x40 } == lb[0]);
    expect(that % byte{ 0x00 } == lb[1]);
  };

  "setup_packet operator=="_test = []() {
    std::array<byte, 8> raw{ byte{ 0x80 }, byte{ 0x06 }, byte{ 0x00 },
                             byte{ 0x01 }, byte{ 0x00 }, byte{ 0x00 },
                             byte{ 0x12 }, byte{ 0x00 } };
    setup_packet a(raw);
    setup_packet b(raw);
    setup_packet c{};

    expect(a == b);
    expect(a != c);
  };

  "setup_packet get_type invalid"_test = []() {
    // bits 6-5 = 0b11 (value 3) is reserved/invalid
    std::array<byte, 8> raw{ byte{ 0b01100000 }, 0, 0, 0, 0, 0, 0, 0 };
    setup_packet pkt(raw);
    expect(setup_packet::request_type::invalid == pkt.get_type());
  };

  "setup_packet get_recipient invalid"_test = []() {
    // bits 4-0 = 3 is reserved/invalid
    std::array<byte, 8> raw{ byte{ 0x03 }, 0, 0, 0, 0, 0, 0, 0 };
    setup_packet pkt(raw);
    expect(setup_packet::request_recipient::invalid == pkt.get_recipient());
  };

  "setup_packet from_le_bytes"_test = []() {
    auto result = setup_packet::from_le_bytes(byte{ 0x34 }, byte{ 0x12 });
    expect(that % u16{ 0x1234 } == result);
  };

  "setup_packet to_le_u16"_test = []() {
    auto arr = setup_packet::to_le_u16(0xABCD);
    expect(that % byte{ 0xCD } == arr[0]);
    expect(that % byte{ 0xAB } == arr[1]);
  };
};

boost::ut::suite<"determine_standard_request_test"> std_request_test = []() {
  using namespace boost::ut;

  "determine_standard_request valid standard requests"_test = []() {
    auto make = [](u8 req) {
      return setup_packet(setup_packet::args{
        .device_to_host = false,
        .type = setup_packet::request_type::standard,
        .recipient = setup_packet::request_recipient::device,
        .request = req,
        .value = 0,
        .index = 0,
        .length = 0,
      });
    };

    expect(standard_request_types::get_status ==
           determine_standard_request(make(0x00)));
    expect(standard_request_types::clear_feature ==
           determine_standard_request(make(0x01)));
    expect(standard_request_types::set_feature ==
           determine_standard_request(make(0x03)));
    expect(standard_request_types::set_address ==
           determine_standard_request(make(0x05)));
    expect(standard_request_types::get_descriptor ==
           determine_standard_request(make(0x06)));
    expect(standard_request_types::set_descriptor ==
           determine_standard_request(make(0x07)));
    expect(standard_request_types::get_configuration ==
           determine_standard_request(make(0x08)));
    expect(standard_request_types::set_configuration ==
           determine_standard_request(make(0x09)));
    expect(standard_request_types::get_interface ==
           determine_standard_request(make(0x0A)));
    expect(standard_request_types::set_interface ==
           determine_standard_request(make(0x11)));
    expect(standard_request_types::synch_frame ==
           determine_standard_request(make(0x12)));
  };

  "determine_standard_request returns invalid for non-standard type"_test =
    []() {
      setup_packet pkt(setup_packet::args{
        .device_to_host = false,
        .type = setup_packet::request_type::class_t,
        .recipient = setup_packet::request_recipient::device,
        .request = 0x06,
        .value = 0,
        .index = 0,
        .length = 0,
      });
      expect(standard_request_types::invalid ==
             determine_standard_request(pkt));
    };

  "determine_standard_request returns invalid for reserved request 0x04"_test =
    []() {
      setup_packet pkt(setup_packet::args{
        .device_to_host = false,
        .type = setup_packet::request_type::standard,
        .recipient = setup_packet::request_recipient::device,
        .request = 0x04,
        .value = 0,
        .index = 0,
        .length = 0,
      });
      expect(standard_request_types::invalid ==
             determine_standard_request(pkt));
    };

  "determine_standard_request returns invalid for out-of-range request"_test =
    []() {
      setup_packet pkt(setup_packet::args{
        .device_to_host = false,
        .type = setup_packet::request_type::standard,
        .recipient = setup_packet::request_recipient::device,
        .request = 0x13,
        .value = 0,
        .index = 0,
        .length = 0,
      });
      expect(standard_request_types::invalid ==
             determine_standard_request(pkt));
    };
};

boost::ut::suite<"endpoint_io_test"> endpoint_io_test = []() {
  using namespace boost::ut;

  "endpoint_io read"_test = []() {
    mock_endpoint_io eio;
    std::array<byte, 4> buf{};
    auto ss = make_writable_scatter_bytes(buf);

    expect(that % not eio.read_called);
    auto n = eio.read(ss);
    expect(that % eio.read_called);
    expect(that % usize{ 4 } == n);
  };

  "endpoint_io write"_test = []() {
    mock_endpoint_io eio;
    std::array<byte, 3> data{ byte{ 1 }, byte{ 2 }, byte{ 3 } };
    auto ss = make_scatter_bytes(data);

    expect(that % not eio.write_called);
    auto n = eio.write(ss);
    expect(that % eio.write_called);
    expect(that % usize{ 3 } == n);
  };
};

boost::ut::suite<"interface_descriptor_types_test"> descriptor_types_test =
  []() {
    using namespace boost::ut;
    using descriptor_count = interface::descriptor_count;
    using descriptor_start = interface::descriptor_start;

    "descriptor_count operator<=>"_test = []() {
      descriptor_count a{ 1, 2 };
      descriptor_count b{ 1, 2 };
      descriptor_count c{ 2, 1 };

      expect(a == b);
      expect(a != c);
    };

    "descriptor_start operator<=>"_test = []() {
      descriptor_start a{ .interface = 0, .string = 1 };
      descriptor_start b{ .interface = 0, .string = 1 };
      descriptor_start c{ .interface = 1, .string = 0 };

      expect(a == b);
      expect(a != c);
    };

    "descriptor_start with nullopt"_test = []() {
      descriptor_start a{ .interface = std::nullopt, .string = std::nullopt };
      descriptor_start b{ .interface = std::nullopt, .string = std::nullopt };
      descriptor_start c{ .interface = 0, .string = std::nullopt };

      expect(a == b);
      expect(a != c);
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

  "interface::handle_host_event"_test = []() mutable {
    using host_event = hal::v5::usb::host_event;
    mock iface;

    iface.handle_host_event(host_event::reset);
    expect(that % iface.handle_host_event_called);
    expect(host_event::reset == iface.last_host_event);

    iface.handle_host_event(host_event::suspend_with_wakeup);
    expect(host_event::suspend_with_wakeup == iface.last_host_event);

    iface.handle_host_event(host_event::resume);
    expect(host_event::resume == iface.last_host_event);
  };

  "interface default handle_host_event is no-op"_test = []() mutable {
    using host_event = hal::v5::usb::host_event;
    struct noop_iface : public interface
    {
      [[nodiscard]] descriptor_count driver_write_descriptors(
        descriptor_start,
        endpoint_io&) override
      {
        return {};
      }
      [[nodiscard]] bool driver_write_string_descriptor(u8,
                                                        endpoint_io&) override
      {
        return false;
      }
      bool driver_handle_request(setup_packet const&, endpoint_io&) override
      {
        return false;
      }
      // driver_handle_host_event not overridden — default is no-op
    } iface;

    expect(nothrow([&] { iface.handle_host_event(host_event::reset); }));
    expect(nothrow([&] { iface.handle_host_event(host_event::sleep); }));
  };
};
}  // namespace hal::usb
