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

#pragma once
#include <libhal/error.hpp>
#include <libhal/usb.hpp>

// Mock implementation for usb_endpoint

namespace hal::v5 {

class mock_usb_endpoint : public usb_endpoint
{
public:
  usb_endpoint_info m_info{};
  bool m_stall_called{ false };
  bool m_should_stall{ false };
  bool m_reset_called{ false };

protected:
  [[nodiscard]] usb_endpoint_info driver_info() const override
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
class mock_usb_control_endpoint : public usb_control_endpoint
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
  [[nodiscard]] usb_endpoint_info driver_info() const override
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

}  // namespace hal::v5
