#ifndef RPCMP_PLATFORM_POCKET_SOUND_MMIO_CLIENT_HPP
#define RPCMP_PLATFORM_POCKET_SOUND_MMIO_CLIENT_HPP

#include "rpcmp/platform/pocket/device_queue_mmio.hpp"
#include "rpcmp/player/mdx_output_history.hpp"
#include "rpcmp/player/playback_transport.hpp"

namespace rpcmp::platform::pocket {

inline constexpr std::uintptr_t kSoundMmioBase = 0x4000'0400U;
inline constexpr std::uint64_t kSoundMailboxTimeoutUs = 1'000;

class SoundClock {
public:
  virtual ~SoundClock() = default;
  [[nodiscard]] virtual std::uint64_t now_us() noexcept = 0;
};

enum class SoundChannel : std::uint8_t { Control, Feed, Capture, Journal };
enum class SoundSubmit : std::uint8_t {
  Accepted,
  Busy,
  Invalid,
  NotInitialized,
  Incompatible,
  Faulted,
  ClockFailure
};
enum class SoundTransfer : std::uint8_t { Complete, Timeout, Protocol, ClockFailure };
enum class SoundReply : std::uint8_t { Success = 1, Invalid = 2, Stale = 3, Failed = 4 };
enum class SoundJournalReply : std::uint8_t { Success = 1, Empty = 2, Invalid = 3, Stale = 4 };

struct SoundQuery {
  std::uint64_t ticket{}, epoch{};
};
struct SoundJournalQuery {
  std::uint64_t ticket{}, epoch{}, head_sequence{}; // Zero selects Peek; otherwise Pop.
};
struct SoundPrefix {
  std::uint64_t prefix{}, completed_loops{};
  bool valid{}, checkpoint{}, ended{};
};
struct SoundControlCompletion {
  SoundTransfer transfer{SoundTransfer::Complete};
  player::AudioControlRequest request{};
  SoundReply result{SoundReply::Invalid};
  std::uint64_t frame{}, epoch{}, prepared_generation{};
};
struct SoundFeedCompletion {
  SoundTransfer transfer{SoundTransfer::Complete};
  player::MdxFeedCompletion item{};
};
struct SoundCaptureCompletion {
  SoundTransfer transfer{SoundTransfer::Complete};
  SoundQuery request{};
  SoundReply result{SoundReply::Invalid};
  std::uint64_t epoch{}, prepared_generation{};
  player::MediaEnvelopeSnapshot media{};
  bool quiescent{}, resetting{}, device_idle{}, media_enabled{}, fault{}, inhibited{};
  std::uint8_t queued{};
  SoundPrefix output{}, pending{};
};
struct SoundJournalCompletion {
  SoundTransfer transfer{SoundTransfer::Complete};
  SoundJournalQuery request{};
  SoundJournalReply result{SoundJournalReply::Invalid};
  std::uint64_t epoch{}, lost{}, next_sequence{};
  std::uint8_t queued{};
  bool head_valid{}, latest_valid{}, exhausted{};
  player::MdxOutputRecord head{}, latest{};
};

// One CPU owner. Core calls service independently of publication/rendering.
class SoundMmioClient {
public:
  SoundMmioClient(IMmio32& mmio, SoundClock& clock) noexcept : mmio_(mmio), clock_(clock) {}
  SoundMmioClient(const SoundMmioClient&) = delete;
  SoundMmioClient& operator=(const SoundMmioClient&) = delete;
  [[nodiscard]] SoundSubmit initialize() noexcept;
  [[nodiscard]] SoundSubmit control(const player::AudioControlRequest& request) noexcept;
  [[nodiscard]] SoundSubmit feed(const player::MdxSourceOffer& item) noexcept;
  [[nodiscard]] SoundSubmit capture(SoundQuery query) noexcept;
  [[nodiscard]] SoundSubmit journal(SoundJournalQuery query) noexcept;
  void service() noexcept;
  void emergency_silence() noexcept;
  [[nodiscard]] std::optional<SoundControlCompletion> take_control() noexcept;
  [[nodiscard]] std::optional<SoundFeedCompletion> take_feed() noexcept;
  [[nodiscard]] std::optional<SoundCaptureCompletion> take_capture() noexcept;
  [[nodiscard]] std::optional<SoundJournalCompletion> take_journal() noexcept;
  [[nodiscard]] bool busy(SoundChannel channel) const noexcept;
  [[nodiscard]] bool faulted() const noexcept { return critical_failure_ || clock_failed_; }
  [[nodiscard]] std::uint32_t status() const noexcept { return status_; }

private:
  struct Slot {
    std::uint64_t submitted_at{};
    bool borrowed{}, result_ready{}, abandoned{};
  };
  [[nodiscard]] SoundSubmit prepare(SoundChannel channel, bool reset = false) noexcept;
  void submit(SoundChannel channel) noexcept;
  [[nodiscard]] bool read_clock(std::uint64_t& value) noexcept;
  void fail(SoundChannel channel, SoundTransfer reason) noexcept;
  [[nodiscard]] std::uint32_t read(std::uintptr_t offset) noexcept;
  void write(std::uintptr_t offset, std::uint32_t value) noexcept;
  [[nodiscard]] std::uint64_t read64(std::uintptr_t offset) noexcept;
  void write64(std::uintptr_t offset, std::uint64_t value) noexcept;
  [[nodiscard]] bool read_control() noexcept;
  [[nodiscard]] bool read_feed() noexcept;
  [[nodiscard]] bool read_capture() noexcept;
  [[nodiscard]] bool read_journal() noexcept;
  [[nodiscard]] bool read_prefix(std::uintptr_t offset, SoundPrefix& prefix) noexcept;
  [[nodiscard]] bool read_record(std::uintptr_t offset, std::uint64_t epoch,
                                 player::MdxOutputRecord& record) noexcept;
  IMmio32& mmio_;
  SoundClock& clock_;
  std::array<Slot, 4> slots_{};
  SoundControlCompletion control_{};
  SoundFeedCompletion feed_{};
  SoundCaptureCompletion capture_{};
  SoundJournalCompletion journal_{};
  std::uint64_t last_clock_{};
  std::uint32_t status_{};
  bool initialized_{}, critical_failure_{}, clock_failed_{}, reset_can_recover_{};
};

} // namespace rpcmp::platform::pocket
#endif
