#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <print>
#include <span>
#include <string_view>

#include "../error.hpp"
#include "../units.hpp"

namespace hal {

struct poly_constructor
{
  poly_constructor(std::span<hal::byte> p_memory, bool& p_constructed)
    : m_memory(p_memory)
    , m_constructed(&p_constructed)
  {
  }

  poly_constructor(poly_constructor const& p_other) = delete;
  poly_constructor& operator=(poly_constructor const& p_other) = delete;
  poly_constructor(poly_constructor&& p_other) = delete;
  poly_constructor& operator=(poly_constructor&& p_other) = delete;

  template<typename T, class... Args>
  void construct(Args&&... args) const
  {
    if (m_memory.size() < sizeof(T)) {
      hal::safe_throw(hal::resource_unavailable_try_again(nullptr));
    }
    // Use ssid and password to connect to AP
    new (m_memory.data()) T(args...);

    *m_constructed = true;
  }

  std::span<hal::byte> m_memory{};
  bool* m_constructed;
};

// This is something I'd use to provide memory for the types acquired
// via the virtual APIs
template<class interface,
         size_t memory_for_object,
         size_t alignment = alignof(std::max_align_t)>
struct alignas(alignment) poly_object
{
  interface* operator->()
  {
    if (not constructed) {
      hal::safe_throw(hal::unknown(this));
    }
    return std::bit_cast<interface*>(m_object_memory.data());
  }
  interface* operator*()
  {
    if (not constructed) {
      hal::safe_throw(hal::unknown(this));
    }
    return std::bit_cast<interface*>(m_object_memory.data());
  }

  poly_constructor get_constructor()
  {
    return poly_constructor(m_object_memory, constructed);
  }

  ~poly_object()
  {
    if (constructed) {
      get_internal().~interface();
    }
  }

  // Needs a destructor and other stuff
private:
  interface& get_internal()
  {
    return *reinterpret_cast<interface*>(m_object_memory.data());
  }

  std::array<hal::byte, memory_for_object> m_object_memory{};
  bool constructed = false;
};

class socket
{
public:
  void write(std::span<std::uint8_t const> p_data)
  {
    driver_write(p_data);
  }

  std::span<std::uint8_t const> read(std::span<std::uint8_t> p_data)
  {
    return driver_read(p_data);
  }

  virtual ~socket() = default;

private:
  virtual void driver_write(std::span<std::uint8_t const> p_data) = 0;
  virtual std::span<std::uint8_t const> driver_read(
    std::span<std::uint8_t> p_data) = 0;
};

class server_connection
{
public:
  template<std::size_t allocation_size = sizeof(std::uintptr_t) * 8>
  [[nodiscard("Dropping the return object will cause it to be deallocated")]]
  auto connect(std::string_view p_url, std::uint16_t p_port)
  {
    poly_object<socket, allocation_size> obj;
    driver_connect(obj.get_constructor(), p_url, p_port);
    return obj;
  }
  virtual ~server_connection() = default;

private:
  virtual void driver_connect(poly_constructor const& p_object_storage,
                              std::string_view p_url,
                              std::uint16_t p_port) = 0;
  virtual socket& driver_connect(std::string_view p_url,
                                 std::uint16_t p_port) = 0;
};

class wifi
{
public:
  template<std::size_t allocation_size = sizeof(std::uintptr_t*) * 8>
  [[nodiscard("Dropping the return object will cause it to be deallocated")]]
  auto connect(std::string_view p_ssid, std::string_view p_password)
  {
    poly_object<server_connection, allocation_size> obj;
    driver_connect(obj.get_constructor(), p_ssid, p_password);
    return obj;
  }

  virtual ~wifi() = default;

private:
  virtual void driver_connect(poly_constructor const& p_object_storage,
                              std::string_view p_ssid,
                              std::string_view p_password) = 0;
  virtual server_connection& driver_connect2(std::string_view p_ssid,
                                             std::string_view p_password) = 0;
};

}  // namespace hal
