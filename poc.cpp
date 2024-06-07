#pragma leco tool
import fork;
import jute;
import silog;
import yoyo;

namespace frk {
constexpr auto signature(const char (&id)[4]) {
  unsigned char s[]{0x89, 0, 0, 0, 0x0D, 0x0A, 0x1A, 0x0A};
  for (auto i = 0; i < 3; i++) {
    s[i + 1] = id[i];
  }

  return [s](auto &&w) { return w.write(s, sizeof(s)); };
}
} // namespace frk

static void create_file() {
  yoyo::file_writer::open("out/test.png")
      .fmap(frk::signature("PNG"))
      .log_error([] { throw 0; });
}

int main() try {
  create_file();
  return 0;
} catch (...) {
  return 1;
}
