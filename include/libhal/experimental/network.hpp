#pragma once

#include <span>
#include <string_view>

#include "../units.hpp"

namespace hal {

class socket
{
public:
  bool connected()
  {
    return driver_connected();
  }

  void connect(std::string_view p_url, std::uint16_t p_port)
  {
    driver_connect(p_url, p_port);
  }

  void write(std::span<hal::byte const> p_data)
  {
    driver_write(p_data);
  }

  std::span<hal::byte const> read(std::span<hal::byte> p_data)
  {
    return driver_read(p_data);
  }

  void disconnect()
  {
    driver_disconnect();
  }

  virtual ~socket() = default;

private:
  virtual void driver_disconnect() = 0;
  virtual void driver_connect(std::string_view p_url, std::uint16_t p_port) = 0;
  virtual void driver_write(std::span<hal::byte const> p_data) = 0;
  virtual std::span<hal::byte const> driver_read(
    std::span<hal::byte> p_data) = 0;
  virtual bool driver_connected() = 0;
};

class wifi
{
public:
  void connect(std::string_view p_ssid, std::string_view p_password)
  {
    driver_connect(p_ssid, p_password);
  }

  bool connected()
  {
    return driver_connected();
  }

  void disconnect()
  {
    driver_disconnect();
  }

  virtual ~wifi() = default;

private:
  virtual void driver_connect(std::string_view p_ssid,
                              std::string_view p_password) = 0;
  virtual bool driver_connected() = 0;
  virtual void driver_disconnect() = 0;
};

class ip_network
{
public:
  struct ipv4
  {
    std::array<std::uint8_t, 4> address;
  };

  /**
   * @brief Connect to a IP (internet protocol) network
   *
   * @param p_address - ipv4 address OR nullopt which means DHCP
   */
  void connect(std::optional<ipv4> p_address = std::nullopt)
  {
    driver_connect(p_address);
  }

  bool connected()
  {
    return driver_connected();
  }

  void disconnect()
  {
    driver_disconnect();
  }

  virtual ~ip_network() = default;

private:
  virtual void driver_connect(std::optional<ipv4> p_address) = 0;
  virtual bool driver_connected() = 0;
  virtual void driver_disconnect() = 0;
};
}  // namespace hal
