#pragma leco tool
import fork;
import silog;
import yoyo;

void fail(const char *msg) {
  silog::log(silog::error, "Error: %s", msg);
  throw 0;
}

auto write_chunk(yoyo::writer *w, unsigned i) {
  return frk::push('Chnk', w, i);
}
void create_file() {
  yoyo::file_writer::open("out/test.dat")
      .fmap([](auto &&w) {
        return frk::push('MyDt', &w, [&](auto) {
          return write_chunk(&w, 10)
              .fmap([&] { return write_chunk(&w, 30); })
              .fmap([&] { return write_chunk(&w, 60); });
        });
      })
      .take(fail);
}

auto read_chunk(frk::pair p) {
  auto [fourcc, data] = p;
  return data.read_u32().map([&](auto n) {
    silog::log(silog::info, "FourCC: %08X - Data: %d", fourcc, n);
  });
}
mno::req<void> read_list(frk::pair p) {
  auto [fourcc, data] = p;
  return frk::read_list(&data, read_chunk);
}
void read_file() {
  auto r = yoyo::file_reader::open("out/test.dat").take(fail);
  frk::find('MyDt', &r)
      .fmap([](auto r) { return frk::read_list(&r, read_chunk); })
      .take(fail);
}

int main() {
  create_file();
  read_file();
}
