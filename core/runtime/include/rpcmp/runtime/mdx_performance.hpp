#ifndef RPCMP_RUNTIME_MDX_PERFORMANCE_HPP
#define RPCMP_RUNTIME_MDX_PERFORMANCE_HPP

#include <array>
#include <cstdint>
#include <optional>

namespace rpcmp::runtime::mdx {

inline constexpr std::uint16_t kMdxPerformanceCapacity = 256;
struct Ym2151ObservedChannel {
  bool gate_known{true};
  bool key_on{};
  std::optional<std::uint16_t> pitch_64;
  std::optional<std::uint8_t> voice;
};
enum class Ym2151PerformanceKind : std::uint8_t { KeyOn, KeyOff, PitchChanged, InstrumentChanged };
struct Ym2151PerformanceEvent {
  std::uint16_t after_write{};
  std::uint8_t channel{};
  Ym2151PerformanceKind kind{Ym2151PerformanceKind::KeyOn};
  Ym2151ObservedChannel state{};
};
struct Ym2151PerformanceBatch {
  std::array<Ym2151ObservedChannel, 8> channels{};
  std::array<Ym2151PerformanceEvent, kMdxPerformanceCapacity> events{};
  std::uint16_t count{};
  std::uint16_t write_count{};
  std::uint32_t lost_before{};
  bool capture_lost{};
};
struct SequencedPerformance {
  std::uint64_t at_tick{};
  Ym2151PerformanceBatch batch{};
};

// Separate from driver state: direct register writes can change another channel.
struct Ym2151ObservationState {
  std::array<std::uint8_t, 256> registers{};
  std::array<bool, 256> known{};
  std::array<std::uint8_t, 8> key_masks{};
  std::array<Ym2151ObservedChannel, 8> channels{};
  bool csm{};
};

// Transactional router scratch. Capacity affects only observation retention.
class Ym2151ObservationBuffer {
public:
  void begin_tick(const Ym2151ObservationState& state) noexcept;
  void write(Ym2151ObservationState& state, std::uint8_t address, std::uint8_t value,
             std::uint16_t after_write, bool direct) noexcept;
  void pitch(Ym2151ObservationState& state, std::uint8_t channel,
             std::uint16_t after_write) noexcept;
  void voice(Ym2151ObservationState& state, std::uint8_t channel, std::uint8_t voice,
             std::uint16_t after_write) noexcept;
  void copy_to(const Ym2151ObservationState& state, std::uint16_t write_count,
               Ym2151PerformanceBatch& output) const noexcept;

private:
  void append(const Ym2151ObservationState& state, std::uint8_t channel, Ym2151PerformanceKind kind,
              std::uint16_t after_write) noexcept;
  Ym2151PerformanceBatch batch_{};
  std::uint16_t first_{};
};

} // namespace rpcmp::runtime::mdx

#endif // RPCMP_RUNTIME_MDX_PERFORMANCE_HPP
