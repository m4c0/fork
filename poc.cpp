#pragma leco tool
import fork;
import jute;
import traits;
import yoyo;

using namespace traits::ints;

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
