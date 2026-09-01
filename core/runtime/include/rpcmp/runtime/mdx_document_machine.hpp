#ifndef RPCMP_RUNTIME_MDX_DOCUMENT_MACHINE_HPP
#define RPCMP_RUNTIME_MDX_DOCUMENT_MACHINE_HPP

#include "rpcmp/runtime/mdx_track_machine.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rpcmp::runtime::mdx {

inline constexpr std::size_t kMdxFmTrackCount = 8;
inline constexpr std::size_t kMdxMaxSemanticActionsPerBatch = 4'096;

struct SemanticAction {
  std::uint8_t logical_channel{};
  SemanticInstruction instruction{};
};

struct DocumentPlaybackState {
  std::array<TrackPlaybackState, kMdxTrackCount> tracks{};
};

struct DocumentTickBatch {
  std::array<SemanticAction, kMdxMaxSemanticActionsPerBatch> actions{};
  std::size_t count{};
};

struct DocumentTickScratch {
  DocumentPlaybackState candidate_state{};
  DocumentTickBatch pending_batch{};
  TrackTickTrace track_trace{};
  TrackTickScratch track_scratch{};
};

// Services FM tracks A-H in their historical order. Release-channel effects
// take effect at their exact position in that order. State and the complete
// semantic batch are committed only after every track succeeds.
[[nodiscard]] DecodeResult advance_document_tick(const MdxDocument& document,
                                                 DocumentPlaybackState& state,
                                                 DocumentTickBatch& batch,
                                                 DocumentTickScratch& scratch,
                                                 PlaybackLimits limits = PlaybackLimits{}) noexcept;

} // namespace rpcmp::runtime::mdx

#endif // RPCMP_RUNTIME_MDX_DOCUMENT_MACHINE_HPP
