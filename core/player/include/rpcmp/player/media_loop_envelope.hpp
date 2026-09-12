#ifndef RPCMP_PLAYER_MEDIA_LOOP_ENVELOPE_HPP
#define RPCMP_PLAYER_MEDIA_LOOP_ENVELOPE_HPP

#include <array>
#include <cstdint>
#include <limits>
#include <optional>

namespace rpcmp::player {

inline constexpr std::uint16_t kMediaProgressCapacity = 256;
inline constexpr std::uint32_t kMediaGainDenominator = 240'000;
inline constexpr std::uint32_t kMediaRestoreFrames = 960;

struct StereoFrame {
  std::int16_t left{};
  std::int16_t right{};
};
struct MediaProgressInterval {
  std::uint64_t play_generation{};
  std::uint64_t sequence{};
  std::uint64_t at_frame{};
  std::uint64_t until_frame{};
  std::uint64_t completed_loops{};
  bool ended{};
};
enum class MediaAdmission : std::uint8_t {
  Accepted,
  Full,
  StaleGeneration,
  Invalid,
  Closed,
  ResourceExhausted
};
enum class MediaPhase : std::uint8_t { Steady, Fading, RestoringGain };
enum class MediaEnd : std::uint8_t { None, Stopped, NaturalEnd, RepeatOne, LoopLimit };
enum class MediaFailure : std::uint8_t { None, DeviceFault, Protocol, Underrun, ResourceExhausted };
enum class MediaControlAction : std::uint8_t { Keep, Pause, Resume, Stop };
enum class MediaControlDisposition : std::uint8_t { None, Applied, StaleGeneration, Rejected };
struct MediaRepeatUpdate {
  std::uint64_t revision{};
  std::optional<std::uint32_t> target{2};
};
struct MediaBoundaryControl {
  bool device_fault{};
  MediaControlAction action{MediaControlAction::Keep};
  std::optional<MediaRepeatUpdate> repeat;
  std::uint64_t play_generation{};
};
struct MediaEnvelopeSnapshot {
  std::uint64_t play_generation{};
  std::uint64_t frame{};
  std::uint64_t completed_loops{};
  std::uint64_t policy_revision{};
  std::optional<std::uint32_t> target{2};
  MediaPhase phase{MediaPhase::Steady};
  MediaEnd end{MediaEnd::None};
  MediaFailure failure{MediaFailure::None};
  bool paused{};
  std::uint32_t gain{};
  std::uint32_t ramp_elapsed{};
  std::uint32_t ramp_duration{};
};
struct MediaFrameResult {
  StereoFrame output{};
  bool consumed{};
  MediaControlDisposition control{MediaControlDisposition::None};
};

// Audio-boundary model. The adapter owns mapping, physical quiescence and
// retaining every upstream state whenever advance does not consume its input.
class MediaLoopEnvelope {
public:
  explicit MediaLoopEnvelope(
      std::uint64_t max_frames = std::numeric_limits<std::uint64_t>::max()) noexcept;
  [[nodiscard]] MediaAdmission begin(std::uint64_t generation, std::optional<std::uint32_t> target,
                                     std::uint64_t policy_revision) noexcept;
  [[nodiscard]] MediaAdmission enqueue(const MediaProgressInterval& progress) noexcept;
  [[nodiscard]] MediaFrameResult advance(StereoFrame source,
                                         const MediaBoundaryControl& control = {}) noexcept;
  [[nodiscard]] MediaEnvelopeSnapshot snapshot() const noexcept;

private:
  [[nodiscard]] std::uint32_t current_gain() const noexcept;
  [[nodiscard]] std::uint32_t elapsed() const noexcept;
  void ramp(MediaPhase phase, std::uint32_t duration, std::uint32_t start_gain) noexcept;
  void finish(MediaEnd reason) noexcept;
  void fail(MediaFailure reason) noexcept;
  const std::uint64_t max_frames_;
  MediaEnvelopeSnapshot state_{};
  std::array<MediaProgressInterval, kMediaProgressCapacity> queue_{};
  std::uint16_t head_{};
  std::uint16_t count_{};
  std::uint64_t last_sequence_{};
  std::uint64_t last_until_{};
  std::uint64_t last_loops_{};
  std::uint64_t covered_until_{};
  bool end_enqueued_{};
  std::uint64_t ramp_anchor_{};
  std::uint32_t ramp_start_gain_{};
  std::uint32_t ramp_duration_{};
};

} // namespace rpcmp::player

#endif // RPCMP_PLAYER_MEDIA_LOOP_ENVELOPE_HPP
