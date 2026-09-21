#include "hybrid_playback.hpp"
#include "rpcmp/platform/pocket/bitmap_font.hpp"
#include "rpcmp/platform/pocket/minimal_display.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstring>
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
  const auto index_bytes = static_cast<std::size_t>(data[16]) * 112 +
                           static_cast<std::size_t>(data[20] | (data[21] << 8U)) * 128;
  word(data, 28, lib::crc32({data.data() + 48, index_bytes}));
  word(data, 32, lib::crc32({data.data(), 32}));
}
Bytes fixture() {
  // Independently authored HPL2 wire fixture: one list, FM then PCM.
  Bytes b(48 + 112 + 256 + 36, 0);
  word(b, 0, 0x324c5048);
  word(b, 4, 2);
  word(b, 8, 112);
  word(b, 12, 128);
  word(b, 16, 1);
  word(b, 20, 2);
  word(b, 24, static_cast<std::uint32_t>(b.size()));
  word(b, 48, 1);
  word(b, 52, 2);
  word(b, 56, 6);
  const std::string name = "曲集";
  std::copy(name.begin(), name.end(), b.begin() + 60);
  const Bytes fm{0, 0, 255, 255, 0, 10, 0, 8, 0, 0, 11, 0};
  const Bytes mixed{0, 0, 0, 0, 0, 10, 0, 8, 0, 0, 22, 0};
  const Bytes pdx{0, 0, 0, 0, 0, 10, 0, 2, 0, 0, 33, 0};
  for (unsigned i = 0; i < 2; ++i) {
    const auto at = 160 + i * 128;
    const auto offset = 416 + i * 12;
    const auto& blob = i == 0 ? fm : mixed;
    word(b, at, offset);
    word(b, at + 4, 12);
    word(b, at + 8, lib::crc32({blob.data(), blob.size()}));
    std::copy(blob.begin(), blob.end(), b.begin() + offset);
    const std::string title = i == 0 ? "FM 試験曲" : "PCM 日本語";
    word(b, at + 24, static_cast<std::uint32_t>(title.size()));
    std::copy(title.begin(), title.end(), b.begin() + at + 28);
  }
  word(b, 288 + 12, 440);
  word(b, 288 + 16, 12);
  word(b, 288 + 20, lib::crc32({pdx.data(), pdx.size()}));
  std::copy(pdx.begin(), pdx.end(), b.begin() + 440);
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
Bytes split_fixture() {
  const auto single = fixture();
  Bytes result(single.size() + 112, 0);
  std::copy_n(single.begin(), 160, result.begin());
  word(result, 16, 2);
  word(result, 24, static_cast<std::uint32_t>(result.size()));
  word(result, 52, 1);
  word(result, 160, 2);
  word(result, 164, 1);
  word(result, 168, 1);
  result[172] = 'B';
  std::copy(single.begin() + 160, single.end(), result.begin() + 272);
  word(result, 272, 528);
  word(result, 400, 540);
  word(result, 412, 552);
  seal(result);
  return result;
}
std::uint32_t tick{}, status{1}, fm_free{1024}, pcm_free{4096}, opened{}, rendered{};
bool reject_open{}, reject_render{}, end_block{}, empty_end{}, reset_stuck{}, pause_stuck{};
bool repeat_stuck{};
std::vector<std::pair<unsigned, std::uint32_t>> writes;
RpcmpHybridBlock block;
void reset_model() {
  tick = opened = rendered = 0;
  status = 1;
  fm_free = 1024;
  pcm_free = 4096;
  reject_open = reject_render = end_block = empty_end = reset_stuck = pause_stuck = false;
  repeat_stuck = false;
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
  source.data[428] ^= 1;
  RPCMP_CHECK(suite, !catalog->load({2}, {a.data(), a.size()}, {b.data(), b.size()}));
  for (unsigned bad = 0; bad < 18; ++bad) {
    source.data = fixture();
    switch (bad) {
    case 0:
      word(source.data, 0, 0x314c5048);
      break; // HPL1 must be rejected.
    case 1:
      word(source.data, 16, 101);
      break;
    case 2:
      word(source.data, 160, 16);
      break;
    case 3:
      word(source.data, 164, 0xffffffff);
      break;
    case 4:
      word(source.data, 176, 1);
      break;
    case 5:
      word(source.data, 184, 97);
      break;
    case 6:
      source.data[188] = 0xc0;
      break; // Overlong UTF-8.
    case 7:
      source.data[188] = 0;
      break;
    case 8:
      source.data[287] = 1;
      break; // Reserved word.
    case 9:
      source.data[248] = 1;
      break; // Title padding.
    case 10:
      word(source.data, 52, 301);
      break;
    case 11:
      word(source.data, 48, 2);
      break; // Gap/overlap in list ranges.
    case 12:
      word(source.data, 52, 1);
      break; // Unowned track.
    case 13:
      source.data[60] = 0xc0;
      break;
    case 14:
      word(source.data, 156, 1);
      break;
    case 15:
      word(source.data, 36, 1);
      break;
    case 16:
      word(source.data, 20, 30001);
      break;
    case 17:
      word(source.data, 20, 0);
      break;
    default:
      break;
    }
    if (bad != 1 && bad != 16 && bad != 17)
      seal(source.data);
    else
      word(source.data, 32, lib::crc32({source.data.data(), 32}));
    RPCMP_CHECK(suite,
                !catalog->open(source) && catalog->count() == 0 && catalog->playlist_count() == 0);
  }
  source.data = fixture();
  source.data[28] ^= 1;
  word(source.data, 32, lib::crc32({source.data.data(), 32}));
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
  controller.input(0x10, true, 1); // Open the list without playing.
  RPCMP_CHECK(suite, opened == 0);
  controller.input(2, true, 2); // Browse to PCM.
  RPCMP_CHECK(suite, controller.view().selected == 1 && opened == 0);
  controller.input(0x10, true, 3);
  RPCMP_CHECK(suite, opened == 22 && player.snapshot().state == api::State::Playing);
  controller.update(player.snapshot());
  pocket::MinimalDisplay display;
  display.begin(controller.view());
  Bytes pixels(640 * 480 + 2, 0xee);
  for (unsigned i = 0; i < 180; ++i) {
    if (i == 10 || i == 30 || i == 60 || i == 90)
      player.submit({api::CommandKind::CycleRepeat, {}});
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
  explicit Many(const std::uint32_t lists = 1) : lists_(lists) {}
  std::uint32_t count() const noexcept override { return lists_ * 300; }
  std::uint32_t playlist_count() const noexcept override { return lists_; }
  api::Playlist playlist(const api::PlaylistId id) const noexcept override {
    return id.value > 0 && id.value <= lists_
               ? api::Playlist{id, {1U + (id.value - 1) * 300}, 300, title({1})}
               : api::Playlist{};
  }
  api::PlaylistId playlist_for(const rpcmp::contracts::TrackId id) const noexcept override {
    return id.value > 0 && id.value <= count()
               ? api::PlaylistId{static_cast<std::uint32_t>((id.value - 1) / 300 + 1)}
               : api::PlaylistId{};
  }
  rpcmp::contracts::CatalogText title(rpcmp::contracts::TrackId id) const noexcept override {
    rpcmp::contracts::CatalogText result;
    if (id.value > 0 && id.value <= count()) {
      result.bytes[0] = 'X';
      result.length = 1;
    }
    return result;
  }

private:
  std::uint32_t lists_;
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
  api::Error set_paused(bool) override { return api::Error::None; }
  api::Error set_repeat(api::RepeatMode) override { return api::Error::None; }
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

  for (unsigned i = 0; i < 3; ++i)
    player.submit({api::CommandKind::CycleRepeat, {}});
  port.fail_all = true; // Even repeat-one must terminate an all-error list.
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
void multiple_playlists(rpcmp::test::Suite& suite) {
  Many list{100};
  SequencePort port;
  rpcmp::player::minimal::Player player{list, port};
  RPCMP_CHECK(suite, player.initialize());
  ui::Controller controller{list, player};
  std::uint32_t now = 0;
  const auto press = [&](const std::uint32_t key) {
    controller.input(0, true, ++now);
    controller.input(key, true, ++now);
    controller.update(player.snapshot());
  };
  RPCMP_CHECK(suite, controller.view().count == 100 && controller.view().playlist.value == 0);
  press(8); // Playlist 14.
  press(0x10);
  RPCMP_CHECK(suite, controller.view().playlist.value == 14 && port.attempts.empty());
  press(8); // Track 13, second page.
  press(0x10);
  RPCMP_CHECK(suite,
              player.snapshot().track.value == 3913 && player.snapshot().playlist.value == 14);
  press(0x40);
  const auto attempts = port.attempts;
  press(1); // Fixed back row does not discard the browsing position.
  RPCMP_CHECK(suite, controller.view().back_selected && controller.view().first == 12);
  press(0x10);
  RPCMP_CHECK(suite, controller.view().playlist.value == 0 && controller.view().selected == 13 &&
                         controller.view().playing[0]);
  press(8);
  press(0x10); // Playlist 27, while playlist 14 remains paused.
  for (unsigned i = 0; i < 3; ++i)
    press(8);
  RPCMP_CHECK(suite, controller.view().playlist.value == 27 && controller.view().selected == 36 &&
                         port.attempts == attempts &&
                         player.snapshot().state == api::State::Paused);
  press(0x40);
  port.end = true;
  player.service();
  player.service();
  controller.update(player.snapshot());
  RPCMP_CHECK(suite, player.snapshot().track.value == 3914 &&
                         player.snapshot().playlist.value == 14 &&
                         controller.view().selected == 36 && controller.view().first == 36);
  press(0x10);
  RPCMP_CHECK(suite, player.snapshot().track.value == 7837 &&
                         player.snapshot().playlist.value == 27 && controller.view().playing[1]);
  press(1);
  press(0x10);
  press(4);
  press(0x10);
  RPCMP_CHECK(suite, controller.view().playlist.value == 14 && controller.view().selected == 12 &&
                         controller.view().first == 12 && !controller.view().back_selected);
  player.submit({api::CommandKind::PlayTrack, {4200}}); // Last in list 14, not last globally.
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Ended && port.attempts.back() == 4200);
  for (unsigned i = 0; i < 3; ++i)
    press(0x80);
  player.submit({api::CommandKind::PlayTrack, {4200}});
  player.service();
  player.service();
  RPCMP_CHECK(suite, player.snapshot().track.value == 4200 &&
                         player.snapshot().state == api::State::Playing);
  port.bad_open = 4200;
  player.submit({api::CommandKind::PlayTrack, {4200}});
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Error && port.attempts.back() == 4200);
  press(0x80); // Back to two loops.
  press(1);
  press(0x10);
  for (unsigned i = 0; i < 8; ++i)
    press(8);
  press(0x10);
  for (unsigned i = 0; i < 25; ++i)
    press(8);
  press(0x10);
  RPCMP_CHECK(suite,
              player.snapshot().track.value == 30000 && player.snapshot().playlist.value == 100);
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Ended);
  ui::Controller restarted{list, player};
  RPCMP_CHECK(suite, restarted.view().playlist.value == 0 && restarted.view().selected == 0);
  restarted.input(0, true, 0);
  restarted.input(0x10, true, 1);
  RPCMP_CHECK(suite, restarted.view().selected == 0 && restarted.view().first == 0);
}

std::vector<std::pair<unsigned, std::uint32_t>> multi_audio(rpcmp::test::Suite& suite,
                                                            bool browse) {
  reset_model();
  Source source;
  source.data = split_fixture();
  const auto catalog = std::make_unique<lib::PreparedPlaylist>();
  RPCMP_CHECK(suite, catalog->open(source) && catalog->playlist_count() == 2);
  RPCMP_CHECK(suite,
              catalog->playlist_for({1}).value == 1 && catalog->playlist_for({2}).value == 2);
  pocket::HybridPlayback backend{*catalog};
  rpcmp::player::minimal::Player player{*catalog, backend};
  RPCMP_CHECK(suite, player.initialize());
  player.submit({api::CommandKind::PlayTrack, {1}});
  const auto reads = source.reads;
  ui::Controller controller{*catalog, player};
  controller.input(0, true, 0);
  if (browse) {
    controller.input(2, true, 1);
    controller.input(0x10, true, 2); // Open the other list without playing it.
  }
  controller.update(player.snapshot());
  pocket::MinimalDisplay display;
  display.begin(controller.view());
  Bytes pixels(std::size_t{640} * 480);
  for (unsigned i = 0; i < 180; ++i) {
    player.service();
    if (browse && i % 3 == 0)
      RPCMP_CHECK(suite, display.pump(pixels.data(), pixels.size()));
  }
  RPCMP_CHECK(suite,
              source.reads == reads && opened == 11 && player.snapshot().playlist.value == 1);
  if (browse)
    RPCMP_CHECK(suite, controller.view().playlist.value == 2 && !controller.view().playing[1]);
  return writes;
}
void navigation(rpcmp::test::Suite& suite) {
  Many list;
  Sink sink;
  ui::Controller controller{list, sink};
  controller.input(0, true, 0);
  controller.input(0x10, true, 0); // Open the single list.
  controller.input(8, true, 1);
  RPCMP_CHECK(suite, controller.view().selected == 12 && controller.view().first == 12);
  controller.input(8, true, 350000);
  RPCMP_CHECK(suite, controller.view().selected == 12);
  controller.input(8, true, 350001);
  RPCMP_CHECK(suite, controller.view().selected == 24);
  for (unsigned i = 1; i < 30; ++i)
    controller.input(8, true, 350001 + i * 80000);
  RPCMP_CHECK(suite, controller.view().selected == 299 && sink.calls == 0);
  controller.update({api::kVersion, 1, api::State::Playing, {1}, api::Error::None});
  controller.update({api::kVersion, 2, api::State::Playing, {2}, api::Error::None});
  RPCMP_CHECK(suite, controller.view().selected == 299 && controller.view().first == 299 / 12 * 12);
  controller.input(0x10, false, 4000000);
  controller.input(0x10, true, 4000001);
  RPCMP_CHECK(suite, sink.calls == 0);
}
void repeat_cases(rpcmp::test::Suite& suite) {
  reset_model();
  Source source;
  const auto storage = std::make_unique<lib::PreparedPlaylist>();
  auto& catalog = *storage;
  RPCMP_CHECK(suite, catalog.open(source));
  pocket::HybridPlayback backend{catalog};
  rpcmp::player::minimal::Player player{catalog, backend};
  RPCMP_CHECK(suite, player.initialize());
  ui::Controller controller{catalog, player};
  controller.input(0x80, true, 0);
  RPCMP_CHECK(suite, player.snapshot().repeat == api::RepeatMode::Two);
  controller.input(0, true, 1);
  controller.input(0x80, true, 2);
  controller.input(0x80, true, 1'000'002); // Y is edge-triggered.
  RPCMP_CHECK(suite, player.snapshot().repeat == api::RepeatMode::Three && opened == 0);
  controller.input(0x10, true, 1'000'003); // Open list.
  controller.input(0, true, 1'000'003);
  controller.input(0x10, true, 1'000'003); // Start track.
  RPCMP_CHECK(suite, opened == 11 && player.snapshot().repeat == api::RepeatMode::Three);
  player.service();
  const auto calls = rendered;
  player.submit({api::CommandKind::TogglePause, {}});
  controller.input(0x80, true, 1'000'004);
  controller.update(player.snapshot());
  RPCMP_CHECK(suite, controller.view().playback.repeat == api::RepeatMode::Five &&
                         player.snapshot().state == api::State::Paused && rendered == calls);
  player.submit({api::CommandKind::CycleRepeat, {}});
  RPCMP_CHECK(suite, player.snapshot().repeat == api::RepeatMode::One);
  player.submit({api::CommandKind::Stop, {}});
  RPCMP_CHECK(suite, player.snapshot().repeat == api::RepeatMode::One && status == 1);
  player.submit({api::CommandKind::PlayTrack, {2}});
  player.service();
  status |= 8; // A finite song ends at the last entry: repeat that entry.
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Advancing);
  player.service();
  RPCMP_CHECK(suite, player.snapshot().track.value == 2 && opened == 22 && rendered == 0);
  player.service();
  status |= 8;
  player.service();
  player.submit({api::CommandKind::CycleRepeat, {}}); // Cancel a pending repeat.
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Ended &&
                         player.snapshot().repeat == api::RepeatMode::Two);
  for (unsigned i = 0; i < 3; ++i)
    player.submit({api::CommandKind::CycleRepeat, {}});
  player.submit({api::CommandKind::PlayTrack, {1}});
  source.data[416] ^= 1; // A repeated entry that fails to reload must be skipped.
  player.service();
  status |= 8;
  player.service();
  player.service();
  RPCMP_CHECK(suite,
              player.snapshot().skipped_count == 1 && player.snapshot().skipped_track.value == 1);
  player.service();
  RPCMP_CHECK(suite,
              player.snapshot().track.value == 2 && player.snapshot().state == api::State::Playing);
  controller.input(0, true, 1'000'005);
  controller.input(0xf0, true, 1'000'006); // B wins, including over Y.
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Stopped &&
                         player.snapshot().repeat == api::RepeatMode::One);
  player.submit({api::CommandKind::PlayTrack, {2}});
  repeat_stuck = true;
  player.submit({api::CommandKind::CycleRepeat, {}});
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Error &&
                         player.snapshot().error == api::Error::Timeout && status == 1);
  repeat_stuck = false;
  // Malformed representation, following the other enum-boundary fixtures.
  const std::uint8_t invalid_byte = 4;
  api::RepeatMode invalid_mode{};
  std::memcpy(&invalid_mode, &invalid_byte, sizeof(invalid_byte));
  RPCMP_CHECK(suite, backend.set_repeat(invalid_mode) == api::Error::Renderer);
  rpcmp::player::minimal::Player restarted{catalog, backend};
  RPCMP_CHECK(suite, restarted.initialize() && restarted.snapshot().repeat == api::RepeatMode::Two);
  ui::Bindings bindings;
  bindings.repeat = 0x100;
  ui::Controller remapped{catalog, restarted, bindings};
  remapped.input(0, true, 0);
  remapped.input(0x80, true, 1);
  RPCMP_CHECK(suite, restarted.snapshot().repeat == api::RepeatMode::Two);
  remapped.input(0x100, true, 2);
  RPCMP_CHECK(suite, restarted.snapshot().repeat == api::RepeatMode::Three);
  remapped.input(0, false, 3);
  remapped.input(0x100, true, 4);
  RPCMP_CHECK(suite, restarted.snapshot().repeat == api::RepeatMode::Three);
}
void backpressure(rpcmp::test::Suite& suite) {
  reset_model();
  Source source;
  const auto storage = std::make_unique<lib::PreparedPlaylist>();
  auto& catalog = *storage;
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
void pause_cases(rpcmp::test::Suite& suite) {
  reset_model();
  Source source;
  const auto storage = std::make_unique<lib::PreparedPlaylist>();
  auto& catalog = *storage;
  RPCMP_CHECK(suite, catalog.open(source));
  pocket::HybridPlayback backend{catalog};
  rpcmp::player::minimal::Player player{catalog, backend};
  RPCMP_CHECK(suite, player.initialize());
  ui::Controller controller{catalog, player};
  controller.input(0x40, true, 0); // Held at boot must do nothing.
  controller.input(0, true, 1);
  controller.input(0x10, true, 2); // Open list.
  controller.input(0, true, 2);
  controller.input(0x10, true, 2); // Start track.
  player.service();
  const auto before = rendered;
  controller.input(0x40, true, 3); // X pauses without closing/reopening the song.
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Paused && status == 0x107);
  const auto held_writes = writes;
  for (unsigned i = 0; i < 100; ++i)
    player.service();
  tick += 10'000'000;
  controller.input(0x40, true, 10'000'003); // No key repeat for X.
  RPCMP_CHECK(suite, rendered == before && writes == held_writes);
  controller.input(2, true, 10'000'004); // Browse another song while paused.
  controller.update(player.snapshot());
  RPCMP_CHECK(suite,
              controller.view().selected == 1 && controller.view().playback.track.value == 1);
  controller.input(0x40, true, 10'000'005);
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Playing && opened == 11);
  player.service();
  RPCMP_CHECK(suite, rendered == before + 1 && status == 7);
  player.submit({api::CommandKind::TogglePause, {}});
  controller.input(0x50, true, 10'000'006); // A wins over X and selects the browsed song.
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Playing && opened == 22);
  player.submit({api::CommandKind::TogglePause, {}}); // Before the first render/start.
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Paused && rendered == 0 && status == 1);
  player.submit({api::CommandKind::TogglePause, {}});
  player.service();
  RPCMP_CHECK(suite, rendered == 1 && status == 7);
  player.submit({api::CommandKind::TogglePause, {}});
  controller.input(0, true, 10'000'007);
  controller.input(0x70, true, 10'000'008); // B wins over A and X, including paused playback.
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Stopped && status == 1);
  player.submit({api::CommandKind::TogglePause, {}});
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Stopped);

  // Change only the UI binding; commands and backend behavior stay the same.
  ui::Bindings bindings;
  bindings.pause = 0x80;
  bindings.repeat = 0x100;
  ui::Controller remapped{catalog, player, bindings};
  remapped.input(0, true, 0);
  remapped.input(0x10, true, 1);
  remapped.input(0, true, 1);
  remapped.input(0x10, true, 1);
  remapped.input(0x40, true, 2);
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Playing);
  remapped.input(0x80, true, 3);
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Paused);
  remapped.input(0, false, 4);
  remapped.input(0x80, true, 5); // Reconnect while held does not resume.
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Paused);
  player.submit({api::CommandKind::Stop, {}});

  // Retain a rendered block under FIFO backpressure across a long pause.
  player.submit({api::CommandKind::PlayTrack, {1}});
  player.service();
  fm_free = 0;
  player.service();
  const auto pending_count = rendered;
  player.submit({api::CommandKind::TogglePause, {}});
  tick += 10'000'000;
  player.service();
  player.submit({api::CommandKind::TogglePause, {}});
  fm_free = 1024;
  player.service();
  RPCMP_CHECK(suite, rendered == pending_count && player.snapshot().state == api::State::Playing);
  end_block = true;
  player.service();
  player.submit({api::CommandKind::TogglePause, {}});
  tick += 10'000'000;
  status |= 8; // A completion observed after the pause must wait for resume.
  player.service();
  RPCMP_CHECK(suite,
              player.snapshot().state == api::State::Paused && player.snapshot().track.value == 1);
  status &= ~8U;
  player.submit({api::CommandKind::TogglePause, {}});
  player.service(); // The old EOF deadline excludes paused wall time.
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Playing);
  status = 9;
  player.service();
  player.service();
  RPCMP_CHECK(suite,
              player.snapshot().state == api::State::Playing && player.snapshot().track.value == 2);

  player.service();
  pause_stuck = true;
  player.submit({api::CommandKind::TogglePause, {}});
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Error &&
                         player.snapshot().error == api::Error::Timeout && status == 1);
  pause_stuck = false;
  player.submit({api::CommandKind::PlayTrack, {2}});
  player.service();
  status = 0x17;
  player.submit({api::CommandKind::TogglePause, {}});
  RPCMP_CHECK(suite, player.snapshot().error == api::Error::Audio && status == 1);
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
  reset_model();
  pocket::HybridPlayback backend{*catalog};
  rpcmp::player::minimal::Player player{*catalog, backend};
  if (!player.initialize())
    return 1;
  ui::Controller controller{*catalog, player};
  controller.input(0, true, 0);
  controller.input(0x10, true, 1); // Opening a playlist must remain silent.
  if (opened != 0 || player.snapshot().state != api::State::Stopped)
    return 1;
  controller.input(0, true, 2);
  controller.input(0x10, true, 3);
  if (player.snapshot().state != api::State::Playing || player.snapshot().playlist.value != 1)
    return 1;
  if (catalog->playlist_count() > 1) {
    const auto before = source.reads;
    controller.input(1, true, 4);
    controller.input(0x10, true, 5);
    controller.input(2, true, 6);
    controller.input(0x10, true, 7);
    if (source.reads != before || player.snapshot().playlist.value != 1 ||
        controller.view().playlist.value != 2)
      return 1;
    controller.input(0, true, 8);
    controller.input(0x10, true, 9);
    if (player.snapshot().playlist.value != 2 ||
        player.snapshot().track != catalog->playlist({2}).first)
      return 1;
  }
  const auto list = catalog->playlist(player.snapshot().playlist);
  player.submit({api::CommandKind::PlayTrack, {list.first.value + list.count - 1}});
  status = 9;
  player.service();
  if (player.snapshot().state != api::State::Ended)
    return 1;
  std::cout << "playlist payloads: PASS tracks=" << catalog->count() << '\n';
  return 0;
}
void preview(rpcmp::test::Suite& suite, const char* path) {
  Source source;
  const auto storage = std::make_unique<lib::PreparedPlaylist>();
  auto& catalog = *storage;
  RPCMP_CHECK(suite, catalog.open(source));
  Sink sink;
  ui::Controller controller{catalog, sink};
  controller.input(0, true, 0);
  controller.input(0x10, true, 1);
  api::PlayerSnapshot playing{api::kVersion, 1, api::State::Playing, {2}, api::Error::None};
  playing.playlist = {1};
  controller.update(playing);
  pocket::MinimalDisplay display;
  display.begin(controller.view());
  Bytes pixels(std::size_t{640} * 480);
  for (unsigned i = 0; i < 120; ++i)
    RPCMP_CHECK(suite, display.pump(pixels.data(), pixels.size()));
  RPCMP_CHECK(suite, display.complete());
  RPCMP_CHECK(suite, pixels[(80 + 48 + 8) * 640 + 2] == 3 && pixels[(80 + 24 + 8) * 640 + 2] == 0);
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
  api::PlayerSnapshot paused{api::kVersion, 2, api::State::Paused, {2}, api::Error::None};
  paused.repeat = api::RepeatMode::One;
  paused.playlist = {1};
  controller.update(paused);
  display.begin(controller.view());
  while (!display.complete())
    RPCMP_CHECK(suite, display.pump(pixels.data(), pixels.size()));
  RPCMP_CHECK(suite, pixels[(80 + 48 + 8) * 640 + 2] == 3);
  const auto paused_glyph = pocket::bitmap_glyph(0x4e00); // 一時停止中
  for (unsigned y = 0; y < 16; ++y)
    for (unsigned x = 0; x < 16; ++x)
      RPCMP_CHECK(suite, pixels[(400 + y) * 640 + 16 + x] ==
                             ((paused_glyph.rows[y * 2 + x / 8] & (128U >> (x % 8))) ? 3 : 0));
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
    status = (status & 0x200U) | 7;
  if (word_index == 2 && value == 4 && !pause_stuck)
    status |= 0x100;
  if (word_index == 2 && value == 8 && !pause_stuck)
    status &= ~0x100U;
  if (word_index == 2 && value == 16 && !repeat_stuck)
    status ^= 0x200U;
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
  multiple_playlists(suite);
  RPCMP_CHECK(suite, multi_audio(suite, false) == multi_audio(suite, true));
  navigation(suite);
  backpressure(suite);
  pause_cases(suite);
  repeat_cases(suite);
  preview(suite, argc == 2 ? argv[1] : nullptr);
  const auto headless = playback_case(suite, false), delayed = playback_case(suite, true);
  RPCMP_CHECK(suite, headless == delayed);
  RPCMP_CHECK(suite, headless.size() > 100);
  // Per block: timestamp, FM operation, then PCM left/right pairs.
  RPCMP_CHECK(suite, headless[2].first == 8 && headless[3].first == 9 && headless[4].first == 4);
  return suite.finish("minimal player catalog/control/audio/display");
}
