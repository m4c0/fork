export module fork;
import traits;
import yoyo;

using namespace traits::ints;

export namespace frk {
using fourcc_t = uint32_t;

struct pair {
  fourcc_t fourcc;
  yoyo::subreader data;
};
mno::req<pair> read(yoyo::reader *r) {
  pair res{};
  return r->read_u32()
      .map([&](auto fourcc) { res.fourcc = fourcc; })

      .fmap([&] { return r->read_u32(); })
      .fmap([&](auto len) { return yoyo::subreader::create(r, len); })

      .map([&](auto data) {
        res.data = data;
        return res;
      });
}

[[nodiscard]] mno::req<void> push(fourcc_t fourcc, yoyo::writer *w, auto &&fn) {
  uint32_t start{};
  uint32_t end{};
  return w->write_u32(fourcc)
      .fmap([w] { return w->write_u32(0); })

      .fmap([w] { return w->tellp(); })
      .map([&](auto s) { start = s; })

      .fmap([&] { return fn(w); })

      .fmap([w] { return w->tellp(); })
      .map([&](auto e) { end = e; })
      .fmap([&] { return w->seekp(start - 4); })

      .fmap([&] { return w->write_u32(end - start); })
      .fmap([&] { return w->seekp(end); });
}
} // namespace frk

module :private;
