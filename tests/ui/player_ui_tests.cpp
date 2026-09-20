#include "rpcmp/ui/player_ui.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace rpcmp::ui::v2;
namespace c = rpcmp::contracts;
using c::TransportState;
template <typename T> const T& required(const std::optional<T>& value) {
  if (!value)
    throw std::logic_error("missing authored value");
  return *value;
}
template <typename T> T& required(std::optional<T>& value) {
  if (!value)
    throw std::logic_error("missing authored value");
  return *value;
}
c::CatalogText text(const std::string& value) {
  c::CatalogText result;
  if (value.size() > result.bytes.size())
    throw std::logic_error("oversized authored title");
  std::copy(value.begin(), value.end(), result.bytes.begin());
  result.length = static_cast<std::uint16_t>(value.size());
  return result;
}
class Catalog final : public c::CatalogReader {
public:
  c::CatalogStatus status() const noexcept override { return state; }
  c::CatalogAlbumPage albums(const c::CatalogPageQuery& query) const override {
    ++reads;
    c::CatalogAlbumPage result;
    result.header = header(query, state.album_count);
    for (std::uint16_t i = 0; i < result.header.count; ++i) {
      const auto ordinal = query.start_ordinal + i;
      result.items[i] = {{10U + ordinal},
                         ordinal,
                         text("Album " + std::to_string(ordinal)),
                         state.album_count == 300 ? 1U
                         : ordinal == 0           ? 20U
                                                  : 2U};
    }
    if (corrupt)
      result.header.count = 17;
    return result;
  }
  c::CatalogTrackPage tracks(c::AlbumId album, const c::CatalogPageQuery& query) const override {
    ++reads;
    c::CatalogTrackPage result;
    result.album_id = album;
    result.header = header(query, state.album_count == 300 ? 1U : album.value == 10 ? 20U : 2U);
    for (std::uint16_t i = 0; i < result.header.count; ++i) {
      const auto ordinal = query.start_ordinal + i;
      result.items[i] = {
          {album.value * 100 + ordinal}, album, ordinal, text("Track " + std::to_string(ordinal))};
    }
    if (wrong_query)
      ++result.header.query.generation.value;
    return result;
  }
  static c::CatalogPageHeader header(const c::CatalogPageQuery& query, std::uint32_t total) {
    c::CatalogPageHeader result;
    result.query = query;
    if (query.start_ordinal > total) {
      result.error = c::CatalogQueryError::InvalidStart;
      return result;
    }
    result.total = total;
    result.count = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(query.limit, total - query.start_ordinal));
    const auto end = query.start_ordinal + result.count;
    if (end < total)
      result.next_start = end;
    return result;
  }
  c::CatalogStatus state{1, {1}, c::CatalogPhase::Ready, c::CatalogFailure::None, 2, 22};
  mutable unsigned reads{};
  bool corrupt{}, wrong_query{};
};
class Commands final : public api::CommandIngress {
public:
  api::CommandResult submit(const api::PlayerCommand& command) override {
    sent.push_back(command);
    return {api::kSchemaVersion,
            command.command_id + (wrong_id ? 1 : 0),
            rejection == api::CommandReason::None ? api::CommandOutcome::Accepted
                                                  : api::CommandOutcome::Rejected,
            rejection,
            required(command.expected_snapshot_sequence),
            std::nullopt};
  }
  std::vector<api::PlayerCommand> sent;
  api::CommandReason rejection{api::CommandReason::None};
  bool wrong_id{};
};
struct Rig {
  explicit Rig(rpcmp::test::Suite& checks, UiConfiguration config = {})
      : suite(checks), ui(commands, catalog, config) {
    snapshot.sequence = 10;
    snapshot.published_at_us = 100;
    snapshot.library = catalog.state;
    snapshot.capabilities.bits |= api::kTransportCommands | api::kStatePreservingPause |
                                  api::kPolicyObservations | api::kRepeatControl |
                                  api::kPolicyCommandResults | api::kPlaybackNavigation;
    snapshot.policy = api::PlaybackPolicyObservation{};
    snapshot.policy_commands = api::PolicyCommandObservation{};
    snapshot.navigation = api::PlaybackNavigationObservation{true, false, std::nullopt};
    snapshot.transport = snapshot.projected = TransportState::Playing;
    snapshot.play_generation = 2;
    snapshot.track = api::SelectedTrack{{{1000}, {10}, 0, text("Current title")}, text("Album 0")};
    snapshot.position_frames = 96'000;
    snapshot.prepared = true;
    publish();
  }
  void publish() {
    RPCMP_CHECK(suite, api::valid_player_snapshot(snapshot));
    ui.update(snapshot);
  }
  void advance() {
    ++snapshot.sequence;
    ++snapshot.published_at_us;
    publish();
  }
  void action(Action action) { ui.dispatch(action); }
  void library() {
    action(Action::Down);    // Repeat
    action(Action::Left);    // View
    action(Action::Confirm); // Keyboard
    action(Action::Confirm); // Library
    action(Action::Left);    // List
    RPCMP_CHECK(suite, ui.view().main == View::Library && ui.view().focus == Focus::List);
  }
  void stopped() {
    ++snapshot.play_generation;
    snapshot.transport = snapshot.projected = TransportState::Stopped;
    snapshot.position_frames = 0;
    snapshot.prepared = false;
    snapshot.silence_confirmed = true;
    snapshot.pending_intent.reset();
    snapshot.preparation.reset();
    snapshot.audio_control.reset();
    advance();
  }
  void policy_ack(api::PolicyCommandOutcome outcome = api::PolicyCommandOutcome::Applied) {
    const auto& command = commands.sent.back();
    RPCMP_CHECK(suite, command.kind == api::CommandKind::SetPlaybackPolicy);
    if (outcome == api::PolicyCommandOutcome::Applied) {
      required(snapshot.policy).desired = required(command.policy);
      ++required(snapshot.policy).revision;
    }
    required(snapshot.policy_commands).last = {command.command_id,
                                               required(snapshot.policy).revision, outcome};
    advance();
  }
  rpcmp::test::Suite& suite;
  Catalog catalog;
  Commands commands;
  PlayerUi ui;
  api::PlayerSnapshot snapshot;
};

void input_edges(rpcmp::test::Suite& suite) {
  InputMapper mapper;
  RPCMP_CHECK(suite, mapper.sample({1}, 0) == Action::None); // held at connection
  RPCMP_CHECK(suite, mapper.sample({1}, 100) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({0}, 101) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({3}, 102) == Action::Back); // B wins A, A is discarded
  RPCMP_CHECK(suite, mapper.sample({1}, 103) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({0}, 104) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({13}, 105) == Action::Confirm); // L+R cancel
  RPCMP_CHECK(suite, mapper.sample({4}, 106) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({0}, 107) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({8}, 108) == Action::Next);
  RPCMP_CHECK(suite, mapper.sample({8}, 900'000) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({16}, 1'000'000) == Action::Up);
  RPCMP_CHECK(suite, mapper.sample({16}, 1'349'999) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({16}, 1'350'000) == Action::Up);
  RPCMP_CHECK(suite, mapper.sample({16}, 1'429'999) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({16}, 8'000'000) == Action::Up);
  RPCMP_CHECK(suite, mapper.sample({16}, 8'000'001) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({16 | 32 | 128}, 8'000'002) == Action::Right);
  RPCMP_CHECK(suite, mapper.sample({16 | 64}, 8'000'003) == Action::Up);
  RPCMP_CHECK(suite, mapper.sample({1, false}, 8'000'004) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({1}, 8'000'005) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({0}, 8'000'006) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({1}, 8'000'007) == Action::Confirm);
  RPCMP_CHECK(suite, mapper.sample({1}, 5) == Action::None); // clock reversal rearms
  RPCMP_CHECK(suite, mapper.sample({1}, 6) == Action::None);
  InputBindings swapped;
  std::swap(swapped.masks[0], swapped.masks[3]);
  mapper.rebind(swapped);
  RPCMP_CHECK(suite, mapper.sample({2}, 7) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({0}, 8) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({2}, 9) == Action::Confirm);
  swapped.masks[0] = swapped.masks[1];
  mapper.rebind(swapped);
  RPCMP_CHECK(suite, !mapper.valid() && mapper.sample({0xFFFF}, 10) == Action::None);
  swapped = {};
  swapped.repeat_delay_us = 0;
  mapper.rebind(swapped);
  RPCMP_CHECK(suite, !mapper.valid());
  mapper.rebind({});
  constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
  RPCMP_CHECK(suite, mapper.sample({0}, maximum - 1) == Action::None);
  RPCMP_CHECK(suite, mapper.sample({32}, maximum) == Action::Down);
  RPCMP_CHECK(suite, mapper.sample({32}, maximum) == Action::None);
}

void transport_and_stop(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.action(Action::Confirm);
  RPCMP_CHECK(suite, rig.commands.sent.size() == 1 &&
                         rig.commands.sent.back().kind == api::CommandKind::Pause);
  rig.action(Action::Confirm);
  RPCMP_CHECK(suite, rig.commands.sent.size() == 1 && rig.ui.view().diagnostic == Diagnostic::Busy);
  rig.snapshot.projected = TransportState::Paused;
  rig.snapshot.audio_control =
      api::PendingAudioControl{2, 2, api::AudioControlKind::Pause, std::nullopt};
  rig.advance();
  RPCMP_CHECK(suite, rig.ui.view().transport_pending);
  rig.snapshot.transport = TransportState::Paused;
  rig.snapshot.audio_control.reset();
  rig.advance();
  rig.action(Action::Confirm);
  RPCMP_CHECK(suite, rig.commands.sent.back().kind == api::CommandKind::Resume);
  rig.action(Action::Back); // supersedes unobserved Resume
  RPCMP_CHECK(suite, rig.commands.sent.back().kind == api::CommandKind::Stop);
  rig.action(Action::Back);
  RPCMP_CHECK(suite, rig.commands.sent.size() == 3 && rig.ui.view().main == View::Tracker);
  ++rig.snapshot.play_generation;
  rig.snapshot.transport = rig.snapshot.projected = TransportState::Stopped;
  rig.snapshot.position_frames = 0;
  rig.snapshot.prepared = false;
  rig.advance();
  rig.action(Action::Back);
  RPCMP_CHECK(suite, rig.ui.view().transport_pending && rig.ui.view().main == View::Tracker);
  rig.snapshot.silence_confirmed = true;
  rig.snapshot.preparation = api::PendingPreparation{3, 2, {{1}, {1000}}, true};
  rig.advance();
  RPCMP_CHECK(suite, rig.ui.view().transport_pending);
  rig.snapshot.preparation.reset();
  rig.advance();
  RPCMP_CHECK(suite, !rig.ui.view().transport_pending && rig.ui.view().main == View::Tracker);
  rig.action(Action::Back);
  RPCMP_CHECK(suite, rig.ui.view().main == View::Tracker);
  rig.library();
  rig.action(Action::Confirm);
  rig.action(Action::Back);
  RPCMP_CHECK(suite,
              rig.ui.view().browser.level == BrowseLevel::Albums && rig.commands.sent.size() == 3);
  rig.action(Action::Right);   // PlayPause
  rig.action(Action::Right);   // Stop
  rig.action(Action::Confirm); // already stopped is a no-op, not Back or an endless wait
  RPCMP_CHECK(suite, rig.commands.sent.size() == 4 && !rig.ui.view().transport_pending &&
                         rig.ui.view().browser.level == BrowseLevel::Albums);

  Rig held(suite);
  held.ui.input({0}, 0);
  held.ui.input({2}, 1);
  held.stopped();
  held.ui.input({2}, 2);
  RPCMP_CHECK(suite, held.ui.view().main == View::Tracker);
  held.ui.input({0}, 3);
  held.ui.input({2}, 4);
  RPCMP_CHECK(suite, held.ui.view().main == View::Tracker && held.commands.sent.size() == 1);

  Rig existing(suite);
  existing.stopped();
  existing.snapshot.silence_confirmed = false;
  existing.snapshot.audio_control = api::PendingAudioControl{
      3, existing.snapshot.play_generation, api::AudioControlKind::Reset, std::nullopt};
  existing.advance();
  existing.action(Action::Back);
  existing.snapshot.audio_control.reset();
  existing.snapshot.silence_confirmed = true;
  existing.advance();
  RPCMP_CHECK(suite, !existing.ui.view().transport_pending);
  existing.action(Action::Back);
  RPCMP_CHECK(suite, existing.ui.view().main == View::Tracker);
}

void browsing(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.library();
  RPCMP_CHECK(suite, rig.commands.sent.empty() && rig.ui.view().browser.valid);
  rig.action(Action::Confirm);
  for (unsigned i = 0; i < 18; ++i)
    rig.action(Action::Down);
  RPCMP_CHECK(suite, rig.ui.view().browser.cursor == 18 &&
                         rig.ui.view().browser.tracks.header.query.start_ordinal == 16 &&
                         required(rig.ui.view().snapshot.track).item.track_id.value == 1000 &&
                         rig.commands.sent.empty());
  rig.commands.rejection = api::CommandReason::ResourceBusy;
  rig.action(Action::Confirm);
  RPCMP_CHECK(suite, rig.ui.view().main == View::Library && rig.ui.view().focus == Focus::List &&
                         rig.ui.view().rejection == api::CommandReason::ResourceBusy);
  rig.commands.rejection = api::CommandReason::None;
  rig.action(Action::Confirm);
  RPCMP_CHECK(suite, rig.ui.view().main == View::Keyboard &&
                         rig.ui.view().focus == Focus::PlayPause &&
                         required(rig.commands.sent.back().selection).track_id.value == 1018 &&
                         required(rig.commands.sent.back().expected_snapshot_sequence) == 10);
  rig.action(Action::Confirm);
  RPCMP_CHECK(suite, rig.commands.sent.size() == 2); // old Playing cannot turn selection into Pause
  ++rig.snapshot.play_generation;
  required(rig.snapshot.track).item.track_id.value = 1018;
  required(rig.snapshot.track).item.track_ordinal = 18;
  rig.snapshot.transport = rig.snapshot.projected = TransportState::Loading;
  rig.snapshot.position_frames = 0;
  rig.snapshot.preparation =
      api::PendingPreparation{4, rig.snapshot.play_generation, {{1}, {1018}}, false};
  rig.advance();
  rig.action(Action::Confirm);
  RPCMP_CHECK(suite, rig.commands.sent.size() == 2);
  rig.action(Action::Down);
  rig.action(Action::Left);
  rig.action(Action::Confirm);
  RPCMP_CHECK(suite, rig.ui.view().main == View::Library && rig.ui.view().browser.cursor == 18);
  rig.action(Action::Next);
  RPCMP_CHECK(suite, rig.commands.sent.back().kind == api::CommandKind::NextTrack &&
                         !rig.commands.sent.back().selection &&
                         rig.ui.view().focus == Focus::ViewSwitch &&
                         rig.ui.view().browser.cursor == 18);
  rig.catalog.state.generation.value = 2;
  rig.snapshot.library = rig.catalog.state;
  rig.snapshot.track.reset();
  rig.snapshot.preparation.reset();
  rig.snapshot.prepared = false;
  rig.snapshot.navigation = api::PlaybackNavigationObservation{};
  rig.snapshot.transport = rig.snapshot.projected = TransportState::Empty;
  rig.advance();
  RPCMP_CHECK(suite, rig.ui.view().browser.level == BrowseLevel::Albums &&
                         rig.ui.view().browser.cursor == 0 &&
                         rig.ui.view().browser.generation.value == 2 &&
                         rig.ui.view().focus == Focus::ViewSwitch);

  Rig bad(suite);
  bad.catalog.corrupt = true;
  bad.library();
  bad.action(Action::Confirm);
  RPCMP_CHECK(suite, !bad.ui.view().browser.valid && bad.commands.sent.empty());
  Rig stale(suite);
  stale.library();
  stale.catalog.wrong_query = true;
  stale.action(Action::Confirm);
  stale.action(Action::Confirm);
  RPCMP_CHECK(suite, !stale.ui.view().browser.valid && stale.commands.sent.empty());
  Rig changed(suite);
  changed.library();
  changed.action(Action::Confirm);
  ++changed.catalog.state.generation.value;
  changed.action(Action::Confirm);
  RPCMP_CHECK(suite, !changed.ui.view().browser.valid && changed.commands.sent.empty());

  Rig many(suite);
  many.catalog.state.album_count = many.catalog.state.track_count = 300;
  many.snapshot.library = many.catalog.state;
  many.snapshot.track =
      api::SelectedTrack{{{30900}, {309}, 0, text("Last track")}, text("Album 299")};
  many.stopped();
  many.library();
  for (unsigned i = 0; i < 299; ++i)
    many.action(Action::Down);
  many.action(Action::Confirm);
  RPCMP_CHECK(suite, many.ui.view().browser.valid &&
                         required(many.ui.view().browser.album).album_id.value == 309 &&
                         many.catalog.reads <= 21);
}

void panel_context(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.action(Action::Up);
  RPCMP_CHECK(suite, rig.ui.view().focus == Focus::List && rig.ui.view().main == View::Tracker);
  rig.action(Action::Confirm);
  RPCMP_CHECK(suite, rig.commands.sent.size() == 1 &&
                         rig.commands.sent.back().kind == api::CommandKind::Pause);
  rig.action(Action::Right);
  RPCMP_CHECK(suite, rig.ui.view().focus == Focus::PlayPause);

  Rig browse(suite);
  browse.library();
  browse.action(Action::Confirm);
  browse.action(Action::Back);
  RPCMP_CHECK(suite, browse.ui.view().main == View::Library &&
                         browse.ui.view().focus == Focus::List &&
                         browse.ui.view().browser.level == BrowseLevel::Albums &&
                         browse.commands.sent.empty()); // browsing Back must not stop audio

  Rig failed(suite);
  failed.snapshot.transport = failed.snapshot.projected = TransportState::Error;
  failed.snapshot.error = api::PlaybackError{api::PlaybackErrorCode::ResetFailed, true};
  failed.snapshot.prepared = false;
  failed.snapshot.navigation = api::PlaybackNavigationObservation{};
  failed.advance();
  failed.action(Action::Up);
  RPCMP_CHECK(suite, failed.ui.view().focus == Focus::List);
  failed.action(Action::Right);
  failed.library();
  failed.action(Action::Confirm);
  failed.action(Action::Back);
  RPCMP_CHECK(suite, failed.ui.view().browser.level == BrowseLevel::Albums &&
                         failed.commands.sent.empty() && failed.ui.view().snapshot.error);
}

void policies(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.action(Action::Down); // Repeat, not pause
  for (unsigned i = 0; i < 9; ++i)
    rig.action(Action::Confirm);
  RPCMP_CHECK(suite, rig.commands.sent.size() == 1 && rig.ui.view().queued_policy_actions == 8 &&
                         rig.ui.view().diagnostic == Diagnostic::PolicyQueueFull &&
                         required(rig.commands.sent[0].policy).count == 3U);
  rig.policy_ack();
  RPCMP_CHECK(suite,
              rig.commands.sent.size() == 2 && required(rig.commands.sent[1].policy).count == 5U);
  rig.publish(); // duplicate snapshot cannot pop the next request
  RPCMP_CHECK(suite, rig.commands.sent.size() == 2);
  rig.policy_ack(api::PolicyCommandOutcome::Failed);
  RPCMP_CHECK(suite, rig.commands.sent.size() == 3 &&
                         required(rig.commands.sent[2].policy).count == 5U &&
                         rig.ui.view().diagnostic == Diagnostic::CommandFailed);
  rig.policy_ack();
  RPCMP_CHECK(suite,
              required(rig.commands.sent.back().policy).repeat == api::RepeatMode::RepeatOne);
  rig.policy_ack();
  RPCMP_CHECK(suite, required(rig.commands.sent.back().policy).repeat == api::RepeatMode::Default);
  while (rig.ui.view().queued_policy_actions != 0)
    rig.policy_ack();
  RPCMP_CHECK(suite, rig.commands.sent.size() == 8 && rig.ui.view().focus == Focus::Repeat);

  Rig mixed(suite);
  mixed.action(Action::Down);
  mixed.action(Action::Confirm);
  mixed.action(Action::Right);
  mixed.action(Action::Confirm); // Shuffle, queued semantically
  mixed.policy_ack();
  RPCMP_CHECK(suite, required(mixed.commands.sent.back().policy).order ==
                             api::PlaybackOrder::ShuffleLibrary &&
                         required(mixed.commands.sent.back().policy).count == 3U);
  mixed.policy_ack();
  mixed.action(Action::Left);
  mixed.action(Action::Confirm);
  RPCMP_CHECK(suite, required(mixed.commands.sent.back().policy).order ==
                             api::PlaybackOrder::ShuffleLibrary &&
                         required(mixed.commands.sent.back().policy).count == 5U);
  required(mixed.snapshot.policy_commands).last = {999, required(mixed.snapshot.policy).revision,
                                                   api::PolicyCommandOutcome::Applied};
  mixed.advance();
  RPCMP_CHECK(suite, mixed.ui.view().queued_policy_actions == 0 &&
                         mixed.ui.view().diagnostic == Diagnostic::Protocol);

  Rig reject(suite);
  reject.commands.rejection = api::CommandReason::QueueFull;
  reject.action(Action::Down);
  reject.action(Action::Confirm);
  RPCMP_CHECK(suite, reject.ui.view().queued_policy_actions == 0 &&
                         reject.ui.view().rejection == api::CommandReason::QueueFull);
  Rig missing(suite);
  missing.snapshot.policy_commands.reset();
  missing.snapshot.capabilities.bits &= ~api::kPolicyCommandResults;
  missing.advance();
  missing.action(Action::Down);
  missing.action(Action::Confirm);
  RPCMP_CHECK(suite, missing.commands.sent.empty() &&
                         missing.ui.view().diagnostic == Diagnostic::Unsupported);
}

void history(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  api::PerformanceHistorySnapshot history;
  history.play_generation = rig.snapshot.play_generation;
  history.observed_through_frame = rig.snapshot.position_frames;
  history.channels = api::unknown_performance_channels();
  history.availability = api::PerformanceAvailability::Available;
  const api::PerformanceChannel on{0, true, std::uint8_t{60}, std::int16_t{0},
                                   api::MdxFmV1{std::uint8_t{0x2A}}};
  auto other = on;
  other.channel_id = 1;
  other.note = std::uint8_t{64};
  auto off = on;
  off.key_on = false;
  history.events[0] = {1, {100, on, api::PerformanceKind::KeyOn}};
  history.events[1] = {2, {100, other, api::PerformanceKind::KeyOn}};
  history.events[2] = {3, {100, off, api::PerformanceKind::KeyOff}};
  history.events[3] = {4, {100, on, api::PerformanceKind::KeyOn}};
  history.count = 4;
  history.next_sequence = 5;
  history.channels[0] = off; // keyboard truth can differ from the visible last event
  rig.snapshot.capabilities.bits |= api::kPerformanceHistory;
  rig.snapshot.performance_history = history;
  rig.advance();
  const auto& tracker = rig.ui.view().tracker;
  RPCMP_CHECK(
      suite,
      tracker.count == 3 && tracker.rows[0].first_sequence == 1 &&
          tracker.rows[1].first_sequence == 3 && tracker.rows[2].first_sequence == 4 &&
          tracker.rows[0].cells[0] && tracker.rows[0].cells[1] && !tracker.rows[1].cells[1] &&
          required(required(rig.ui.view().snapshot.performance_history).channels[0].key_on) ==
              false);
  rig.snapshot.sequence += 20;
  rig.publish();
  RPCMP_CHECK(suite, !rig.ui.view().tracker.gap); // skipped snapshots, no skipped events
  history.count = 1;
  history.events[0] = {8, {200, off, api::PerformanceKind::KeyOff}};
  history.next_sequence = 9;
  history.retention_lost = true;
  history.availability = api::PerformanceAvailability::Degraded;
  rig.snapshot.performance_history = history;
  rig.advance();
  RPCMP_CHECK(suite, rig.ui.view().tracker.gap && rig.ui.view().tracker.retained_earlier &&
                         rig.ui.view().tracker.count == 1);
  for (std::uint16_t i = 0; i < 256; ++i)
    history.events[i] = {static_cast<std::uint64_t>(i) + 9,
                         {static_cast<std::uint64_t>(i) + 201, on, api::PerformanceKind::KeyOn}};
  history.count = 256;
  history.next_sequence = 265;
  rig.snapshot.performance_history = history;
  rig.advance();
  RPCMP_CHECK(suite, rig.ui.view().tracker.count == 16 &&
                         rig.ui.view().tracker.rows[0].first_sequence == 249 &&
                         rig.ui.view().tracker.rows[15].first_sequence == 264);
  for (std::uint16_t row = 0; row < 16; ++row) {
    const auto& actual = rig.ui.view().tracker.rows[row];
    RPCMP_CHECK(suite, actual.first_sequence == 249U + row && actual.at_frame == 441U + row &&
                           required(actual.cells[0]).channel.note == on.note && !actual.cells[1]);
  }
  // Twenty-one two-channel rows retain rows 5..20 in chronological order.
  for (std::size_t row = 0; row < 21; ++row) {
    history.events[2 * row] = {265U + 2U * row, {500U + row, on, api::PerformanceKind::KeyOn}};
    history.events[2 * row + 1] = {266U + 2U * row,
                                   {500U + row, other, api::PerformanceKind::KeyOn}};
  }
  history.count = 42;
  history.next_sequence = 307;
  rig.snapshot.performance_history = history;
  rig.advance();
  RPCMP_CHECK(suite, rig.ui.view().tracker.count == 16);
  for (std::uint16_t row = 0; row < 16; ++row) {
    const auto& actual = rig.ui.view().tracker.rows[row];
    RPCMP_CHECK(suite, actual.first_sequence == 275U + 2U * row && actual.at_frame == 505U + row &&
                           required(actual.cells[0]).channel.note == on.note &&
                           required(actual.cells[1]).channel.note == other.note &&
                           !actual.cells[2]);
  }
  // A sequence gap and a repeated channel each start a row, even at the same frame.
  history.events[42] = {308, {520, on, api::PerformanceKind::KeyOn}};
  history.events[43] = {309, {520, off, api::PerformanceKind::KeyOff}};
  history.count = 44;
  history.next_sequence = 310;
  history.capture_lost = true;
  rig.snapshot.performance_history = history;
  rig.advance();
  RPCMP_CHECK(suite, rig.ui.view().tracker.rows[0].first_sequence == 279 &&
                         rig.ui.view().tracker.rows[14].first_sequence == 308 &&
                         rig.ui.view().tracker.rows[15].first_sequence == 309 &&
                         !rig.ui.view().tracker.rows[14].cells[1] &&
                         required(rig.ui.view().tracker.rows[15].cells[0]).channel.key_on == false);
  // Start the next generation with the contiguous portion of the fixture.
  history.count = 42;
  history.next_sequence = 307;
  history.capture_lost = false;
  ++rig.snapshot.play_generation;
  history.play_generation = rig.snapshot.play_generation;
  rig.snapshot.performance_history = history;
  rig.advance();
  RPCMP_CHECK(suite, !rig.ui.view().tracker.gap && rig.ui.view().tracker.retained_earlier);
  history = {};
  history.channels = api::unknown_performance_channels();
  history.play_generation = rig.snapshot.play_generation;
  history.observed_through_frame = rig.snapshot.position_frames;
  history.availability = api::PerformanceAvailability::Invalid;
  rig.snapshot.performance_history = history;
  rig.advance();
  RPCMP_CHECK(suite, rig.ui.view().tracker.count == 0 && rig.ui.view().tracker.availability ==
                                                             api::PerformanceAvailability::Invalid);
}

void boundaries(rpcmp::test::Suite& suite) {
  UiConfiguration config;
  config.first_command_id = std::numeric_limits<std::uint64_t>::max();
  Rig exhausted(suite, config);
  exhausted.action(Action::Next);
  exhausted.action(Action::Next);
  RPCMP_CHECK(suite, exhausted.commands.sent.size() == 1 &&
                         exhausted.ui.view().diagnostic == Diagnostic::CommandExhausted);
  Rig malformed(suite);
  malformed.snapshot.schema_version = 999;
  malformed.ui.update(malformed.snapshot);
  malformed.action(Action::Confirm);
  RPCMP_CHECK(suite, malformed.commands.sent.empty() && !malformed.ui.view().valid_snapshot);
  malformed.snapshot.schema_version = api::kSchemaVersion;
  malformed.snapshot.sequence = 9;
  malformed.ui.update(malformed.snapshot);
  RPCMP_CHECK(suite, !malformed.ui.view().valid_snapshot);
  Rig reply(suite);
  reply.commands.wrong_id = true;
  reply.action(Action::Confirm);
  RPCMP_CHECK(suite, !reply.ui.view().transport_pending &&
                         reply.ui.view().diagnostic == Diagnostic::Protocol);
  config = {};
  static_assert(sizeof(Focus) == 1);
  const std::uint8_t invalid_focus_byte = 255;
  std::memcpy(&config.focus[0][0], &invalid_focus_byte, 1);
  Rig invalid_focus(suite, config);
  invalid_focus.action(Action::Down);
  RPCMP_CHECK(suite, invalid_focus.ui.view().focus == Focus::PlayPause &&
                         invalid_focus.ui.view().diagnostic == Diagnostic::InvalidFocus);
  config = {};
  config.focus[2][1] = Focus::ViewSwitch;
  Rig custom(suite, config);
  custom.action(Action::Down);
  custom.action(Action::Confirm);
  RPCMP_CHECK(suite, custom.ui.view().main == View::Keyboard && custom.commands.sent.empty());
  Rig ended(suite);
  ended.action(Action::Confirm); // Pause admitted; source ends before UI observes pause
  ended.snapshot.transport = ended.snapshot.projected = TransportState::Ended;
  ended.advance();
  ended.action(Action::Confirm);
  RPCMP_CHECK(suite, ended.commands.sent.size() == 2 &&
                         ended.commands.sent.back().kind == api::CommandKind::Play);
}

void reject_regressed_history_atomically(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  api::PerformanceHistorySnapshot history;
  history.channels = api::unknown_performance_channels();
  history.play_generation = rig.snapshot.play_generation;
  history.observed_through_frame = rig.snapshot.position_frames;
  history.availability = api::PerformanceAvailability::Available;
  const api::PerformanceChannel on{0, true, std::uint8_t{60}, std::nullopt, std::nullopt};
  history.events[0] = {1, {100, on, api::PerformanceKind::KeyOn}};
  history.events[1] = {2, {200, on, api::PerformanceKind::KeyOn}};
  history.count = 2;
  history.next_sequence = 3;
  rig.snapshot.capabilities.bits |= api::kPerformanceHistory;
  rig.snapshot.performance_history = history;
  rig.advance();
  rig.action(Action::Down);
  rig.action(Action::Confirm);
  auto bad = rig.snapshot;
  ++bad.sequence;
  required(bad.performance_history).count = 1;
  required(bad.performance_history).next_sequence = 2;
  required(bad.policy).desired = required(rig.commands.sent.back().policy);
  required(bad.policy).revision = 2;
  required(bad.policy_commands).last = {rig.commands.sent.back().command_id, 2,
                                        api::PolicyCommandOutcome::Applied};
  RPCMP_CHECK(suite, api::valid_player_snapshot(bad)); // invalid only against the prior observation
  rig.ui.update(bad);
  RPCMP_CHECK(suite, !rig.ui.view().valid_snapshot &&
                         rig.ui.view().snapshot.sequence == rig.snapshot.sequence);
  RPCMP_CHECK(suite, rig.ui.view().queued_policy_actions == 1 && rig.ui.view().tracker.count == 2);
  rig.ui.update(bad);
  RPCMP_CHECK(suite, !rig.ui.view().valid_snapshot && rig.ui.view().queued_policy_actions == 1);
  rig.policy_ack();
  RPCMP_CHECK(suite, rig.ui.view().valid_snapshot && rig.ui.view().queued_policy_actions == 0);
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  input_edges(suite);
  transport_and_stop(suite);
  browsing(suite);
  panel_context(suite);
  policies(suite);
  history(suite);
  boundaries(suite);
  reject_regressed_history_atomically(suite);
  return suite.finish("Album UI input and snapshots");
}
