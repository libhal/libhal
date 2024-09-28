#include "libhal/error.hpp"
#include <libhal/experimental/pimpl.hpp>

#include <boost/ut.hpp>

struct my_socket : public hal::socket
{
  my_socket(std::string_view p_url, std::uint16_t p_port)
    : m_url(p_url)
    , m_port(p_port)
  {
  }

  ~my_socket()
  {
    printf("~my_socket\n");
  }

  void driver_write(std::span<std::uint8_t const> p_data) override
  {
    printf("%.*s, %d: ", int(m_url.size()), m_url.data(), m_port);
    for (auto const byte : p_data) {
      printf("0x%02X ", byte);
    }
    puts("");
  }

  std::span<std::uint8_t const> driver_read(
    std::span<std::uint8_t> p_data) override
  {
    std::ranges::fill(p_data, 0xFF);
    return p_data;
  }

  std::string_view m_url;
  std::uint16_t m_port;
};

struct my_server_connection : public hal::server_connection
{
  my_server_connection(std::string_view p_ssid, std::string_view p_password)
    : m_ssid(p_ssid)
    , m_password(p_password)
  {
  }

  ~my_server_connection()
  {
    printf("~my_server_connection\n");
  }

  void driver_connect(hal::poly_constructor const& p_memory,
                      std::string_view p_url,
                      std::uint16_t p_port) override
  {
    p_memory.construct<my_socket>(p_url, p_port);
  };

  std::string_view m_ssid;
  std::string_view m_password;
};

struct my_wifi : public hal::wifi
{
  my_wifi() = default;

  ~my_wifi()
  {
    printf("~my_wifi\n");
  }

  void driver_connect(hal::poly_constructor const& p_memory,
                      std::string_view p_ssid,
                      std::string_view p_password) override
  {
    // Use ssid and password to connect to AP
    p_memory.construct<my_server_connection>(p_ssid, p_password);
  };
};

namespace hal {  // NOLINT

class connection_aborted : public hal::exception
{
  connection_aborted(void* p_instance)
    : hal::exception(std::errc::connection_aborted, p_instance)
  {
  }
};

class network_down : public hal::exception
{
  network_down(void* p_instance)
    : hal::exception(std::errc::network_down, p_instance)
  {
  }
};

void test_pimpl()
{  // NOLINT
  using namespace boost::ut;
  using namespace std::literals;

  "pimpl::pimpl()"_test = []() {
    // Setup
    my_wifi wifi_object;
    try {
      auto connection = wifi_object.connect("ssid", "password");
      try {
        auto socket = connection->connect("example.com", 80);
        socket->write(std::to_array<hal::byte>({ 0xFF, 0xAB, 0xCD, 0xEF }));
      } catch (hal::connection_aborted const&) {
        // Reconnect to server...
      }
    } catch (hal::network_down const&) {
      // Reconnect to wifi
    }
    // Exercise
    // Verify
  };
};
}  // namespace hal
