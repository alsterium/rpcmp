#ifndef RPCMP_RUNTIME_MDX_TRACK_MACHINE_HPP
#define RPCMP_RUNTIME_MDX_TRACK_MACHINE_HPP

#include "rpcmp/runtime/mdx_semantic.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rpcmp::runtime::mdx {

inline constexpr std::size_t kMdxMaxInstructionsPerTick = 4'096;
inline constexpr std::size_t kMdxMaxBranchesPerTick = 4'096;
inline constexpr std::size_t kMdxMaxRepeatFrames = 64;

struct PlaybackLimits {
  std::size_t max_instructions_per_tick{kMdxMaxInstructionsPerTick};
  std::size_t max_branches_per_tick{kMdxMaxBranchesPerTick};
  std::size_t max_repeat_frames{kMdxMaxRepeatFrames};
};

struct RepeatFrame {
  std::size_t body_offset{};
  std::uint16_t remaining{};
};

struct TrackPlaybackState {
  std::size_t cursor{};
  std::uint16_t remaining_ticks{};
  std::array<RepeatFrame, kMdxMaxRepeatFrames> repeats{};
  std::size_t repeat_depth{};
  bool waiting{};
  bool ended{};
};

struct TrackTickTrace {
  std::array<SemanticInstruction, kMdxMaxInstructionsPerTick> instructions{};
  std::size_t count{};
};

struct TrackTickScratch {
  TrackTickTrace pending{};
};

// Executes one driver-tick service for one already prepared track. State and
// trace are committed together only on success. The source bytes stay immutable.
[[nodiscard]] DecodeResult advance_track_tick(const TrackView& track, TrackPlaybackState& state,
                                              TrackTickTrace& trace, TrackTickScratch& scratch,
                                              PlaybackLimits limits = PlaybackLimits{}) noexcept;

// Releases an ee wait without advancing time. Document-level A-H service uses
// this when an ef action targets the waiting logical channel.
void release_track_wait(TrackPlaybackState& state) noexcept;

} // namespace rpcmp::runtime::mdx

#endif // RPCMP_RUNTIME_MDX_TRACK_MACHINE_HPP
