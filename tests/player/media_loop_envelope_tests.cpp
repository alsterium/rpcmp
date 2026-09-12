#include "rpcmp/player/media_loop_envelope.hpp"
#include "rpcmp/runtime/mdx_engine.hpp"
#include "test_support.hpp"

#include <cstring>
#include <tuple>
#include <utility>

namespace {
using namespace rpcmp::player;
MediaBoundaryControl action(MediaControlAction value) { return {false, value, std::nullopt, 1}; }
MediaBoundaryControl policy(std::uint64_t revision, std::optional<std::uint32_t> target,
                            MediaControlAction value = MediaControlAction::Keep) {
  return {false, value, MediaRepeatUpdate{revision, target}, 1};
}
bool render_to(MediaLoopEnvelope& envelope, std::uint64_t target) {
  const auto start = envelope.snapshot().frame;
  if (target < start)
    return false;
  for (auto frame = start; frame < target; ++frame)
    if (!envelope.advance({32767, -32768}).consumed)
      return false;
  return envelope.snapshot().frame == target;
}
void check_sample(rpcmp::test::Suite& suite, const MediaFrameResult& result, std::int16_t left,
                  std::int16_t right, bool consumed = true) {
  RPCMP_CHECK(suite, result.output.left == left && result.output.right == right &&
                         result.consumed == consumed);
}
void prepare_loop(rpcmp::test::Suite& suite, MediaLoopEnvelope& envelope,
                  std::optional<std::uint32_t> target = 2) {
  RPCMP_CHECK(suite, envelope.begin(1, target, 1) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, envelope.enqueue({1, 1, 0, 2, 0, false}) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, envelope.enqueue({1, 2, 2, 1'000'000, 2, false}) == MediaAdmission::Accepted);
}

void exact_boundaries(rpcmp::test::Suite& suite) {
  MediaLoopEnvelope envelope;
  RPCMP_CHECK(suite, envelope.begin(1, 2, 1) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, envelope.enqueue({1, 1, 0, 1'920'000, 0, false}) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite,
              envelope.enqueue({1, 2, 1'920'000, 3'360'000, 1, false}) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite,
              envelope.enqueue({1, 3, 3'360'000, 4'800'000, 2, false}) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, render_to(envelope, 3'360'000));
  RPCMP_CHECK(suite, envelope.snapshot().completed_loops == 1 &&
                         envelope.snapshot().phase == MediaPhase::Steady);
  check_sample(suite, envelope.advance({32767, -32768}), 32767, -32768);
  RPCMP_CHECK(suite, envelope.snapshot().completed_loops == 2 &&
                         envelope.snapshot().phase == MediaPhase::Fading &&
                         envelope.snapshot().ramp_elapsed == 1);
  RPCMP_CHECK(suite, render_to(envelope, 3'599'999));
  check_sample(suite, envelope.advance({32767, -32768}), 0, 0);
  check_sample(suite, envelope.advance({32767, -32768}), 0, 0, false);
  RPCMP_CHECK(suite, envelope.snapshot().frame == 3'600'000 &&
                         envelope.snapshot().end == MediaEnd::LoopLimit &&
                         envelope.snapshot().failure == MediaFailure::None);

  MediaLoopEnvelope rounding;
  RPCMP_CHECK(suite, rounding.begin(1, 1, 1) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, rounding.enqueue({1, 1, 0, 240'000, 1, false}) == MediaAdmission::Accepted);
  check_sample(suite, rounding.advance({1, -1}), 1, -1);
  check_sample(suite, rounding.advance({1, -1}), 0, 0); // toward zero, not floor
  RPCMP_CHECK(suite, render_to(rounding, 120'000));
  check_sample(suite, rounding.advance({32767, -32768}), 16383, -16384);
  RPCMP_CHECK(suite, render_to(rounding, 240'000));
  check_sample(suite, rounding.advance({10, -10}), 0, 0, false);
  RPCMP_CHECK(suite,
              rounding.snapshot().end == MediaEnd::LoopLimit); // no extra sample/coverage needed
}

void policy_races(rpcmp::test::Suite& suite) {
  MediaLoopEnvelope envelope;
  RPCMP_CHECK(suite, envelope.begin(1, 2, 1) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, envelope.enqueue({1, 1, 0, 2, 0, false}) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, envelope.enqueue({1, 2, 2, 4, 2, false}) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, envelope.enqueue({1, 3, 4, 1'000'000, 3, false}) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, render_to(envelope, 2));
  check_sample(suite, envelope.advance({100, -100}, policy(2, 3)), 100, -100);
  RPCMP_CHECK(suite, envelope.snapshot().phase == MediaPhase::Steady &&
                         envelope.snapshot().completed_loops == 2);
  check_sample(suite, envelope.advance({100, -100}, policy(2, 3)), 100, -100);
  check_sample(suite, envelope.advance({100, -100}), 100, -100);
  RPCMP_CHECK(suite, envelope.snapshot().phase == MediaPhase::Fading &&
                         envelope.snapshot().ramp_elapsed == 1);
  RPCMP_CHECK(suite, render_to(envelope, 14));
  static_cast<void>(envelope.advance({}, policy(3, 2)));
  RPCMP_CHECK(suite, envelope.snapshot().ramp_elapsed == 11); // original anchor 4
  static_cast<void>(envelope.advance({}, policy(4, 1)));
  RPCMP_CHECK(suite, envelope.snapshot().ramp_elapsed == 12);

  MediaLoopEnvelope late_target;
  prepare_loop(suite, late_target, std::nullopt);
  RPCMP_CHECK(suite, render_to(late_target, 80));
  check_sample(suite, late_target.advance({100, -100}, policy(2, 2)), 100, -100);
  RPCMP_CHECK(suite, late_target.snapshot().phase == MediaPhase::Fading &&
                         late_target.snapshot().ramp_elapsed == 1);
}

void pause_and_restoration(rpcmp::test::Suite& suite) {
  MediaLoopEnvelope envelope;
  prepare_loop(suite, envelope);
  RPCMP_CHECK(suite, render_to(envelope, 120'002));
  RPCMP_CHECK(suite, envelope.snapshot().gain == 120'000);
  check_sample(suite, envelope.advance({30000, -30000}, action(MediaControlAction::Pause)), 0, 0,
               false);
  check_sample(suite, envelope.advance({}, policy(2, std::nullopt)), 0, 0, false);
  for (unsigned i = 0; i < 100; ++i)
    check_sample(suite, envelope.advance({30000, -30000}), 0, 0, false);
  RPCMP_CHECK(suite, envelope.snapshot().frame == 120'002 && envelope.snapshot().gain == 120'000 &&
                         envelope.snapshot().ramp_elapsed == 120'000 &&
                         envelope.snapshot().phase == MediaPhase::Fading);
  check_sample(suite, envelope.advance({30000, -30000}, action(MediaControlAction::Resume)), 15000,
               -15000);
  RPCMP_CHECK(suite, envelope.snapshot().phase == MediaPhase::RestoringGain &&
                         envelope.snapshot().gain == 120'125);
  RPCMP_CHECK(suite, render_to(envelope, 120'482));
  RPCMP_CHECK(suite, envelope.snapshot().gain == 180'000);
  check_sample(suite, envelope.advance({30000, -30000}, policy(3, 1)), 22500, -22500);
  RPCMP_CHECK(suite, envelope.snapshot().phase == MediaPhase::Fading &&
                         envelope.snapshot().ramp_elapsed == 1);

  MediaLoopEnvelope restored;
  prepare_loop(suite, restored);
  RPCMP_CHECK(suite, render_to(restored, 120'002));
  static_cast<void>(restored.advance({}, policy(2, 5)));
  RPCMP_CHECK(suite, render_to(restored, 120'962));
  check_sample(suite, restored.advance({32767, -32768}), 32767, -32768);
  RPCMP_CHECK(suite, restored.snapshot().phase == MediaPhase::Steady &&
                         restored.snapshot().ramp_duration == 0);

  MediaLoopEnvelope boundary;
  prepare_loop(suite, boundary);
  RPCMP_CHECK(suite, render_to(boundary, 240'002));
  check_sample(suite, boundary.advance({}, policy(2, std::nullopt)), 0, 0);
  RPCMP_CHECK(suite, boundary.snapshot().end == MediaEnd::None &&
                         boundary.snapshot().phase == MediaPhase::RestoringGain);
  RPCMP_CHECK(suite, boundary.snapshot().gain == 250); // restore from zero, 240000/960 per frame

  MediaLoopEnvelope held;
  prepare_loop(suite, held);
  RPCMP_CHECK(suite, render_to(held, 2));
  check_sample(suite, held.advance({}, action(MediaControlAction::Pause)), 0, 0, false);
  RPCMP_CHECK(suite, held.snapshot().completed_loops == 0);
  check_sample(suite, held.advance({1, -1}, policy(2, std::nullopt, MediaControlAction::Resume)), 1,
               -1);
  RPCMP_CHECK(suite,
              held.snapshot().completed_loops == 2 && held.snapshot().phase == MediaPhase::Steady);
}

void finite_end_and_fault(rpcmp::test::Suite& suite) {
  for (const auto target : std::array<std::optional<std::uint32_t>, 3>{2, 3, std::nullopt}) {
    MediaLoopEnvelope envelope;
    RPCMP_CHECK(suite, envelope.begin(1, target, 1) == MediaAdmission::Accepted);
    RPCMP_CHECK(suite, envelope.enqueue({1, 1, 0, 2, 0, false}) == MediaAdmission::Accepted);
    RPCMP_CHECK(suite, envelope.enqueue({1, 2, 2, 3, 0, true}) == MediaAdmission::Accepted);
    RPCMP_CHECK(suite, render_to(envelope, 2));
    check_sample(suite, envelope.advance({5, -5}, action(MediaControlAction::Pause)), 0, 0, false);
    RPCMP_CHECK(suite, envelope.snapshot().end == MediaEnd::None);
    check_sample(suite, envelope.advance({5, -5}, action(MediaControlAction::Resume)), 0, 0, false);
    RPCMP_CHECK(suite, envelope.snapshot().frame == 2 && envelope.snapshot().gain == 0 &&
                           envelope.snapshot().end ==
                               (target ? MediaEnd::NaturalEnd : MediaEnd::RepeatOne));
  }
  MediaLoopEnvelope stopped;
  prepare_loop(suite, stopped);
  RPCMP_CHECK(suite, render_to(stopped, 2));
  check_sample(suite, stopped.advance({}, action(MediaControlAction::Stop)), 0, 0, false);
  RPCMP_CHECK(suite, stopped.snapshot().completed_loops == 0 &&
                         stopped.snapshot().end == MediaEnd::Stopped);
  RPCMP_CHECK(suite,
              stopped.enqueue({1, 3, 1'000'000, 1'000'001, 2, true}) == MediaAdmission::Closed);
  auto control = action(MediaControlAction::Stop);
  control.device_fault = true;
  check_sample(suite, stopped.advance({}, control), 0, 0, false);
  RPCMP_CHECK(suite, stopped.snapshot().failure == MediaFailure::DeviceFault &&
                         stopped.snapshot().end == MediaEnd::None);
  RPCMP_CHECK(suite, stopped.begin(2, 2, 1) ==
                         MediaAdmission::Accepted); // adapter has reset/quiesced old stream
  RPCMP_CHECK(suite, stopped.enqueue({1, 1, 0, 10, 9, true}) == MediaAdmission::StaleGeneration);
  RPCMP_CHECK(suite, stopped.enqueue({2, 1, 0, 10, 0, false}) == MediaAdmission::Accepted);
  check_sample(suite, stopped.advance({5, -5}), 5, -5);
  // A delayed control from the old stream cannot stop the newly reset stream.
  const auto old_stop = stopped.advance({5, -5}, action(MediaControlAction::Stop));
  check_sample(suite, old_stop, 5, -5);
  RPCMP_CHECK(suite, old_stop.control == MediaControlDisposition::StaleGeneration);
  RPCMP_CHECK(suite, stopped.snapshot().end == MediaEnd::None && stopped.snapshot().frame == 2);
  const auto old_policy = stopped.advance({5, -5}, policy(999, std::nullopt));
  RPCMP_CHECK(suite, old_policy.control == MediaControlDisposition::StaleGeneration &&
                         old_policy.consumed);
  RPCMP_CHECK(suite, stopped.snapshot().policy_revision == 1 && stopped.snapshot().target == 2U);
  auto future = action(MediaControlAction::Stop);
  future.play_generation = 3;
  const auto rejected = stopped.advance({5, -5}, future);
  check_sample(suite, rejected, 0, 0, false);
  RPCMP_CHECK(suite, rejected.control == MediaControlDisposition::Rejected &&
                         stopped.snapshot().failure == MediaFailure::Protocol);
  MediaLoopEnvelope priority;
  RPCMP_CHECK(suite, priority.begin(2, 2, 1) == MediaAdmission::Accepted);
  control.device_fault = true; // old generation, but a shared fault still wins
  check_sample(suite, priority.advance({5, -5}, control), 0, 0, false);
  RPCMP_CHECK(suite, priority.snapshot().failure == MediaFailure::DeviceFault);

  MediaLoopEnvelope revision;
  prepare_loop(suite, revision);
  check_sample(suite, revision.advance({5, -5}, policy(2, 3)), 5, -5);
  check_sample(suite, revision.advance({5, -5}, policy(1, 2)), 0, 0, false);
  RPCMP_CHECK(suite, revision.snapshot().failure == MediaFailure::Protocol &&
                         revision.snapshot().policy_revision == 2 &&
                         revision.snapshot().target == 3U);
}

void admission_and_limits(rpcmp::test::Suite& suite) {
  MediaLoopEnvelope envelope;
  RPCMP_CHECK(suite, envelope.begin(2, std::nullopt, 1) == MediaAdmission::Accepted);
  for (const auto invalid :
       std::array<MediaProgressInterval, 5>{MediaProgressInterval{2, 0, 0, 1, 0, false},
                                            {2, 2, 0, 1, 0, false},
                                            {2, 1, 1, 2, 0, false},
                                            {2, 1, 0, 0, 0, false},
                                            {3, 1, 0, 1, 0, false}})
    RPCMP_CHECK(suite, envelope.enqueue(invalid) == MediaAdmission::Invalid);
  for (std::uint64_t i = 0; i < 256; ++i)
    RPCMP_CHECK(suite,
                envelope.enqueue({2, i + 1, i, i + 1, i, false}) == MediaAdmission::Accepted);
  const MediaProgressInterval next{2, 257, 256, 257, 256, false};
  RPCMP_CHECK(suite, envelope.enqueue(next) == MediaAdmission::Full);
  check_sample(suite, envelope.advance({1, -1}), 1, -1);
  RPCMP_CHECK(suite, envelope.enqueue(next) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, envelope.begin(2, 2, 2) == MediaAdmission::StaleGeneration);
  RPCMP_CHECK(suite, envelope.begin(3, 0, 2) == MediaAdmission::Invalid);
  RPCMP_CHECK(suite, render_to(envelope, 257));
  check_sample(suite, envelope.advance({1, -1}), 0, 0, false);
  RPCMP_CHECK(suite, envelope.snapshot().failure == MediaFailure::Underrun &&
                         envelope.snapshot().frame == 257 &&
                         envelope.snapshot().end == MediaEnd::None &&
                         envelope.snapshot().completed_loops == 256);

  MediaLoopEnvelope regress;
  RPCMP_CHECK(suite, regress.begin(1, std::nullopt, 1) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, regress.enqueue({1, 1, 0, 2, 5, false}) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, regress.enqueue({1, 2, 2, 3, 4, false}) == MediaAdmission::Invalid);
  RPCMP_CHECK(suite, regress.enqueue({1, 2, 2, 3, 5, true}) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, regress.enqueue({1, 3, 3, 4, 5, true}) == MediaAdmission::Invalid);

  MediaLoopEnvelope limited(3);
  RPCMP_CHECK(suite, limited.begin(1, 2, 1) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, limited.enqueue({1, 1, 0, 10, 0, false}) == MediaAdmission::Accepted);
  RPCMP_CHECK(suite, render_to(limited, 3));
  check_sample(suite, limited.advance({1, -1}), 0, 0, false);
  RPCMP_CHECK(suite, limited.snapshot().failure == MediaFailure::ResourceExhausted &&
                         limited.snapshot().frame == 3);
  MediaLoopEnvelope zero(0);
  RPCMP_CHECK(suite, zero.begin(1, 2, 1) == MediaAdmission::Invalid);

  MediaLoopEnvelope maximum;
  const auto max = std::numeric_limits<std::uint64_t>::max();
  RPCMP_CHECK(suite, maximum.begin(max, std::numeric_limits<std::uint32_t>::max(), max) ==
                         MediaAdmission::Accepted);
  RPCMP_CHECK(suite, maximum.enqueue({max, 1, 0, 10, max, false}) == MediaAdmission::Accepted);
  check_sample(suite, maximum.advance({1, -1}), 1, -1);
  RPCMP_CHECK(suite, maximum.snapshot().completed_loops == max &&
                         maximum.snapshot().phase == MediaPhase::Fading);
  RPCMP_CHECK(suite, maximum.begin(0, 2, 1) == MediaAdmission::Invalid);

  for (const auto invalid : std::array<MediaRepeatUpdate, 4>{
           MediaRepeatUpdate{0, 2}, {1, 3}, {2, 0}, {0, std::nullopt}}) {
    MediaLoopEnvelope bad;
    prepare_loop(suite, bad);
    check_sample(suite, bad.advance({1, -1}, {false, MediaControlAction::Keep, invalid, 1}), 0, 0,
                 false);
    RPCMP_CHECK(suite, bad.snapshot().failure == MediaFailure::Protocol &&
                           bad.snapshot().frame == 0 && bad.snapshot().policy_revision == 1);
  }
  MediaLoopEnvelope tag;
  prepare_loop(suite, tag);
  auto invalid = action(MediaControlAction::Keep);
  const std::uint8_t raw = 255;
  std::memcpy(&invalid.action, &raw, sizeof(raw));
  check_sample(suite, tag.advance({1, -1}, invalid), 0, 0, false);
  RPCMP_CHECK(suite, tag.snapshot().failure == MediaFailure::Protocol);
}

auto rendering_trace(rpcmp::test::Suite& suite, bool pause_and_read) {
  MediaLoopEnvelope envelope;
  prepare_loop(suite, envelope);
  std::array<std::pair<std::int16_t, std::int16_t>, 1000> trace;
  for (unsigned frame = 0; frame < trace.size(); ++frame) {
    const auto value = static_cast<std::int16_t>(static_cast<int>(frame) - 500);
    const StereoFrame source{value, static_cast<std::int16_t>(-value)};
    MediaBoundaryControl control;
    if (pause_and_read && (frame == 23 || frame == 300)) {
      const auto before = envelope.snapshot();
      check_sample(suite, envelope.advance(source, action(MediaControlAction::Pause)), 0, 0, false);
      for (unsigned idle = 0; idle < 100; ++idle)
        check_sample(suite, envelope.advance(source), 0, 0, false);
      RPCMP_CHECK(suite, envelope.snapshot().frame == before.frame &&
                             envelope.snapshot().gain == before.gain);
      control = action(MediaControlAction::Resume);
    }
    const auto result = envelope.advance(source, control);
    RPCMP_CHECK(suite, result.consumed);
    trace[frame] = {result.output.left, result.output.right};
    for (unsigned read = 0; read < (pause_and_read ? 7U : 0U); ++read)
      static_cast<void>(envelope.snapshot());
  }
  return std::make_tuple(trace, envelope.snapshot().frame, envelope.snapshot().gain,
                         envelope.snapshot().completed_loops);
}

void mapped_mdx(rpcmp::test::Suite& suite) {
  namespace mdx = rpcmp::runtime::mdx;
  constexpr std::array<std::uint8_t, 2> end{0xf1, 0};
  constexpr std::array<std::uint8_t, 4> loop{0, 0xf1, 0xff, 0xfc};
  mdx::MdxDocument document;
  for (std::size_t i = 0; i < document.tracks.size(); ++i)
    document.tracks[i] = {static_cast<std::uint8_t>(i),
                          i < 8 ? mdx::TrackTarget::Ym2151 : mdx::TrackTarget::LegacyAdpcm,
                          i * 100,
                          {end.data(), end.size()}};
  document.tracks[0].source = {loop.data(), loop.size()};
  static mdx::MdxEngineScratch scratch;
  mdx::MdxEngineState engine;
  mdx::TimedYm2151Batch batch;
  MediaLoopEnvelope envelope;
  RPCMP_CHECK(suite, envelope.begin(1, 2, 1) == MediaAdmission::Accepted);
  // Authored host mapping: synthetic samples have zero pipeline delay and use
  // the engine's 48 kHz clock. This is not a Pocket latency assumption.
  for (std::uint64_t tick = 0; tick < 3; ++tick) {
    RPCMP_CHECK(suite, mdx::advance_mdx_tick(document, 48'000, engine, batch, scratch).ok());
    RPCMP_CHECK(suite,
                envelope.enqueue({1, tick + 1, engine.progress.at_tick,
                                  engine.timeline.scheduler_tick, engine.progress.completed_loops,
                                  engine.progress.ended}) == MediaAdmission::Accepted);
  }
  RPCMP_CHECK(suite, render_to(envelope, 1376));
  check_sample(suite, envelope.advance({4000, -4000}), 4000, -4000);
  RPCMP_CHECK(suite, envelope.snapshot().phase == MediaPhase::Fading &&
                         envelope.snapshot().completed_loops == 2);
  RPCMP_CHECK(suite, render_to(envelope, 1500));
  check_sample(suite, envelope.advance({4000, -4000}), 3997, -3997);
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  exact_boundaries(suite);
  policy_races(suite);
  pause_and_restoration(suite);
  finite_end_and_fault(suite);
  admission_and_limits(suite);
  RPCMP_CHECK(suite, rendering_trace(suite, false) == rendering_trace(suite, true));
  mapped_mdx(suite);
  return suite.finish("Media loop envelope");
}
