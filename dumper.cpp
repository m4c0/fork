// #pragma leco tool
#define GOPT_IMPLEMENTATION
#include "../gopt/gopt.h"

import fork;
import hai;
import silog;
import yoyo;

static int usage() {
  silog::log(silog::error, "usage: dumper.exe -i <file> [-r <fourcc>...]");
  return 1;
}

static hai::varray<unsigned> fourcc_recurses{1024};

static mno::req<void> dump_item(frk::pair p, unsigned ind) {
  auto [fourcc, data] = p;
  const char *fccc = reinterpret_cast<char *>(&fourcc);

  return data.size().fmap([&](auto size) {
    silog::log(silog::info, "%*sfound %.4s with %ld bytes", ind, "", fccc,
               size);
    for (auto fcr : fourcc_recurses) {
      if (fcr == fourcc) {
        return frk::read_list(&data,
                              [&](auto p) { return dump_item(p, ind + 2); });
      }
    }
    return mno::req<void>{};
  });
}

int main(int argc, char **argv) {
  gopt opts;
  GOPT(opts, argc, argv, "i:r:");

  const char *input{};

  char *val{};
  char ch;
  while (0 != (ch = gopt_parse(&opts, &val))) {
    switch (ch) {
    case 'i':
      input = val;
      break;
    case 'r': {
      unsigned fourcc = *reinterpret_cast<unsigned *>(val);
      fourcc_recurses.push_back_doubling(fourcc);
      break;
    }
    default:
      return usage();
    }
  }

  if (input == nullptr)
    return usage();

  yoyo::file_reader::open(input)
      .fmap([](auto &&r) {
        return frk::read_list(&r, [&](auto p) { return dump_item(p, 0); });
      })
      .log_error();
}
