#pragma leco tool
import fork;
import silog;
import yoyo;

void fail(const char *msg) {
  silog::log(silog::error, "Error: %s", msg);
  throw 0;
}

auto write_chunk(yoyo::writer *w, unsigned i) {
  return frk::push('Chnk', w, [&](auto) { return w->write_u32(i); });
}
void create_file() {
  yoyo::file_writer w{"out/test.dat"};
  frk::push('MyDt', &w, [&](auto) {
    return write_chunk(&w, 10)
        .fmap([&] { return write_chunk(&w, 30); })
        .fmap([&] { return write_chunk(&w, 60); });
  }).take(fail);
}

auto read_chunk(frk::pair p) {
  auto [fourcc, data] = p;
  return data.read_u32().map([&](auto n) {
    silog::log(silog::info, "FourCC: %08X - Data: %d", fourcc, n);
  });
}
void read_file() {
  auto r = yoyo::file_reader::open("out/test.dat").take(fail);
  while (!r.eof().take(fail)) {
    frk::read(&r).fmap(read_chunk).take(fail);
  }
}

int main() {
  create_file();
  read_file();
}
