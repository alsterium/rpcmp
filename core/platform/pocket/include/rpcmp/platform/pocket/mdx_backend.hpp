#ifndef RPCMP_PLATFORM_POCKET_MDX_BACKEND_HPP
#define RPCMP_PLATFORM_POCKET_MDX_BACKEND_HPP

#include "rpcmp/platform/pocket/sound_mmio_client.hpp"
#include "rpcmp/player/mdx_library_session.hpp"

namespace rpcmp::platform::pocket {

// One control owner; keep this large workspace in persistent storage, not a frame.
class PocketMdxBackend final : public player::PreparationPort,
                               public player::AudioTransportPort,
                               public player::PerformanceReader {
public:
  PocketMdxBackend(const player::CatalogSession& catalog, SoundMmioClient& client) noexcept
      : catalog_(catalog), client_(client) {}
  PocketMdxBackend(const PocketMdxBackend&) = delete;
  PocketMdxBackend& operator=(const PocketMdxBackend&) = delete;
  [[nodiscard]] SoundSubmit initialize() noexcept;
  [[nodiscard]] bool supports_pause() const noexcept override { return initialized_; }
  [[nodiscard]] bool supports_policy() const noexcept override { return initialized_; }
  [[nodiscard]] bool begin(const player::PreparationRequest& request) override;
  void cancel(std::uint64_t operation_id) override;
  [[nodiscard]] player::PreparationProgress poll(std::uint64_t operation_id) override;
  void release() override;
  [[nodiscard]] bool begin(const player::AudioControlRequest& request) override;
  [[nodiscard]] player::AudioObservation observe() override;
  void emergency_silence() override;
  void service() noexcept;
  void copy_to(contracts::v2::PerformanceHistorySnapshot& output) const noexcept override;

private:
  void fail() noexcept;
  void stop_source() noexcept;
  void control_result(const SoundControlCompletion& result) noexcept;
  void capture_result(const SoundCaptureCompletion& result) noexcept;
  void feed_result(const SoundFeedCompletion& result) noexcept;
  void journal_result(const SoundJournalCompletion& result) noexcept;
  void prepare_document() noexcept;
  void supply() noexcept;
  void request_control() noexcept;
  void request_capture() noexcept;
  void request_journal() noexcept;
  void publish_history() noexcept;

  const player::CatalogSession& catalog_;
  SoundMmioClient& client_;
  player::MdxLibrarySession session_{};
  runtime::mdx::MdxDocument document_scratch_{};
  runtime::mdx::DocumentValidation validation_scratch_{};
  player::MdxSourceWorkspace workspace_{};
  player::RetainedMdxSource source_{};
  player::MdxProducedCheckpoint checkpoint_{};
  player::MdxOutputHistory history_{3'579'545};
  std::optional<player::PreparationRequest> preparation_;
  player::PreparationProgress preparation_state_{player::PreparationProgress::Cancelled};
  std::optional<player::AudioControlRequest> control_;
  std::optional<player::AudioControlCompletion> completion_;
  std::optional<player::MdxSourceOffer> offer_;
  std::optional<SoundCaptureCompletion> capture_;
  std::optional<player::MdxOutputRecord> latest_record_;
  std::uint64_t epoch_{}, reset_generation_{}, started_generation_{}, acknowledged_control_{};
  std::uint64_t capture_ticket_{}, capture_authority_{}, journal_ticket_{}, pop_sequence_{};
  std::uint64_t journal_pause_control_{}, drained_pause_control_{}, closed_capture_ticket_{};
  bool initialized_{}, fault_{}, known_inhibit_{}, quiescent_{}, control_submitted_{};
  bool document_loaded_{}, prepared_{}, supplying_{}, prefilled_{}, source_ended_{};
  bool feed_closed_{}, history_ready_{}, journal_available_{};
};

} // namespace rpcmp::platform::pocket
#endif
