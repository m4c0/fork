#pragma leco tool
import fork;
import jute;
import silog;
import traits;
import yoyo;

using namespace traits::ints;

struct ihdr {
  uint8_t data[13];

  static auto filled() {
    return ihdr{{
        0, 0, 0, 1, // Width
        0, 0, 0, 1, // Height
        1,          // Bit Depth
        0,          // Grayscale no-alpha
        0,          // Deflate
        0,          // No filter
        0,          // No interlace
    }};
  }
};
static_assert(sizeof(ihdr) == 13);

struct idat {
  uint8_t data[10];

  static auto filled() {
    // Single black pixel compressed with zlib/deflate
    return idat{{0x78, 1, 0x63, 0x60, 0, 0, 0, 2, 0, 1}};
  }
};

static mno::req<void> do_something_with_ihdr(ihdr h) {
  silog::log(silog::debug, "found IHDR of %dx%d", h.data[3], h.data[7]);
  return {};
}
static mno::req<void> do_something_with_idat(idat h) {
  silog::log(silog::debug, "found IDAT with 3rd byte 0x%x", h.data[2]);
  return {};
}
static mno::req<void> do_something_with_splt(yoyo::subreader r) {
  silog::log(silog::debug, "found sPLT with size %ld", r.raw_size());
  return {};
}
static frk::scan_result::t do_something_with_chunk(jute::view fourcc,
                                                   yoyo::subreader data) {
  silog::log(silog::debug, "scanned %.*s with size %d",
             static_cast<int>(fourcc.size()), fourcc.data(),
             static_cast<int>(data.size().unwrap(0)));
  return fourcc == "IEND" ? frk::scan_result::peek : frk::scan_result::take;
}

static void create_file() {
  yoyo::file_writer::open("out/test.png")
      .fmap(frk::signature("PNG"))
      .fmap(frk::chunk("mehh"))
      .fmap(frk::chunk("IHDR", ihdr::filled()))
      .fmap(frk::chunk("IDAT", idat::filled()))
      .fmap(frk::chunk("sPLT", "test\0\x8", 6))
      .fmap(frk::chunk("IEND"))
      .map(frk::end())
      .trace("creating file")
      .log_error([] { throw 0; });
}

static void read_file_in_sequence() {
  yoyo::file_reader::open("out/test.png")
      .fmap(frk::assert("PNG"))
      .fmap(frk::take<ihdr>("IHDR", do_something_with_ihdr))
      .fmap(frk::take<idat>("IDAT", do_something_with_idat))
      .fmap(frk::take("sPLT", do_something_with_splt))
      .fmap(frk::take("noop")) // safe to ignore if non-existent
      .fmap(frk::take("IEND"))
      .map(frk::end())
      .trace("reading file in sequence")
      .log_error();
}

static void scan_file() {
  yoyo::file_reader::open("out/test.png")
      .fmap(frk::assert("PNG"))
      .fmap(frk::take<ihdr>("IHDR", do_something_with_ihdr))
      .fmap(frk::scan(do_something_with_chunk))
      .fmap(frk::take("IEND"))
      .map(frk::end())
      .trace("scanning file")
      .log_error();
}

int main() try {
  create_file();
  read_file_in_sequence();
  scan_file();
  return 0;
} catch (...) {
  return 1;
}
