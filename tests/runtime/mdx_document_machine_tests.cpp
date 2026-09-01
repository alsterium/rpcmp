#include "rpcmp/runtime/mdx_document_machine.hpp"
#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using rpcmp::runtime::mdx::ByteView;
using rpcmp::runtime::mdx::DecodeError;
using rpcmp::runtime::mdx::DocumentPlaybackState;
using rpcmp::runtime::mdx::DocumentTickBatch;
using rpcmp::runtime::mdx::DocumentTickScratch;
using rpcmp::runtime::mdx::InstructionKind;
using rpcmp::runtime::mdx::MdxDocument;
using rpcmp::runtime::mdx::PlaybackLimits;
using rpcmp::runtime::mdx::TrackTarget;

MdxDocument document_with(const std::array<ByteView, 9>& tracks) {
  MdxDocument document{};
  for (std::size_t index = 0; index < tracks.size(); ++index) {
    document.tracks[index] = {static_cast<std::uint8_t>(index),
                              index < 8 ? TrackTarget::Ym2151 : TrackTarget::LegacyAdpcm,
                              index * 100, tracks[index]};
  }
  return document;
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  const std::array<std::uint8_t, 4> timer_rest{0xff, 0xc8, 0x00, 0x00};
  const std::array<std::uint8_t, 2> rest_end{0x00, 0x00};
  const std::array<std::uint8_t, 2> end{0xf1, 0x00};
  std::array<ByteView, 9> tracks{};
  tracks.fill({end.data(), end.size()});
  tracks[0] = {timer_rest.data(), timer_rest.size()};
  tracks[1] = {rest_end.data(), rest_end.size()};
  const auto ordered_document = document_with(tracks);

  DocumentPlaybackState state{};
  DocumentTickBatch batch{};
  static DocumentTickScratch scratch{};
  RPCMP_CHECK(
      suite,
      rpcmp::runtime::mdx::advance_document_tick(ordered_document, state, batch, scratch).ok());
  RPCMP_CHECK(suite, batch.count == 9);
  RPCMP_CHECK(suite, batch.actions[0].logical_channel == 0);
  RPCMP_CHECK(suite, batch.actions[0].instruction.kind == InstructionKind::TimerB);
  RPCMP_CHECK(suite, batch.actions[1].logical_channel == 0);
  RPCMP_CHECK(suite, batch.actions[1].instruction.kind == InstructionKind::Rest);
  RPCMP_CHECK(suite, batch.actions[2].logical_channel == 1);
  RPCMP_CHECK(suite, batch.actions[2].instruction.kind == InstructionKind::Rest);
  RPCMP_CHECK(suite, batch.actions[8].logical_channel == 7);

  const std::array<std::uint8_t, 4> release_then_end{0xef, 0x01, 0xf1, 0x00};
  const std::array<std::uint8_t, 3> wait_then_end{0xee, 0xf1, 0x00};
  tracks.fill({end.data(), end.size()});
  tracks[0] = {release_then_end.data(), release_then_end.size()};
  tracks[1] = {wait_then_end.data(), wait_then_end.size()};
  const auto release_before_wait = document_with(tracks);
  state = {};
  RPCMP_CHECK(
      suite,
      rpcmp::runtime::mdx::advance_document_tick(release_before_wait, state, batch, scratch).ok());
  RPCMP_CHECK(suite, state.tracks[1].waiting);

  const std::array<std::uint8_t, 3> wait{0xee, 0xf1, 0x00};
  const std::array<std::uint8_t, 4> release_later{0xef, 0x00, 0xf1, 0x00};
  tracks.fill({end.data(), end.size()});
  tracks[0] = {wait.data(), wait.size()};
  tracks[1] = {release_later.data(), release_later.size()};
  const auto release_after_wait = document_with(tracks);
  state = {};
  RPCMP_CHECK(
      suite,
      rpcmp::runtime::mdx::advance_document_tick(release_after_wait, state, batch, scratch).ok());
  RPCMP_CHECK(suite, !state.tracks[0].waiting);
  RPCMP_CHECK(suite, state.tracks[0].cursor == 1);
  RPCMP_CHECK(
      suite,
      rpcmp::runtime::mdx::advance_document_tick(release_after_wait, state, batch, scratch).ok());
  RPCMP_CHECK(suite, state.tracks[0].ended);

  const std::array<std::uint8_t, 3> instant_loop{0xf1, 0xff, 0xfd};
  tracks.fill({end.data(), end.size()});
  tracks[7] = {instant_loop.data(), instant_loop.size()};
  const auto failing_document = document_with(tracks);
  state = {};
  batch.count = 1;
  batch.actions[0].logical_channel = 99;
  PlaybackLimits limits{};
  limits.max_instructions_per_tick = 4;
  limits.max_branches_per_tick = 4;
  const auto exhausted =
      rpcmp::runtime::mdx::advance_document_tick(failing_document, state, batch, scratch, limits);
  RPCMP_CHECK(suite, exhausted.error == DecodeError::BudgetExhausted);
  RPCMP_CHECK(suite, state.tracks[0].cursor == 0);
  RPCMP_CHECK(suite, state.tracks[7].cursor == 0);
  RPCMP_CHECK(suite, batch.count == 1);
  RPCMP_CHECK(suite, batch.actions[0].logical_channel == 99);

  return suite.finish("mdx document playback state machine");
}
