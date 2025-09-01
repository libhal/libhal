#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>

#include <libhal/error.hpp>
#include <libhal/scatter_span.hpp>
#include <libhal/units.hpp>
#include <libhal/usb.hpp>

#include "mock_usb_endpoints.hpp"

#include <boost/ut.hpp>

namespace hal::v5::usb {

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

constexpr u8 iface_desc_length = 9;
constexpr u8 iface_desc_type = 0x4;
constexpr u8 str_desc_type = 0x3;
constexpr u8 iad_length = 0x08;
constexpr u8 iad_type = 0x0B;

std::array<hal::byte, 2> to_le_bytes(u16 n)
{
  return { static_cast<hal::byte>(n & 0xFF),
           static_cast<hal::byte>(n & 0xFF << 8) };
}

class mock_interface : public interface
{
public:
  ~mock_interface() override = default;

  mock_interface(std::string_view p_name, mock_usb_endpoint&& p_mock_ep)
    : m_generic_ep(p_mock_ep)
    , m_name(p_name) {};

  // NOLINTBEGIN
  [[nodiscard]] descriptor_delta driver_write_descriptors(
    endpoint_writer const& p_callback,
    u8 p_iface_starting_idx,
    u8 p_str_starting_idx) override
  {
    m_write_desc_has_ran = true;
    bInterfaceNumber() = p_iface_starting_idx;
    bAlternateSetting() = 0;
    bNumEndpoints() = 1;
    bInterfaceClass() = 2;
    bInterfaceSubClass() = 3;
    bInterfaceProtocol() = 4;
    iInterface() = p_str_starting_idx;

    std::array s = { iface_desc_length, iface_desc_type };
    p_callback(make_scatter_bytes(s, std::span(m_desc_arr)));
    return { .iface_idxes = 1, .str_idxes = 1 };
  }

  [[nodiscard]] bool driver_write_string_descriptor(
    endpoint_writer const& p_callback,
    u8 p_index) override
  {
    if (!m_write_desc_has_ran) {
      // std::println("You must call write_descriptor first");
      safe_throw(hal::operation_not_permitted(this));
    }
    if (p_index != iInterface()) {
      return false;
    }

    std::array header = { str_desc_type, static_cast<byte>(m_name.length()) };
    p_callback(
      make_scatter_bytes(header,
                         std::span(reinterpret_cast<byte const*>(m_name.data()),
                                   m_name.length())));
    return true;
  }

  [[nodiscard]] u16 driver_get_status(req_bitmap p_bmRequestType,
                                      u16 p_wIndex) override
  {
    if (p_bmRequestType.get_recipient() == req_bitmap::recipient::interface &&
        static_cast<byte>(p_wIndex & 0xFF) == bInterfaceNumber()) {
      return 0;
    } else if (p_bmRequestType.get_recipient() ==
                 req_bitmap::recipient::endpoint &&
               static_cast<byte>(p_wIndex & 0xF) ==
                 m_generic_ep.m_info.logical_number()) {
      return m_generic_ep.info().logical_number();
    }

    safe_throw(hal::operation_not_supported(this));
  }

  void driver_manage_features(req_bitmap p_bmRequestType,
                              bool p_set,
                              u16 p_selector,
                              u16 p_wIndex) override
  {
    std::ignore = p_wIndex;  // Not used for this mock

    if (p_bmRequestType.get_recipient() == req_bitmap::recipient::endpoint) {
      m_generic_ep.stall(p_set);
    } else if (p_bmRequestType.get_recipient() ==
               req_bitmap::recipient::interface) {
      if (p_selector == 0) {
        m_test_feature = p_set;
      } else if (p_selector == 1) {
        m_another_test_feature = p_set;
      } else {
        // std::println("Invalid feature selector");
        safe_throw(hal::operation_not_supported(this));
      }
    } else {
      // std::println("Invalid recipient");
      safe_throw(hal::operation_not_supported(this));
    }
  }

  u8 driver_get_interface(u16 p_wIndex) override
  {
    std::ignore = p_wIndex;  // Not used for this mock
    return bAlternateSetting();
  }

  void driver_set_interface(u8 p_alt_setting, u16 p_wIndex) override
  {
    std::ignore = p_wIndex;  // Not used for this mock
    bAlternateSetting() = p_alt_setting;
  }

  bool driver_handle_request(req_bitmap const p_bmRequestType,
                             byte const p_bRequest,
                             u16 const p_wValue,
                             u16 const p_wIndex,
                             u16 const p_wLength,
                             endpoint_writer const& p_callback) override
  {
    if (p_bmRequestType.get_recipient() == req_bitmap::recipient::interface) {
      std::array<byte, 2> pack = { p_bmRequestType.m_bitmap, p_bRequest };
      p_callback(make_scatter_bytes(pack,
                                    to_le_bytes(p_wValue),
                                    to_le_bytes(p_wIndex),
                                    to_le_bytes(p_wLength)));
      return true;
    }
    return false;
  }

  operator std::span<u8 const>() const
  {
    return m_desc_arr;
  }

  constexpr byte& bInterfaceNumber()
  {
    return m_desc_arr[0];
  }

  constexpr byte& bAlternateSetting()
  {
    return m_desc_arr[1];
  }

  constexpr byte& bNumEndpoints()
  {
    return m_desc_arr[2];
  }

  constexpr byte& bInterfaceClass()
  {
    return m_desc_arr[3];
  }

  constexpr byte& bInterfaceSubClass()
  {
    return m_desc_arr[4];
  }

  constexpr byte& bInterfaceProtocol()
  {
    return m_desc_arr[5];
  }

  constexpr byte& iInterface()
  {
    return m_desc_arr[6];
  }

  mock_usb_endpoint m_generic_ep;
  std::string_view m_name;

  bool m_test_feature = false;          // feature 0
  bool m_another_test_feature = false;  // feature 1

private:
  bool m_write_desc_has_ran = false;
  std::array<byte, 7> m_desc_arr;
};

class mock_multi_interface : public usb_interface
{
public:
  ~mock_multi_interface() override = default;

  mock_multi_interface(std::string_view p_iface_name_one,
                       std::string_view p_iface_name_two)
    : m_name_one(p_iface_name_one)
    , m_name_two(p_iface_name_two) {};

  struct mock_iface_descriptor
  {
    byte num;
    byte alt_settings;
    byte str_idx;
    bool feature;
  };

private:
  [[nodiscard]] descriptor_delta driver_write_descriptors(
    endpoint_writer const& p_callback,
    u8 p_iface_number,
    u8 p_str_starting_idx) override
  {
    std::array<byte const, 8> iad_buf{
      iad_length,
      iad_type,
      0,                    // first interface
      2,                    // iface count
      0,                    // class
      0,                    // subclass
      0,                    // proto
      p_str_starting_idx++  // string idx
    };

    std::array<byte const, 2> iface_header = { iface_desc_length,
                                               iface_desc_type };
    std::array<byte const, 5> static_desc_vars = {
      0,  // altsettings
      1,  // num endpoints
      0,  // class
      0,  // subclass
      0,  // protocol
    };

    m_iface_one = { p_iface_number++, 0, p_str_starting_idx++, false };
    m_iface_two = { p_iface_number++, 0, p_str_starting_idx++, false };

    std::array<byte const, 1> iface_one_arr({ m_iface_one.num });
    std::array<byte const, 1> iface_one_str_arr({ m_iface_one.str_idx });

    std::array<byte const, 1> iface_two_arr({ m_iface_two.num });
    std::array<byte const, 1> iface_two_str_arr({ m_iface_two.str_idx });

    auto span_arr = make_scatter_bytes(iad_buf,

                                       // iface one
                                       iface_header,
                                       std::span(iface_one_arr),
                                       static_desc_vars,
                                       std::span(iface_one_str_arr),

                                       // iface two
                                       iface_header,
                                       std::span(iface_two_arr),
                                       static_desc_vars,
                                       std::span(iface_two_str_arr));

    p_callback(span_arr);
    m_wrote_descriptors = true;
    return { 2, 3 };
  }

  [[nodiscard]] bool driver_write_string_descriptor(
    endpoint_writer const& p_callback,
    u8 p_index) override
  {
    if (!m_wrote_descriptors) {
      // std::println("You must call write_descriptors first");
      safe_throw(hal::operation_not_permitted(this));
    }
    std::array<byte, 2> header{ 0, str_desc_type };
    if (p_index == m_iface_one.str_idx) {
      header[0] = m_name_one.length() + 2;
      auto arr = make_scatter_bytes(
        header,
        std::span(reinterpret_cast<byte const*>(m_name_one.data()),
                  m_name_one.length()));
      p_callback(arr);
      return true;
    } else if (p_index == m_iface_two.str_idx) {
      header[0] = m_iface_two.str_idx + 2;
      auto arr = make_scatter_bytes(
        header,
        std::span(reinterpret_cast<byte const*>(m_name_two.data()),
                  m_name_two.length()));

      p_callback(arr);
      return true;
    }

    return false;
  }

  [[nodiscard]] u16 driver_get_status(req_bitmap p_bmRequestType,
                                      u16 p_wIndex) override
  {
    if (p_bmRequestType.get_recipient() != req_bitmap::recipient::interface) {
      // std::println("Unsupported recipient");
      safe_throw(hal::operation_not_supported(this));
    }

    auto iface_idx = p_wIndex & 0xFF;

    if (iface_idx == m_iface_one.num) {
      return m_iface_one.num;
    } else if (iface_idx == m_iface_two.num) {
      return m_iface_two.num;
    }

    // std::println("Invalid interface index");
    safe_throw(hal::operation_not_supported(this));
  }

  void driver_manage_features(req_bitmap p_bmRequestType,
                              bool p_set,
                              u16 p_selector,
                              u16 p_wIndex) override
  {
    std::ignore = p_selector;
    if (p_bmRequestType.get_recipient() != req_bitmap::recipient::interface) {
      // std::println("Unsupported recipient");
      safe_throw(hal::operation_not_supported(this));
    }

    auto iface_idx = p_wIndex & 0xFF;

    if (iface_idx == m_iface_one.num) {
      m_iface_one.feature = p_set;
    } else if (iface_idx == m_iface_two.num) {
      m_iface_two.feature = p_set;
    } else {
      // std::println("Invalid interface index");
      safe_throw(hal::operation_not_supported(this));
    }
  }

  u8 driver_get_interface(u16 p_wIndex) override
  {
    auto iface_idx = p_wIndex & 0xFF;

    if (iface_idx == m_iface_one.num) {
      return m_iface_one.alt_settings;
    } else if (iface_idx == m_iface_two.num) {
      return m_iface_two.alt_settings;
    } else {
      // std::println("Invalid interface index");
      safe_throw(hal::operation_not_supported(this));
    }
  }

  void driver_set_interface(u8 p_alt_setting, u16 p_wIndex) override
  {
    auto iface_idx = p_wIndex & 0xFF;

    if (iface_idx == m_iface_one.num) {
      m_iface_one.alt_settings = p_alt_setting;
    } else if (iface_idx == m_iface_two.num) {
      m_iface_two.alt_settings = p_alt_setting;
    } else {
      // std::println("Invalid interface index");
      safe_throw(hal::operation_not_supported(this));
    }
  }

  bool driver_handle_request(req_bitmap const p_bmRequestType,
                             byte const p_bRequest,
                             u16 const p_wValue,
                             u16 const p_wIndex,
                             u16 const p_wLength,
                             endpoint_writer const& p_callback) override
  {
    std::ignore = p_bmRequestType;
    std::ignore = p_bRequest;
    std::ignore = p_wValue;
    std::ignore = p_wIndex;
    std::ignore = p_wLength;
    std::ignore = p_callback;
    return false;
  }

public:
  mock_iface_descriptor m_iface_one;
  std::string_view m_name_one;
  mock_iface_descriptor m_iface_two;
  std::string_view m_name_two;
  bool m_wrote_descriptors = false;
};
// NOLINTEND

}  // namespace

// Test the usb highlevel interface (interface descriptor level)
boost::ut::suite<"usb_iterface_req_bitmap_test"> req_bitmap_test = []() {
  using namespace boost::ut;
  using bitmap = usb_interface::req_bitmap;

  "req bitmap construction from byte test"_test = []() {
    u8 raw_byte = 0b10000001;
    bitmap bm(raw_byte);

    // Test recipient
    expect(that % static_cast<bool>(bitmap::recipient::interface ==
                                    bm.get_recipient()));

    // Test type
    expect(that % static_cast<bool>(bitmap::type::standard == bm.get_type()));

    // Test direction
    expect(that % true == bm.is_device_to_host());
  };

  "req bitmap construction from enums test"_test = []() {
    bitmap bm(false, bitmap::type::class_t, bitmap::recipient::endpoint);
    u8 raw = bm.m_bitmap;

    // Test direction
    expect(that % false == bool(raw & 1 << 7));

    // Test type
    expect(that % 1 == ((raw & 0b11 << 5) >> 5));

    // Test recipient
    expect(that % 2 == (raw & 0xF));
  };
};

// Test the usb highlevel interface (interface descriptor level)
boost::ut::suite<"usb_interface_test"> usb_interface_test = []() {
  using namespace boost::ut;
  using descriptor_delta = usb_interface::descriptor_delta;
  using req_bitmap = usb_interface::req_bitmap;

  mock_usb_endpoint ep;
  ep.m_info = usb_endpoint_info{ .size = 8, .number = 1, .stalled = false };

  mock_interface iface("Mock", std::move(ep));

  "usb_interface write descriptor test"_test = [iface]() mutable {
    std::array<byte const, 9> desc = {
      iface_desc_length,
      iface_desc_type,
      0,  // bInterfaceNumber
      0,  // bAlternateSetting
      1,  //  bNumEndpoints
      2,  //  bInterfaceClass
      3,  // bInterfaceSubClass
      4,  // bInterfaceProtocol
      1   // iInterface
    };
    auto intended_byte_stream_arr = make_scatter_bytes(desc);
    scatter_span<byte const> intended_byte_stream(intended_byte_stream_arr);
    auto f = [intended_byte_stream](scatter_span<hal::byte const> p_data) {
      // std::ignore = p_data;
      std::ignore = intended_byte_stream;
      // Verify size is correct
      expect(that % iface_desc_length == scatter_span_size(p_data));

      // Verify content is the same
      expect(that % (intended_byte_stream == p_data));
    };
    auto delta = iface.write_descriptors(f, 0, 1);

    expect(descriptor_delta{ 1, 1 } == delta);
  };

  "usb_interface write string descriptor test"_test = [iface]() mutable {
    std::ignore = iface.write_descriptors(
      [](scatter_span<hal::byte const> p_data) { std::ignore = p_data; }, 0, 1);
    std::string expected_iface_name = "Mock";
    auto res = iface.write_string_descriptor(
      [&expected_iface_name](scatter_span<hal::byte const> p_data) {
        std::string_view sv(reinterpret_cast<char const*>(p_data[1].data()),
                            expected_iface_name.length());

        // Test return string
        expect(that % expected_iface_name == sv);
      },
      1);

    // Test that the method succeeded
    expect(that % true == res);
  };

  "usb_interface get_status"_test = [iface]() mutable {
    std::ignore = iface.write_descriptors(
      [](scatter_span<hal::byte const> p_data) { std::ignore = p_data; }, 0, 1);
    req_bitmap iface_bm(
      true, req_bitmap::type::standard, req_bitmap::recipient::interface);

    auto actual_res_iface = iface.get_status(iface_bm, 0);

    expect(that % 0 == actual_res_iface);

    req_bitmap ep_bm(
      true, req_bitmap::type::standard, req_bitmap::recipient::endpoint);

    auto actual_res_ep = iface.get_status(ep_bm, 1);

    expect(that % 1 == actual_res_ep);
  };

  "usb_interface manage alt_settings"_test = [iface]() mutable {
    iface.set_interface(1, 0);

    auto actual_alt_setting = iface.get_interface(0);

    expect(that % 1 == actual_alt_setting);
  };

  "usb_interface handle_request"_test = [iface]() mutable {
    req_bitmap bm = req_bitmap(
      true, req_bitmap::type::standard, req_bitmap::recipient::interface);

    u8 req = 0x01;
    u16 value = 0x0203;
    u16 index = 0x0405;
    u16 length = 0x0607;

    auto value_bytes = to_le_bytes(value);
    auto index_bytes = to_le_bytes(index);
    auto length_bytes = to_le_bytes(length);

    std::array<byte const, 8> expected_payload{
      bm.m_bitmap,     req,
      value_bytes[0],  value_bytes[1],
      index_bytes[0],  index_bytes[1],
      length_bytes[0], length_bytes[1]
    };
    auto actual_res = iface.handle_request(
      bm,
      req,
      value,
      index,
      length,
      [&expected_payload](scatter_span<hal::byte const> p_data) {
        auto a = make_scatter_bytes(expected_payload);
        scatter_span<byte const> expected_ss(a);

        expect(that % (expected_ss == p_data));
      });

    expect(that % true == actual_res);
  };

  "usb_interface manage_features"_test = [iface]() mutable {
    req_bitmap ep_bm(
      true, req_bitmap::type::standard, req_bitmap::recipient::endpoint);

    iface.manage_features(ep_bm, false, 1, 0);
    expect(that % false == iface.m_generic_ep.info().stalled);

    req_bitmap iface_bm(
      true, req_bitmap::type::standard, req_bitmap::recipient::interface);

    iface.manage_features(iface_bm, true, 0, 0);

    expect(that % true == iface.m_test_feature);

    iface.manage_features(iface_bm, true, 1, 0);

    expect(that % true == iface.m_another_test_feature);
  };
};

boost::ut::suite<"usb_interfacem multi interface test"> usb_multi_iface_test =
  []() {
    using namespace boost::ut;
    using descriptor_delta = usb_interface::descriptor_delta;
    using req_bitmap = usb_interface::req_bitmap;

    std::string name_one = "First interface";
    std::string name_two = "Second Interface";
    mock_multi_interface iface(name_one, name_two);

    "usb_interface write descriptor test"_test = [iface]() mutable {
      std::array<byte const, 8> expected_iad_buf = {
        iad_length, iad_type,
        0,  // first interface
        2,  // iface count
        0,  // class
        0,  // subclass
        0,  // proto
        1   // string idx
      };

      std::array<byte const, 9> expected_iface_one_desc = {
        iface_desc_length,
        iface_desc_type,
        0,  // iface number
        0,  // alt settings
        1,  // num endpoints
        0,  // class
        0,  // subclass
        0,  // protocol
        2,  // string idx
      };

      std::array<byte const, 9> expected_iface_two_desc = {
        iface_desc_length,
        iface_desc_type,
        1,  // iface number
        0,  // alt settings
        1,  // num endpoints
        0,  // class
        0,  // subclass
        0,  // protocol
        3,  // string idx
      };

      auto expected_payload = make_scatter_bytes(
        expected_iad_buf, expected_iface_one_desc, expected_iface_two_desc);

      scatter_span<byte const> expected_s(expected_payload);

      auto delta = iface.write_descriptors(
        [&expected_s](scatter_span<hal::byte const> p_data) {
          expect(that % (p_data == expected_s));
        },
        0,
        1);
      std::ignore = delta;
      expect(that % bool(delta == descriptor_delta{ 2, 3 }));
    };

    "usb_interface write string descriptor test"_test = [iface,
                                                         &name_one,
                                                         &name_two]() mutable {
      auto delta = iface.write_descriptors(
        [](scatter_span<hal::byte const> p_data) { std::ignore = p_data; },
        0,
        1);

      expect(that % delta.str_idxes == 3);
      auto first_str_idx = 2;
      auto second_str_idx = 3;
      auto first_res = iface.write_string_descriptor(
        [&name_one](scatter_span<hal::byte const> p_data) {
          std::string_view s(reinterpret_cast<char const*>(p_data[1].data()));
          expect(that % (name_one == s));
        },
        first_str_idx);

      expect(that % true == first_res);

      auto second_res = iface.write_string_descriptor(
        [&name_two](scatter_span<hal::byte const> p_data) {
          std::string_view s(reinterpret_cast<char const*>(p_data[1].data()));

          expect(that % name_two == s);
        },
        second_str_idx);

      expect(that % true == second_res);

      auto failed_res = iface.write_string_descriptor(
        [](scatter_span<hal::byte const> p_data) { std::ignore = p_data; }, 4);

      expect(that % false == failed_res);
    };

    "usb_interface get_status"_test = [iface]() mutable {
      std::ignore = iface.write_descriptors(
        [](scatter_span<hal::byte const> p_data) { std::ignore = p_data; },
        0,
        1);

      req_bitmap bm_one(
        true, req_bitmap::type::standard, req_bitmap::recipient::interface);
      auto actual_one = iface.get_status(bm_one, 0);
      expect(that % 0 == actual_one);

      req_bitmap bm_two(
        true, req_bitmap::type::standard, req_bitmap::recipient::interface);
      auto actual_two = iface.get_status(bm_two, 1);
      expect(that % 1 == actual_two);
    };

    "usb_interface manage manage_features"_test = [iface]() mutable {
      std::ignore = iface.write_descriptors(
        [](scatter_span<hal::byte const> p_data) { std::ignore = p_data; },
        0,
        1);

      req_bitmap bm_one(
        true, req_bitmap::type::standard, req_bitmap::recipient::interface);

      iface.manage_features(bm_one, true, 0, 0);
      expect(that % true == iface.m_iface_one.feature);

      req_bitmap bm_two(
        true, req_bitmap::type::standard, req_bitmap::recipient::interface);
      iface.manage_features(bm_two, true, 0, 1);

      expect(that % true == iface.m_iface_two.feature);
    };

    "usb_interface alt_settings"_test = [iface]() mutable {
      std::ignore = iface.write_descriptors(
        [](scatter_span<hal::byte const> p_data) { std::ignore = p_data; },
        0,
        1);

      u8 const expected_alt_one = 32;
      iface.set_interface(expected_alt_one, 0);
      auto alt_one = iface.get_interface(0);
      expect(that % expected_alt_one == alt_one);

      u8 const expected_alt_two = 64;
      iface.set_interface(expected_alt_two, 1);
      auto alt_two = iface.get_interface(1);
      expect(that % expected_alt_two == alt_two);
    };
  };
}  // namespace hal::v5::usb
