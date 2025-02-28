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

#include "libhal/functional.hpp"
#include "libhal/scatter_span.hpp"
#include "libhal/units.hpp"

namespace hal::v5 {

/**
 * @brief A class representing a usb interface: Examples of this could be a HID
 * device, a microphone, a storage device, etc Is to be paired with a given
 * device and configuration during enumeration
 */
class usb_interface
{
public:
  virtual ~usb_interface() = default;

  using endpoint_writer = hal::callback<void(scatter_span<hal::byte const>)>;

  struct req_bitmap
  {
    enum class type : hal::byte const
    {
      standard = 0,
      class_t = 1,
      vendor = 2,
      invalid
    };

    enum class recipient : hal::byte const
    {
      device = 0,
      interface = 1,
      endpoint = 2,
      invalid
    };

    constexpr req_bitmap(u8 p_bitmap)
      : m_bitmap(p_bitmap)
    {
    }

    constexpr req_bitmap(bool p_device_to_host,
                         type p_type,
                         recipient p_recipient)
    {
      m_bitmap = (p_device_to_host << 7) | (static_cast<u8>(p_type) << 5) |
                 static_cast<u8>(p_recipient);
    }

    [[nodiscard]] constexpr inline type get_type() const
    {
      u8 t = m_bitmap & (0b11 << 5);
      if (t > 2) {
        return type::invalid;
      }

      return static_cast<type>(t);
    }

    [[nodiscard]] constexpr inline recipient get_recipient() const
    {
      u8 r = m_bitmap & 0b1111;
      if (r > 2) {
        return recipient::invalid;
      }

      return static_cast<recipient>(r);
    }

    constexpr inline bool is_device_to_host()
    {
      return bool(m_bitmap & 1 << 7);
    }

    u8 m_bitmap;
  };

  /**
   * @brief Rich type to encapsulate index deltas to be returned by
   * `write_descriptors()`
   */
  struct descriptor_delta
  {
    byte iface_idxes;
    byte str_idxes;

    bool operator<=>(descriptor_delta const& rhs) const = default;
  };

  /**
   *@brief Writes descriptors that this interface is responsible for via a
   * callback. This function may be called multiple times during
   *(re)enumeration.
   *
   * @param p_callback - A hal::callback that returns void and takes a span of
   * const bytes as a parmeter, that span is where the descriptor stream will be
   * written to
   *
   * @param p_iface_starting_idx - The next free index number, interfaces can
   *use multiple interface indexes if desired. It is recomended if more
   *interfaces are required to use values equal or greater to the starting idx,
   *interfaces may not share indexes.
   *
   * @param p_str_starting_idx - The next free string index. Similar to the
   * interfaces, multiple indexes may be used for various descriptors. It is
   * recomended to enumerate strings via the index. String index 0 is always
   * reserved for the language string.
   */
  [[nodiscard]] descriptor_delta write_descriptors(
    endpoint_writer const& p_callback,
    u8 p_iface_starting_idx,  // NOLINT
    u8 p_str_starting_idx)
  {
    return driver_write_descriptors(
      p_callback, p_iface_starting_idx, p_str_starting_idx);
  }

  /**
   * @brief Write string descriptors this interface is responsible for via the
   * callback. Much like write_descriptor. To be called iteritively for as many
   * string descriptors the interface has. The class is responsible for tracking
   * strings(views). This function assumes that `write_descriptor()` has been
   * called already to assign string indexes to all strings managed by this
   * interface.
   *  NOTE: The smallest index that can be used is 1.
   *
   * @param p_callback - The callback with the span that the descriptor should
   * be written to.
   *
   * @param p_index - Which string index's descriptor should be written.
   *
   * @returns If writing the string descriptor was successful
   */
  [[nodiscard]] bool write_string_descriptor(endpoint_writer const& p_callback,
                                             u8 p_index)
  {
    return driver_write_string_descriptor(p_callback, p_index);
  }

  /**
   * @brief To handle GET_STATUS requests for both interfaces and endpoints,
   * returns two bytes of status information. For standard interface requests,
   * status bytes are reserved, a standard request should respond with two bytes
   * of 0. For standard requests to the endpoint, response should be if the
   * device is halted or stalled.
   * For more information:
   * https://www.beyondlogic.org/usbnutshell/usb6.shtml#SetupPacket
   *
   * @param p_bmRequestType - BitMap giving type and recipient information
   * @param p_wIndex - The two byte index from request.
   * @returns Device Status
   */
  // NOLINTBEGIN
  [[nodiscard]] u16 get_status(req_bitmap p_bmRequestType, u16 p_wIndex)
  {
    return driver_get_status(p_bmRequestType, p_wIndex);
  }

  /**
   * @brief Set or clear an interface (or below) feature
   *
   * @param p_bmRequestType - BitMap giving type and recipient information
   * @param p_set - If we are setting or clearing a feature, true is setting.
   * @param p_feature_selector - The feature selector to be set or cleared.
   */
  void manage_features(req_bitmap p_bmRequestType,
                       bool p_set,
                       u16 p_selector,
                       u16 p_wIndex)
  {
    driver_manage_features(p_bmRequestType, p_set, p_selector, p_wIndex);
  }
  /**
   * @brief Handler for GET_INTERFACE request. Typically when this is called the
   * host is requesting the AlternativeSettings field
   * https://www.beyondlogic.org/usbnutshell/usb6.shtml#SetupPacket
   *
   * @param p_wIndex - The index of the interface being requested.
   * @return Two bytes of data to send to the host.
   */
  u8 get_interface(u16 p_wIndex)
  {
    return driver_get_interface(p_wIndex);
  }

  /**
   * @brief Handler for SET_INTERFACE request. Typically when this is called the
   * host is setting the AlternativeSettings field
   * https://www.beyondlogic.org/usbnutshell/usb6.shtml#SetupPacket
   *
   * @param p_wIndex - The index of the interface being set.
   * @param p_alt_setting - The alternative setting number.
   */
  void set_interface(u16 p_alt_setting, u16 p_wIndex)
  {
    driver_set_interface(p_alt_setting, p_wIndex);
  }

  /**
   * @brief Generic handler for requests targetting the interface or below level
   * of a USB device. These could be standard requests or custom requests.
   *
   * @param p_bmRequestType - The BitMap with the request type, direction, and
   * intended recipient.
   * @param p_bRequest - The number for the request being made.
   * @param p_wValue - The parameter being passed to the component with the
   * request.
   * @param p_wIndex - The index or offset of where the request should go.
   * @param p_wLength - The amount of bytes host expects in the response
   * transaction.
   * @param p_callback - The callback to write out the response to the request
   * to the host.
   * @returns If the operation was successful or not.
   */
  bool handle_request(req_bitmap const p_bmRequestType,
                      byte const p_bRequest,
                      u16 const p_wValue,
                      u16 const p_wIndex,
                      u16 const p_wLength,
                      endpoint_writer const& p_callback)
  {
    return driver_handle_request(
      p_bmRequestType, p_bRequest, p_wValue, p_wIndex, p_wLength, p_callback);
  }

private:
  [[nodiscard]] virtual descriptor_delta driver_write_descriptors(
    endpoint_writer const& p_callback,
    u8 p_iface_number,
    u8 p_str_starting_idx) = 0;
  virtual bool driver_handle_request(req_bitmap const p_bmRequestType,
                                     byte const p_bRequest,
                                     u16 const p_wValue,
                                     u16 const p_wIndex,
                                     u16 const p_wLength,
                                     endpoint_writer const& p_callback) = 0;
  [[nodiscard]] virtual bool driver_write_string_descriptor(
    endpoint_writer const& p_callback,
    u8 p_index) = 0;

  [[nodiscard]] virtual u16 driver_get_status(req_bitmap p_bmRequestType,
                                              u16 p_wIndex) = 0;
  virtual void driver_manage_features(req_bitmap p_bmRequestType,
                                      bool p_set,
                                      u16 p_selector,
                                      u16 p_wIndex) = 0;

  virtual u8 driver_get_interface(u16 p_wIndex) = 0;
  virtual void driver_set_interface(u16 p_alt_setting, u16 p_wIndex) = 0;
  // NOLINTEND
};
}  // namespace hal::v5
