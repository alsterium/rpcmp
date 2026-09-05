#ifndef RPCMP_SPIKE_M5_PLAYBACK_HPP
#define RPCMP_SPIKE_M5_PLAYBACK_HPP

#include "rpcmp/player/mdx_library_session.hpp"

#include <cstddef>
#include <cstdint>

namespace rpcmp::spike {

inline constexpr std::uint64_t kM5StartupLead = 2400;
inline constexpr std::uint64_t kM5Lookahead = 4800;
inline constexpr std::uint64_t kM5DeviceTimeout = 96000;

enum class M5Submit : std::uint8_t { Accepted, Backpressure, Fault };
enum class M5PlaybackState : std::uint8_t { Running, Complete, Fault, ResetFailed };
enum class M5PlaybackError : std::uint8_t { None, Clock, Device, Engine, Overflow, Timeout };

class IM5PlaybackDevice {
public:
  virtual ~IM5PlaybackDevice() = default;
  virtual bool now(std::uint64_t& media_tick) noexcept = 0;
  virtual bool status(bool& empty) noexcept = 0;
  virtual M5Submit submit(std::uint64_t media_tick,
                          const runtime::mdx::Ym2151Write& write) noexcept = 0;
  virtual bool reset() noexcept = 0;
};

// Construct only after complete library/MDX admission and device initialization.
// The session, workspace, and source bytes remain alive for this pump's lifetime.
// No rendering, storage, allocation, or physical input belongs in this class.
class M5PlaybackPump {
public:
  M5PlaybackPump(player::MdxLibrarySession& session, player::MdxLibrarySessionWorkspace& workspace,
                 IM5PlaybackDevice& device) noexcept;
  M5PlaybackState service() noexcept;
  [[nodiscard]] M5PlaybackState state() const noexcept { return state_; }
  [[nodiscard]] M5PlaybackError error() const noexcept { return error_; }
  [[nodiscard]] std::uint64_t writes() const noexcept { return writes_; }
  [[nodiscard]] std::uint64_t digest() const noexcept { return digest_; }

private:
  M5PlaybackState fail(M5PlaybackError error) noexcept;
  bool ended() const noexcept;
  bool shifted(std::uint64_t tick, std::uint64_t& due) noexcept;

  player::MdxLibrarySession& session_;
  player::MdxLibrarySessionWorkspace& workspace_;
  IM5PlaybackDevice& device_;
  runtime::mdx::TimedYm2151Batch batch_{};
  std::size_t pending_{};
  std::uint64_t last_now_{};
  std::uint64_t blocked_at_{};
  std::uint64_t writes_{};
  std::uint64_t digest_{};
  bool blocked_{};
  M5PlaybackState state_{M5PlaybackState::Running};
  M5PlaybackError error_{M5PlaybackError::None};
};

} // namespace rpcmp::spike

#endif
