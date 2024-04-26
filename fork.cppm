export module fork;
import no;
import traits;
import yoyo;

using fourcc_t = traits::ints::uint32_t;

export namespace frk {
struct pair {
  fourcc_t fourcc;
  yoyo::subreader data;
};
pair read(yoyo::reader *r);

class wguard : public no::no {
public:
  ~wguard();
};
wguard push(fourcc_t fourcc, yoyo::writer *w);

} // namespace frk
