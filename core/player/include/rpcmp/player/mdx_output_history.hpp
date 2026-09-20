#ifndef RPCMP_PLAYER_MDX_OUTPUT_HISTORY_HPP
#define RPCMP_PLAYER_MDX_OUTPUT_HISTORY_HPP

#include "rpcmp/player/mdx_performance_mapper.hpp"
#include "rpcmp/player/mdx_source_producer.hpp"

namespace rpcmp::player {

inline constexpr std::size_t kMdxDisplayBatchCapacity = 128;
struct MdxOutputRecord {
  std::uint64_t generation{}, epoch{}, sequence{}, frame{}, prefix{};
  bool natural_end{};
};
struct MdxHistoryObservation {
  std::uint64_t generation{}, epoch{}, through_frame{}, prefix{};
  bool natural_end{};
};
enum class MdxHistoryResult : std::uint8_t {
  Accepted,
  Degraded,
  Waiting,
  Stale,
  Invalid,
  Closed,
  Exhausted
};

// Single Core owner. Display loss has no path to audio, device or UI controls.
class MdxOutputHistory final : public PerformanceReader {
public:
  explicit MdxOutputHistory(std::uint32_t chip_clock_hz) noexcept
      : mapper_(history_, chip_clock_hz) {}
  MdxOutputHistory(const MdxOutputHistory&) = delete;
  MdxOutputHistory& operator=(const MdxOutputHistory&) = delete;
  [[nodiscard]] bool begin(std::uint64_t generation, std::uint64_t epoch) noexcept;
  void cancel() noexcept;
  [[nodiscard]] MdxHistoryResult retain(const MdxProducedCheckpoint& checkpoint) noexcept;
  [[nodiscard]] MdxHistoryResult consume(const MdxOutputRecord& record) noexcept;
  [[nodiscard]] MdxHistoryResult recover(const MdxOutputRecord& latest) noexcept;
  [[nodiscard]] MdxHistoryResult observe(const MdxHistoryObservation& observation) noexcept;
  void copy_to(contracts::v2::PerformanceHistorySnapshot& output) const noexcept override;
  [[nodiscard]] std::size_t pending_batches() const noexcept { return count_; }

private:
  struct Batch {
    MdxProducedCheckpoint checkpoint{};
    std::array<std::uint64_t, contracts::v2::kPerformanceCapacity> frames{};
    std::array<bool, contracts::v2::kPerformanceCapacity> known{};
    std::uint16_t mapped{};
    bool unknown_before{};
  };
  [[nodiscard]] MdxHistoryResult apply(const MdxOutputRecord& record, bool recovering) noexcept;
  void lose(std::uint64_t events, bool unknown) noexcept;
  void discard_front() noexcept;
  void invalidate() noexcept;
  PerformanceHistory history_;
  MdxPerformanceMapper mapper_;
  std::array<Batch, kMdxDisplayBatchCapacity> batches_{};
  runtime::mdx::SequencedPerformance current_{}, mapped_{};
  MdxPerformanceBoundary boundary_{};
  std::array<std::uint64_t, contracts::v2::kPerformanceCapacity> losses_{};
  std::size_t first_{}, count_{};
  std::uint64_t generation_{}, epoch_{}, produced_token_{}, produced_tick_{};
  std::uint64_t last_sequence_{}, last_frame_{}, last_prefix_{}, committed_token_{},
      checkpoint_frame_{};
  std::uint64_t pending_loss_{}, observed_frame_{};
  bool open_{}, have_produced_{}, have_record_{}, terminal_{}, unknown_loss_{};
  bool current_valid_{}, publish_ready_{}, sequence_exhausted_{};
};

} // namespace rpcmp::player
#endif
