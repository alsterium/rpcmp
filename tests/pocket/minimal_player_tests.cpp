#include "hybrid_playback.hpp"
#include "rpcmp/platform/pocket/bitmap_font.hpp"
#include "rpcmp/platform/pocket/minimal_display.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <vector>

namespace {
namespace api = rpcmp::contracts::minimal;
namespace lib = rpcmp::library;
namespace pocket = rpcmp::platform::pocket;
namespace ui = rpcmp::ui::minimal;
using Bytes = std::vector<std::uint8_t>;
void word(Bytes& b, std::size_t at, std::uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    b.at(at + i) = static_cast<std::uint8_t>(v >> (i * 8U));
}
void seal(Bytes& data) {
  const auto index_bytes = static_cast<std::size_t>(data[12]) * 128;
  word(data, 20, lib::crc32({data.data() + 32, index_bytes}));
  word(data, 24, lib::crc32({data.data(), 24}));
}
Bytes fixture() {
  // Independently authored wire fixture: two tracks, second has a PDX.
  Bytes b(32 + 256 + 36, 0);
  word(b, 0, 0x314c5048);
  word(b, 4, 1);
  word(b, 8, 128);
  word(b, 12, 2);
  word(b, 16, static_cast<std::uint32_t>(b.size()));
  const Bytes fm{0, 0, 255, 255, 0, 10, 0, 8, 0, 0, 11, 0};
  const Bytes mixed{0, 0, 0, 0, 0, 10, 0, 8, 0, 0, 22, 0};
  const Bytes pdx{0, 0, 0, 0, 0, 10, 0, 2, 0, 0, 33, 0};
  for (unsigned i = 0; i < 2; ++i) {
    const auto at = 32 + i * 128;
    const auto offset = 288 + i * 12;
    const auto& blob = i == 0 ? fm : mixed;
    word(b, at, offset);
    word(b, at + 4, 12);
    word(b, at + 8, lib::crc32({blob.data(), blob.size()}));
    std::copy(blob.begin(), blob.end(), b.begin() + offset);
    const std::string title = i == 0 ? "FM 試験曲" : "PCM 日本語";
    word(b, at + 24, static_cast<std::uint32_t>(title.size()));
    std::copy(title.begin(), title.end(), b.begin() + at + 28);
  }
  word(b, 160 + 12, 312);
  word(b, 160 + 16, 12);
  word(b, 160 + 20, lib::crc32({pdx.data(), pdx.size()}));
  std::copy(pdx.begin(), pdx.end(), b.begin() + 312);
  seal(b);
  return b;
}
class Source final : public lib::PlaylistSource {
public:
  Bytes data{fixture()};
  bool fail{};
  unsigned reads{};
  bool size(std::uint32_t& bytes) override {
    bytes = static_cast<std::uint32_t>(data.size());
    return !fail;
  }
  bool read(std::uint32_t offset, std::uint8_t* out, std::uint32_t size) override {
    ++reads;
    if (fail || offset > data.size() || size > data.size() - offset)
      return false;
    std::copy_n(data.data() + offset, size, out);
    return true;
  }
};
std::uint32_t tick{}, status{1}, fm_free{1024}, pcm_free{4096}, opened{}, rendered{};
bool reject_open{}, reject_render{}, end_block{}, empty_end{}, reset_stuck{};
std::vector<std::pair<unsigned, std::uint32_t>> writes;
RpcmpHybridBlock block;
void reset_model() {
  tick = opened = rendered = 0;
  status = 1;
  fm_free = 1024;
  pcm_free = 4096;
  reject_open = reject_render = end_block = empty_end = reset_stuck = false;
  writes.clear();
  block = {};
}
void catalog_cases(rpcmp::test::Suite& suite) {
  const std::string check = "123456789";
  RPCMP_CHECK(suite, lib::crc32({reinterpret_cast<const std::uint8_t*>(check.data()),
                                 check.size()}) == 0xcbf43926);
  Source source;
  auto catalog = std::make_unique<lib::PreparedPlaylist>();
  RPCMP_CHECK(suite, catalog->open(source) && catalog->count() == 2);
  RPCMP_CHECK(suite, source.reads == 2); // No music payload at startup.
  Bytes a(28, 0xee), b(28, 0xee);
  RPCMP_CHECK(suite, catalog->load({2}, {a.data(), a.size()}, {b.data(), b.size()}));
  RPCMP_CHECK(suite, a[10] == 22 && b[10] == 33);
  RPCMP_CHECK(suite, std::all_of(a.begin() + 12, a.end(), [](auto c) { return c == 0; }));
  RPCMP_CHECK(suite, !catalog->load({0}, {a.data(), a.size()}, {}));
  RPCMP_CHECK(suite, !catalog->load({2}, {a.data(), 12}, {b.data(), b.size()}));
  source.data[300] ^= 1;
  RPCMP_CHECK(suite, !catalog->load({2}, {a.data(), a.size()}, {b.data(), b.size()}));
  for (unsigned bad = 0; bad < 10; ++bad) {
    source.data = fixture();
    switch (bad) {
    case 0:
      word(source.data, 4, 2);
      break;
    case 1:
      word(source.data, 12, 301);
      break;
    case 2:
      word(source.data, 32, 16);
      break;
    case 3:
      word(source.data, 36, 0xffffffff);
      break;
    case 4:
      word(source.data, 32 + 16, 1);
      break;
    case 5:
      word(source.data, 32 + 24, 97);
      break;
    case 6:
      source.data[60] = 0xc0;
      break; // Overlong UTF-8.
    case 7:
      source.data[60] = 0;
      break;
    case 8:
      source.data[155] = 1;
      break; // Reserved word.
    case 9:
      source.data[120] = 1;
      break; // Title padding.
    default:
      break;
    }
    if (bad != 1)
      seal(source.data);
    else
      word(source.data, 24, lib::crc32({source.data.data(), 24}));
    RPCMP_CHECK(suite, !catalog->open(source) && catalog->count() == 0);
  }
  source.data = fixture();
  source.data[20] ^= 1;
  RPCMP_CHECK(suite, !catalog->open(source));
  source.data = fixture();
  source.fail = true;
  RPCMP_CHECK(suite, !catalog->open(source));
}
std::vector<std::pair<unsigned, std::uint32_t>> playback_case(rpcmp::test::Suite& suite,
                                                              bool draw) {
  reset_model();
  Source source;
  auto catalog = std::make_unique<lib::PreparedPlaylist>();
  RPCMP_CHECK(suite, catalog->open(source));
  pocket::HybridPlayback backend{*catalog};
  rpcmp::player::minimal::Player player{*catalog, backend};
  RPCMP_CHECK(suite, player.initialize());
  ui::Controller controller{*catalog, player};
  controller.input(0x10, true, 0);
  RPCMP_CHECK(suite, opened == 0 && player.snapshot().state == api::State::Stopped);
  controller.input(0, true, 1);
  controller.input(2, true, 2); // Browse to PCM.
  RPCMP_CHECK(suite, controller.view().selected == 1 && opened == 0);
  controller.input(0x10, true, 3);
  RPCMP_CHECK(suite, opened == 22 && player.snapshot().state == api::State::Playing);
  controller.update(player.snapshot());
  pocket::MinimalDisplay display;
  display.begin(controller.view());
  Bytes pixels(640 * 480 + 2, 0xee);
  for (unsigned i = 0; i < 180; ++i) {
    player.service();
    if (draw && i % 3 == 0)
      RPCMP_CHECK(suite, display.pump(pixels.data() + 1, pixels.size() - 2));
    if (i == 50) {
      controller.input(1, true, 4);
      RPCMP_CHECK(suite, player.snapshot().track.value == 2 && opened == 22);
    }
  }
  RPCMP_CHECK(suite, pixels.front() == 0xee && pixels.back() == 0xee);
  const auto trace = writes;
  controller.input(0x30, true, 5); // Simultaneous A/B: stop only.
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Stopped && opened == 22);
  controller.input(0, true, 6);
  controller.input(0x10, true, 7);
  RPCMP_CHECK(suite, opened == 11 && player.snapshot().track.value == 1);
  source.fail = true;
  player.submit({api::CommandKind::PlayTrack, {2}});
  RPCMP_CHECK(suite, player.snapshot().error == api::Error::Load && status == 1);
  source.fail = false;
  player.submit({api::CommandKind::PlayTrack, {2}});
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Playing);
  reject_render = true;
  player.service();
  RPCMP_CHECK(suite, player.snapshot().error == api::Error::Renderer && status == 1);
  reject_render = false;
  player.submit({api::CommandKind::PlayTrack, {1}});
  status = 0x11;
  player.service();
  RPCMP_CHECK(suite, player.snapshot().error == api::Error::Audio && status == 1);
  player.submit({api::CommandKind::PlayTrack, {1}});
  status = 9;
  player.service();
  RPCMP_CHECK(suite, status == 1);
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Playing &&
                         player.snapshot().track.value == 2 && opened == 22);
  controller.update(player.snapshot());
  RPCMP_CHECK(suite, controller.view().selected == 0 && controller.view().first == 0);
  status = 9;
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Ended && status == 1);
  const auto calls = rendered;
  player.service();
  RPCMP_CHECK(suite, rendered == calls); // List end must not wrap.
  player.submit({api::CommandKind::PlayTrack, {1}});
  fm_free = 0;
  player.service();
  tick += 1'000'001;
  player.service();
  RPCMP_CHECK(suite, player.snapshot().error == api::Error::Timeout);
  status = 0;
  reset_stuck = true;
  player.submit({api::CommandKind::Stop, {}});
  RPCMP_CHECK(suite, player.snapshot().error == api::Error::Reset);
  const auto before = opened;
  player.submit({api::CommandKind::PlayTrack, {1}});
  RPCMP_CHECK(suite, opened == before);
  return trace;
}
class Many final : public api::TrackList {
public:
  std::uint32_t count() const noexcept override { return 300; }
  rpcmp::contracts::CatalogText title(rpcmp::contracts::TrackId id) const noexcept override {
    rpcmp::contracts::CatalogText result;
    if (id.value > 0 && id.value <= 300) {
      result.bytes[0] = 'X';
      result.length = 1;
    }
    return result;
  }
};
class Sink final : public api::CommandSink {
public:
  unsigned calls{};
  void submit(api::PlayerCommand) override { ++calls; }
};
class SequencePort final : public rpcmp::player::minimal::PlaybackPort {
public:
  std::vector<std::uint64_t> attempts;
  std::uint64_t track{}, bad_open{}, bad_service{};
  bool end{}, fail_all{}, reset_ok{true};
  api::Error open(rpcmp::contracts::TrackId id) override {
    attempts.push_back(id.value);
    track = id.value;
    return fail_all || id.value == bad_open ? api::Error::Load : api::Error::None;
  }
  bool stop() override { return reset_ok; }
  api::Error service(bool& ended) override {
    ended = end;
    return track == bad_service ? api::Error::Audio : api::Error::None;
  }
};
void sequence_cases(rpcmp::test::Suite& suite) {
  Many list;
  SequencePort port;
  rpcmp::player::minimal::Player player{list, port};
  RPCMP_CHECK(suite, player.initialize());
  port.end = true;
  player.submit({api::CommandKind::PlayTrack, {1}});
  for (unsigned i = 0; i < 600; ++i)
    player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Ended && port.attempts.size() == 300);
  for (unsigned i = 0; i < 300; ++i)
    RPCMP_CHECK(suite, port.attempts[i] == i + 1);

  port.attempts.clear();
  port.bad_open = 298;
  port.bad_service = 299;
  player.submit({api::CommandKind::PlayTrack, {297}});
  for (unsigned i = 0; i < 10; ++i)
    player.service();
  RPCMP_CHECK(suite, port.attempts == std::vector<std::uint64_t>({297, 298, 299, 300}));
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Ended &&
                         player.snapshot().skipped_count == 2 &&
                         player.snapshot().skipped_track.value == 299 &&
                         player.snapshot().skipped_error == api::Error::Audio);

  player.submit({api::CommandKind::PlayTrack, {298}});
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Advancing);
  player.submit({api::CommandKind::Stop, {}});
  const auto attempts = port.attempts;
  for (unsigned i = 0; i < 10; ++i)
    player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Stopped && port.attempts == attempts);
  player.submit({api::CommandKind::PlayTrack, {300}});
  RPCMP_CHECK(suite, player.snapshot().skipped_count == 0);
  player.submit({api::CommandKind::Stop, {}}); // Stop wins before the pending end is polled.
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Stopped);

  port.fail_all = true;
  port.attempts.clear();
  player.submit({api::CommandKind::PlayTrack, {1}});
  for (unsigned i = 1; i < 300; ++i) {
    RPCMP_CHECK(suite, port.attempts.size() == i);
    player.service();
  }
  RPCMP_CHECK(suite, port.attempts.size() == 300 && player.snapshot().skipped_count == 300 &&
                         player.snapshot().state == api::State::Error);
  player.service();
  RPCMP_CHECK(suite, port.attempts.size() == 300);
  port.reset_ok = false;
  player.submit({api::CommandKind::PlayTrack, {1}});
  player.service();
  RPCMP_CHECK(suite, player.snapshot().error == api::Error::Reset && port.attempts.size() == 300);
}
void navigation(rpcmp::test::Suite& suite) {
  Many list;
  Sink sink;
  ui::Controller controller{list, sink};
  controller.input(0, true, 0);
  controller.input(8, true, 1);
  RPCMP_CHECK(suite, controller.view().selected == 13 && controller.view().first == 13);
  controller.input(8, true, 350000);
  RPCMP_CHECK(suite, controller.view().selected == 13);
  controller.input(8, true, 350001);
  RPCMP_CHECK(suite, controller.view().selected == 26);
  for (unsigned i = 1; i < 30; ++i)
    controller.input(8, true, 350001 + i * 80000);
  RPCMP_CHECK(suite, controller.view().selected == 299 && sink.calls == 0);
  controller.update({api::kVersion, 1, api::State::Playing, {1}, api::Error::None});
  controller.update({api::kVersion, 2, api::State::Playing, {2}, api::Error::None});
  RPCMP_CHECK(suite, controller.view().selected == 299 && controller.view().first == 299 / 13 * 13);
  controller.input(0x10, false, 4000000);
  controller.input(0x10, true, 4000001);
  RPCMP_CHECK(suite, sink.calls == 0);
}
void backpressure(rpcmp::test::Suite& suite) {
  reset_model();
  Source source;
  lib::PreparedPlaylist catalog;
  RPCMP_CHECK(suite, catalog.open(source));
  pocket::HybridPlayback backend{catalog};
  RPCMP_CHECK(suite, backend.stop() && backend.open({1}) == api::Error::None);
  bool ended{};
  fm_free = 0;
  RPCMP_CHECK(suite, backend.service(ended) == api::Error::None && rendered == 1);
  const auto before = writes;
  RPCMP_CHECK(suite,
              backend.service(ended) == api::Error::None && rendered == 1 && writes == before);
  fm_free = 1024;
  RPCMP_CHECK(suite, backend.service(ended) == api::Error::None && rendered == 1);
  end_block = true;
  const auto start = writes.size();
  RPCMP_CHECK(suite, backend.service(ended) == api::Error::None);
  RPCMP_CHECK(suite, writes[start + 2].first == 8 && writes[start + 2].second == 4);
  RPCMP_CHECK(suite, writes[start + 3].first == 9 && writes[start + 3].second == 0x10000);
  RPCMP_CHECK(suite, writes[start + 4].first == 4);
  const auto total = rendered;
  RPCMP_CHECK(suite, backend.service(ended) == api::Error::None && rendered == total);
  status = 9;
  RPCMP_CHECK(suite, backend.service(ended) == api::Error::None && ended);
  RPCMP_CHECK(suite, backend.stop());
  RPCMP_CHECK(suite, backend.open({1}) == api::Error::None);
  empty_end = true;
  const auto before_empty = writes;
  RPCMP_CHECK(suite, backend.service(ended) == api::Error::None && ended);
  RPCMP_CHECK(suite, writes == before_empty); // An empty finite song never starts the device.
  RPCMP_CHECK(suite, backend.stop());
}
int check_packed_file(const char* path) {
  Source source;
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  const auto length = input.tellg();
  if (length < 32 || length > lib::kPreparedFileLimit)
    return 1;
  source.data.resize(static_cast<std::size_t>(length));
  input.seekg(0);
  input.read(reinterpret_cast<char*>(source.data.data()), length);
  auto catalog = std::make_unique<lib::PreparedPlaylist>();
  if (!input || !catalog->open(source))
    return 1;
  for (std::uint32_t id = 1; id <= catalog->count(); ++id) {
    std::uint32_t a{}, b{};
    if (!catalog->sizes({id}, a, b))
      return 1;
    Bytes mdx(a + 16), pdx(b + 16);
    if (!catalog->load({id}, {mdx.data(), mdx.size()}, {pdx.data(), pdx.size()}))
      return 1;
  }
  std::cout << "playlist payloads: PASS tracks=" << catalog->count() << '\n';
  return 0;
}
void preview(rpcmp::test::Suite& suite, const char* path) {
  Source source;
  lib::PreparedPlaylist catalog;
  RPCMP_CHECK(suite, catalog.open(source));
  Sink sink;
  ui::Controller controller{catalog, sink};
  controller.update({api::kVersion, 1, api::State::Playing, {2}, api::Error::None});
  pocket::MinimalDisplay display;
  display.begin(controller.view());
  Bytes pixels(std::size_t{640} * 480);
  for (unsigned i = 0; i < 120; ++i)
    RPCMP_CHECK(suite, display.pump(pixels.data(), pixels.size()));
  RPCMP_CHECK(suite, display.complete());
  RPCMP_CHECK(suite, pixels[(80 + 24 + 8) * 640 + 2] == 3 && pixels[(80 + 8) * 640 + 2] == 0);
  // Playback is track 2 while the browsing cursor remains on track 1.
  const auto number = pocket::bitmap_glyph('2');
  for (unsigned y = 0; y < 16; ++y)
    for (unsigned x = 0; x < 8; ++x)
      RPCMP_CHECK(suite,
                  pixels[(400 + y) * 640 + 88 + x] == ((number.rows[y] & (128U >> x)) ? 3 : 0));
  const auto glyph = pocket::bitmap_glyph('R');
  for (unsigned y = 0; y < 16; ++y)
    for (unsigned x = 0; x < 8; ++x)
      RPCMP_CHECK(suite,
                  pixels[(16 + y) * 640 + 16 + x] == ((glyph.rows[y] & (128U >> x)) ? 3 : 0));
  if (path) {
    std::ofstream file(path, std::ios::binary);
    file << "P6\n640 480\n255\n";
    for (const auto value : pixels) {
      const auto color = pocket::kMinimalPalette[value];
      const char rgb[]{static_cast<char>(color >> 16U), static_cast<char>(color >> 8U),
                       static_cast<char>(color)};
      file.write(rgb, 3);
    }
    RPCMP_CHECK(suite, file.good());
  }
}
} // namespace
extern "C" std::uint32_t rpcmp_pocket_time_us() {
  tick += 10;
  return tick;
}
extern "C" std::uint32_t rpcmp_player_mmio_read(std::uintptr_t address) {
  if (address == 0x40000404)
    return status;
  if (address == 0x40000418)
    return pcm_free;
  if (address == 0x40000428)
    return fm_free;
  return 0;
}
extern "C" void rpcmp_player_mmio_write(std::uintptr_t address, std::uint32_t value) {
  const auto word_index = static_cast<unsigned>((address - 0x40000400) / 4);
  writes.emplace_back(word_index, value);
  if (word_index == 2 && value == 1 && !reset_stuck)
    status = 1;
  if (word_index == 2 && value == 2)
    status = 7;
}
extern "C" int rpcmp_hybrid_open(const void* mdx, std::uint32_t, const void*, std::uint32_t) {
  opened = static_cast<const std::uint8_t*>(mdx)[10];
  rendered = 0;
  return reject_open ? -1 : 0;
}
extern "C" void rpcmp_hybrid_close() {}
extern "C" const RpcmpHybridBlock* rpcmp_hybrid_render() {
  if (reject_render)
    return nullptr;
  block = {};
  block.first_frame = rendered * 2;
  block.frame_count = 2;
  block.event_count = 1;
  block.events[0] = {block.first_frame, 0x2878};
  block.pcm[0] = 20000;
  block.pcm[1] = -20000;
  block.ended = end_block ? 1 : 0;
  if (empty_end) {
    block.frame_count = block.event_count = 0;
    block.ended = 1;
  }
  ++rendered;
  return &block;
}
int main(int argc, char** argv) {
  if (argc == 3 && std::string{argv[1]} == "--playlist")
    return check_packed_file(argv[2]);
  rpcmp::test::Suite suite;
  catalog_cases(suite);
  sequence_cases(suite);
  navigation(suite);
  backpressure(suite);
  preview(suite, argc == 2 ? argv[1] : nullptr);
  const auto headless = playback_case(suite, false), delayed = playback_case(suite, true);
  RPCMP_CHECK(suite, headless == delayed);
  RPCMP_CHECK(suite, headless.size() > 100);
  // Per block: timestamp, FM operation, then PCM left/right pairs.
  RPCMP_CHECK(suite, headless[2].first == 8 && headless[3].first == 9 && headless[4].first == 4);
  return suite.finish("minimal player catalog/control/audio/display");
}
