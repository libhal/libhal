#include <print>

#include <libhal/error.hpp>
#include <libhal/experimental/network.hpp>
#include <libhal/initializers.hpp>

#include "libhal/timeout.hpp"

#include <boost/ut.hpp>

namespace hal {
class connection_aborted : public hal::exception
{
public:
  connection_aborted(void* p_instance)
    : hal::exception(std::errc::connection_aborted, p_instance)
  {
  }
};

class network_down : public hal::exception
{
public:
  network_down(void* p_instance)
    : hal::exception(std::errc::network_down, p_instance)
  {
  }
};

struct static_or_stack_only
{
  static_or_stack_only() = default;
  static_or_stack_only(static_or_stack_only const&) = delete;
  static_or_stack_only& operator=(static_or_stack_only const&) = delete;
  static_or_stack_only(static_or_stack_only&&) = delete;
  static_or_stack_only& operator=(static_or_stack_only&&) = delete;
  void* operator new(std::size_t) = delete;
};
}  // namespace hal

class my_wifi_device
{
public:
  class my_socket
    : public hal::socket
    , hal::static_or_stack_only
  {
  public:
    ~my_socket()
    {
      printf("~my_socket\n");
    }

    void driver_connect(std::string_view p_url, std::uint16_t p_port) override
    {
      m_driver->socket_connect(m_socket_number, p_url, p_port);
    }

    void driver_write(std::span<hal::byte const> p_data) override
    {
      m_driver->socket_write(m_socket_number, p_data);
    }

    std::span<hal::byte const> driver_read(std::span<hal::byte> p_data) override
    {
      return m_driver->socket_read(m_socket_number, p_data);
    }

    bool driver_connected() override
    {
      return m_driver->socket_connected(m_socket_number);
    }

    void driver_disconnect() override
    {
      return m_driver->socket_disconnect(m_socket_number);
    }

    friend class my_wifi_device;

  private:
    my_socket(my_wifi_device& p_driver, hal::byte p_socket_number)
      : m_driver(&p_driver)
      , m_socket_number(p_socket_number)
    {
    }

    my_wifi_device* m_driver = nullptr;
    hal::byte m_socket_number{};
  };

  class my_ip_network
    : public hal::ip_network
    , hal::static_or_stack_only
  {
  public:
    my_ip_network(my_wifi_device& p_driver)
      : m_driver(&p_driver)
    {
    }

    void driver_connect(std::optional<ipv4> p_address) override
    {
      m_driver->network_connect(p_address);
    }

    bool driver_connected() override
    {
      return m_driver->network_connected();
    }

    void driver_disconnect() override
    {
      m_driver->network_disconnect();
    }

  private:
    my_wifi_device* m_driver = nullptr;
  };

  class my_wifi
    : public hal::wifi
    , hal::static_or_stack_only
  {
  public:
    ~my_wifi()
    {
      printf("~my_wifi\n");
    }

    void driver_connect(std::string_view p_ssid,
                        std::string_view p_password) override
    {
      m_driver->wifi_connect(p_ssid, p_password);
    }

    bool driver_connected() override
    {
      return m_driver->wifi_connected();
    }

    void driver_disconnect() override
    {
      m_driver->wifi_disconnect();
    }

    friend class my_wifi_device;

  private:
    my_wifi(my_wifi_device& p_driver)
      : m_driver(&p_driver)
    {
    }
    my_wifi_device* m_driver = nullptr;
  };

  my_wifi_device() = default;

  my_wifi wifi()
  {
    return my_wifi(*this);  // RVO baby!
  }

  my_ip_network ip_network()
  {
    return my_ip_network(*this);  // RVO baby!
  }

  template<size_t socket_index>
  my_socket socket()
  {
    static_assert(socket_index < 4);
    return my_socket(*this, socket_index);  // RVO baby!
  }

  ~my_wifi_device()
  {
    printf("~my_wifi_device\n");
  }

private:
  void wifi_connect(std::string_view p_ssid, std::string_view p_password)
  {
    m_ssid = p_ssid;
    m_password = p_password;
    printf("ssid: %.*s, password: %.*s\n",
           int(p_ssid.size()),
           p_ssid.data(),
           int(p_password.size()),
           p_password.data());
  }

  bool wifi_connected()
  {
    return not m_ssid.empty() and not m_password.empty();
  }

  void wifi_disconnect()
  {
    m_ssid = "";
    m_password = "";
  }

  void network_connect(std::optional<hal::ip_network::ipv4> p_address)
  {
    m_connection_address = p_address;
  }

  bool network_connected()
  {
    return bool{ m_connection_address };
  }

  void network_disconnect()
  {
    m_connection_address = std::nullopt;
  }

  void socket_connect(hal::byte p_socket_number,
                      std::string_view p_url,
                      std::uint16_t p_port)
  {
    printf("Socket %d connected to: %.*s:%d\n",
           p_socket_number,
           int(p_url.size()),
           p_url.data(),
           p_port);
    m_active_sockets.set(p_socket_number);
  }

  void socket_write(hal::byte p_socket_number,
                    std::span<hal::byte const> p_data)
  {
    if (not socket_connected(p_socket_number)) {
      throw hal::connection_aborted(this);
    }

    printf("[%d] write: \"%.*s\": \n",
           p_socket_number,
           int(p_data.size()),
           p_data.data());
    printf("    ");
    for (auto const byte : p_data) {
      printf("0x%02X ", byte);
    }
    puts("");
  }

  std::span<hal::byte const> socket_read(hal::byte p_socket_number,
                                         std::span<hal::byte> p_data)
  {
    // Only fill up the buffer 8 bytes at a time, to simulate chunks of data
    auto subspan = p_data.first(std::min(p_data.size(), 32uz));
    printf("[%d] read/filling (%p @ %zu) w/ 'a's \n",
           p_socket_number,
           static_cast<void*>(subspan.data()),
           subspan.size());
    std::ranges::fill(subspan, 'a');
    return subspan;
  }

  bool socket_connected(hal::byte p_socket_number)
  {
    return m_active_sockets.test(p_socket_number);
  }

  void socket_disconnect(hal::byte p_socket_number)
  {
    m_active_sockets.reset(p_socket_number);
  }

  std::optional<std::optional<hal::ip_network::ipv4>> m_connection_address;
  std::string_view m_ssid;
  std::string_view m_password;
  std::bitset<4> m_active_sockets;
};

namespace hal {  // NOLINT
class http_wifi_network_manager
{
public:
  static void nop()
  {
  }

  enum class request_type
  {
    get,
    post,
  };

  http_wifi_network_manager(hal::wifi& p_wifi,
                            hal::ip_network& p_ip_network,
                            std::span<hal::socket*> p_socket,
                            hal::callback<void(void)> p_on_connect = nop,
                            hal::callback<void(void)> p_on_disconnect = nop)
    : m_wifi(&p_wifi)
    , m_ip_network(&p_ip_network)
    , m_socket(p_socket)
    , m_on_connect(p_on_connect)
    , m_on_disconnect(p_on_disconnect)
  {
  }

  void http_get(std::string_view p_url,
                std::span<hal::byte> p_receive_buffer,
                hal::timeout auto p_timeout)
  {
    http_request(p_url, {}, p_receive_buffer, request_type::get, p_timeout);
  }

  template<std::size_t receive_length>
  std::array<hal::byte, receive_length> http_get(std::string_view p_url,
                                                 hal::timeout auto p_timeout)
  {
    std::array<hal::byte, receive_length> buffer{};
    http_get(p_url, buffer, p_timeout);
    return buffer;
  }

private:
  void http_request(std::string_view p_url,
                    std::span<hal::byte const> p_transmit_buffer,
                    std::span<hal::byte> p_receive_buffer,
                    request_type p_request_type,
                    hal::timeout_function p_timeout)
  {
    try {
      using namespace std::string_view_literals;
      auto& socket = available_socket(p_timeout);
      socket.connect(p_url, 80);

      // GET /pub/WWW/TheProject.html HTTP/1.1
      // Host: www.w3.org

      constexpr std::array<hal::byte, 17> get_str{ "GET / HTTP/1.1\r\n" };
      constexpr std::array<hal::byte, 18> post_str{ "POST / HTTP/1.1\r\n" };
      constexpr std::array<hal::byte, 6> host_str{ "HOST:" };
      constexpr std::array<hal::byte, 5> end{ "\r\n\r\n" };

      switch (p_request_type) {
        case request_type::get: {
          socket.write(get_str);
        }
        case request_type::post: {
          socket.write(post_str);
        }
      }

      socket.write(host_str);
      // I know this isn't how it should be down, but this is all exposition
      // that I got way too into
      socket.write(std::span(reinterpret_cast<hal::byte const*>(p_url.data()),
                             p_url.size()));
      socket.write(end);

      if (not p_transmit_buffer.empty() &&
          p_request_type == request_type::post) {
        socket.write(std::span(
          reinterpret_cast<hal::byte const*>(p_transmit_buffer.data()),
          p_transmit_buffer.size()));
      }

      while (not p_receive_buffer.empty()) {
        auto const filled = socket.read(p_receive_buffer);
        p_receive_buffer = p_receive_buffer.subspan(filled.size());
        p_timeout();
      }
    } catch (hal::connection_aborted const&) {
      m_on_disconnect();
    } catch (hal::network_down const&) {
      m_on_disconnect();
    }
    reconnect_procedure();
  }

  void reconnect_procedure()
  {
    // do reconnection stuff with
    (void)m_wifi;
    (void)m_ip_network;
  }

  hal::socket& available_socket(hal::timeout_function p_timeout)
  {
    while (true) {
      for (auto const& socket : m_socket) {
        if (not socket->connected()) {
          return *socket;
        }
      }
      p_timeout();
    }
  }
  hal::wifi* m_wifi;
  hal::ip_network* m_ip_network;
  std::span<hal::socket*> m_socket{};
  // May consider separate callbacks for wifi & network disconnections
  hal::callback<void(void)> m_on_connect;
  hal::callback<void(void)> m_on_disconnect;
};

void test_network()
{  // NOLINT
  using namespace boost::ut;
  using namespace std::literals;

  "network::network()"_test = []() {
    // Version 1
    // Setup
    my_wifi_device wifi_device;
    auto wifi = wifi_device.wifi();
    auto ip_network = wifi_device.ip_network();
    auto socket0 = wifi_device.socket<0>();
    auto socket1 = wifi_device.socket<1>();
    auto socket2 = wifi_device.socket<2>();
    auto socket3 = wifi_device.socket<3>();

    auto sockets =
      std::to_array<hal::socket*>({ &socket0, &socket1, &socket2, &socket3 });

    http_wifi_network_manager my_network_manager(wifi, ip_network, sockets);
    std::array<hal::byte, 128> buffer{};
    my_network_manager.http_get(
      "http://example.com", buffer, hal::never_timeout());
    printf("%.*s\n", int(buffer.size()), buffer.data());
  };
};
}  // namespace hal
