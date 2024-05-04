#pragma leco tool
#define GOPT_IMPLEMENTATION
#include "../gopt/gopt.h"

import fork;
import silog;
import yoyo;

static int usage() {
  silog::log(silog::error, "usage: dumper.exe -i <file>");
  return 1;
}

static mno::req<void> dump_item(frk::pair p) {
  auto [fourcc, data] = p;
  const char *fccc = reinterpret_cast<char *>(&fourcc);

  return data.size().map([&](auto size) {
    silog::log(silog::info, "found %.4s with %d bytes", fccc, size);
  });
}

int main(int argc, char **argv) {
  gopt opts;
  GOPT(opts, argc, argv, "i:");

  const char *input{};

  char *val{};
  char ch;
  while (0 != (ch = gopt_parse(&opts, &val))) {
    switch (ch) {
    case 'i':
      input = val;
      break;
    default:
      return usage();
    }
  }

  if (input == nullptr)
    return usage();

  yoyo::file_reader::open(input)
      .fmap([](auto &&r) { return read_list(&r, dump_item); })
      .take([](auto err) {
        silog::log(silog::error, "error reading file: %s", err);
      });
}
