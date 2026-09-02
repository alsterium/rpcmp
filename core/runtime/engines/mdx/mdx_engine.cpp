#include "rpcmp/runtime/mdx_engine.hpp"

namespace rpcmp::runtime::mdx {

DecodeResult prepare_mdx_playback(const MdxDocument& document, DocumentValidation& validation,
                                  MdxEngineScratch& scratch, const DecodeLimits limits) noexcept {
  const auto admitted = validate_document(document, scratch.candidate_validation, limits);
  if (!admitted.ok()) {
    return admitted;
  }
  for (const auto& track : document.tracks) {
    const auto control_flow = validate_control_flow(track, scratch.control_flow, limits);
    if (!control_flow.ok()) {
      return control_flow;
    }
  }
  validation = scratch.candidate_validation;
  return {};
}

DecodeResult advance_mdx_tick(const MdxDocument& document, const std::uint32_t scheduler_tick_rate,
                              MdxEngineState& state, TimedYm2151Batch& batch,
                              MdxEngineScratch& scratch, const PlaybackLimits limits) noexcept {
  scratch.candidate_state = state;
  const auto sequenced = advance_document_tick(document, scratch.candidate_state.document,
                                               scratch.actions, scratch.document, limits);
  if (!sequenced.ok()) {
    return sequenced;
  }
  const auto routed = route_ym2151_batch(document, scratch.actions, scratch.candidate_state.ym2151,
                                         scratch.writes, scratch.ym2151);
  if (!routed.ok()) {
    return routed;
  }
  const auto stamped =
      stamp_ym2151_tick(scratch.actions, scratch.writes, scheduler_tick_rate,
                        scratch.candidate_state.timeline, scratch.pending_batch, scratch.timeline);
  if (!stamped.ok()) {
    return stamped;
  }
  state = scratch.candidate_state;
  batch = scratch.pending_batch;
  return {};
}

} // namespace rpcmp::runtime::mdx
