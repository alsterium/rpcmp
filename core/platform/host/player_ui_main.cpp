#include "rpcmp/platform/host/svg_canvas.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {
namespace c = rpcmp::contracts;
namespace api = c::v2;
namespace ui = rpcmp::ui::v2;
c::CatalogText text(const std::string_view value, const bool truncated = false) {
  c::CatalogText result;
  if (value.size() > result.bytes.size()) {
    std::cerr << "Authored mock text exceeds the catalog bound\n";
    std::exit(1);
  }
  std::copy(value.begin(), value.end(), result.bytes.begin());
  result.length = static_cast<std::uint16_t>(value.size());
  result.truncated = truncated;
  return result;
}
class Catalog final : public c::CatalogReader {
public:
  c::CatalogStatus status() const noexcept override { return state; }
  c::CatalogTrackItem track(const c::AlbumId album, const std::uint32_t ordinal) const {
    auto title = text("FM TEST " + std::to_string(ordinal + 1));
    if (album.value == 1 && ordinal == 5)
      title = long_title
                  ? text("あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほABCDEF", true)
                  : text("FM TEST 06 / 夜のテスト");
    return {{album.value * 100 + ordinal}, album, ordinal, title};
  }
  static c::CatalogText album_name(const c::AlbumId album) {
    return text(album.value == 1 ? "RPCMP DEMO / テストアルバム" : "SECOND ALBUM");
  }
  c::CatalogAlbumPage albums(const c::CatalogPageQuery& query) const override {
    c::CatalogAlbumPage result;
    result.header = header(query, 2);
    for (std::uint16_t i = 0; i < result.header.count; ++i) {
      const auto ordinal = query.start_ordinal + i;
      const c::AlbumId id{ordinal + 1U};
      result.items[i] = {id, ordinal, album_name(id), 12};
    }
    return result;
  }
  c::CatalogTrackPage tracks(const c::AlbumId album,
                             const c::CatalogPageQuery& query) const override {
    c::CatalogTrackPage result;
    result.album_id = album;
    result.header = header(query, 12);
    for (std::uint16_t i = 0; i < result.header.count; ++i)
      result.items[i] = track(album, query.start_ordinal + i);
    return result;
  }
  c::CatalogPageHeader header(const c::CatalogPageQuery& query, const std::uint32_t total) const {
    c::CatalogPageHeader result;
    result.query = query;
    if (state.phase != c::CatalogPhase::Ready)
      result.error = c::CatalogQueryError::Unavailable;
    else if (query.generation != state.generation)
      result.error = c::CatalogQueryError::StaleLibrary;
    else if (query.start_ordinal > total)
      result.error = c::CatalogQueryError::InvalidStart;
    else {
      result.total = total;
      result.count = static_cast<std::uint16_t>(
          std::min<std::uint32_t>(query.limit, total - query.start_ordinal));
      if (query.start_ordinal + result.count < total)
        result.next_start = query.start_ordinal + result.count;
    }
    return result;
  }
  c::CatalogStatus state{1, {1}, c::CatalogPhase::Ready, c::CatalogFailure::None, 2, 24};
  bool long_title{};
};
class Commands final : public api::CommandIngress {
public:
  api::CommandResult submit(const api::PlayerCommand& command) override {
    ++calls;
    return {api::kSchemaVersion,
            command.command_id,
            api::CommandOutcome::Rejected,
            api::CommandReason::UnsupportedCapability,
            command.expected_snapshot_sequence.value_or(0),
            std::nullopt};
  }
  unsigned calls{};
};
api::PlayerSnapshot snapshot(Catalog& catalog, const std::string_view mode) {
  api::PlayerSnapshot result;
  result.sequence = 1;
  result.capabilities.bits |= api::kTransportCommands | api::kStatePreservingPause |
                              api::kPolicyObservations | api::kRepeatControl |
                              api::kPolicyCommandResults | api::kPlaybackNavigation |
                              api::kPerformanceHistory | api::kPlaybackSettings;
  result.library = catalog.state;
  result.play_generation = 1;
  result.transport = result.projected =
      mode == "paused" ? c::TransportState::Paused : c::TransportState::Playing;
  result.track = api::SelectedTrack{catalog.track({1}, 5), Catalog::album_name({1})};
  result.position_frames = 7'344'000;
  result.prepared = true;
  result.policy = api::PlaybackPolicyObservation{};
  result.policy_commands = api::PolicyCommandObservation{};
  result.navigation = api::PlaybackNavigationObservation{true, true, std::nullopt};
  result.settings = api::PlaybackSettingsObservation{1, 1, api::SettingsRestore::Restored,
                                                     api::SettingsSave::Saved, std::nullopt};
  api::PerformanceHistorySnapshot history;
  history.play_generation = 1;
  history.observed_through_frame = result.position_frames;
  history.channels = api::unknown_performance_channels();
  history.availability = api::PerformanceAvailability::Available;
  for (std::uint16_t row = 0; row < 16; ++row) {
    for (std::uint16_t channel = 0; channel < api::kPerformanceChannels; ++channel) {
      const bool on = (row + channel) % 5 != 0;
      api::PerformanceChannel state{
          channel, on, static_cast<std::uint8_t>(48 + (row + channel * 3) % 36), std::int16_t{0},
          api::MdxFmV1{static_cast<std::uint8_t>(0x20 + channel)}};
      if (channel == 7) {
        state.note.reset();
        state.fine_pitch_cents.reset();
        state.mdx_fm.reset();
      }
      const auto index = history.count++;
      const auto sequence =
          static_cast<std::uint64_t>(index) + 1 + (mode == "gap" && index >= 64 ? 4 : 0);
      history.events[index] = {sequence,
                               {static_cast<std::uint64_t>(row) * 4096, state,
                                !on        ? api::PerformanceKind::KeyOff
                                : row == 7 ? api::PerformanceKind::InstrumentChanged
                                           : api::PerformanceKind::KeyOn}};
      history.channels[channel] = state;
      history.next_sequence = sequence + 1;
    }
  }
  if (mode == "gap") {
    history.capture_lost = true;
    history.availability = api::PerformanceAvailability::Degraded;
  }
  if (mode == "empty" || mode == "error") {
    result.position_frames = 0;
    result.prepared = false;
    result.navigation = api::PlaybackNavigationObservation{};
    history = {};
    history.play_generation = 1;
    history.channels = api::unknown_performance_channels();
    if (mode == "empty") {
      catalog.state = {1, {1}, c::CatalogPhase::Empty, c::CatalogFailure::None, 0, 0};
      result.library = catalog.state;
      result.track.reset();
      result.transport = result.projected = c::TransportState::Empty;
    } else {
      result.transport = result.projected = c::TransportState::Error;
      result.error = api::PlaybackError{api::PlaybackErrorCode::Preparation, false};
    }
  }
  if (mode == "unsupported")
    result.capabilities.bits &= ~api::kPerformanceHistory;
  else
    result.performance_history = history;
  return result;
}
} // namespace

int main(int argc, char** argv) {
  const std::string_view mode = argc == 4 ? argv[3] : "playing";
  if ((argc != 3 && argc != 4) ||
      (std::string_view{argv[1]} != "tracker" && std::string_view{argv[1]} != "keyboard" &&
       std::string_view{argv[1]} != "library") ||
      (mode != "playing" && mode != "paused" && mode != "empty" && mode != "error" &&
       mode != "gap" && mode != "long" && mode != "unsupported")) {
    std::cerr << "Usage: rpcmp_player_ui_mock tracker|keyboard|library output.svg "
                 "[playing|paused|empty|error|gap|long|unsupported]\n";
    return 2;
  }
  Catalog catalog;
  catalog.long_title = mode == "long";
  Commands commands;
  ui::PlayerUi player(commands, catalog);
  const auto observation = snapshot(catalog, mode);
  if (!api::valid_player_snapshot(observation)) {
    std::cerr << "Invalid authored mock snapshot\n";
    return 1;
  }
  player.update(observation);
  const std::string_view main_view = argv[1];
  if (main_view != "tracker") {
    player.dispatch(ui::Action::Down);
    player.dispatch(ui::Action::Left);
    player.dispatch(ui::Action::Confirm);
    if (main_view == "library") {
      player.dispatch(ui::Action::Confirm);
      player.dispatch(ui::Action::Left);
      if (mode != "empty") {
        player.dispatch(ui::Action::Confirm);
        for (unsigned i = 0; i < 5; ++i)
          player.dispatch(ui::Action::Down);
      }
    }
  }
  if (commands.calls != 0) {
    std::cerr << "Display setup submitted an unexpected command\n";
    return 1;
  }
  std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
  if (!output) {
    std::cerr << "Cannot open SVG output\n";
    return 1;
  }
  rpcmp::platform::host::SvgCanvas canvas(output);
  ui::render_player(player.view(), canvas);
  const bool drawn = canvas.finish();
  output.close();
  if (!drawn || !output) {
    std::cerr << "SVG rendering failed\n";
    return 1;
  }
  return 0;
}
