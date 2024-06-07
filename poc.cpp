#pragma leco tool
import fork;
import jute;
import traits;
import yoyo;

using namespace traits::ints;

namespace frk {
constexpr auto signature(const char (&id)[4]) {
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
template <typename T> constexpr auto chunk(const char (&fourcc)[5], T data) {
  return [=](auto &&w) {
    return chunk(traits::move(w), fourcc, &data, sizeof(T));
  };
}
constexpr auto chunk(const char (&fourcc)[5]) {
  return [=](auto &&w) { return chunk(traits::move(w), fourcc, nullptr, 0); };
}
} // namespace frk

struct ihdr {
  uint8_t data[13]{
      0, 0, 0, 1, // Width
      0, 0, 0, 1, // Height
      1,          // Bit Depth
      0,          // Grayscale no-alpha
      0,          // Deflate
      0,          // No filter
      0,          // No interlace
  };
};
static_assert(sizeof(ihdr) == 13);

struct idat {
  // Single black pixel compressed with zlib/deflate
  uint8_t data[10]{0x78, 1, 0x63, 0x60, 0, 0, 0, 2, 0, 1};
};

static void create_file() {
  yoyo::file_writer::open("out/test.png")
      .fmap(frk::signature("PNG"))
      .fmap(frk::chunk("IHDR", ihdr{}))
      .fmap(frk::chunk("IDAT", idat{}))
      .fmap(frk::chunk("IEND"))
      .map([](auto &&) {})
      .log_error([] { throw 0; });
}

int main() try {
  create_file();
  return 0;
} catch (...) {
  return 1;
}
