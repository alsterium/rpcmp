#include "rpcmp/player/mdx_performance_mapper.hpp"
#include "rpcmp/player/snapshot_publisher.hpp"
#include "rpcmp/runtime/mdx_engine.hpp"
#include "test_support.hpp"

#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
namespace mdx = rpcmp::runtime::mdx;
namespace api = rpcmp::contracts::v2;
using namespace rpcmp::player;

template <typename T> const T& required(const std::optional<T>& value) {
  if (!value)
    throw std::logic_error("missing authored observation");
  return *value;
}
mdx::SemanticAction action(std::uint8_t channel, mdx::InstructionKind kind, std::uint8_t value = 0,
                           std::uint8_t second = 0, std::uint16_t duration = 1) {
  return {channel, {kind, 0, duration, 0, value, second}};
}
mdx::SemanticAction raw(std::uint8_t address, std::uint8_t value) {
  return action(0, mdx::InstructionKind::DirectWrite, address, value);
}
void voice(mdx::MdxDocument& document, std::uint8_t index) {
  auto& result = document.voices[index];
  result.present = true;
  result.slot_mask = 0x0f;
  result.feedback_connection = 0;
  for (std::size_t group = 0; group < result.operators.size(); ++group)
    for (std::size_t op = 0; op < result.operators[group].size(); ++op)
      result.operators[group][op] = static_cast<std::uint8_t>(group * 4 + op + 1);
}
struct Router {
  Router() { voice(document, 3); }
  mdx::DecodeResult route(const mdx::DocumentTickBatch& actions, bool capture = true) {
    static mdx::Ym2151RouterScratch scratch;
    return mdx::route_ym2151_batch(document, actions, state, writes, scratch,
                                   capture ? &observed : nullptr);
  }
  mdx::DecodeResult tick(std::initializer_list<mdx::SemanticAction> actions) {
    mdx::DocumentTickBatch batch;
    for (const auto& item : actions)
      batch.actions[batch.count++] = item;
    return route(batch);
  }
  mdx::MdxDocument document{};
  mdx::Ym2151RouterState state{};
  mdx::Ym2151WriteBatch writes{};
  mdx::Ym2151PerformanceBatch observed{};
};

void notes_and_lifecycle(rpcmp::test::Suite& suite) {
  using K = mdx::InstructionKind;
  using E = mdx::Ym2151PerformanceKind;
  Router router;
  RPCMP_CHECK(suite, router.tick({action(0, K::SelectVoice, 3), action(0, K::Note, 0xad)}).ok());
  RPCMP_CHECK(suite, router.writes.count == 29 && router.observed.count == 3);
  RPCMP_CHECK(suite, router.observed.events[0].kind == E::InstrumentChanged &&
                         router.observed.events[0].after_write == 25 &&
                         router.observed.events[0].state.voice == 3);
  RPCMP_CHECK(suite, router.observed.events[1].kind == E::PitchChanged &&
                         router.observed.events[1].after_write == 27 &&
                         router.observed.events[1].state.pitch_64 == 45 * 64 + 5);
  RPCMP_CHECK(suite, router.observed.events[2].kind == E::KeyOn &&
                         router.observed.events[2].after_write == 29 &&
                         router.observed.channels[0].key_on);
  RPCMP_CHECK(suite, router.tick({action(0, K::Note, 0xad)}).ok());
  RPCMP_CHECK(suite, router.writes.count == 6 && router.observed.count == 2 &&
                         router.observed.events[0].kind == E::KeyOff &&
                         router.observed.events[0].after_write == 1 &&
                         router.observed.events[1].kind == E::KeyOn &&
                         router.observed.events[1].after_write == 6);
  RPCMP_CHECK(suite, router.tick({}).ok());
  RPCMP_CHECK(suite, router.observed.count == 1 && router.observed.events[0].kind == E::KeyOff);

  Router tied;
  RPCMP_CHECK(suite, tied.tick({action(0, K::SelectVoice, 3), action(0, K::SuppressKeyOff),
                                action(0, K::Note, 0xad)})
                         .ok());
  RPCMP_CHECK(suite, tied.tick({action(0, K::Note, 0xae)}).ok());
  RPCMP_CHECK(suite, tied.writes.count == 4 && tied.observed.count == 1 &&
                         tied.observed.events[0].kind == E::PitchChanged &&
                         tied.observed.events[0].state.key_on &&
                         tied.observed.events[0].state.pitch_64 == 46 * 64 + 5);

  Router delayed;
  RPCMP_CHECK(suite, delayed
                         .tick({action(0, K::SelectVoice, 3), action(0, K::KeyOnDelay, 2),
                                action(0, K::Note, 0xad, 0, 4)})
                         .ok());
  RPCMP_CHECK(suite, delayed.observed.count == 0 && !delayed.observed.channels[0].key_on &&
                         !delayed.observed.channels[0].voice);
  RPCMP_CHECK(suite, delayed.tick({}).ok() && delayed.observed.count == 0);
  RPCMP_CHECK(suite, delayed.tick({}).ok() && delayed.observed.count == 3 &&
                         delayed.observed.events[2].kind == E::KeyOn);

  Router sliding;
  auto portamento = action(0, K::Portamento);
  portamento.instruction.signed_value = 1;
  RPCMP_CHECK(
      suite,
      sliding.tick({action(0, K::SelectVoice, 3), portamento, action(0, K::Note, 0xad, 0, 4)})
          .ok());
  RPCMP_CHECK(suite, sliding.tick({}).ok());
  RPCMP_CHECK(suite, sliding.writes.count == 2 && sliding.observed.count == 1 &&
                         sliding.observed.events[0].kind == E::PitchChanged &&
                         sliding.observed.events[0].state.pitch_64 == 45 * 64 + 6);

  Router silent;
  silent.document.voices[3].slot_mask = 0;
  RPCMP_CHECK(suite, silent.tick({action(0, K::SelectVoice, 3), action(0, K::Note, 0xad)}).ok());
  RPCMP_CHECK(suite, silent.state.channels[0].key_on && !silent.observed.channels[0].key_on &&
                         silent.observed.count == 2);
}

void direct_writes(rpcmp::test::Suite& suite) {
  using E = mdx::Ym2151PerformanceKind;
  using K = mdx::InstructionKind;
  Router router;
  RPCMP_CHECK(suite, router
                         .tick({raw(0x29, 0x3e), raw(0x31, 0), raw(0x08, 0x09), raw(0x08, 0x09),
                                raw(0x08, 0x19), raw(0x08, 0x09), raw(0x08, 0x01), raw(0x08, 0x09)})
                         .ok());
  RPCMP_CHECK(suite, router.writes.count == 8 && router.observed.count == 5 &&
                         router.observed.channels[1].pitch_64 == 47 * 64);
  const std::array<std::uint16_t, 5> ordinals{2, 3, 5, 7, 8};
  const std::array kinds{E::PitchChanged, E::KeyOn, E::KeyOn, E::KeyOff, E::KeyOn};
  for (std::size_t i = 0; i < ordinals.size(); ++i)
    RPCMP_CHECK(suite, router.observed.events[i].channel == 1 &&
                           router.observed.events[i].after_write == ordinals[i] &&
                           router.observed.events[i].kind == kinds[i]);
  for (std::size_t i = 0; i < router.writes.count; ++i)
    RPCMP_CHECK(suite, router.writes.writes[i].logical_channel == 0);
  RPCMP_CHECK(suite, router.tick({raw(0x29, 0x03)}).ok());
  RPCMP_CHECK(suite, router.observed.count == 1 && !router.observed.channels[1].pitch_64);

  Router partial;
  RPCMP_CHECK(suite, partial.tick({raw(0x28, 0x3e)}).ok());
  RPCMP_CHECK(suite, partial.observed.count == 0 && !partial.observed.channels[0].pitch_64);
  RPCMP_CHECK(suite, partial.tick({raw(0x30, 0)}).ok());
  RPCMP_CHECK(suite,
              partial.observed.count == 1 && partial.observed.channels[0].pitch_64 == 47 * 64);

  Router instrument;
  RPCMP_CHECK(suite, instrument
                         .tick({action(0, K::SelectVoice, 3), action(0, K::SuppressKeyOff),
                                action(0, K::Note, 0xad)})
                         .ok());
  RPCMP_CHECK(suite, instrument.tick({raw(0x40, 1), raw(0x40, 2)}).ok());
  RPCMP_CHECK(suite, instrument.observed.count == 1 &&
                         instrument.observed.events[0].kind == E::InstrumentChanged &&
                         !instrument.observed.channels[0].voice);
  RPCMP_CHECK(suite, instrument.tick({action(0, K::Note, 0xad)}).ok());
  RPCMP_CHECK(suite, !instrument.observed.channels[0].voice);
  voice(instrument.document, 4);
  RPCMP_CHECK(suite,
              instrument.tick({action(0, K::SelectVoice, 4), action(0, K::Note, 0xad)}).ok());
  RPCMP_CHECK(suite, instrument.observed.channels[0].voice == 4);
  RPCMP_CHECK(suite, instrument.tick({raw(0x20, 0x40)}).ok());
  RPCMP_CHECK(suite, instrument.observed.channels[0].voice == 4);
  RPCMP_CHECK(suite, instrument.tick({raw(0x20, 0x41)}).ok());
  RPCMP_CHECK(suite, !instrument.observed.channels[0].voice);

  Router noise;
  RPCMP_CHECK(suite,
              noise.tick({raw(0x2f, 0x3e), raw(0x37, 0), raw(0x08, 0x7f), raw(0x0f, 0x80)}).ok());
  RPCMP_CHECK(suite, noise.observed.channels[7].key_on && !noise.observed.channels[7].pitch_64 &&
                         noise.observed.events[2].kind == E::PitchChanged);
  RPCMP_CHECK(suite, noise.tick({raw(0x0f, 0)}).ok());
  RPCMP_CHECK(suite, noise.observed.channels[7].pitch_64 == 47 * 64);
  RPCMP_CHECK(suite, noise.tick({raw(0x14, 0x80), raw(0x08, 0x7b)}).ok());
  RPCMP_CHECK(suite, noise.observed.capture_lost && noise.observed.count == 0);
  for (const auto& channel : noise.observed.channels)
    RPCMP_CHECK(suite, !channel.gate_known);
  RPCMP_CHECK(suite, noise.tick({}).ok() && noise.observed.capture_lost);
  RPCMP_CHECK(suite, noise.tick({raw(0x14, 0), raw(0x08, 0x7b)}).ok());
  RPCMP_CHECK(suite, !noise.observed.channels[3].gate_known && noise.observed.count == 0);
  RPCMP_CHECK(suite, noise.tick({}).ok() && noise.observed.capture_lost);
  RPCMP_CHECK(suite, noise.tick({raw(0x08, 3), raw(0x08, 0x7b)}).ok());
  RPCMP_CHECK(suite, noise.observed.channels[3].gate_known && noise.observed.channels[3].key_on &&
                         noise.observed.count == 2 && noise.observed.events[0].kind == E::KeyOff &&
                         noise.observed.events[1].kind == E::KeyOn);
}

void overflow_and_rollback(rpcmp::test::Suite& suite) {
  Router observed;
  Router discarded;
  mdx::DocumentTickBatch actions;
  for (std::size_t i = 0; i < 300; ++i)
    actions.actions[actions.count++] = raw(0x08, i % 2 == 0 ? 8 : 0);
  RPCMP_CHECK(suite, observed.route(actions).ok() && discarded.route(actions, false).ok());
  RPCMP_CHECK(suite, observed.writes.count == 300 && observed.observed.count == 256 &&
                         observed.observed.lost_before == 44 && !observed.observed.capture_lost &&
                         observed.observed.events[0].after_write == 45 &&
                         observed.observed.events[255].after_write == 300);
  RPCMP_CHECK(suite, observed.writes.count == discarded.writes.count);
  PerformanceHistory history;
  MdxPerformanceMapper mapper(history, 3'579'545);
  RPCMP_CHECK(suite, history.begin(1));
  RPCMP_CHECK(suite, mapper.commit(1, {0, observed.observed}, {0, 100, 100}) ==
                         MdxPerformanceResult::Applied);
  api::PerformanceHistorySnapshot retained;
  history.copy_to(retained);
  RPCMP_CHECK(suite, api::valid_performance_history(retained) && retained.count == 256 &&
                         retained.events[0].sequence == 45 && retained.next_sequence == 301 &&
                         retained.capture_lost && !retained.retention_lost &&
                         retained.channels[0].key_on == false);
  for (std::size_t i = 0; i < observed.writes.count; ++i) {
    const auto& a = observed.writes.writes[i];
    const auto& b = discarded.writes.writes[i];
    RPCMP_CHECK(suite, a.address == 8 && a.value == (i % 2 == 0 ? 8 : 0) &&
                           a.address == b.address && a.value == b.value &&
                           a.logical_channel == b.logical_channel);
  }
  const auto before = observed.state.observation;
  const auto failed = observed.tick({raw(0x08, 8), action(1, mdx::InstructionKind::SelectVoice, 99),
                                     action(1, mdx::InstructionKind::Note, 0x80)});
  RPCMP_CHECK(suite, failed.error == mdx::DecodeError::MissingVoice &&
                         observed.observed.count == 256 && observed.writes.count == 300 &&
                         observed.state.observation.registers == before.registers &&
                         observed.state.observation.known == before.known &&
                         observed.state.observation.key_masks == before.key_masks);
}

api::PerformanceHistorySnapshot snapshot(const PerformanceHistory& history) {
  api::PerformanceHistorySnapshot result;
  history.copy_to(result);
  return result;
}

void pitch_and_mapping_validation(rpcmp::test::Suite& suite) {
  mdx::SequencedPerformance source;
  source.at_tick = 10;
  source.batch.channels[0].pitch_64 = std::uint16_t{47 * 64};
  source.batch.channels[1].pitch_64 = std::uint16_t{56 * 64};
  source.batch.channels[2].pitch_64 = std::uint16_t{45 * 64 + 5};
  source.batch.channels[4].pitch_64 = std::uint16_t{32};
  source.batch.channels[5].pitch_64 = std::uint16_t{63};
  for (const std::uint32_t clock : {3'579'545U, 4'000'000U}) {
    PerformanceHistory history;
    MdxPerformanceMapper mapper(history, clock);
    RPCMP_CHECK(suite, history.begin(7));
    RPCMP_CHECK(suite, mapper.commit(7, source, {10, 100, 99}) == MdxPerformanceResult::Future);
    RPCMP_CHECK(suite, snapshot(history).next_sequence == 1 && !snapshot(history).capture_lost &&
                           snapshot(history).observed_through_frame == 0);
    RPCMP_CHECK(suite, mapper.commit(7, source, {10, 100, 100}) == MdxPerformanceResult::Applied);
    const auto value = snapshot(history);
    RPCMP_CHECK(suite, api::valid_performance_history(value));
    if (clock == 3'579'545) {
      RPCMP_CHECK(suite,
                  value.channels[0].note == 60 && value.channels[0].fine_pitch_cents == 0 &&
                      value.channels[1].note == 69 && value.channels[1].fine_pitch_cents == 0 &&
                      value.channels[2].note == 58 && value.channels[2].fine_pitch_cents == 8);
      RPCMP_CHECK(suite,
                  value.channels[4].note == 14 && value.channels[4].fine_pitch_cents == -50 &&
                      value.channels[5].note == 14 && value.channels[5].fine_pitch_cents == -2);
    } else {
      RPCMP_CHECK(suite, value.channels[0].note == 62 && value.channels[0].fine_pitch_cents == -8 &&
                             value.channels[2].note == 60 &&
                             value.channels[2].fine_pitch_cents == 0);
    }
    RPCMP_CHECK(suite, !value.channels[3].note && !value.channels[3].fine_pitch_cents &&
                           !value.channels[3].mdx_fm);
    RPCMP_CHECK(suite, mapper.commit(6, source, {10, 100, 100}) == MdxPerformanceResult::Stale);
  }
  PerformanceHistory history;
  RPCMP_CHECK(suite, history.begin(7));
  MdxPerformanceMapper mapper(history, 3'579'545);
  MdxPerformanceMapper unsupported(history, 0);
  RPCMP_CHECK(suite, unsupported.commit(7, source, {10, 0, 0}) == MdxPerformanceResult::Invalid);
  RPCMP_CHECK(suite, mapper.commit(0, source, {10, 0, 0}) == MdxPerformanceResult::Invalid);
  RPCMP_CHECK(suite, mapper.commit(8, source, {10, 0, 0}) == MdxPerformanceResult::Invalid);
  RPCMP_CHECK(suite, mapper.commit(7, source, {11, 0, 0}) == MdxPerformanceResult::Invalid);
  source.batch.write_count = source.batch.count = 2;
  source.batch.events[0] = {1,
                            0,
                            mdx::Ym2151PerformanceKind::KeyOn,
                            {true, true, std::uint16_t{47 * 64}, std::uint8_t{3}}};
  source.batch.events[1] = {2,
                            0,
                            mdx::Ym2151PerformanceKind::KeyOff,
                            {true, false, std::uint16_t{47 * 64}, std::uint8_t{3}}};
  const auto reject = [&](auto mutate) {
    auto invalid = source;
    mutate(invalid.batch);
    RPCMP_CHECK(suite, mapper.commit(7, invalid, {10, 0, 0}) == MdxPerformanceResult::Invalid);
    RPCMP_CHECK(suite, snapshot(history).count == 0 && snapshot(history).next_sequence == 1);
  };
  reject([](auto& b) { b.count = 257; });
  reject([](auto& b) { b.write_count = 8193; });
  reject([](auto& b) { b.lost_before = 8193; });
  reject([](auto& b) { b.channels[0].pitch_64 = std::uint16_t{0x1800}; });
  reject([](auto& b) { b.events[0].channel = 8; });
  reject([](auto& b) { b.events[0].after_write = 0; });
  reject([](auto& b) { b.events[0].after_write = 3; });
  reject([](auto& b) {
    b.events[0].after_write = 2;
    b.events[1].after_write = 1;
  });
  reject([](auto& b) { b.events[0].state.gate_known = false; });
  reject([](auto& b) { b.events[0].state.key_on = false; });
  reject([](auto& b) { b.events[1].state.key_on = true; });
  reject([](auto& b) {
    const std::uint8_t invalid = 255;
    static_assert(sizeof(b.events[0].kind) == sizeof(invalid));
    std::memcpy(&b.events[0].kind, &invalid, sizeof(invalid));
  });
  RPCMP_CHECK(suite, mapper.commit(7, source, {10, 0, 0}) == MdxPerformanceResult::Applied);
  RPCMP_CHECK(suite,
              snapshot(history).count == 2 &&
                  required(snapshot(history).events[0].change.channel.mdx_fm).voice_number == 3);
  RPCMP_CHECK(suite, history.commit({7,
                                     0,
                                     api::unknown_performance_channels(),
                                     {},
                                     std::numeric_limits<std::uint64_t>::max()}) ==
                         PerformanceCaptureResult::Exhausted);
  RPCMP_CHECK(suite, mapper.commit(7, source, {10, 0, 0}) == MdxPerformanceResult::Exhausted);
}

void individual_commit_frames(rpcmp::test::Suite& suite) {
  mdx::SequencedPerformance source;
  source.at_tick = 688;
  source.batch.count = source.batch.write_count = 3;
  for (std::uint16_t i = 0; i < 3; ++i) {
    const bool on = i != 1;
    source.batch.events[i] = {static_cast<std::uint16_t>(i + 1),
                              0,
                              on ? mdx::Ym2151PerformanceKind::KeyOn
                                 : mdx::Ym2151PerformanceKind::KeyOff,
                              {true, on, std::uint16_t{47 * 64}, std::uint8_t{3}}};
  }
  source.batch.channels[0] = source.batch.events[2].state;
  MdxPerformanceBoundary boundary{688, 110, 109};
  auto& frames = boundary.event_frames.emplace();
  frames.count = 3;
  frames.frames[0] = 100;
  frames.frames[1] = 103;
  frames.frames[2] = 110;
  PerformanceHistory history;
  MdxPerformanceMapper mapper(history, 3'579'545);
  RPCMP_CHECK(suite, history.begin(9));
  RPCMP_CHECK(suite, mapper.commit(9, source, boundary) == MdxPerformanceResult::Future);
  RPCMP_CHECK(suite, snapshot(history).count == 0 && !snapshot(history).capture_lost);
  boundary.through_frame = 110;
  RPCMP_CHECK(suite, mapper.commit(9, source, boundary) == MdxPerformanceResult::Applied);
  const auto result = snapshot(history);
  RPCMP_CHECK(suite, result.count == 3 && result.channels[0].key_on == true);
  RPCMP_CHECK(suite, result.events[0].change.at_frame == 100);
  RPCMP_CHECK(suite, result.events[1].change.at_frame == 103);
  RPCMP_CHECK(suite, result.events[2].change.at_frame == 110);
  const auto reject = [&](auto mutate) {
    auto invalid = boundary;
    if (!invalid.event_frames)
      throw std::logic_error("missing authored frame mapping");
    mutate(*invalid.event_frames);
    RPCMP_CHECK(suite, mapper.commit(9, source, invalid) == MdxPerformanceResult::Invalid);
    const auto after = snapshot(history);
    RPCMP_CHECK(suite, after.count == 3 && after.next_sequence == 4 && !after.capture_lost);
  };
  reject([](auto& f) { f.count = 2; });
  reject([](auto& f) { f.count = 257; });
  reject([](auto& f) { f.frames[1] = 99; });
  reject([](auto& f) { f.frames[2] = 111; });
  source.batch.events[1].after_write = 1;
  RPCMP_CHECK(suite, mapper.commit(9, source, boundary) == MdxPerformanceResult::Invalid);
  RPCMP_CHECK(suite, !snapshot(history).capture_lost);
}

std::vector<std::uint8_t> catalog_fixture() {
  std::ifstream input(std::string{RPCMP_SOURCE_DIR} + "/tests/fixtures/rpcmlib/album.rpcmlib.hex");
  std::vector<std::uint8_t> bytes;
  std::string pair;
  char c{};
  while (input.get(c)) {
    if (c == '\r' || c == '\n')
      continue;
    pair += c;
    if (pair.size() == 2) {
      bytes.push_back(static_cast<std::uint8_t>(std::stoul(pair, nullptr, 16)));
      pair.clear();
    }
  }
  return bytes;
}
void engine_to_publication(rpcmp::test::Suite& suite) {
  constexpr std::array<std::uint8_t, 2> end{0xf1, 0};
  constexpr std::array<std::uint8_t, 8> music{0xfd, 3, 0xad, 0, 0xad, 0, 0xf1, 0};
  mdx::MdxDocument document;
  voice(document, 3);
  for (std::size_t i = 0; i < document.tracks.size(); ++i)
    document.tracks[i] = {static_cast<std::uint8_t>(i),
                          i < 8 ? mdx::TrackTarget::Ym2151 : mdx::TrackTarget::LegacyAdpcm,
                          0,
                          {end.data(), end.size()}};
  document.tracks[0].source = {music.data(), music.size()};
  static mdx::MdxEngineScratch scratch;
  mdx::DocumentValidation validation;
  RPCMP_CHECK(suite, mdx::prepare_mdx_playback(document, validation, scratch).ok());
  mdx::MdxEngineState rejected_engine;
  rejected_engine.timeline.scheduler_tick = std::numeric_limits<std::uint64_t>::max();
  mdx::TimedYm2151Batch rejected_writes;
  rejected_writes.count = 1;
  rejected_writes.writes[0] = {99, {0x1b, 2, 0}};
  RPCMP_CHECK(
      suite,
      mdx::advance_mdx_tick(document, 48'000, rejected_engine, rejected_writes, scratch).error ==
          mdx::DecodeError::ArithmeticOverflow);
  RPCMP_CHECK(suite, rejected_engine.performance.batch.count == 0 &&
                         !rejected_engine.ym2151.observation.channels[0].voice &&
                         !rejected_engine.ym2151.observation.channels[0].pitch_64 &&
                         rejected_engine.ym2151.observation.key_masks[0] == 0 &&
                         rejected_writes.count == 1 && rejected_writes.writes[0].at_tick == 99);
  mdx::MdxEngineState engine;
  mdx::TimedYm2151Batch writes;
  PerformanceHistory history;
  MdxPerformanceMapper mapper(history, 3'579'545);
  RPCMP_CHECK(suite, history.begin(7));
  CatalogSession catalog;
  const auto bytes = catalog_fixture();
  const auto ticket = catalog.begin_open();
  RPCMP_CHECK(suite,
              ticket.ok() &&
                  catalog.complete_open(ticket.generation, {bytes.data(), bytes.size()}).ok());
  SnapshotPublisher publisher(catalog, 0, &history);
  TransportSnapshot transport;
  transport.catalog = catalog.status();
  transport.selection = PlaybackSelection{ticket.generation, {0x47305869eca5f89aULL}};
  transport.transport = transport.projected = rpcmp::contracts::TransportState::Playing;
  transport.play_generation = 7;
  transport.prepared = true;
  for (unsigned tick = 0; tick < 3; ++tick) {
    RPCMP_CHECK(suite, mdx::advance_mdx_tick(document, 48'000, engine, writes, scratch).ok());
    const std::uint64_t at_tick = tick * std::uint64_t{688};
    RPCMP_CHECK(suite, engine.performance.at_tick == at_tick && engine.progress.at_tick == at_tick);
    for (std::size_t i = 0; i < writes.count; ++i)
      RPCMP_CHECK(suite, writes.writes[i].at_tick == at_tick);
    const auto frame = 100 + at_tick;
    RPCMP_CHECK(suite, mapper.commit(7, engine.performance, {at_tick, frame, frame - 1}) ==
                           MdxPerformanceResult::Future);
    RPCMP_CHECK(suite, mapper.commit(7, engine.performance, {at_tick, frame, frame}) ==
                           MdxPerformanceResult::Applied);
    transport.media_frame = frame;
    RPCMP_CHECK(suite, publisher.publish(tick, transport) == PublicationResult::Ok);
    const auto published = publisher.latest();
    RPCMP_CHECK(suite, api::valid_player_snapshot(published));
    const auto& seen = required(published.performance_history);
    RPCMP_CHECK(suite, seen.observed_through_frame == frame && seen.channels[0].note == 58 &&
                           seen.channels[0].key_on == (tick < 2));
    const std::array<std::uint16_t, 3> expected_counts{3, 5, 6};
    RPCMP_CHECK(suite, seen.count == expected_counts[tick]);
  }
  const auto published = publisher.latest();
  const auto before = engine.performance;
  const auto key_masks = engine.ym2151.observation.key_masks;
  RPCMP_CHECK(suite, mdx::advance_mdx_tick(document, 0, engine, writes, scratch).error ==
                         mdx::DecodeError::InvalidLimits);
  RPCMP_CHECK(suite, engine.performance.at_tick == before.at_tick &&
                         engine.performance.batch.count == before.batch.count &&
                         engine.ym2151.observation.key_masks == key_masks);
  RPCMP_CHECK(suite, history.begin(8));
  RPCMP_CHECK(suite, mapper.commit(7, engine.performance, {before.at_tick, 2000, 2000}) ==
                         MdxPerformanceResult::Stale);
  RPCMP_CHECK(suite,
              snapshot(history).count == 0 && required(published.performance_history).count == 6);
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  notes_and_lifecycle(suite);
  direct_writes(suite);
  overflow_and_rollback(suite);
  pitch_and_mapping_validation(suite);
  individual_commit_frames(suite);
  engine_to_publication(suite);
  return suite.finish("MDX performance observations and commit mapping");
}
