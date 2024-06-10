export module fork;
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
export constexpr auto chunk(const char (&fourcc)[5]) {
  return [=](auto &&w) { return chunk(traits::move(w), fourcc, nullptr, 0); };
}

inline auto take(auto &r, jute::view fourcc, void *data, unsigned size) {
  uint32_t len{};
  char buf[5]{};
  bool found{};
  return r.read_u32_be()
      .map([&](auto l) { len = l; })
      .fmap([&] { return r.read(buf, 4); })
      .map([&] { found = fourcc == buf; })
      .fmap([&] {
        if (!found || data == nullptr)
          return r.seekg(len, yoyo::seek_mode::current);

        return r.read(data, size);
      })
      .fmap([&] { return r.read_u32_be(); })
      .map([&](auto crc) {
        /* TODO: check crc */
        return found;
      });
}
export template <typename T>
constexpr auto take(const char (&fourcc)[5], traits::is_callable<T> auto &&fn) {
  return [&](auto &&r) {
    T data{};
    return take(r, fourcc, &data, sizeof(T))
        .assert([&](auto found) { return found || (fourcc[0] & 0x10) != 0; },
                "missing critical chunk")
        .fmap([&](auto found) {
          if (found)
            fn(data);
          return mno::req{traits::move(r)};
        });
  };
}
export constexpr auto take(const char (&fourcc)[5]) {
  return [&](auto &&r) {
    return take(r, fourcc, nullptr, 0).fmap([&](auto found) {
      return mno::req{traits::move(r)};
    });
  };
}

inline auto read(auto &r, auto &fn) {
  yoyo::subreader in{};
  uint32_t len{};
  char buf[5]{};
  return r.read_u32_be()
      .map([&](auto l) { len = l; })
      .fmap([&] { return r.read(buf, 4); })
      .fmap([&] { return yoyo::subreader::create(&r, len); })
      .fmap([&](auto sub) {
        in = sub;
        return r.seekg(len, yoyo::seek_mode::current);
      })
      .fmap([&] { return r.read_u32_be(); })
      .map([&](auto crc) { /* TODO: check crc */ })
      .fmap([&] { return in.seekg(0, yoyo::seek_mode::set); })
      .map([&] { return fn(buf, in); })
      .fmap([&](auto res) {
        return in.seekg(0, yoyo::seek_mode::end)
            .fmap([&] { return r.seekg(4, yoyo::seek_mode::current); })
            .map([&] { return res; });
      });
}
export constexpr auto
scan(traits::is_callable<jute::view, yoyo::subreader> auto &&fn) {
  return [&](auto &&r) {
    mno::req<bool> res{true};
    while (res.is_valid() && res.unwrap(false)) {
      res = read(r, fn);
    }
    return res
        .if_failed([&](auto msg) {
          return r.eof().unwrap(false) ? mno::req{false}
                                       : mno::req<bool>::failed(msg);
        })
        .fmap([&](auto) { return mno::req{traits::move(r)}; });
  };
}

// TODO: "find": searches for a fourcc, fails if skipping a critical chunk
} // namespace frk
