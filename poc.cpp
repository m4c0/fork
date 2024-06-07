#pragma leco tool
import fork;
import silog;
import yoyo;

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
      .log_error([] { throw 0; });
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
  auto r = yoyo::file_reader::open("out/test.dat").log_error([] { throw 0; });
  frk::find('MyDt', &r)
      .fmap([](auto r) { return frk::read_list(&r, read_chunk); })
      .log_error([] { throw 0; });
}

int main() {
  create_file();
  read_file();
}
