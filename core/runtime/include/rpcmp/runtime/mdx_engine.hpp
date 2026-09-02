#ifndef RPCMP_RUNTIME_MDX_ENGINE_HPP
#define RPCMP_RUNTIME_MDX_ENGINE_HPP

#include "rpcmp/runtime/mdx_timeline.hpp"

#include <cstdint>

namespace rpcmp::runtime::mdx {

struct MdxEngineState {
  DocumentPlaybackState document{};
  Ym2151RouterState ym2151{};
  MdxTimelineState timeline{};
};

struct MdxEngineScratch {
  MdxEngineState candidate_state{};
  DocumentValidation candidate_validation{};
  DocumentTickBatch actions{};
  Ym2151WriteBatch writes{};
  TimedYm2151Batch pending_batch{};
  ControlFlowScratch control_flow{};
  DocumentTickScratch document{};
  Ym2151RouterScratch ym2151{};
  MdxTimelineScratch timeline{};
};

// Admits the complete immutable document, including inert-P capability and all
// control-flow boundaries, before any playback state or device work exists.
[[nodiscard]] DecodeResult prepare_mdx_playback(const MdxDocument& document,
                                                DocumentValidation& validation,
                                                MdxEngineScratch& scratch,
                                                DecodeLimits limits = DecodeLimits{}) noexcept;

// Executes one A-H driver tick transactionally across document sequencing,
// YM2151 routing, and absolute scheduler timestamp conversion.
[[nodiscard]] DecodeResult advance_mdx_tick(const MdxDocument& document,
                                            std::uint32_t scheduler_tick_rate,
                                            MdxEngineState& state, TimedYm2151Batch& batch,
                                            MdxEngineScratch& scratch,
                                            PlaybackLimits limits = PlaybackLimits{}) noexcept;

} // namespace rpcmp::runtime::mdx

#endif // RPCMP_RUNTIME_MDX_ENGINE_HPP
