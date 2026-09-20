#ifndef RPCMP_PLAYER_MDX_SOURCE_PRODUCER_HPP
#define RPCMP_PLAYER_MDX_SOURCE_PRODUCER_HPP

#include "rpcmp/runtime/mdx_engine.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace rpcmp::player {

inline constexpr std::uint32_t kMdxSourceTickRate = 12'288'000;
enum class MdxSourceResult : std::uint8_t {
  Accepted,
  Full,
  Busy,
  Closed,
  Stale,
  Invalid,
  Exhausted,
  EngineFailure,
  ProtocolError,
  DeviceFailure
};
enum class MdxFeedStatus : std::uint8_t {
  Accepted = 1,
  Full = 2,
  Invalid = 3,
  Closed = 4,
  Stale = 5,
  Failed = 6
};
struct MdxSourceContinuity {
  std::uint64_t at_tick{};
  std::uint64_t completed_loops{};
  std::uint64_t accepted_token{};
};
struct MdxSourceOffer {
  std::uint64_t generation{};
  std::uint64_t epoch{};
  std::uint64_t token{}; // Local adapter identity, not an additional MMIO field.
  std::uint64_t at_tick{};
  std::uint64_t until_tick{};
  std::uint64_t completed_loops{};
  std::uint8_t address{};
  std::uint8_t value{};
  bool marker{};
  bool ended{};
};
struct MdxFeedCompletion {
  std::uint64_t generation{};
  std::uint64_t epoch{};
  std::uint64_t token{};
  MdxFeedStatus status{MdxFeedStatus::Failed};
};

[[nodiscard]] MdxSourceResult
validate_mdx_source_tick(const runtime::mdx::TimedYm2151Batch& writes,
                         const runtime::mdx::SequencedProgress& progress, std::uint64_t next_tick,
                         const MdxSourceContinuity& continuity) noexcept;

class RetainedMdxSource {
public:
  // The caller must first confirm the physical sound Reset for this epoch.
  [[nodiscard]] MdxSourceResult begin(std::uint64_t generation, std::uint64_t epoch) noexcept;
  void cancel() noexcept;
  [[nodiscard]] MdxSourceResult availability() const noexcept;
  [[nodiscard]] MdxSourceResult retain(const runtime::mdx::TimedYm2151Batch& writes,
                                       const runtime::mdx::SequencedProgress& progress,
                                       std::uint64_t next_tick) noexcept;
  [[nodiscard]] std::optional<MdxSourceOffer> take_offer() noexcept;
  [[nodiscard]] MdxSourceResult complete(const MdxFeedCompletion& completion) noexcept;
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  [[nodiscard]] std::uint64_t epoch() const noexcept { return epoch_; }
  [[nodiscard]] std::uint64_t accepted_token() const noexcept { return continuity_.accepted_token; }

private:
  enum class State : std::uint8_t { Uninitialized, Ready, Pending, Ended, Cancelled, Faulted };
  struct RegisterWrite {
    std::uint8_t address{}, value{};
  };
  std::array<RegisterWrite, runtime::mdx::kMdxMaxYm2151WritesPerBatch> writes_{};
  runtime::mdx::SequencedProgress progress_{};
  MdxSourceContinuity continuity_{};
  std::uint64_t generation_{}, epoch_{}, next_tick_{};
  std::uint16_t count_{}, cursor_{};
  State state_{State::Uninitialized};
  bool in_flight_{};
};

// Copy this into independent display retention; it is not an audible update.
struct MdxProducedCheckpoint {
  std::uint64_t generation{};
  std::uint64_t epoch{};
  std::uint64_t preceding_token{};
  std::uint64_t marker_token{};
  runtime::mdx::SequencedPerformance performance{};
};
struct MdxSourceWorkspace {
  runtime::mdx::MdxEngineScratch engine{};
  runtime::mdx::MdxEngineState candidate{};
  runtime::mdx::TimedYm2151Batch batch{};
};
struct MdxProductionResult {
  MdxSourceResult status{MdxSourceResult::Closed};
  runtime::mdx::DecodeResult engine{};
};

// Document is already admitted and immutable. Workspace/state must not alias.
// Pending audio never advances the engine. All output commits on success only.
[[nodiscard]] MdxProductionResult
produce_mdx_tick(const runtime::mdx::MdxDocument& document, runtime::mdx::MdxEngineState& state,
                 RetainedMdxSource& source, MdxProducedCheckpoint& checkpoint,
                 MdxSourceWorkspace& workspace,
                 runtime::mdx::PlaybackLimits limits = runtime::mdx::PlaybackLimits{}) noexcept;

} // namespace rpcmp::player

#endif
