#include "rpcmp/player/mdx_source_producer.hpp"
#include "test_support.hpp"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace rpcmp::player;
namespace mdx = rpcmp::runtime::mdx;
using R = MdxSourceResult;
using S = MdxFeedStatus;

MdxSourceOffer take(RetainedMdxSource& source) {
  const auto value = source.take_offer();
  if (!value)
    throw std::logic_error("expected an owned offer");
  return *value;
}
MdxFeedCompletion reply(const MdxSourceOffer& offer, S status = S::Accepted) {
  return {offer.generation, offer.epoch, offer.token, status};
}
bool same(const MdxSourceOffer& a, const MdxSourceOffer& b) {
  return a.generation == b.generation && a.epoch == b.epoch && a.token == b.token &&
         a.at_tick == b.at_tick && a.until_tick == b.until_tick &&
         a.completed_loops == b.completed_loops && a.address == b.address && a.value == b.value &&
         a.marker == b.marker && a.ended == b.ended;
}
mdx::TimedYm2151Batch& writes(std::size_t count, std::uint64_t at = 0) {
  static mdx::TimedYm2151Batch result;
  result.count = count;
  for (std::size_t i = 0; i < count && i < result.writes.size(); ++i)
    result.writes[i] = {at,
                        {static_cast<std::uint8_t>(i % 256),
                         static_cast<std::uint8_t>((i / 256) % 256),
                         static_cast<std::uint8_t>(i % 8)}};
  return result;
}

void maximum_and_copy(rpcmp::test::Suite& suite) {
  RetainedMdxSource source;
  RPCMP_CHECK(suite, source.availability() == R::Closed && !source.take_offer());
  RPCMP_CHECK(suite, source.begin(7, 4) == R::Accepted);
  auto& input = writes(8192);
  RPCMP_CHECK(suite, source.retain(input, {0, 3, false}, 1000) == R::Accepted);
  for (auto& item : input.writes)
    item = {999, {255, 255, 255}};
  RPCMP_CHECK(suite, source.retain(input, {0, 3, false}, 1000) == R::Busy);
  for (std::uint64_t i = 0; i < 8193; ++i) {
    const auto offered = take(source);
    RPCMP_CHECK(suite, !source.take_offer() && source.accepted_token() == i);
    RPCMP_CHECK(suite, offered.generation == 7 && offered.epoch == 4 && offered.token == i + 1 &&
                           offered.at_tick == 0 && !offered.ended);
    if (i < 8192) {
      RPCMP_CHECK(suite, !offered.marker && offered.address == i % 256 &&
                             offered.value == i / 256 && offered.until_tick == 0 &&
                             offered.completed_loops == 0);
    } else {
      RPCMP_CHECK(suite, offered.marker && offered.address == 0 && offered.value == 0 &&
                             offered.until_tick == 1000 && offered.completed_loops == 3);
    }
    // Every write and the marker survives repeated backpressure verbatim.
    for (unsigned retry = 0; retry < 3; ++retry) {
      RPCMP_CHECK(suite, source.complete(reply(offered, S::Full)) == R::Full);
      RPCMP_CHECK(suite, source.accepted_token() == i);
      RPCMP_CHECK(suite, same(offered, take(source)));
    }
    RPCMP_CHECK(suite, source.complete(reply(offered)) == R::Accepted);
  }
  RPCMP_CHECK(suite, source.availability() == R::Accepted && source.accepted_token() == 8193);
  RPCMP_CHECK(suite, source.retain(writes(0, 1000), {1000, 2, false}, 2000) == R::Invalid);
  RPCMP_CHECK(suite, source.retain(writes(0, 999), {999, 3, false}, 2000) == R::Invalid);
  RPCMP_CHECK(suite, source.retain(writes(0, 1000), {1000, 3, true}, 0) == R::Accepted);
  const auto end = take(source);
  RPCMP_CHECK(suite, end.marker && end.ended && end.token == 8194 && end.at_tick == 1000 &&
                         end.until_tick == 0 && end.completed_loops == 3);
  RPCMP_CHECK(suite, source.complete(reply(end)) == R::Accepted);
  RPCMP_CHECK(suite, source.availability() == R::Closed && !source.take_offer());
}

void validation(rpcmp::test::Suite& suite) {
  RetainedMdxSource source;
  RPCMP_CHECK(suite, source.begin(0, 1) == R::Invalid && source.begin(1, 0) == R::Invalid);
  RPCMP_CHECK(suite, source.begin(1, 1) == R::Accepted);
  auto& input = writes(8193);
  RPCMP_CHECK(suite, source.retain(input, {0, 0, false}, 1) == R::Invalid);
  input.count = 8192;
  input.writes[8191].at_tick = 1;
  RPCMP_CHECK(suite, source.retain(input, {0, 0, false}, 1) == R::Invalid);
  input.writes[8191].at_tick = 0;
  input.writes[8191].write.logical_channel = 8;
  RPCMP_CHECK(suite, source.retain(input, {0, 0, false}, 1) == R::Invalid);
  RPCMP_CHECK(suite, source.availability() == R::Accepted && !source.take_offer() &&
                         source.accepted_token() == 0);
  input = writes(1);
  RPCMP_CHECK(suite, source.retain(input, {0, 0, false}, 0) == R::Invalid);
  RPCMP_CHECK(suite, source.retain(input, {0, 0, true}, 1) == R::Invalid);
  constexpr auto max = std::numeric_limits<std::uint64_t>::max();
  RPCMP_CHECK(suite,
              validate_mdx_source_tick(input, {0, 0, false}, 1, {0, 0, max - 2}) == R::Accepted);
  RPCMP_CHECK(suite,
              validate_mdx_source_tick(input, {0, 0, false}, 1, {0, 0, max - 1}) == R::Exhausted);
  auto& empty = writes(0);
  RPCMP_CHECK(suite, validate_mdx_source_tick(empty, {max, max, true}, 0, {max, max, max - 1}) ==
                         R::Accepted);
  RPCMP_CHECK(suite, validate_mdx_source_tick(empty, {max, max, true}, 0, {max, max, max}) ==
                         R::Exhausted);
  RPCMP_CHECK(suite, validate_mdx_source_tick(empty, {max - 1, 0, false}, max, {max - 1, 0, 0}) ==
                         R::Accepted);
  RPCMP_CHECK(suite,
              validate_mdx_source_tick(empty, {max, 0, false}, 0, {max, 0, 0}) == R::Invalid);
  RPCMP_CHECK(suite, source.retain(writes(1), {0, 0, false}, 1) == R::Accepted);
  RPCMP_CHECK(suite, take(source).token == 1);
}

void identity_and_failures(rpcmp::test::Suite& suite) {
  RetainedMdxSource source;
  RPCMP_CHECK(suite, source.begin(9, 5) == R::Accepted);
  RPCMP_CHECK(suite, source.retain(writes(2), {0, 0, false}, 10) == R::Accepted);
  const auto first = take(source);
  RPCMP_CHECK(suite, source.complete(reply(first)) == R::Accepted);
  const auto old = take(source);
  RPCMP_CHECK(suite, old.token == 2);
  RPCMP_CHECK(suite, source.begin(8, 6) == R::Invalid && source.begin(9, 5) == R::Invalid);
  RPCMP_CHECK(suite, source.accepted_token() == 1 && !source.take_offer());
  // Same prepared generation after another confirmed sound Reset, new epoch.
  RPCMP_CHECK(suite, source.begin(9, 6) == R::Accepted);
  RPCMP_CHECK(suite, source.retain(writes(0), {0, 0, true}, 0) == R::Accepted);
  const auto current = take(source);
  RPCMP_CHECK(suite, source.complete(reply(old)) == R::Stale && source.accepted_token() == 0);
  auto wrong_generation = reply(current);
  ++wrong_generation.generation;
  RPCMP_CHECK(suite, source.complete(wrong_generation) == R::Stale && !source.take_offer());
  RPCMP_CHECK(suite, source.complete(reply(current)) == R::Accepted);
  RPCMP_CHECK(suite, source.complete(reply(current)) == R::ProtocolError);
  RPCMP_CHECK(suite, source.availability() == R::DeviceFailure && source.accepted_token() == 1);

  for (const auto status : {S::Invalid, S::Closed, S::Stale, S::Failed}) {
    RetainedMdxSource failed;
    RPCMP_CHECK(suite, failed.begin(1, 1) == R::Accepted);
    RPCMP_CHECK(suite, failed.retain(writes(1), {0, 0, false}, 10) == R::Accepted);
    const auto item = take(failed);
    RPCMP_CHECK(suite, failed.complete(reply(item, status)) == R::DeviceFailure);
    RPCMP_CHECK(suite, failed.complete(reply(item)) == R::DeviceFailure);
    RPCMP_CHECK(suite, !failed.take_offer() && failed.accepted_token() == 0);
  }
  for (const auto wrong_token : {false, true}) {
    RetainedMdxSource failed;
    RPCMP_CHECK(suite, failed.begin(1, 1) == R::Accepted);
    RPCMP_CHECK(suite, failed.retain(writes(1), {0, 0, false}, 10) == R::Accepted);
    auto result = reply(take(failed));
    if (wrong_token)
      ++result.token;
    else
      result.status = static_cast<S>(0);
    RPCMP_CHECK(suite, failed.complete(result) == R::ProtocolError && !failed.take_offer());
  }
  RPCMP_CHECK(suite, source.begin(10, 7) == R::Accepted);
  RPCMP_CHECK(suite, source.retain(writes(1), {0, 0, false}, 10) == R::Accepted);
  const auto cancelled = take(source);
  source.cancel();
  RPCMP_CHECK(suite, source.complete(reply(cancelled)) == R::Stale && !source.take_offer());
  RPCMP_CHECK(suite, source.availability() == R::Closed && source.accepted_token() == 0);
}

mdx::MdxDocument document(mdx::ByteView first) {
  static constexpr std::array<std::uint8_t, 2> end{0xf1, 0};
  mdx::MdxDocument result;
  for (std::size_t i = 0; i < result.tracks.size(); ++i)
    result.tracks[i] = {static_cast<std::uint8_t>(i),
                        i < 8 ? mdx::TrackTarget::Ym2151 : mdx::TrackTarget::LegacyAdpcm,
                        i * 100,
                        {end.data(), end.size()}};
  result.tracks[0].source = first;
  return result;
}

void engine_connection(rpcmp::test::Suite& suite) {
  // Timer B, raw KC/KF/key on, one rest tick, then loop to byte zero.
  constexpr std::array<std::uint8_t, 15> bytes{0xff, 0xc8, 0xfe, 0x28, 0x3c, 0xfe, 0x30, 0,
                                               0xfe, 8,    0x78, 0,    0xf1, 0xff, 0xf1};
  const auto input = document({bytes.data(), bytes.size()});
  static MdxSourceWorkspace workspace;
  mdx::DocumentValidation validation;
  RPCMP_CHECK(suite, mdx::prepare_mdx_playback(input, validation, workspace.engine).ok());
  RetainedMdxSource source;
  mdx::MdxEngineState state;
  MdxProducedCheckpoint checkpoint;
  RPCMP_CHECK(suite, source.begin(11, 13) == R::Accepted);
  // 56*1024*12,288,000/4,000,000 = 22,020,096/125 retained audio ticks.
  for (std::uint64_t tick = 0; tick < 17; ++tick) {
    const auto at = tick * 22'020'096 / 125;
    const auto until = (tick + 1) * 22'020'096 / 125;
    RPCMP_CHECK(suite, produce_mdx_tick(input, state, source, checkpoint, workspace).status ==
                           R::Accepted);
    RPCMP_CHECK(suite, state.timeline.scheduler_tick == until && state.progress.at_tick == at &&
                           state.progress.completed_loops == tick);
    RPCMP_CHECK(suite, checkpoint.generation == 11 && checkpoint.epoch == 13 &&
                           checkpoint.preceding_token == tick * 5 &&
                           checkpoint.marker_token == tick * 5 + 5 &&
                           checkpoint.performance.at_tick == at);
    if (tick == 0) {
      RPCMP_CHECK(suite, checkpoint.performance.batch.count == 2 &&
                             checkpoint.performance.batch.events[0].after_write == 3 &&
                             checkpoint.performance.batch.events[1].after_write == 4);
    }
    constexpr std::array<std::uint8_t, 4> addresses{0x12, 0x28, 0x30, 8};
    constexpr std::array<std::uint8_t, 4> values{0xc8, 0x3c, 0, 0x78};
    for (unsigned index = 0; index < 5; ++index) {
      const auto offered = take(source);
      RPCMP_CHECK(suite, offered.at_tick == at && offered.token == tick * 5 + index + 1);
      if (index < 4)
        RPCMP_CHECK(suite, offered.address == addresses[index] && offered.value == values[index] &&
                               !offered.marker);
      else
        RPCMP_CHECK(suite, offered.marker && offered.until_tick == until &&
                               offered.completed_loops == tick);
      for (unsigned retry = 0; retry < 4; ++retry) {
        RPCMP_CHECK(suite, source.complete(reply(offered, S::Full)) == R::Full);
        RPCMP_CHECK(suite, produce_mdx_tick(input, state, source, checkpoint, workspace).status ==
                               R::Busy);
        RPCMP_CHECK(suite,
                    state.timeline.scheduler_tick == until && checkpoint.performance.at_tick == at);
        RPCMP_CHECK(suite, same(offered, take(source)));
      }
      RPCMP_CHECK(suite, source.complete(reply(offered)) == R::Accepted);
    }
  }
  source.cancel();
  const auto old_time = state.timeline.scheduler_tick;
  RPCMP_CHECK(suite,
              produce_mdx_tick(input, state, source, checkpoint, workspace).status == R::Closed);
  RPCMP_CHECK(suite, state.timeline.scheduler_tick == old_time);

  constexpr std::array<std::uint8_t, 6> missing_voice{0xfd, 4, 0x80, 0, 0xf1, 0};
  const auto bad = document({missing_voice.data(), missing_voice.size()});
  RPCMP_CHECK(suite, source.begin(12, 14) == R::Accepted);
  state = {};
  const auto before = checkpoint.marker_token;
  const auto failed = produce_mdx_tick(bad, state, source, checkpoint, workspace);
  RPCMP_CHECK(suite, failed.status == R::EngineFailure &&
                         failed.engine.error == mdx::DecodeError::MissingVoice);
  RPCMP_CHECK(suite, state.timeline.scheduler_tick == 0 && state.document.tracks[0].cursor == 0 &&
                         checkpoint.marker_token == before && !source.take_offer());
  // A valid candidate whose start disagrees with the retained stream also rolls back.
  state.timeline.scheduler_tick = 10;
  RPCMP_CHECK(suite,
              produce_mdx_tick(input, state, source, checkpoint, workspace).status == R::Invalid);
  RPCMP_CHECK(suite, state.timeline.scheduler_tick == 10 && checkpoint.marker_token == before &&
                         !source.take_offer());
  state = {};
  constexpr std::array<std::uint8_t, 2> end{0xf1, 0};
  const auto finite = document({end.data(), end.size()});
  RPCMP_CHECK(suite,
              produce_mdx_tick(finite, state, source, checkpoint, workspace).status == R::Accepted);
  const auto marker = take(source);
  RPCMP_CHECK(suite, marker.marker && marker.ended && marker.until_tick == 0 && marker.token == 1 &&
                         checkpoint.performance.batch.write_count == 0 &&
                         checkpoint.marker_token == 1);
  RPCMP_CHECK(suite, source.complete(reply(marker)) == R::Accepted);
  RPCMP_CHECK(suite,
              produce_mdx_tick(finite, state, source, checkpoint, workspace).status == R::Closed);
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  maximum_and_copy(suite);
  validation(suite);
  identity_and_failures(suite);
  engine_connection(suite);
  std::cout << "Host sizes: retained=" << sizeof(RetainedMdxSource)
            << " workspace=" << sizeof(MdxSourceWorkspace)
            << " checkpoint=" << sizeof(MdxProducedCheckpoint) << '\n';
  return suite.finish("Retained MDX source producer");
}
