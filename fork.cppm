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
pair read(yoyo::reader *r);

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
      .fmap([&] { return w->seekp(start); })

      .fmap([&] { return w->write_u32(end - start); })
      .fmap([&] { return w->seekp(end); });
}
} // namespace frk

module :private;
