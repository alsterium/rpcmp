#include "rpcmp/runtime/mdx_engine.hpp"
#include "test_support.hpp"

#include <array>
#include <limits>

namespace {
using namespace rpcmp::runtime::mdx;
constexpr std::array<std::uint8_t, 2> kEnd{0xf1, 0x00};
// One or two rest ticks, then a back edge from offset 4 to offset 0.
constexpr std::array<std::uint8_t, 4> kFast{0x00, 0xf1, 0xff, 0xfc};
constexpr std::array<std::uint8_t, 4> kSlow{0x01, 0xf1, 0xff, 0xfc};
template <std::size_t N> ByteView bytes(const std::array<std::uint8_t, N>& data) {
  return {data.data(), data.size()};
}
MdxDocument document(ByteView first = bytes(kEnd)) {
  MdxDocument result;
  for (std::size_t i = 0; i < result.tracks.size(); ++i)
    result.tracks[i] = {static_cast<std::uint8_t>(i),
                        i < 8 ? TrackTarget::Ym2151 : TrackTarget::LegacyAdpcm, i * 100,
                        bytes(kEnd)};
  result.tracks[0].source = first;
  return result;
}

void aggregate(rpcmp::test::Suite& suite) {
  auto input = document(bytes(kFast));
  input.tracks[1].source = bytes(kSlow);
  DocumentPlaybackState state;
  DocumentTickBatch batch;
  static DocumentTickScratch scratch;
  // P is excluded even if its runtime state is not marked ended.
  RPCMP_CHECK(suite, !state.tracks[8].ended);
  constexpr std::array<std::uint64_t, 7> expected{0, 0, 1, 1, 2, 2, 3};
  for (std::size_t tick = 0; tick < expected.size(); ++tick) {
    RPCMP_CHECK(suite, advance_document_tick(input, state, batch, scratch).ok());
    RPCMP_CHECK(suite, state.tracks[0].completed_loops == tick);
    RPCMP_CHECK(suite, state.tracks[1].completed_loops == expected[tick]);
    RPCMP_CHECK(suite, state.completed_loops == expected[tick] && !state.ended);
  }
  // At tick 2, B ends while A completes its second loop. Count only after both.
  constexpr std::array<std::uint8_t, 3> finite{0x01, 0xf1, 0x00};
  input.tracks[1].source = bytes(finite);
  state = {};
  for (unsigned tick = 0; tick < 3; ++tick)
    RPCMP_CHECK(suite, advance_document_tick(input, state, batch, scratch).ok());
  RPCMP_CHECK(suite, state.tracks[1].ended && state.completed_loops == 2 && !state.ended);

  constexpr std::array<std::uint8_t, 3> waiting{0xee, 0xf1, 0x00};
  input.tracks[1].source = bytes(waiting);
  state = {};
  for (unsigned tick = 0; tick < 10; ++tick)
    RPCMP_CHECK(suite, advance_document_tick(input, state, batch, scratch).ok());
  RPCMP_CHECK(suite, state.tracks[0].completed_loops == 9 && state.tracks[1].waiting &&
                         state.completed_loops == 0 && !state.ended);

  input = document();
  state = {};
  state.completed_loops = 7;
  RPCMP_CHECK(suite, advance_document_tick(input, state, batch, scratch).ok());
  RPCMP_CHECK(suite, state.ended && state.completed_loops == 7 && !state.tracks[8].ended);
}

void short_repeats(rpcmp::test::Suite& suite) {
  // Nested 2x2 repeats of rest, followed by whole-track loop back to the start.
  constexpr std::array<std::uint8_t, 16> nested{0xf6, 2,    0,    0xf6, 2,    0,    0,    0xf5,
                                                0xff, 0xfc, 0xf5, 0xff, 0xf6, 0xf1, 0xff, 0xf0};
  auto input = document(bytes(nested));
  DocumentPlaybackState state;
  DocumentTickBatch batch;
  static DocumentTickScratch scratch;
  for (unsigned tick = 0; tick < 9; ++tick) {
    RPCMP_CHECK(suite, advance_document_tick(input, state, batch, scratch).ok());
    RPCMP_CHECK(suite, state.completed_loops == tick / 4);
  }
  // Final-repeat escape jumps past RepeatEnd; neither event counts as TrackLoop.
  constexpr std::array<std::uint8_t, 12> escaped{0xf6, 2,    0,    0,    0xf4, 0,
                                                 1,    0xf5, 0xff, 0xf9, 0xf1, 0};
  input = document(bytes(escaped));
  state = {};
  for (unsigned tick = 0; tick < 3; ++tick) {
    RPCMP_CHECK(suite, advance_document_tick(input, state, batch, scratch).ok());
    RPCMP_CHECK(suite, state.completed_loops == 0);
  }
  RPCMP_CHECK(suite, state.ended);
}

void limits_and_rollback(rpcmp::test::Suite& suite) {
  auto input = document(bytes(kFast));
  DocumentPlaybackState state;
  DocumentTickBatch batch;
  static DocumentTickScratch scratch;
  state.tracks[0].cursor = 1;
  state.tracks[0].completed_loops = std::numeric_limits<std::uint32_t>::max();
  RPCMP_CHECK(suite, advance_document_tick(input, state, batch, scratch).ok());
  RPCMP_CHECK(suite, state.completed_loops == 4'294'967'296ULL);
  state.tracks[0].completed_loops = std::numeric_limits<std::uint64_t>::max() - 1;
  RPCMP_CHECK(suite, advance_document_tick(input, state, batch, scratch).ok());
  RPCMP_CHECK(suite, state.completed_loops == std::numeric_limits<std::uint64_t>::max());
  batch.count = 1;
  batch.actions[0].logical_channel = 99;
  const auto before = state;
  const auto overflow = advance_document_tick(input, state, batch, scratch);
  RPCMP_CHECK(suite, overflow.error == DecodeError::ArithmeticOverflow &&
                         overflow.byte_offset == 1 && overflow.logical_track == 0);
  RPCMP_CHECK(suite, state.tracks[0].completed_loops == before.tracks[0].completed_loops &&
                         state.tracks[0].remaining_ticks == before.tracks[0].remaining_ticks &&
                         state.tracks[0].cursor == before.tracks[0].cursor &&
                         state.completed_loops == before.completed_loops);
  RPCMP_CHECK(suite, batch.count == 1 && batch.actions[0].logical_channel == 99);

  constexpr std::array<std::uint8_t, 1> unsupported{0xe0};
  input.tracks[7].source = bytes(unsupported);
  state = {};
  state.tracks[0].cursor = 1;
  state.tracks[0].completed_loops = 9;
  const auto later = advance_document_tick(input, state, batch, scratch);
  RPCMP_CHECK(suite, !later.ok() && later.logical_track == 7);
  RPCMP_CHECK(suite, state.tracks[0].completed_loops == 9 && state.tracks[0].cursor == 1 &&
                         !state.tracks[1].ended && state.completed_loops == 0 && !state.ended);
  RPCMP_CHECK(suite, batch.count == 1 && batch.actions[0].logical_channel == 99);
}

void timestamp_and_engine_rollback(rpcmp::test::Suite& suite) {
  auto input = document(bytes(kFast));
  MdxEngineState state;
  TimedYm2151Batch batch;
  static MdxEngineScratch scratch;
  for (unsigned tick = 0; tick < 4; ++tick) {
    RPCMP_CHECK(suite, advance_mdx_tick(input, 48'000, state, batch, scratch).ok());
    // Timer B=200: interval is 86016/125 frames. Rest/loops produce no writes.
    constexpr std::array<std::uint64_t, 4> timestamps{0, 688, 1376, 2064};
    RPCMP_CHECK(suite, batch.count == 0 && state.progress.at_tick == timestamps[tick] &&
                           state.progress.completed_loops == tick && !state.progress.ended);
  }
  const auto retained = state.progress;
  constexpr std::array<std::uint8_t, 6> missing_voice{0xfd, 4, 0x80, 0, 0xf1, 0};
  input.tracks[7].source = bytes(missing_voice);
  state.document.tracks[7] = {};
  batch.count = 1;
  batch.writes[0].at_tick = 99;
  const auto routing = advance_mdx_tick(input, 48'000, state, batch, scratch);
  RPCMP_CHECK(suite, routing.error == DecodeError::MissingVoice);
  RPCMP_CHECK(suite, state.document.tracks[0].completed_loops == 3 &&
                         state.progress.at_tick == retained.at_tick &&
                         state.progress.completed_loops == retained.completed_loops &&
                         !state.progress.ended && batch.count == 1 &&
                         batch.writes[0].at_tick == 99);
  input = document(bytes(kFast));
  state.document.tracks[7].ended = true;
  state.timeline.scheduler_tick = std::numeric_limits<std::uint64_t>::max();
  const auto time = advance_mdx_tick(input, 48'000, state, batch, scratch);
  RPCMP_CHECK(suite, time.error == DecodeError::ArithmeticOverflow);
  RPCMP_CHECK(suite, state.document.tracks[0].completed_loops == 3 &&
                         state.progress.at_tick == retained.at_tick &&
                         state.progress.completed_loops == retained.completed_loops &&
                         batch.count == 1 && batch.writes[0].at_tick == 99);

  state = {};
  input = document();
  state.timeline.scheduler_tick = 17;
  RPCMP_CHECK(suite, advance_mdx_tick(input, 12'288'000, state, batch, scratch).ok());
  RPCMP_CHECK(suite, state.progress.ended && state.progress.at_tick == 17 &&
                         state.progress.completed_loops == 0);
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  aggregate(suite);
  short_repeats(suite);
  limits_and_rollback(suite);
  timestamp_and_engine_rollback(suite);
  return suite.finish("MDX sequencing progress");
}
