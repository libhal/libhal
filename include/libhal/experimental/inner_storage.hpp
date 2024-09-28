#pragma once

#include <span>
#include <string_view>

#include "../units.hpp"

namespace hal {

class socket
{
public:
  void write(std::span<hal::byte const> p_data)
  {
    driver_write(p_data);
  }
  std::span<hal::byte const> read(std::span<hal::byte> p_data)
  {
    return driver_read(p_data);
  }
  virtual ~socket() = default;

private:
  virtual void driver_write(std::span<hal::byte const> p_data) = 0;
  virtual std::span<hal::byte const> driver_read(
    std::span<hal::byte> p_data) = 0;
};

class server_connector
{
public:
  auto& connect(std::string_view p_url, std::uint16_t p_port)
  {
    return driver_connect(p_url, p_port);
  }

  void disconnect()
  {
    return driver_disconnect();
  }

  virtual ~server_connector() = default;

private:
  virtual socket& driver_connect(std::string_view p_url,
                                 std::uint16_t p_port) = 0;
  virtual void driver_disconnect() = 0;
};

class auto_closing_server_connector
{
public:
  auto_closing_server_connector(server_connector& p_server_connector)
    : m_server_connector(&p_server_connector)
  {
  }

  auto& connect(std::string_view p_url, std::uint16_t p_port)
  {
    return m_server_connector->connect(p_url, p_port);
  }

  ~auto_closing_server_connector()
  {
    m_server_connector->disconnect();
  }

private:
  server_connector* m_server_connector;
};

class wifi
{
public:
  void connect(std::string_view p_ssid, std::string_view p_password)
  {
    driver_connect(p_ssid, p_password);
  }

  void disconnect()
  {
    driver_disconnect();
  }

  auto_closing_server_connector acquire_available_server_connector()
  {
    return auto_closing_server_connector(
      driver_acquire_available_server_connector());
  }

  virtual ~wifi() = default;

private:
  virtual server_connector& driver_connect(std::string_view p_ssid,
                                           std::string_view p_password) = 0;
  virtual void driver_disconnect() = 0;
  virtual server_connector& driver_acquire_available_server_connector() = 0;
};
}  // namespace hal
