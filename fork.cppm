export module fork;
import jute;
import missingno;
import traits;

using namespace traits::ints;

namespace frk {
export constexpr auto signature(const char (&id)[4]) {
  unsigned char s[]{0x89, 0, 0, 0, 0x0D, 0x0A, 0x1A, 0x0A};
  for (auto i = 0; i < 3; i++) {
    s[i + 1] = id[i];
  }

  return [s](auto &&w) {
    return w.write(s, sizeof(s)).map([&] { return traits::move(w); });
  };
}

constexpr const auto crc_table = [] {
  struct {
    uint32_t data[256]{};
  } res;

  for (auto n = 0; n < 256; n++) {
    uint32_t c = n;
    for (auto k = 0; k < 8; k++) {
      if (c & 1)
        c = 0xedb88320U ^ (c >> 1);
      else
        c >>= 1;
    }
    res.data[n] = c;
  }

  return res;
}();
inline constexpr uint32_t crc_byte(uint32_t c, uint8_t b) {
  auto idx = (c ^ b) & 0xff;
  return crc_table.data[idx] ^ (c >> 8);
}
inline constexpr auto crc(jute::view fourcc, const uint8_t *buf, unsigned len) {
  uint32_t c = ~0U;
  for (auto b : fourcc) {
    c = crc_byte(c, b);
  }
  for (auto n = 0; n < len; n++) {
    c = crc_byte(c, buf[n]);
  }
  return c ^ ~0U;
}

inline auto chunk(auto &&w, jute::view fourcc, const void *data,
                  unsigned size) {
  auto crc = frk::crc(fourcc, static_cast<const uint8_t *>(data), size);
  return w.write_u32_be(size)
      .fmap([&] { return w.write(fourcc.data(), fourcc.size()); })
      .fmap([&] { return size == 0 ? mno::req<void>{} : w.write(data, size); })
      .fmap([&] { return w.write_u32_be(crc); })
      .map([&] { return traits::move(w); });
}
export template <typename T>
constexpr auto chunk(const char (&fourcc)[5], T data) {
  return [=](auto &&w) {
    return chunk(traits::move(w), fourcc, &data, sizeof(T));
  };
}
export constexpr auto chunk(const char (&fourcc)[5]) {
  return [=](auto &&w) { return chunk(traits::move(w), fourcc, nullptr, 0); };
}
} // namespace frk
