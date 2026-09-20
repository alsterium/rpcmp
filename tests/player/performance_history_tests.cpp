#include "rpcmp/player/performance_history.hpp"
#include "test_support.hpp"

#include <iostream>
#include <limits>
#include <type_traits>

namespace {
namespace api = rpcmp::contracts::v2;
using namespace rpcmp::player;

api::PerformanceChange change(std::uint64_t frame, std::uint16_t channel, bool on,
                              std::uint8_t note) {
  return {frame,
          {channel, on, note, std::nullopt, api::MdxFmV1{std::uint8_t{0x2a}}},
          on ? api::PerformanceKind::KeyOn : api::PerformanceKind::KeyOff};
}
api::PerformanceHistorySnapshot copy(const PerformanceReader& reader) {
  api::PerformanceHistorySnapshot result;
  reader.copy_to(result);
  return result;
}

void sequence_and_loss(rpcmp::test::Suite& suite) {
  PerformanceHistory history;
  RPCMP_CHECK(suite, api::valid_performance_history(copy(history)));
  RPCMP_CHECK(suite, !history.begin(0) && history.begin(7) && !history.begin(7));
  const std::array notes{change(100, 0, true, 60), change(100, 1, true, 64),
                         change(100, 0, false, 60), change(100, 0, true, 60)};
  auto channels = api::unknown_performance_channels();
  channels[0] = notes[3].channel;
  channels[1] = notes[1].channel;
  PerformanceCommit input{7, 99, channels, notes};
  RPCMP_CHECK(suite, history.commit(input) == PerformanceCaptureResult::Invalid);
  RPCMP_CHECK(suite, copy(history).count == 0 && copy(history).observed_through_frame == 0);
  input.through_frame = 100;
  RPCMP_CHECK(suite, history.commit(input) == PerformanceCaptureResult::Applied);
  auto snapshot = copy(history);
  RPCMP_CHECK(suite, api::valid_performance_history(snapshot) && snapshot.count == 4 &&
                         snapshot.next_sequence == 5 && snapshot.capture_lost &&
                         snapshot.availability == api::PerformanceAvailability::Degraded);
  for (std::size_t i = 0; i < notes.size(); ++i)
    RPCMP_CHECK(suite,
                snapshot.events[i].sequence == i + 1U && snapshot.events[i].change == notes[i]);
  RPCMP_CHECK(suite, snapshot.channels == channels);
  snapshot.events[0].change.channel.note = std::uint8_t{1};
  RPCMP_CHECK(suite, copy(history).events[0].change.channel.note == 60);
  input.play_generation = 6;
  RPCMP_CHECK(suite, history.commit(input) == PerformanceCaptureResult::Stale);
  RPCMP_CHECK(suite, copy(history).next_sequence == 5);
  RPCMP_CHECK(suite, history.begin(8));
  RPCMP_CHECK(suite, copy(history).count == 0 && !copy(history).capture_lost);
  for (std::uint64_t i = 1; i <= 300; ++i) {
    const std::array events{change(i, 0, i % 2 == 1, 60)};
    channels[0] = events[0].channel;
    RPCMP_CHECK(suite,
                history.commit({8, i, channels, events}) == PerformanceCaptureResult::Applied);
    if (i == 256)
      RPCMP_CHECK(suite, copy(history).count == 256 && !copy(history).retention_lost);
    if (i == 257)
      RPCMP_CHECK(suite, copy(history).events[0].sequence == 2 && copy(history).retention_lost);
  }
  snapshot = copy(history);
  RPCMP_CHECK(suite, api::valid_performance_history(snapshot) && snapshot.count == 256 &&
                         snapshot.events[0].sequence == 45 &&
                         snapshot.events[255].sequence == 300 && snapshot.next_sequence == 301 &&
                         snapshot.retention_lost && !snapshot.capture_lost);
  // A checkpoint recovers the current keyboard even if no transitions were captured.
  channels[0] = {0, true, std::uint8_t{72}, std::int16_t{-3}, std::nullopt};
  RPCMP_CHECK(suite,
              history.commit({8, 310, channels, {}, 4, true}) == PerformanceCaptureResult::Applied);
  snapshot = copy(history);
  RPCMP_CHECK(suite, api::valid_performance_history(snapshot) && snapshot.next_sequence == 305 &&
                         snapshot.capture_lost && snapshot.channels[0].note == 72 &&
                         snapshot.events[255].sequence == 300);
  const std::array final{change(311, 0, false, 72)};
  channels[0] = final[0].channel;
  RPCMP_CHECK(suite,
              history.commit({8, 311, channels, final}) == PerformanceCaptureResult::Applied);
  RPCMP_CHECK(suite, copy(history).events[255].sequence == 305 &&
                         api::valid_performance_history(copy(history)));
}

void malformed_and_exhaustion(rpcmp::test::Suite& suite) {
  const auto reject = [&](auto mutate) {
    PerformanceHistory history;
    RPCMP_CHECK(suite, history.begin(4));
    std::array events{change(10, 0, true, 60), change(11, 1, false, 64)};
    PerformanceCommit input{4, 11, api::unknown_performance_channels(), events};
    mutate(input, events);
    RPCMP_CHECK(suite, history.commit(input) == PerformanceCaptureResult::Invalid);
    const auto snapshot = copy(history);
    RPCMP_CHECK(suite, snapshot.count == 0 && snapshot.next_sequence == 1 &&
                           snapshot.observed_through_frame == 0 && snapshot.capture_lost &&
                           api::valid_performance_history(snapshot));
  };
  reject([](auto& i, auto&) { i.play_generation = 0; });
  reject([](auto& i, auto&) { i.play_generation = 5; });
  reject([](auto& i, auto&) { i.channels[7].channel_id = 8; });
  reject([](auto& i, auto&) { i.channels[0].note = std::uint8_t{128}; });
  reject([](auto& i, auto&) { i.changes.data = nullptr; });
  reject([](auto&, auto& e) { e[1].at_frame = 9; });
  reject([](auto&, auto& e) { e[1].at_frame = 12; });
  reject([](auto&, auto& e) { e[1].channel.channel_id = 8; });
  reject([](auto&, auto& e) { e[0].channel.key_on.reset(); });
  reject([](auto&, auto& e) { e[0].channel.key_on = false; });
  reject([](auto&, auto& e) { e[1].channel.key_on = true; });
  reject([](auto&, auto& e) {
    e[0].channel.note.reset();
    e[0].channel.fine_pitch_cents = std::int16_t{1};
  });
  const std::array<api::PerformanceChange, 257> oversized{};
  reject([&](auto& i, auto&) { i.changes = oversized; });

  PerformanceHistory history;
  RPCMP_CHECK(suite, history.begin(1));
  const std::array event{change(0, 0, true, 60)};
  auto channels = api::unknown_performance_channels();
  channels[0] = event[0].channel;
  constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
  RPCMP_CHECK(suite, history.commit({1, 0, channels, event, maximum - 2}) ==
                         PerformanceCaptureResult::Applied);
  RPCMP_CHECK(suite, copy(history).events[0].sequence == maximum - 1 &&
                         copy(history).next_sequence == maximum);
  RPCMP_CHECK(suite,
              history.commit({1, 0, channels, event}) == PerformanceCaptureResult::Exhausted);
  channels[0] = {0, false, std::nullopt, std::nullopt, std::nullopt};
  RPCMP_CHECK(suite, history.commit({1, 200, channels, {}}) == PerformanceCaptureResult::Exhausted);
  const auto exhausted = copy(history);
  RPCMP_CHECK(suite, api::valid_performance_history(exhausted) && exhausted.count == 1 &&
                         exhausted.observed_through_frame == 200 &&
                         exhausted.channels[0].key_on == false &&
                         exhausted.availability == api::PerformanceAvailability::Exhausted);
  RPCMP_CHECK(suite, !history.begin(1) && history.begin(2));
  RPCMP_CHECK(suite, copy(history).next_sequence == 1 && copy(history).count == 0);
  RPCMP_CHECK(suite,
              history.commit({2, 0, channels, {}, maximum}) == PerformanceCaptureResult::Exhausted);
}

void public_validation(rpcmp::test::Suite& suite) {
  PerformanceHistory history;
  RPCMP_CHECK(suite, history.begin(7));
  const std::array events{change(10, 0, true, 60), change(11, 0, false, 60)};
  RPCMP_CHECK(suite, history.commit({7, 11, api::unknown_performance_channels(), events}) ==
                         PerformanceCaptureResult::Applied);
  const auto good = copy(history);
  RPCMP_CHECK(suite, api::valid_performance_history(good));
  const auto reject = [&](auto mutate) {
    auto value = good;
    mutate(value);
    RPCMP_CHECK(suite, !api::valid_performance_history(value));
  };
  reject([](auto& h) { h.version = 2; });
  reject([](auto& h) { h.count = 257; });
  reject([](auto& h) { h.play_generation = 0; });
  reject([](auto& h) { h.channels[1].channel_id = 0; });
  reject([](auto& h) { h.next_sequence = 0; });
  reject([](auto& h) { h.next_sequence = 2; });
  reject([](auto& h) { h.next_sequence = 4; });
  reject([](auto& h) { h.events[1].sequence = 1; });
  reject([](auto& h) { h.events[1].change.at_frame = 9; });
  reject([](auto& h) { h.events[1].change.at_frame = 12; });
  reject([](auto& h) { h.capture_lost = true; });
  reject([](auto& h) { h.availability = api::PerformanceAvailability::Degraded; });
  reject([](auto& h) { h.availability = api::PerformanceAvailability::Waiting; });
  reject([](auto& h) { h.availability = api::PerformanceAvailability::Exhausted; });
  auto unknown = events[0];
  unknown.channel.note.reset();
  unknown.channel.mdx_fm.reset();
  RPCMP_CHECK(suite, api::valid_performance_change(unknown));
}

void positioned_losses(rpcmp::test::Suite& suite) {
  const std::array events{change(10, 0, true, 60), change(20, 0, false, 60),
                          change(30, 0, true, 60)};
  const std::array<std::uint64_t, 3> omitted{0, 2, 0};
  auto channels = api::unknown_performance_channels();
  channels[0] = events[2].channel;
  PerformanceCommit input{1, 30, channels, events, 1, false, {omitted.data(), omitted.size()}, 3};
  PerformanceHistory history;
  RPCMP_CHECK(suite, history.begin(1));
  RPCMP_CHECK(suite, history.commit(input) == PerformanceCaptureResult::Applied);
  const auto result = copy(history);
  RPCMP_CHECK(suite, api::valid_performance_history(result) && result.capture_lost &&
                         result.count == 3 && result.next_sequence == 10 &&
                         result.events[0].sequence == 2 && result.events[1].sequence == 5 &&
                         result.events[2].sequence == 6);
  for (const bool null_data : {false, true}) {
    PerformanceHistory invalid;
    RPCMP_CHECK(suite, invalid.begin(1));
    auto malformed = input;
    if (null_data)
      malformed.lost_before_changes.data = nullptr;
    else
      malformed.lost_before_changes.count = 2;
    RPCMP_CHECK(suite, invalid.commit(malformed) == PerformanceCaptureResult::Invalid);
    RPCMP_CHECK(suite, copy(invalid).next_sequence == 1 && copy(invalid).count == 0 &&
                           copy(invalid).observed_through_frame == 0);
  }
  // Sum overflow cannot disguise an exhausted event sequence as a small advance.
  constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
  const std::array<std::uint64_t, 3> huge{maximum - 3, 2, 2};
  input.through_frame = 31;
  input.changes = {};
  input.lost_before_changes.count = 0;
  input.lost_before = maximum - 1;
  input.lost_after = 3;
  RPCMP_CHECK(suite, history.commit(input) == PerformanceCaptureResult::Exhausted);
  const auto exhausted = copy(history);
  RPCMP_CHECK(suite, exhausted.next_sequence == maximum && exhausted.events == result.events &&
                         exhausted.channels == channels &&
                         api::valid_performance_history(exhausted));
  PerformanceHistory middle_overflow;
  RPCMP_CHECK(suite, middle_overflow.begin(1));
  input.changes = events;
  input.lost_before = 0;
  input.lost_after = 0;
  input.lost_before_changes = {huge.data(), huge.size()};
  RPCMP_CHECK(suite, middle_overflow.commit(input) == PerformanceCaptureResult::Exhausted);
  RPCMP_CHECK(suite,
              copy(middle_overflow).count == 0 && copy(middle_overflow).channels == channels);
}
} // namespace

int main() {
  static_assert(std::is_trivially_copyable_v<api::PerformanceHistorySnapshot>);
  rpcmp::test::Suite suite;
  sequence_and_loss(suite);
  malformed_and_exhaustion(suite);
  public_validation(suite);
  positioned_losses(suite);
  std::cout << "bytes: event=" << sizeof(api::PerformanceEvent)
            << " snapshot=" << sizeof(api::PerformanceHistorySnapshot)
            << " collector=" << sizeof(PerformanceHistory) << '\n';
  return suite.finish("Committed performance history");
}
