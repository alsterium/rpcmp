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
std::uint32_t model_frames{2};
std::vector<std::pair<unsigned, std::uint32_t>> writes;
RpcmpHybridBlock block;
void reset_model() {
  tick = opened = rendered = 0;
  status = 1;
  fm_free = 1024;
  pcm_free = 4096;
  reject_open = reject_render = end_block = empty_end = reset_stuck = pause_stuck = false;
  repeat_stuck = false;
  model_frames = 2;
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
  auto animated = controller.view();
  animated.playing_title.length = animated.playing_list.length = 90;
  std::fill_n(animated.playing_title.bytes.data(), 90, 'W');
  std::fill_n(animated.playing_list.bytes.data(), 90, 'M');
  animated.info_scroll_tick = 40;
  display.begin(animated);
  Bytes pixels(640 * 480 + 2, 0xee);
  for (unsigned i = 0; i < 420; ++i) {
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
  RPCMP_CHECK(suite, !draw || display.complete());
  RPCMP_CHECK(suite, pixels.front() == 0xee && pixels.back() == 0xee);
  const auto trace = writes;
  controller.input(0x40, true, 5);
  controller.input(0x30, true, 6); // In controls, simultaneous A/B stops only.
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Stopped && opened == 22);
  controller.input(0, true, 7);
  controller.input(0x40, true, 8);
  controller.input(0x10, true, 9);
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
    seconds = 0;
    return fail_all || id.value == bad_open ? api::Error::Load : api::Error::None;
  }
  std::uint64_t seconds{};
  std::uint64_t elapsed_seconds() const noexcept override { return seconds; }
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
  press(8); // Track 14, second page (13 actual tracks per page).
  press(0x10);
  RPCMP_CHECK(suite,
              player.snapshot().track.value == 3914 && player.snapshot().playlist.value == 14);
  press(0x40);
  press(0x10); // Play/pause icon.
  press(0x40);
  const auto attempts = port.attempts;
  press(0x20); // B returns without stopping.
  RPCMP_CHECK(suite, controller.view().playlist.value == 0 && controller.view().selected == 13 &&
                         controller.view().playing[0]);
  press(8);
  press(0x10); // Playlist 27 while list 14 remains paused.
  for (unsigned i = 0; i < 3; ++i)
    press(8);
  RPCMP_CHECK(suite, controller.view().playlist.value == 27 && controller.view().selected == 39 &&
                         port.attempts == attempts &&
                         player.snapshot().state == api::State::Paused);
  press(0x40);
  press(0x10);
  press(0x40);
  port.end = true;
  player.service();
  player.service();
  controller.update(player.snapshot());
  RPCMP_CHECK(suite, player.snapshot().track.value == 3915 &&
                         player.snapshot().playlist.value == 14 &&
                         controller.view().selected == 39 && controller.view().first == 39);
  press(0x10);
  RPCMP_CHECK(suite, player.snapshot().track.value == 7840 &&
                         player.snapshot().playlist.value == 27 && controller.view().playing[0]);
  press(0x20);
  press(4);
  press(0x10);
  RPCMP_CHECK(suite, controller.view().playlist.value == 14 && controller.view().selected == 13 &&
                         controller.view().first == 13);
  player.submit({api::CommandKind::PlayTrack, {4200}});
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Ended && port.attempts.back() == 4200);
  for (unsigned i = 0; i < 3; ++i)
    player.submit({api::CommandKind::CycleRepeat, {}});
  player.submit({api::CommandKind::PlayTrack, {4200}});
  player.service();
  player.service();
  RPCMP_CHECK(suite, player.snapshot().track.value == 4200 &&
                         player.snapshot().state == api::State::Playing);
  port.bad_open = 4200;
  player.submit({api::CommandKind::PlayTrack, {4200}});
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Error && port.attempts.back() == 4200);
  player.submit({api::CommandKind::CycleRepeat, {}});
  press(0x20);
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
    RPCMP_CHECK(suite, controller.view().playlist.value == 2 && !controller.view().playing[0]);
  return writes;
}
void navigation(rpcmp::test::Suite& suite) {
  Many list;
  Sink sink;
  ui::Controller controller{list, sink};
  controller.input(0, true, 0);
  controller.input(0x10, true, 0);
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
  controller.input(0x10, true, 5000001);
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
  RPCMP_CHECK(suite, player.snapshot().repeat == api::RepeatMode::Two);
  player.submit({api::CommandKind::CycleRepeat, {}});
  RPCMP_CHECK(suite, player.snapshot().repeat == api::RepeatMode::Three && opened == 0);
  player.submit({api::CommandKind::PlayTrack, {1}});
  RPCMP_CHECK(suite, opened == 11 && player.snapshot().repeat == api::RepeatMode::Three);
  player.service();
  const auto calls = rendered;
  player.submit({api::CommandKind::PlayPause, {}});
  player.submit({api::CommandKind::CycleRepeat, {}});
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
  controller.input(0x40, true, 1'000'006);
  controller.input(0xf0, true, 1'000'007); // B wins over A and X in controls.
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
  // Rebinding and edge behavior are exercised through the new control panel below.
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
  controller.input(0x40, true, 0); // Held at boot must not switch panels.
  controller.input(0, true, 1);
  controller.input(0x10, true, 2);
  controller.input(0, true, 2);
  controller.input(0x10, true, 2);
  player.service();
  const auto before = rendered;
  controller.update(player.snapshot());
  controller.input(0x40, true, 3);
  controller.input(0x10, true, 4); // A on the play/pause icon.
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Paused && status == 0x107);
  const auto held_writes = writes;
  for (unsigned i = 0; i < 100; ++i)
    player.service();
  tick += 10'000'000;
  controller.input(0x10, true, 10'000'004);
  RPCMP_CHECK(suite, rendered == before && writes == held_writes);
  controller.input(0x40, true, 10'000'005);
  controller.input(2, true, 10'000'006);
  controller.update(player.snapshot());
  RPCMP_CHECK(suite,
              controller.view().selected == 1 && controller.view().playback.track.value == 1);
  controller.input(0x40, true, 10'000'007);
  controller.input(0x10, true, 10'000'008);
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Playing && opened == 11);
  player.service();
  RPCMP_CHECK(suite, rendered == before + 1 && status == 7);
  player.submit({api::CommandKind::PlayPause, {}});
  controller.input(0x40, true, 10'000'009);
  controller.input(0x90, true, 10'000'010); // Y is unassigned; A plays selected PCM.
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Playing && opened == 22);
  player.submit({api::CommandKind::PlayPause, {}});
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Paused && rendered == 0 && status == 1);
  player.submit({api::CommandKind::PlayPause, {}});
  player.service();
  RPCMP_CHECK(suite, rendered == 1 && status == 7);
  player.submit({api::CommandKind::PlayPause, {}});
  controller.input(0x40, true, 10'000'011);
  controller.input(0x30, true, 10'000'012); // B wins over A in controls.
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Stopped && status == 1);
  player.submit({api::CommandKind::PlayPause, {}}); // Restart last successfully played PCM.
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Playing && opened == 22);
  player.submit({api::CommandKind::Stop, {}});

  // Retain a rendered block under FIFO backpressure across a long pause.
  player.submit({api::CommandKind::PlayTrack, {1}});
  player.service();
  fm_free = 0;
  player.service();
  const auto pending_count = rendered;
  player.submit({api::CommandKind::PlayPause, {}});
  tick += 10'000'000;
  player.service();
  player.submit({api::CommandKind::PlayPause, {}});
  fm_free = 1024;
  player.service();
  RPCMP_CHECK(suite, rendered == pending_count && player.snapshot().state == api::State::Playing);
  end_block = true;
  player.service();
  player.submit({api::CommandKind::PlayPause, {}});
  tick += 10'000'000;
  status |= 8; // A completion observed after the pause must wait for resume.
  player.service();
  RPCMP_CHECK(suite,
              player.snapshot().state == api::State::Paused && player.snapshot().track.value == 1);
  status &= ~8U;
  player.submit({api::CommandKind::PlayPause, {}});
  player.service(); // The old EOF deadline excludes paused wall time.
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Playing);
  status = 9;
  player.service();
  player.service();
  RPCMP_CHECK(suite,
              player.snapshot().state == api::State::Playing && player.snapshot().track.value == 2);

  player.service();
  pause_stuck = true;
  player.submit({api::CommandKind::PlayPause, {}});
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Error &&
                         player.snapshot().error == api::Error::Timeout && status == 1);
  pause_stuck = false;
  player.submit({api::CommandKind::PlayTrack, {2}});
  player.service();
  status = 0x17;
  player.submit({api::CommandKind::PlayPause, {}});
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
    controller.input(0x20, true, 4);
    controller.input(0, true, 5);
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
void panel_x(rpcmp::test::Suite& suite) {
  Many lists{2};
  Sink sink;
  ui::Controller controller{lists, sink};
  controller.input(0, true, 0);
  controller.input(0x40, true, 1);
  RPCMP_CHECK(suite, controller.view().panel == ui::Panel::Controls);
  controller.input(0x40, true, 1'000'001);
  RPCMP_CHECK(suite, controller.view().panel == ui::Panel::Controls);
  controller.input(0, true, 1'000'002);
  controller.input(0x40, true, 1'000'003);
  RPCMP_CHECK(suite, controller.view().panel == ui::Panel::List);
}
void info_clock(rpcmp::test::Suite& suite) {
  Many lists{2};
  Sink sink;
  ui::Controller controller{lists, sink};
  controller.input(0, true, 0);
  api::PlayerSnapshot snapshot{api::kVersion, 1, api::State::Playing, {1}, api::Error::None};
  snapshot.last_played = {1};
  controller.update(snapshot);
  controller.input(0, true, 1'200'000);
  RPCMP_CHECK(suite, controller.view().info_scroll_tick == 24);
  snapshot.sequence++;
  snapshot.elapsed_seconds = 1;
  controller.update(snapshot);
  RPCMP_CHECK(suite, controller.view().info_scroll_tick == 24);
  controller.input(0x40, true, 1'250'000);
  snapshot.sequence++;
  snapshot.state = api::State::Paused;
  controller.update(snapshot);
  RPCMP_CHECK(suite, controller.view().info_scroll_tick == 25);
  controller.input(0, true, 61'250'000);
  RPCMP_CHECK(suite, controller.view().info_scroll_tick == 1225);
  snapshot.sequence++;
  snapshot.state = api::State::Stopped;
  controller.update(snapshot);
  RPCMP_CHECK(suite, controller.view().info_scroll_tick == 1225);
  controller.input(0x40, true, 61'250'001);
  controller.input(2, true, 61'250'002); // Browsing must not restart information scrolling.
  RPCMP_CHECK(suite, controller.view().info_scroll_tick == 1225);
  snapshot.sequence++;
  snapshot.last_played = {301};
  controller.update(snapshot);
  RPCMP_CHECK(suite, controller.view().info_scroll_tick == 0);
  controller.input(0, true, 61'750'002);
  RPCMP_CHECK(suite, controller.view().info_scroll_tick == 10);
}
void controls_and_time(rpcmp::test::Suite& suite) {
  Many lists{2};
  SequencePort port;
  rpcmp::player::minimal::Player player{lists, port};
  RPCMP_CHECK(suite, player.initialize());
  ui::Controller controller{lists, player};
  controller.input(0, true, 0);
  std::uint32_t now{};
  const auto press = [&](std::uint32_t key) {
    controller.input(0, true, ++now);
    controller.update(player.snapshot());
    controller.input(key, true, ++now);
    controller.update(player.snapshot());
  };
  player.submit({api::CommandKind::PlayPause, {}});
  player.submit({api::CommandKind::Previous, {}});
  player.submit({api::CommandKind::Next, {}});
  press(0x20); // Root B has no effect.
  press(0x40);
  RPCMP_CHECK(suite, controller.view().icon == ui::Icon::PlayPause);
  press(0x10);
  RPCMP_CHECK(suite, port.attempts.empty());
  press(0x40);
  press(0x10); // Open first list, then play first track.
  RPCMP_CHECK(suite, port.attempts.empty());
  press(0x10);
  press(0x380); // L, R and Y have no default actions.
  RPCMP_CHECK(suite, controller.view().panel == ui::Panel::List);
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Playing &&
                         player.snapshot().repeat == api::RepeatMode::Two);
  press(0x40);
  now += 1'000'000;
  controller.input(0x40, true, now); // Holding X does not toggle again.
  RPCMP_CHECK(suite, controller.view().panel == ui::Panel::Controls);
  press(4);
  RPCMP_CHECK(suite, !controller.view().enabled[0]);
  const auto attempts = port.attempts;
  press(0x10);
  RPCMP_CHECK(suite, port.attempts == attempts);
  press(8);
  port.seconds = 83;
  player.service();
  press(0x10);
  const auto paused = player.snapshot();
  RPCMP_CHECK(suite, paused.state == api::State::Paused && paused.elapsed_seconds == 83);
  port.seconds = 100; // A paused Core must not advance the published clock.
  player.service();
  RPCMP_CHECK(suite, player.snapshot().sequence == paused.sequence &&
                         player.snapshot().elapsed_seconds == 83);
  press(0x40);
  press(0x20);
  press(2);
  press(0x10);
  press(8); // Browse track 14 in the other list, without changing playback.
  RPCMP_CHECK(suite, controller.view().playlist.value == 2 && controller.view().selected == 13 &&
                         controller.view().playing_number == 1);
  press(0x40);
  RPCMP_CHECK(suite, controller.view().icon == ui::Icon::PlayPause);
  press(8);
  press(0x10); // Next starts track 2 of the playback list, even while paused.
  RPCMP_CHECK(suite, player.snapshot().track.value == 2 && player.snapshot().playlist.value == 1 &&
                         player.snapshot().state == api::State::Playing &&
                         player.snapshot().elapsed_seconds == 0 &&
                         controller.view().selected == 13);
  press(0x20);
  press(4);
  press(0x10); // Restart last played, not the browsing selection.
  RPCMP_CHECK(suite,
              player.snapshot().track.value == 2 && player.snapshot().state == api::State::Playing);
  press(2);
  press(0x10);
  now += 1'000'000;
  controller.input(0x10, true, now);
  RPCMP_CHECK(suite, player.snapshot().repeat == api::RepeatMode::Three);
  press(0x140); // X with unassigned L still toggles once.
  RPCMP_CHECK(suite, controller.view().panel == ui::Panel::List);
  press(0x40);
  RPCMP_CHECK(suite, controller.view().icon == ui::Icon::Repeat);
  press(4);
  press(0x10);
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Stopped);
  press(1);
  press(0x10);
  RPCMP_CHECK(suite,
              player.snapshot().track.value == 1 && player.snapshot().state == api::State::Playing);
  player.submit({api::CommandKind::PlayTrack, {300}});
  player.submit({api::CommandKind::PlayPause, {}});
  const auto edge = player.snapshot();
  player.submit({api::CommandKind::Next, {}});
  RPCMP_CHECK(suite, player.snapshot().sequence == edge.sequence &&
                         player.snapshot().state == api::State::Paused);
  port.bad_open = 301;
  player.submit({api::CommandKind::PlayTrack, {301}});
  RPCMP_CHECK(suite, player.snapshot().last_played.value == 300);
  player.submit({api::CommandKind::Stop, {}});
  player.submit({api::CommandKind::PlayPause, {}});
  RPCMP_CHECK(suite, player.snapshot().track.value == 300 && player.snapshot().playlist.value == 1);
  port.seconds = 2;
  player.service();
  const auto clock = player.snapshot();
  player.service();
  RPCMP_CHECK(suite, clock.elapsed_seconds == 2 && player.snapshot().sequence == clock.sequence);
  ui::Bindings remapped;
  remapped.panel = 0x400;
  remapped.confirm = 0x80;
  ui::Controller custom{lists, player, remapped};
  custom.update(player.snapshot());
  custom.input(0x400, true, 0);
  custom.input(0x400, true, 900'000);
  RPCMP_CHECK(suite, custom.view().panel == ui::Panel::List);
  custom.input(0, true, 900'001);
  custom.input(0x408, true, 900'002);
  custom.input(8, true, 1'900'002);
  RPCMP_CHECK(suite, custom.view().panel == ui::Panel::Controls &&
                         custom.view().icon == ui::Icon::PlayPause);
  custom.input(0, true, 1'900'003);
  custom.input(0x80, true, 1'900'004);
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Paused);
  Source single;
  single.data = split_fixture();
  const auto catalog = std::make_unique<lib::PreparedPlaylist>();
  RPCMP_CHECK(suite, catalog->open(single));
  ui::Controller one_entry{*catalog, player};
  auto snapshot = player.snapshot();
  snapshot.last_played = {1};
  one_entry.update(snapshot);
  RPCMP_CHECK(suite, !one_entry.view().enabled[0] && !one_entry.view().enabled[2]);
}
void consumed_time(rpcmp::test::Suite& suite) {
  Source source;
  const auto catalog = std::make_unique<lib::PreparedPlaylist>();
  RPCMP_CHECK(suite, catalog->open(source));
  reset_model();
  model_frames = 1000;
  pocket::HybridPlayback backend{*catalog};
  rpcmp::player::minimal::Player player{*catalog, backend};
  RPCMP_CHECK(suite, player.initialize());
  player.submit({api::CommandKind::PlayTrack, {2}});
  for (unsigned i = 0; i < 63; ++i)
    player.service();
  RPCMP_CHECK(suite, rendered == 63 && player.snapshot().elapsed_seconds == 0);
  fm_free = 0;           // Retain the next block; queued frames must not count as elapsed.
  pcm_free = 4096 - 501; // 63000 submitted - 501 queued = 62499 consumed.
  player.service();
  RPCMP_CHECK(suite, player.snapshot().elapsed_seconds == 0);
  pcm_free = 4096 - 500; // Exactly one second at the HYB4 source rate.
  player.service();
  RPCMP_CHECK(suite, player.snapshot().elapsed_seconds == 1);
  player.submit({api::CommandKind::PlayPause, {}});
  const auto paused = player.snapshot();
  tick += 60'000'000;
  pcm_free = 4096;
  for (unsigned i = 0; i < 50; ++i)
    player.service();
  RPCMP_CHECK(suite, player.snapshot().sequence == paused.sequence &&
                         player.snapshot().elapsed_seconds == 1);
  player.submit({api::CommandKind::PlayPause, {}});
  fm_free = 1024;
  player.service();
  RPCMP_CHECK(suite, rendered == 64 && player.snapshot().state == api::State::Playing);
  status = 9;
  player.service();
  RPCMP_CHECK(suite, player.snapshot().state == api::State::Ended &&
                         player.snapshot().elapsed_seconds == 1);
  player.submit({api::CommandKind::Stop, {}});
  RPCMP_CHECK(suite, player.snapshot().elapsed_seconds == 0);
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
  playing.last_played = {2};
  playing.elapsed_seconds = 83;
  controller.update(playing);
  pocket::MinimalDisplay display;
  display.begin(controller.view());
  Bytes guarded(std::size_t{640} * 480 + 2, 0xee);
  auto* pixels = guarded.data() + 1;
  RPCMP_CHECK(suite, !display.pump(pixels, std::size_t{640} * 480 - 1));
  while (!display.complete())
    RPCMP_CHECK(suite, display.pump(pixels, std::size_t{640} * 480));
  RPCMP_CHECK(suite, guarded.front() == 0xee && guarded.back() == 0xee);
  // Authored layout: list starts at y70, pitch22. Cursor on row0, playing row1.
  RPCMP_CHECK(suite, pixels[70 * 640 + 14] == 2 && pixels[92 * 640 + 14] == 0);
  RPCMP_CHECK(suite, pixels[100 * 640 + 18] == 7 && pixels[78 * 640 + 18] != 7);
  RPCMP_CHECK(suite, pixels[40 * 640 + 8] == 3 && pixels[370 * 640 + 440] == 5);
  // The info row is playing entry 2/2, not browsing entry 1.
  const auto number = pocket::bitmap_glyph('2');
  for (unsigned y = 0; y < 16; ++y)
    for (unsigned x = 0; x < 8; ++x)
      RPCMP_CHECK(suite,
                  pixels[(426 + y) * 640 + 20 + x] == ((number.rows[y] & (128U >> x)) ? 3 : 0));
  const auto glyph = pocket::bitmap_glyph('R');
  for (unsigned y = 0; y < 16; ++y)
    for (unsigned x = 0; x < 8; ++x)
      RPCMP_CHECK(suite,
                  pixels[(16 + y) * 640 + 16 + x] == ((glyph.rows[y] & (128U >> x)) ? 3 : 0));
  auto paused = playing;
  paused.sequence = 2;
  paused.state = api::State::Paused;
  paused.repeat = api::RepeatMode::One;
  controller.update(paused);
  controller.input(0x40, true, 2);
  display.begin(controller.view());
  while (!display.complete())
    RPCMP_CHECK(suite, display.pump(pixels, std::size_t{640} * 480));
  RPCMP_CHECK(suite, pixels[70 * 640 + 14] == 6 && pixels[100 * 640 + 18] == 7);
  RPCMP_CHECK(suite, pixels[40 * 640 + 8] == 5 && pixels[370 * 640 + 440] == 3);
  RPCMP_CHECK(suite, !controller.view().enabled[2]);
  auto long_view = controller.view();
  long_view.panel = ui::Panel::List;
  long_view.titles[0].length = 90;
  std::fill_n(long_view.titles[0].bytes.data(), 90, 'W');
  long_view.titles[1].length = 67; // Exactly 536 pixels: no ellipsis is needed.
  std::fill_n(long_view.titles[1].bytes.data(), 67, 'W');
  RPCMP_CHECK(suite, pocket::MinimalDisplay::marquee_needed(long_view));
  long_view.scroll_tick = 19; // Dwell must not move the selected row.
  Bytes first(std::size_t{640} * 480), second(first.size());
  display.begin(long_view);
  while (!display.complete())
    RPCMP_CHECK(suite, display.pump(first.data(), first.size()));
  const auto fitting = pocket::bitmap_glyph('W');
  for (unsigned y = 0; y < 16; ++y)
    for (unsigned x = 0; x < 8; ++x)
      RPCMP_CHECK(suite,
                  first[(95 + y) * 640 + 600 + x] == ((fitting.rows[y] & (128U >> x)) ? 1 : 0));
  long_view.scroll_tick = 0;
  display.begin(long_view);
  while (!display.complete())
    RPCMP_CHECK(suite, display.pump(second.data(), second.size()));
  RPCMP_CHECK(suite, first == second);
  long_view.scroll_tick = 24;
  display.begin(long_view);
  while (!display.complete())
    RPCMP_CHECK(suite, display.pump(second.data(), second.size()));
  RPCMP_CHECK(suite, first != second);
  // Scrolling is clipped inside the selected name; all other pixels stay identical.
  for (unsigned y = 0; y < 480; ++y)
    for (unsigned x = 0; x < 640; ++x)
      if (y < 73 || y >= 89 || x < 72 || x >= 608)
        RPCMP_CHECK(suite, first[static_cast<std::size_t>(y) * 640 + x] ==
                               second[static_cast<std::size_t>(y) * 640 + x]);
  long_view.panel = ui::Panel::Controls;
  RPCMP_CHECK(suite, !pocket::MinimalDisplay::marquee_needed(long_view));
  long_view.playing_title.length = 60; // 480 pixels in a 400-pixel field.
  long_view.playing_list.length = 70;  // 560 pixels, with a different end dwell.
  std::fill_n(long_view.playing_title.bytes.data(), 60, 'W');
  std::fill_n(long_view.playing_list.bytes.data(), 70, 'M');
  long_view.playing_title.bytes[0] = 'I';
  long_view.playing_list.bytes[0] = 'I';
  RPCMP_CHECK(suite, pocket::MinimalDisplay::marquee_needed(long_view));
  const auto render = [&](std::uint32_t tick_value, Bytes& target) {
    long_view.info_scroll_tick = tick_value;
    display.begin(long_view);
    while (!display.complete())
      RPCMP_CHECK(suite, display.pump(target.data(), target.size()));
  };
  render(0, first);
  render(19, second);
  RPCMP_CHECK(suite, first == second);
  render(40, second); // One second of motion after the dwell: exactly 32 pixels.
  RPCMP_CHECK(suite, first != second);
  for (unsigned y = 0; y < 480; ++y)
    for (unsigned x = 0; x < 640; ++x) {
      const bool info = ((y >= 378 && y < 394) || (y >= 402 && y < 418)) && x >= 20 && x < 420;
      const auto pos = static_cast<std::size_t>(y) * 640 + x;
      if (!info)
        RPCMP_CHECK(suite, first[pos] == second[pos]);
      else if (x < 388)
        RPCMP_CHECK(suite, first[pos + 32] == second[pos]);
    }
  render(70, first); // Title has reached its 80-pixel end; list continues moving.
  render(89, second);
  constexpr auto info_begin = std::ptrdiff_t{378} * 640;
  constexpr auto info_end = std::ptrdiff_t{394} * 640;
  RPCMP_CHECK(suite, std::equal(first.begin() + info_begin, first.begin() + info_end,
                                second.begin() + info_begin));
  render(90, first); // Title cycle restarts independently of the longer playlist.
  render(0, second);
  RPCMP_CHECK(suite, std::equal(first.begin() + info_begin, first.begin() + info_end,
                                second.begin() + info_begin));
  RPCMP_CHECK(suite, first != second);
  long_view.playing_title.length = long_view.playing_list.length = 50; // Exactly 400 pixels.
  RPCMP_CHECK(suite, !pocket::MinimalDisplay::marquee_needed(long_view));
  render(0, first);
  render(40, second);
  RPCMP_CHECK(suite, first == second);
  if (path) {
    std::ofstream file(path, std::ios::binary);
    file << "P6\n640 480\n255\n";
    for (std::size_t i = 0; i < std::size_t{640} * 480; ++i) {
      const auto color = pocket::kMinimalPalette[pixels[i]];
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
  block.first_frame = rendered * model_frames;
  block.frame_count = model_frames;
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
  panel_x(suite);
  info_clock(suite);
  controls_and_time(suite);
  consumed_time(suite);
  preview(suite, argc == 2 ? argv[1] : nullptr);
  const auto headless = playback_case(suite, false), delayed = playback_case(suite, true);
  RPCMP_CHECK(suite, headless == delayed);
  RPCMP_CHECK(suite, headless.size() > 100);
  // Per block: timestamp, FM operation, then PCM left/right pairs.
  RPCMP_CHECK(suite, headless[2].first == 8 && headless[3].first == 9 && headless[4].first == 4);
  return suite.finish("minimal player catalog/control/audio/display");
}
