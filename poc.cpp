#pragma leco tool
import fork;
import silog;
import yoyo;

void fail(const char *msg) {
  silog::log(silog::error, "Error: %s", msg);
  throw 0;
}

void create_file() {
  yoyo::file_writer w{"out/test.dat"};
  auto root = frk::push('MyDt', &w);
  for (auto i = 0; i < 10; i++) {
    auto d = frk::push('Chnk', &w);
    w.write_u32(i * 3).take(fail);
  }
}

void read_file() {
  auto r = yoyo::file_reader::open("out/test.dat").take(fail);
  while (!r.eof().take(fail)) {
    auto [fourcc, data] = frk::read(&r);
    auto n = data.read_u32().take(fail);
    silog::log(silog::info, "FourCC: %08X - Data: %d", fourcc, n);
  }
}

int main() {
  create_file();
  read_file();
}
