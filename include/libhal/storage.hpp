#pragma once

#include <cstdint>
#include <span>

#include "units.hpp"

class storage
{
public:
  virtual ~storage() = default;

private:
  virtual bool driver_is_media_present() = 0;
  virtual bool driver_is_read_only() = 0;
  virtual std::uint64_t driver_capacity() = 0;
  virtual void driver_erase(std::uint64_t p_address,
                            std::uint32_t p_length) = 0;
  virtual void driver_write(std::uint64_t p_address,
                            std::span<const hal::byte> p_data) = 0;
  virtual void driver_read(std::uint64_t p_address,
                           std::span<hal::byte> p_buffer) = 0;
};