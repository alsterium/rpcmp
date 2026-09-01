#include "rpcmp/runtime/mdx_document_machine.hpp"

namespace rpcmp::runtime::mdx {
namespace {

DecodeResult failure(const DecodeError error, const std::size_t offset,
                     const std::uint8_t track) noexcept {
  return {error, offset, track};
}

} // namespace

DecodeResult advance_document_tick(const MdxDocument& document, DocumentPlaybackState& state,
                                   DocumentTickBatch& batch, DocumentTickScratch& scratch,
                                   const PlaybackLimits limits) noexcept {
  scratch.candidate_state = state;
  scratch.pending_batch.count = 0;

  for (std::size_t index = 0; index < kMdxFmTrackCount; ++index) {
    const TrackView& track = document.tracks[index];
    if (track.logical_channel != index || track.target != TrackTarget::Ym2151) {
      return failure(DecodeError::RangeOutsideInput, track.source_offset, track.logical_channel);
    }

    const auto result = advance_track_tick(track, scratch.candidate_state.tracks[index],
                                           scratch.track_trace, scratch.track_scratch, limits);
    if (!result.ok()) {
      return result;
    }
    if (scratch.track_trace.count > kMdxMaxSemanticActionsPerBatch - scratch.pending_batch.count) {
      return failure(DecodeError::BudgetExhausted, track.source_offset, track.logical_channel);
    }

    for (std::size_t action_index = 0; action_index < scratch.track_trace.count; ++action_index) {
      const SemanticInstruction instruction = scratch.track_trace.instructions[action_index];
      scratch.pending_batch.actions[scratch.pending_batch.count++] = {track.logical_channel,
                                                                      instruction};
      if (instruction.kind == InstructionKind::ReleaseChannel) {
        release_track_wait(scratch.candidate_state.tracks[instruction.value]);
      }
    }
  }

  state = scratch.candidate_state;
  batch = scratch.pending_batch;
  return {};
}

} // namespace rpcmp::runtime::mdx
