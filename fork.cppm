export module fork;
import hai;
import jute;
import missingno;
import traits;
import yoyo;

using namespace traits::ints;

namespace frk {
export constexpr auto signature(const char (&id)[4]) {
  unsigned char s[]{0x89, 0, 0, 0, 0x0D, 0x0A, 0x1A, 0x0A};
  for (auto i = 0; i < 3; i++) {
    s[i + 1] = id[i];
  }

  return [s](auto &&w) {
    return w.write(s, sizeof(s)).map([&] { return traits::move(w); });
  };
}
export constexpr auto assert(const char (&id)[4]) {
  unsigned char s[]{0x89, 0, 0, 0, 0x0D, 0x0A, 0x1A, 0x0A};
  for (auto i = 0; i < 3; i++) {
    s[i + 1] = id[i];
  }

  return [s](auto &&r) {
    return r.read_u64()
        .assert(
            [s](uint64_t n) {
              return n == *reinterpret_cast<const uint64_t *>(s);
            },
            "Mismatched signature")
        .fmap([&](auto) { return mno::req{traits::move(r)}; });
  };
}

/// Utility function to "end" the chain of writer moves
export constexpr auto end() {
  return [](auto &&) {};
}

/// Utility to reset a stream back to the point after the file signature
export constexpr auto reset() {
  return [](auto &&w) {
    return w.seekg(8, yoyo::seek_mode::set).fmap([&] {
      return mno::req{traits::move(w)};
    });
  };
}

constexpr const auto crc_table = [] {
  struct {
    uint32_t data[256]{};
  } res;

  for (auto n = 0; n < 256; n++) {
    uint32_t c = n;
    for (auto k = 0; k < 8; k++) {
      if (c & 1)
        c = 0xedb88320U ^ (c >> 1);
      else
        c >>= 1;
    }
    res.data[n] = c;
  }

  return res;
}();
inline constexpr uint32_t crc_byte(uint32_t c, uint8_t b) {
  auto idx = (c ^ b) & 0xff;
  return crc_table.data[idx] ^ (c >> 8);
}
inline constexpr auto crc(jute::view fourcc, const uint8_t *buf, unsigned len) {
  uint32_t c = ~0U;
  for (auto b : fourcc) {
    c = crc_byte(c, b);
  }
  for (auto n = 0; n < len; n++) {
    c = crc_byte(c, buf[n]);
  }
  return c ^ ~0U;
}

export constexpr const auto critical(jute::view fourcc) {
  return (fourcc[0] & 0x20) == 0;
}
export constexpr const auto safe_to_copy(jute::view fourcc) {
  return (fourcc[3] & 0x20) != 0;
}

inline auto chunk(auto &&w, jute::view fourcc, const void *data,
                  unsigned size) {
  auto crc = frk::crc(fourcc, static_cast<const uint8_t *>(data), size);
  return w.write_u32_be(size)
      .fmap([&] { return w.write(fourcc.data(), fourcc.size()); })
      .fmap([&] { return size == 0 ? mno::req<void>{} : w.write(data, size); })
      .fmap([&] { return w.write_u32_be(crc); })
      .map([&] { return traits::move(w); });
}
export template <typename T>
constexpr auto chunk(const char (&fourcc)[5], T data) {
  return [=](auto &&w) {
    return chunk(traits::move(w), fourcc, &data, sizeof(T));
  };
}
export constexpr auto chunk(const char (&fourcc)[5], const void *data,
                            unsigned size) {
  return [=](auto &&w) { return chunk(traits::move(w), fourcc, data, size); };
}
export constexpr auto chunk(const char (&fourcc)[5]) {
  return [=](auto &&w) { return chunk(traits::move(w), fourcc, nullptr, 0); };
}

export enum class scan_action { take, peek, stop };
export struct scan_result {
  using t = mno::req<scan_action>;
  /// Moves to the next and continue
  static constexpr const auto take = mno::req{scan_action::take};
  /// Moves to the next and stop
  static constexpr const auto stop = mno::req{scan_action::stop};
  /// Moves to the previous and stop
  static constexpr const auto peek = mno::req{scan_action::peek};
};
inline auto scan_once(auto &r, auto &fn) {
  yoyo::subreader in{};
  uint32_t len{};
  char buf[5]{};
  return r.read_u32_be()
      .trace("reading length of chunk")
      .map([&](auto l) { len = l; })
      .fmap([&] { return r.read(buf, 4).trace("reading fourcc"); })
      .fmap([&] { return yoyo::subreader::create(&r, len); })
      .fmap([&](auto sub) {
        in = sub;
        return r.seekg(len, yoyo::seek_mode::current);
      })
      .fmap([&] { return r.read_u32_be().trace("reading CRC"); })
      .map([&](auto crc) { /* TODO: check crc */ })
      .fmap([&] { return in.seekg(0, yoyo::seek_mode::set); })
      .fmap([&] { return fn(jute::view{buf}, in); })
      .fmap([&](scan_action res) {
        int pos = res == scan_action::peek ? -8 : len + 4;
        return in.seekg(0, yoyo::seek_mode::set)
            .fmap([&] { return r.seekg(pos, yoyo::seek_mode::current); })
            .map([&] { return res == scan_action::take; });
      });
}
auto run_scan(auto &r, auto &fn) {
  mno::req<bool> cont{true};
  while (cont.is_valid() && cont.unwrap(false)) {
    cont = scan_once(r, fn);
  }
  return cont.map([](auto) { return true; }).if_failed([&](auto msg) {
    return r.eof().unwrap(false) ? mno::req{false}
                                 : mno::req<bool>::failed(msg);
  });
}
export constexpr auto scan(traits::is_callable_r<scan_result::t, jute::view,
                                                 yoyo::subreader> auto &&fn) {
  return [&](auto &&r) {
    return run_scan(r, fn)
        .fmap([&](auto) { return mno::req{traits::move(r)}; })
        .trace("scanning file");
  };
}

export constexpr auto take(const char (&fourcc)[5],
                           traits::is_callable<yoyo::subreader> auto &&fn) {
  return [&](auto &&r) {
    const auto scanner = [&](auto fcc, auto rdr) {
      if (fourcc == fcc)
        return fn(rdr).map([] { return scan_action::stop; });
      if (critical(fcc) && !critical(fourcc))
        return scan_result::peek;
      if (critical(fcc))
        return scan_result::t::failed("critical chunk " + fcc + " skipped");
      return scan_result::take;
    };
    return run_scan(r, scanner)
        .fmap([&](auto found) {
          if (!found && critical(fourcc))
            return mno::req<bool>::failed("missing critical chunk");
          return mno::req{found};
        })
        .fmap([&](auto found) { return mno::req{traits::move(r)}; })
        .trace("expecting " + jute::view{fourcc});
  };
}

export constexpr auto take(const char (&fourcc)[5]) {
  return take(fourcc, [&](auto) { return mno::req<void>{}; });
}
export template <typename T>
constexpr auto take(const char (&fourcc)[5], traits::is_callable<T> auto &&fn) {
  return take(fourcc, [&](auto rdr) {
    T data{};
    return rdr.read(&data, sizeof(data)).fmap([&] { return fn(data); });
  });
}

export constexpr auto take_all(const char (&fourcc)[5],
                               traits::is_callable<yoyo::subreader> auto &&fn) {
  return [&](auto &&r) {
    bool got_it = false;
    const auto scanner = [&](auto fcc, auto rdr) {
      if (fourcc == fcc) {
        got_it = true;
        return fn(rdr).map([] { return scan_action::take; });
      }
      if (critical(fcc) && (!critical(fourcc) || got_it))
        return scan_result::peek;
      if (critical(fcc))
        return scan_result::t::failed("critical chunk " + fcc + " skipped");
      return scan_result::take;
    };
    return run_scan(r, scanner)
        .fmap([&](auto found) {
          if (!found && critical(fourcc))
            return mno::req<bool>::failed("missing critical chunk");
          return mno::req{found};
        })
        .fmap([&](auto found) { return mno::req{traits::move(r)}; })
        .trace("expecting " + jute::view{fourcc});
  };
}
export constexpr auto take_all(const char (&fourcc)[5]) {
  return take_all(fourcc, [&](auto) { return mno::req<void>{}; });
}
export template <typename T>
constexpr auto take_all(const char (&fourcc)[5],
                        traits::is_callable<T> auto &&fn) {
  return take_all(fourcc, [&](auto rdr) {
    T data{};
    return rdr.read(&data, sizeof(data)).fmap([&] { return fn(data); });
  });
}

} // namespace frk

namespace frk::copy {
export auto start(const char (&id)[4], const char *file) {
  return yoyo::file_writer::open(file).fmap(frk::signature(id)).map(frk::end());
}
export constexpr auto chunk(const char (&fourcc)[5], const char *file) {
  return frk::take(fourcc, [=](yoyo::subreader r) {
    if (r.raw_size() == 0) {
      return yoyo::file_writer::append(file)
          .fmap(frk::chunk(fourcc))
          .map(frk::end());
    }
    hai::array<char> data{static_cast<unsigned>(r.raw_size())};
    return r.read(data.begin(), data.size())
        .fmap([&] { return yoyo::file_writer::append(file); })
        .fmap(frk::chunk(fourcc, data.begin(), data.size()))
        .map(frk::end())
        .trace(jute::heap{} + "copying chunk " + fourcc + " into " +
               jute::view::unsafe(file));
  });
}
} // namespace frk::copy
