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

inline auto chunk(auto &&w, jute::view fourcc, const void *data,
                  unsigned size) {
  return w.write_u32_be(size)
      .fmap([&] { return w.write(fourcc.data(), fourcc.size()); })
      .fmap([&] { return size == 0 ? mno::req<void>{} : w.write(data, size); })
      .fmap([&] { return w.write_u32_be(0); })
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
  uint32_t width{0x01000000};  // BE
  uint32_t height{0x01000000}; // BE
  uint8_t bit_depth{8};
  uint8_t colour_type{0}; // Grayscale no-alpha
  uint8_t compression{0}; // Deflate
  uint8_t filter{0};      // Adaptative
  uint8_t interlace{0};   // No interlace
};

static void create_file() {
  yoyo::file_writer::open("out/test.png")
      .fmap(frk::signature("PNG"))
      .fmap(frk::chunk("IHDR", ihdr{}))
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
