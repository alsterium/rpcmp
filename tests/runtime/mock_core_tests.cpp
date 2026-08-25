#include "rpcmp/runtime/mock_core.hpp"
#include "test_support.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace rpcmp;

class EventSink final : public runtime::IFakeDeviceSink {
public:
  void write(const runtime::FakeDeviceEvent& event) override { events.push_back(event); }
  std::vector<runtime::FakeDeviceEvent> events;
};

class SnapshotSink final : public runtime::ISnapshotObserver {
public:
  void published(const contracts::PlayerSnapshot& snapshot) override {
    snapshots.push_back(snapshot);
  }
  std::vector<contracts::PlayerSnapshot> snapshots;
};

contracts::PlayerCommand make_command(const std::uint64_t id, contracts::Command payload,
                                      const std::optional<std::uint64_t> expected = std::nullopt) {
  return {contracts::kSchemaVersion, id, expected, std::move(payload)};
}

std::string signature(const contracts::PlayerSnapshot& snapshot) {
  std::ostringstream output;
  output << snapshot.schema_version << '|' << snapshot.sequence << '|' << snapshot.published_at_tick
         << '|' << static_cast<unsigned>(snapshot.transport) << '|'
         << snapshot.position.position_ticks << '|' << snapshot.position.tick_rate << '|'
         << snapshot.channels.size();
  if (snapshot.track.has_value()) {
    output << '|' << snapshot.track->track_id.value << '|' << snapshot.track->title << '|'
           << snapshot.track->artist;
  } else {
    output << "|none";
  }
  for (const auto& channel : snapshot.channels) {
    output << '|' << channel.channel_id.value << ':' << channel.enabled << ':' << channel.muted
           << ':' << channel.solo << ':' << channel.key_on << ':'
           << (channel.note.has_value() ? std::to_string(*channel.note) : "none") << ':'
           << static_cast<unsigned>(channel.activity);
  }
  return output.str();
}

struct Trace {
  std::vector<std::string> snapshots;
  std::vector<runtime::FakeDeviceEvent> events;
  bool monotonic{true};
  bool eight_channels{true};
};

Trace run_trace(const bool read_latest) {
  EventSink events;
  SnapshotSink snapshots;
  runtime::MockCore core(&events, &snapshots);
  core.submit(make_command(1, contracts::OpenLibrary{"m0:library"}, 0));
  core.submit(make_command(2, contracts::LoadTrack{contracts::TrackId{1}}, 0));
  core.advance_to(1'000);
  if (read_latest) {
    static_cast<void>(core.latest());
  }
  core.submit(make_command(3, contracts::Play{}, 1));
  core.advance_to(60'000);
  core.submit(make_command(4, contracts::Pause{}, 60));
  core.advance_to(90'000);
  if (read_latest) {
    static_cast<void>(core.latest());
  }
  core.submit(make_command(5, contracts::Resume{}, 90));
  core.advance_to(150'000);
  core.submit(make_command(6, contracts::SetChannelMute{contracts::ChannelId{0}, true}, 150));
  core.submit(make_command(7, contracts::SetChannelSolo{contracts::ChannelId{1}, true}, 150));
  core.submit(make_command(8, contracts::ClearChannelOverrides{}, 150));
  core.submit(make_command(9, contracts::Stop{}, 150));
  core.advance_to(151'000);

  Trace trace;
  std::uint64_t previous_sequence = 0;
  std::uint64_t previous_tick = 0;
  for (const auto& snapshot : snapshots.snapshots) {
    if (snapshot.sequence <= previous_sequence || snapshot.published_at_tick <= previous_tick) {
      trace.monotonic = false;
    }
    trace.eight_channels = trace.eight_channels && snapshot.channels.size() == 8U;
    previous_sequence = snapshot.sequence;
    previous_tick = snapshot.published_at_tick;
    trace.snapshots.push_back(signature(snapshot));
  }
  trace.events = events.events;
  return trace;
}

void check_command_rules(rpcmp::test::Suite& suite) {
  runtime::MockCore core;
  const auto open = make_command(1, contracts::OpenLibrary{"m0:library"});
  RPCMP_CHECK(suite, core.submit(open).outcome == contracts::CommandOutcome::Accepted);
  RPCMP_CHECK(suite, core.submit(open).outcome == contracts::CommandOutcome::Duplicate);
  RPCMP_CHECK(suite,
              core.submit(make_command(2, contracts::LoadTrack{contracts::TrackId{999}})).reason ==
                  contracts::CommandReason::UnknownTrack);
  RPCMP_CHECK(suite,
              core.submit(make_command(3, contracts::LoadTrack{contracts::TrackId{1}}, 9)).reason ==
                  contracts::CommandReason::StaleSnapshotSequence);
  RPCMP_CHECK(
      suite, core.submit(make_command(4, contracts::LoadTrack{contracts::TrackId{1}}, 0)).outcome ==
                 contracts::CommandOutcome::Accepted);
  RPCMP_CHECK(suite, core.submit(make_command(5, contracts::Play{}, 0)).outcome ==
                         contracts::CommandOutcome::Accepted);
  RPCMP_CHECK(suite, core.advance_to(1'000) == runtime::AdvanceResult::Ok);
  RPCMP_CHECK(suite, core.latest().transport == contracts::TransportState::Playing);
  RPCMP_CHECK(suite, core.submit(make_command(6, contracts::Seek{12})).reason ==
                         contracts::CommandReason::UnsupportedCapability);
  RPCMP_CHECK(suite,
              core.submit(make_command(7, contracts::SetChannelMute{contracts::ChannelId{8}, true}))
                      .reason == contracts::CommandReason::UnknownChannel);

  auto unsupported_schema = make_command(8, contracts::Stop{});
  unsupported_schema.schema_version = 2;
  RPCMP_CHECK(suite, core.submit(unsupported_schema).reason ==
                         contracts::CommandReason::UnsupportedSchema);
  RPCMP_CHECK(suite, core.submit(make_command(0, contracts::Stop{})).reason ==
                         contracts::CommandReason::InvalidCommandId);

  runtime::MockCore malformed;
  RPCMP_CHECK(suite,
              malformed
                      .submit(make_command(1, contracts::OpenLibrary{std::string(
                                                  contracts::kMaxLibraryReferenceBytes + 1U, 'x')}))
                      .reason == contracts::CommandReason::MalformedRequest);
}

enum class FixtureState : std::uint8_t { Closed, Open, Stopped, Playing, Paused, Ended };

void prepare_state(runtime::MockCore& core, const FixtureState state) {
  if (state == FixtureState::Closed) {
    return;
  }
  core.submit(make_command(1, contracts::OpenLibrary{"m0:library"}));
  if (state == FixtureState::Open) {
    core.advance_to(1'000);
    return;
  }
  const auto track_id =
      state == FixtureState::Ended ? contracts::TrackId{3} : contracts::TrackId{1};
  core.submit(make_command(2, contracts::LoadTrack{track_id}));
  if (state == FixtureState::Stopped) {
    core.advance_to(1'000);
    return;
  }
  core.submit(make_command(3, contracts::Play{}));
  if (state == FixtureState::Playing) {
    core.advance_to(1'000);
    return;
  }
  if (state == FixtureState::Paused) {
    core.advance_to(1'000);
    core.submit(make_command(4, contracts::Pause{}));
    core.advance_to(2'000);
    return;
  }
  core.advance_to(6'000);
}

void check_transition_matrix(rpcmp::test::Suite& suite) {
  using Reason = contracts::CommandReason;
  constexpr auto accepted = Reason::None;
  constexpr auto invalid = Reason::InvalidState;
  constexpr auto busy = Reason::ResourceBusy;
  constexpr auto unsupported = Reason::UnsupportedCapability;

  struct MatrixCase {
    contracts::Command payload;
    std::array<Reason, 6> expected;
  };
  const std::vector<MatrixCase> cases{
      {contracts::OpenLibrary{"m0:library"}, {accepted, busy, busy, busy, busy, busy}},
      {contracts::CloseLibrary{}, {invalid, accepted, accepted, accepted, accepted, accepted}},
      {contracts::LoadTrack{contracts::TrackId{1}},
       {invalid, accepted, accepted, accepted, accepted, accepted}},
      {contracts::Play{}, {invalid, invalid, accepted, accepted, accepted, accepted}},
      {contracts::Pause{}, {invalid, invalid, invalid, accepted, accepted, invalid}},
      {contracts::Resume{}, {invalid, invalid, invalid, accepted, accepted, invalid}},
      {contracts::Stop{}, {invalid, invalid, accepted, accepted, accepted, accepted}},
      {contracts::TogglePause{}, {invalid, invalid, invalid, accepted, accepted, invalid}},
      {contracts::NextTrack{}, {invalid, accepted, accepted, accepted, accepted, accepted}},
      {contracts::PreviousTrack{}, {invalid, accepted, accepted, accepted, accepted, accepted}},
      {contracts::Seek{0},
       {unsupported, unsupported, unsupported, unsupported, unsupported, unsupported}},
      {contracts::SetChannelMute{contracts::ChannelId{0}, true},
       {invalid, invalid, accepted, accepted, accepted, accepted}},
      {contracts::SetChannelSolo{contracts::ChannelId{0}, true},
       {invalid, invalid, accepted, accepted, accepted, accepted}},
      {contracts::ClearChannelOverrides{},
       {invalid, invalid, accepted, accepted, accepted, accepted}},
  };

  for (const auto& test_case : cases) {
    for (std::size_t state_index = 0; state_index < test_case.expected.size(); ++state_index) {
      runtime::MockCore core;
      prepare_state(core, static_cast<FixtureState>(state_index));
      const auto result = core.submit(make_command(100, test_case.payload));
      if (test_case.expected[state_index] == accepted) {
        RPCMP_CHECK(suite, result.outcome == contracts::CommandOutcome::Accepted);
      } else {
        RPCMP_CHECK(suite, result.reason == test_case.expected[state_index]);
      }
    }
  }
}

void check_queue_bound(rpcmp::test::Suite& suite) {
  runtime::MockCore core;
  core.submit(make_command(1, contracts::OpenLibrary{"m0:library"}));
  core.submit(make_command(2, contracts::LoadTrack{contracts::TrackId{1}}));
  core.advance_to(1'000);
  for (std::uint64_t index = 0; index < runtime::kCommandQueueCapacity; ++index) {
    const auto result = core.submit(make_command(
        3 + index, contracts::SetChannelMute{contracts::ChannelId{0}, index % 2U == 0U}));
    RPCMP_CHECK(suite, result.outcome == contracts::CommandOutcome::Accepted);
  }
  RPCMP_CHECK(suite, core.pending_command_count() == runtime::kCommandQueueCapacity);
  RPCMP_CHECK(suite,
              core.submit(make_command(3 + runtime::kCommandQueueCapacity,
                                       contracts::SetChannelMute{contracts::ChannelId{0}, false}))
                      .reason == contracts::CommandReason::QueueFull);
}

void check_old_id_is_safe(rpcmp::test::Suite& suite) {
  runtime::MockCore core;
  core.submit(make_command(1, contracts::Stop{}));
  for (std::uint64_t id = 2; id <= 70; ++id) {
    core.submit(make_command(id, contracts::Stop{}));
  }
  RPCMP_CHECK(suite, core.submit(make_command(1, contracts::Stop{})).reason ==
                         contracts::CommandReason::StaleCommandId);
}

void check_pause_and_end(rpcmp::test::Suite& suite) {
  runtime::MockCore core;
  core.submit(make_command(1, contracts::OpenLibrary{"m0:library"}));
  core.submit(make_command(2, contracts::LoadTrack{contracts::TrackId{1}}));
  core.submit(make_command(3, contracts::Play{}));
  core.advance_to(10'000);
  const auto playing_position = core.latest().position.position_ticks;
  core.submit(make_command(4, contracts::Pause{}));
  core.advance_to(20'000);
  RPCMP_CHECK(suite, core.latest().position.position_ticks == playing_position);
  RPCMP_CHECK(suite, core.latest().transport == contracts::TransportState::Paused);
  RPCMP_CHECK(suite, core.advance_to(19'000) == runtime::AdvanceResult::TimeReversed);

  runtime::MockCore finite;
  finite.submit(make_command(1, contracts::OpenLibrary{"m0:library"}));
  finite.submit(make_command(2, contracts::LoadTrack{contracts::TrackId{3}}));
  finite.submit(make_command(3, contracts::Play{}));
  finite.advance_to(6'000);
  RPCMP_CHECK(suite, finite.latest().transport == contracts::TransportState::Ended);
  RPCMP_CHECK(suite, finite.latest().position.position_ticks == 5'000);
  RPCMP_CHECK(suite, finite.submit(make_command(4, contracts::Play{})).outcome ==
                         contracts::CommandOutcome::Accepted);
  finite.advance_to(7'000);
  RPCMP_CHECK(suite, finite.latest().transport == contracts::TransportState::Playing);
  RPCMP_CHECK(suite, finite.latest().position.position_ticks == 1'000);
}

} // namespace

int main() {
  rpcmp::test::Suite suite;

  const auto no_reads = run_trace(false);
  const auto sparse_reads = run_trace(true);
  RPCMP_CHECK(suite, no_reads.snapshots.size() >= 120U);
  RPCMP_CHECK(suite, no_reads.monotonic);
  RPCMP_CHECK(suite, no_reads.eight_channels);
  RPCMP_CHECK(suite, no_reads.snapshots == sparse_reads.snapshots);
  RPCMP_CHECK(suite, no_reads.events == sparse_reads.events);

  check_command_rules(suite);
  check_transition_matrix(suite);
  check_queue_bound(suite);
  check_old_id_is_safe(suite);
  check_pause_and_end(suite);

  runtime::MockCore bounded;
  RPCMP_CHECK(suite, bounded.advance_to((runtime::kMaxCatchUpSnapshots + 1U) *
                                        runtime::kSnapshotCadenceTicks) ==
                         runtime::AdvanceResult::CatchUpLimit);

  return suite.finish("runtime");
}
