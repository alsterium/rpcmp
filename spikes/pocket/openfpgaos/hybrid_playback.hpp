#ifndef RPCMP_HYBRID_PLAYBACK_HPP
#define RPCMP_HYBRID_PLAYBACK_HPP

#include "hybrid_renderer.h"
#include "rpcmp/library/prepared_playlist.hpp"
#include "rpcmp/player/minimal_player.hpp"

namespace rpcmp::platform::pocket {
struct HybridMetrics {
  std::uint32_t render_us{}, feed_us{}, minimum_queued{4096};
};
class HybridPlayback final : public player::minimal::PlaybackPort {
public:
  explicit HybridPlayback(library::PreparedPlaylist& tracks) : tracks_(tracks) {}
  contracts::minimal::Error open(contracts::TrackId id) override;
  bool stop() override;
  contracts::minimal::Error set_paused(bool paused) override;
  contracts::minimal::Error service(bool& ended) override;
  [[nodiscard]] HybridMetrics metrics() const noexcept { return metrics_; }

private:
  library::PreparedPlaylist& tracks_;
  const RpcmpHybridBlock* pending_{};
  HybridMetrics metrics_{};
  std::uint32_t wait_started_{}, pause_started_{};
  bool started_{}, eof_{}, paused_{};
};
} // namespace rpcmp::platform::pocket
#endif
