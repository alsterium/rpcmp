#include "rpcmp/player/mdx_output_history.hpp"
#include "test_support.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace rpcmp::player;
namespace api = rpcmp::contracts::v2;
namespace mdx = rpcmp::runtime::mdx;
using R = MdxHistoryResult;

MdxProducedCheckpoint checkpoint(std::uint64_t preceding, std::uint16_t writes,
                                 std::initializer_list<std::uint16_t> events) {
  MdxProducedCheckpoint result{1, 1, preceding, preceding + writes + 1, {}};
  result.performance.at_tick = preceding * 100;
  auto& batch = result.performance.batch;
  batch.write_count = writes;
  batch.channels[0] = {true, true, std::uint16_t{45 * 64}, std::uint8_t{7}};
  for (const auto write : events)
    batch.events[batch.count++] = {write, 0, mdx::Ym2151PerformanceKind::KeyOn, batch.channels[0]};
  return result;
}
api::PerformanceHistorySnapshot copy(const PerformanceReader& reader) {
  api::PerformanceHistorySnapshot result;
  reader.copy_to(result);
  return result;
}
void publish(rpcmp::test::Suite& suite, MdxOutputHistory& history, std::uint64_t position,
             std::uint64_t prefix, bool natural = false) {
  RPCMP_CHECK(suite, history.observe({1, 1, position, prefix, natural}) == R::Accepted);
  RPCMP_CHECK(suite, api::valid_performance_history(copy(history)));
}

void exact_frames(rpcmp::test::Suite& suite) {
  MdxOutputHistory history(3'579'545);
  RPCMP_CHECK(suite, api::valid_performance_history(copy(history)) && history.begin(1, 1));
  auto input = checkpoint(0, 3, {1, 2, 2, 3});
  RPCMP_CHECK(suite, history.retain(input) == R::Accepted);
  input.performance.batch = {}; // Retention owns both transitions and checkpoint.
  RPCMP_CHECK(suite, history.retain(checkpoint(4, 0, {})) == R::Accepted);
  RPCMP_CHECK(suite, history.consume({1, 1, 1, 100, 1}) == R::Accepted);
  RPCMP_CHECK(suite, history.observe({1, 1, 101, 1}) == R::Waiting);
  RPCMP_CHECK(suite, copy(history).availability == api::PerformanceAvailability::Waiting);
  RPCMP_CHECK(suite, history.consume({1, 1, 2, 103, 2}) == R::Accepted);
  RPCMP_CHECK(suite, history.consume({1, 1, 3, 110, 5}) == R::Accepted);
  RPCMP_CHECK(suite, history.pending_batches() == 0);
  publish(suite, history, 111, 5);
  const auto good = copy(history);
  RPCMP_CHECK(suite, good.count == 4 && good.next_sequence == 5 && !good.capture_lost);
  constexpr std::array<std::uint64_t, 4> frames{100, 103, 103, 110};
  for (std::size_t i = 0; i < frames.size(); ++i)
    RPCMP_CHECK(suite,
                good.events[i].sequence == i + 1 && good.events[i].change.at_frame == frames[i]);
  RPCMP_CHECK(suite, good.channels[0].note == 58 && good.channels[0].key_on == true);
  for (unsigned i = 0; i < 100; ++i)
    RPCMP_CHECK(suite,
                copy(history).events == good.events && copy(history).channels == good.channels);
  publish(suite, history, 150, 5); // Idle output and a held pause append no events.
  publish(suite, history, 150, 5);
  RPCMP_CHECK(suite, copy(history).count == 4 && copy(history).next_sequence == 5);
  RPCMP_CHECK(suite, history.observe({1, 1, 149, 5}) == R::Stale);
  RPCMP_CHECK(suite, history.retain(checkpoint(5, 0, {})) == R::Accepted);
  RPCMP_CHECK(suite, history.observe({1, 1, 150, 6, true}) == R::Waiting);
  RPCMP_CHECK(suite, history.consume({1, 1, 4, 150, 6, true}) == R::Accepted);
  publish(suite, history, 150, 6, true);
  RPCMP_CHECK(suite, copy(history).events == good.events);
  RPCMP_CHECK(suite, history.consume({1, 1, 4, 150, 6, true}) == R::Stale);
  RPCMP_CHECK(suite, history.consume({1, 1, 5, 151, 6, true}) == R::Closed);
}

void missing_middle(rpcmp::test::Suite& suite) {
  for (const bool exact_prefix : {false, true}) {
    MdxOutputHistory history(3'579'545);
    RPCMP_CHECK(suite, history.begin(1, 1));
    RPCMP_CHECK(suite, history.retain(checkpoint(0, 3, {1, 2, 3})) == R::Accepted);
    RPCMP_CHECK(suite, history.consume({1, 1, 1, 100, 1}) == R::Accepted);
    RPCMP_CHECK(suite, history.consume({1, 1, 3, 110, exact_prefix ? 3U : 4U}) == R::Degraded);
    if (exact_prefix)
      RPCMP_CHECK(suite, history.consume({1, 1, 4, 111, 4}) == R::Accepted);
    publish(suite, history, 112, 4);
    auto result = copy(history);
    RPCMP_CHECK(suite, result.capture_lost && result.count == (exact_prefix ? 2 : 1) &&
                           result.next_sequence == 4 && result.events[0].sequence == 1 &&
                           result.events[0].change.at_frame == 100 &&
                           result.channels[0].key_on == true);
    if (exact_prefix)
      RPCMP_CHECK(suite, result.events[1].sequence == 3 && result.events[1].change.at_frame == 110);
    RPCMP_CHECK(suite, history.retain(checkpoint(4, 1, {1})) == R::Accepted);
    RPCMP_CHECK(suite, history.consume({1, 1, exact_prefix ? 5U : 4U, 120, 6}) == R::Accepted);
    publish(suite, history, 121, 6);
    result = copy(history);
    RPCMP_CHECK(suite, result.events[result.count - 1].sequence == 4 && result.next_sequence == 5);
  }
}

void retention_and_source_gap(rpcmp::test::Suite& suite) {
  MdxOutputHistory history(3'579'545);
  RPCMP_CHECK(suite, history.begin(1, 1));
  for (std::uint64_t i = 0; i < 20; ++i)
    RPCMP_CHECK(suite,
                history.retain(checkpoint(i * 2, 1, {1})) == (i < 16 ? R::Accepted : R::Degraded));
  RPCMP_CHECK(suite, history.pending_batches() == 16);
  RPCMP_CHECK(suite, history.consume({1, 1, 1, 100, 8}) == R::Accepted);
  RPCMP_CHECK(suite, history.observe({1, 1, 101, 8}) == R::Waiting);
  RPCMP_CHECK(suite, history.consume({1, 1, 2, 110, 40}) == R::Accepted);
  publish(suite, history, 111, 40);
  const auto result = copy(history);
  RPCMP_CHECK(suite, result.capture_lost && result.count == 16 && result.next_sequence == 21);
  for (std::uint16_t i = 0; i < 16; ++i)
    RPCMP_CHECK(suite,
                result.events[i].sequence == i + 5U && result.events[i].change.at_frame == 110);

  MdxOutputHistory gap(3'579'545);
  RPCMP_CHECK(suite, gap.begin(1, 1));
  RPCMP_CHECK(suite, gap.retain(checkpoint(0, 1, {1})) == R::Accepted);
  // Missing checkpoint 2..4 is still in the future when the earlier batch completes.
  RPCMP_CHECK(suite, gap.retain(checkpoint(4, 1, {1})) == R::Degraded);
  RPCMP_CHECK(suite, gap.consume({1, 1, 1, 10, 2}) == R::Accepted);
  publish(suite, gap, 11, 2);
  RPCMP_CHECK(suite, !copy(gap).capture_lost && copy(gap).count == 1);
  RPCMP_CHECK(suite, gap.consume({1, 1, 2, 20, 4}) == R::Accepted);
  RPCMP_CHECK(suite, gap.observe({1, 1, 21, 4}) == R::Waiting);
  RPCMP_CHECK(suite, gap.consume({1, 1, 3, 30, 6}) == R::Accepted);
  publish(suite, gap, 31, 6);
  RPCMP_CHECK(suite,
              copy(gap).capture_lost && copy(gap).count == 2 && copy(gap).next_sequence == 3);
}

void recovery_and_identity(rpcmp::test::Suite& suite) {
  for (const bool exhausted : {false, true}) {
    MdxOutputHistory history(4'000'000);
    RPCMP_CHECK(suite, !history.begin(0, 1) && !history.begin(1, 0) && history.begin(1, 1));
    RPCMP_CHECK(suite, history.retain(checkpoint(0, 3, {1, 2, 3})) == R::Accepted);
    RPCMP_CHECK(suite, history.retain(checkpoint(0, 3, {1, 2, 3})) == R::Stale);
    RPCMP_CHECK(suite, history.consume({2, 1, 1, 10, 1}) == R::Stale);
    RPCMP_CHECK(suite, history.consume({1, 2, 1, 10, 1}) == R::Stale);
    RPCMP_CHECK(suite, history.consume({1, 1, 1, 10, 1}) == R::Accepted);
    // The independent latest copy repairs current channels, never missing event times.
    RPCMP_CHECK(suite, history.recover({1, 1, exhausted ? 0U : 40U, 20, 4}) == R::Degraded);
    publish(suite, history, 21, 4);
    const auto restored = copy(history);
    RPCMP_CHECK(suite,
                restored.capture_lost && restored.count == 1 && restored.next_sequence == 4 &&
                    restored.events[0].change.at_frame == 10 && restored.channels[0].note == 60);
    if (exhausted) {
      RPCMP_CHECK(suite, history.retain(checkpoint(4, 1, {1})) == R::Accepted);
      RPCMP_CHECK(suite, history.recover({1, 1, 0, 30, 5}) == R::Degraded);
      RPCMP_CHECK(suite, history.recover({1, 1, 0, 30, 5}) == R::Stale);
      RPCMP_CHECK(suite, history.recover({1, 1, 0, 20, 4}) == R::Stale);
      RPCMP_CHECK(suite, history.pending_batches() == 1);
      RPCMP_CHECK(suite, history.recover({1, 1, 0, 31, 6}) == R::Degraded);
      publish(suite, history, 32, 6);
      RPCMP_CHECK(suite, copy(history).events[1].sequence == 4 &&
                             copy(history).events[1].change.at_frame == 30);
    }
    RPCMP_CHECK(suite, !history.begin(1, 2) && !history.begin(2, 1));
    history.cancel();
    RPCMP_CHECK(suite, history.retain(checkpoint(4, 1, {1})) == R::Closed &&
                           history.consume({1, 1, 41, 40, 4, true}) == R::Closed &&
                           history.observe({1, 1, 40, 4, true}) == R::Closed);
    RPCMP_CHECK(suite, copy(history).availability == api::PerformanceAvailability::Waiting);
    RPCMP_CHECK(suite, history.begin(2, 2));
    RPCMP_CHECK(suite, history.retain(checkpoint(0, 1, {1})) == R::Stale);
    auto next = checkpoint(0, 0, {});
    next.generation = next.epoch = 2;
    RPCMP_CHECK(suite, history.retain(next) == R::Accepted);
    RPCMP_CHECK(suite, history.consume({2, 2, 1, 0, 1, true}) == R::Accepted);
    RPCMP_CHECK(suite, history.observe({2, 2, 0, 1, true}) == R::Accepted);
    RPCMP_CHECK(suite, copy(history).count == 0 && !copy(history).capture_lost);
  }
  MdxOutputHistory end(3'579'545);
  RPCMP_CHECK(suite, end.begin(1, 1));
  RPCMP_CHECK(suite, end.retain(checkpoint(0, 2, {1, 2})) == R::Accepted);
  RPCMP_CHECK(suite, end.consume({1, 1, 1, 10, 1}) == R::Accepted);
  RPCMP_CHECK(suite, end.consume({1, 1, 3, 20, 3, true}) == R::Degraded);
  publish(suite, end, 20, 3, true);
  RPCMP_CHECK(suite,
              copy(end).count == 1 && copy(end).next_sequence == 3 && copy(end).capture_lost);
}

void validation_and_limits(rpcmp::test::Suite& suite) {
  constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
  const auto bad_batch = [&](auto mutate) {
    MdxOutputHistory history(3'579'545);
    RPCMP_CHECK(suite, history.begin(1, 1));
    auto input = checkpoint(0, 3, {1, 2, 3});
    mutate(input);
    RPCMP_CHECK(suite, history.retain(input) == R::Invalid);
    RPCMP_CHECK(suite,
                history.pending_batches() == 0 && api::valid_performance_history(copy(history)));
    RPCMP_CHECK(suite, history.retain(checkpoint(0, 0, {})) == R::Accepted);
    RPCMP_CHECK(suite, history.consume({1, 1, 1, 10, 1}) == R::Accepted);
    publish(suite, history, 11, 1);
    RPCMP_CHECK(suite, copy(history).capture_lost);
  };
  bad_batch([](auto& i) { i.performance.batch.count = 257; });
  bad_batch([](auto& i) { i.performance.batch.write_count = 8193; });
  bad_batch([](auto& i) { i.preceding_token = maximum - 2; });
  bad_batch([](auto& i) { i.marker_token = 3; });
  bad_batch([](auto& i) { i.performance.batch.events[0].after_write = 0; });
  bad_batch([](auto& i) { i.performance.batch.events[2].after_write = 1; });
  bad_batch([](auto& i) { i.performance.batch.events[2].after_write = 4; });
  const auto bad_record = [&](auto mutate) {
    MdxOutputHistory history(3'579'545);
    RPCMP_CHECK(suite, history.begin(1, 1));
    RPCMP_CHECK(suite, history.retain(checkpoint(0, 1, {1})) == R::Accepted);
    MdxOutputRecord input{1, 1, 1, 10, 2};
    mutate(input);
    RPCMP_CHECK(suite, history.consume(input) == R::Invalid);
    RPCMP_CHECK(suite, history.pending_batches() == 0);
    RPCMP_CHECK(suite, history.retain(checkpoint(2, 0, {})) == R::Accepted);
    RPCMP_CHECK(suite, history.consume({1, 1, 1, 20, 3}) == R::Accepted);
    publish(suite, history, 21, 3);
    RPCMP_CHECK(suite, copy(history).capture_lost && copy(history).next_sequence == 2);
  };
  bad_record([](auto& i) { i.sequence = 0; });
  bad_record([](auto& i) { i.sequence = maximum; });
  bad_record([](auto& i) { i.prefix = 0; });
  bad_record([](auto& i) { i.prefix = 3; });
  bad_record([](auto& i) { i.frame = maximum; });

  MdxOutputHistory full(3'579'545);
  RPCMP_CHECK(suite, full.begin(1, 1));
  auto input = checkpoint(0, 8192, {});
  input.performance.batch.count = 256;
  input.performance.batch.lost_before = 17;
  for (std::uint16_t i = 0; i < 256; ++i)
    input.performance.batch.events[i] = {static_cast<std::uint16_t>((i + 1U) * 32U), 0,
                                         mdx::Ym2151PerformanceKind::KeyOn,
                                         input.performance.batch.channels[0]};
  RPCMP_CHECK(suite, full.retain(input) == R::Accepted);
  RPCMP_CHECK(suite, full.consume({1, 1, 1, maximum - 1, 8193}) == R::Accepted);
  publish(suite, full, maximum, 8193);
  RPCMP_CHECK(suite, copy(full).count == 256 && copy(full).events[0].sequence == 18 &&
                         copy(full).events[255].sequence == 273 && copy(full).next_sequence == 274);
  RPCMP_CHECK(suite, full.consume({1, 1, 2, maximum, 8193, true}) == R::Accepted);
  publish(suite, full, maximum, 8193, true);

  MdxOutputHistory high(3'579'545);
  RPCMP_CHECK(suite, high.begin(1, 1));
  auto last = checkpoint(0, 1, {1});
  last.preceding_token = maximum - 2;
  last.marker_token = maximum;
  RPCMP_CHECK(suite, high.retain(last) == R::Degraded);
  RPCMP_CHECK(suite, high.consume({1, 1, 1, 1, maximum}) == R::Accepted);
  publish(suite, high, 2, maximum);
  RPCMP_CHECK(suite, copy(high).capture_lost && copy(high).count == 1);

  // Bad semantic observations discard only display data; a later complete
  // independent checkpoint on the same boundary restores current channels.
  MdxOutputHistory semantic(3'579'545);
  RPCMP_CHECK(suite, semantic.begin(1, 1));
  auto malformed = checkpoint(0, 1, {1});
  malformed.performance.batch.events[0].channel = 8;
  RPCMP_CHECK(suite, semantic.retain(malformed) == R::Accepted);
  RPCMP_CHECK(suite, semantic.retain(checkpoint(2, 1, {1})) == R::Accepted);
  RPCMP_CHECK(suite, semantic.consume({1, 1, 1, 10, 4}) == R::Invalid);
  publish(suite, semantic, 11, 4);
  RPCMP_CHECK(suite, copy(semantic).capture_lost && copy(semantic).count == 1 &&
                         copy(semantic).events[0].sequence == 2 &&
                         copy(semantic).next_sequence == 3);

  for (unsigned kind = 0; kind < 4; ++kind) {
    MdxOutputHistory ordered(3'579'545);
    RPCMP_CHECK(suite, ordered.begin(1, 1));
    RPCMP_CHECK(suite, ordered.retain(checkpoint(0, 3, {1, 2, 3})) == R::Accepted);
    RPCMP_CHECK(suite, ordered.consume({1, 1, 1, 10, 2}) == R::Accepted);
    MdxOutputRecord wrong{1, 1, 2, 11, 4};
    if (kind == 0)
      wrong.frame = 10;
    if (kind == 1)
      wrong.frame = 9;
    if (kind == 2)
      wrong.prefix = 1;
    if (kind == 3)
      wrong.prefix = 2;
    RPCMP_CHECK(suite, ordered.consume(wrong) == R::Invalid);
    RPCMP_CHECK(suite, ordered.observe({1, 1, 12, 4}) == R::Waiting);
  }
}

std::vector<MdxSourceOffer> engine_trace(rpcmp::test::Suite& suite, unsigned read_interval,
                                         bool delay_records) {
  constexpr std::array<std::uint8_t, 15> bytes{0xff, 0xc8, 0xfe, 0x28, 0x3c, 0xfe, 0x30, 0,
                                               0xfe, 8,    0x78, 0,    0xf1, 0xff, 0xf1};
  constexpr std::array<std::uint8_t, 2> end{0xf1, 0};
  mdx::MdxDocument document;
  for (std::size_t i = 0; i < document.tracks.size(); ++i)
    document.tracks[i] = {static_cast<std::uint8_t>(i),
                          i < 8 ? mdx::TrackTarget::Ym2151 : mdx::TrackTarget::LegacyAdpcm,
                          i * 100,
                          {end.data(), end.size()}};
  document.tracks[0].source = {bytes.data(), bytes.size()};
  static MdxSourceWorkspace workspace;
  mdx::DocumentValidation validation;
  RPCMP_CHECK(suite, mdx::prepare_mdx_playback(document, validation, workspace.engine).ok());
  mdx::MdxEngineState state;
  RetainedMdxSource source;
  MdxOutputHistory history(3'579'545);
  MdxProducedCheckpoint produced;
  RPCMP_CHECK(suite, source.begin(1, 1) == MdxSourceResult::Accepted && history.begin(1, 1));
  std::vector<MdxSourceOffer> trace;
  for (std::uint64_t tick = 0; tick < 32; ++tick) {
    RPCMP_CHECK(suite, produce_mdx_tick(document, state, source, produced, workspace).status ==
                           MdxSourceResult::Accepted);
    RPCMP_CHECK(suite, history.retain(produced) ==
                           (delay_records && tick >= 16 ? R::Degraded : R::Accepted));
    while (const auto offer = source.take_offer()) {
      trace.push_back(*offer);
      RPCMP_CHECK(suite, source.complete({1, 1, offer->token, MdxFeedStatus::Full}) ==
                             MdxSourceResult::Full);
      const auto retry = source.take_offer();
      if (!retry)
        throw std::logic_error("missing retained audio retry");
      trace.push_back(*retry);
      RPCMP_CHECK(suite, source.complete({1, 1, retry->token, MdxFeedStatus::Accepted}) ==
                             MdxSourceResult::Accepted);
    }
    if (!delay_records) {
      RPCMP_CHECK(suite, history.consume({1, 1, tick + 1U, tick * 100U, produced.marker_token}) ==
                             R::Accepted);
      publish(suite, history, tick * 100U + 1, produced.marker_token);
    }
    if (read_interval != 0 && tick % read_interval == 0)
      RPCMP_CHECK(suite, api::valid_performance_history(copy(history)));
  }
  if (delay_records) {
    RPCMP_CHECK(suite, history.consume({1, 1, 1, 3200, produced.marker_token}) == R::Accepted);
    publish(suite, history, 3201, produced.marker_token);
    RPCMP_CHECK(suite, copy(history).capture_lost && copy(history).channels[0].key_on == true);
  }
  RPCMP_CHECK(suite, source.accepted_token() == 160 && trace.size() == 320);
  return trace;
}

void audio_independence(rpcmp::test::Suite& suite) {
  const auto dense = engine_trace(suite, 1, false);
  for (const auto& other : {engine_trace(suite, 7, false), engine_trace(suite, 0, false),
                            engine_trace(suite, 0, true)}) {
    RPCMP_CHECK(suite, other.size() == dense.size());
    for (std::size_t i = 0; i < dense.size() && i < other.size(); ++i) {
      const auto& a = dense[i];
      const auto& b = other[i];
      RPCMP_CHECK(suite, a.generation == b.generation && a.epoch == b.epoch && a.token == b.token &&
                             a.at_tick == b.at_tick && a.until_tick == b.until_tick &&
                             a.completed_loops == b.completed_loops && a.address == b.address &&
                             a.value == b.value && a.marker == b.marker && a.ended == b.ended);
    }
  }
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  exact_frames(suite);
  missing_middle(suite);
  retention_and_source_gap(suite);
  recovery_and_identity(suite);
  validation_and_limits(suite);
  audio_independence(suite);
  std::cout << "Host bytes: output history=" << sizeof(MdxOutputHistory) << '\n';
  return suite.finish("MDX output history");
}
