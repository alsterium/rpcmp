#ifndef RPCMP_PLATFORM_POCKET_PLAYER_APPLICATION_HPP
#define RPCMP_PLATFORM_POCKET_PLAYER_APPLICATION_HPP

#include "rpcmp/platform/pocket/bitmap_canvas.hpp"
#include "rpcmp/platform/pocket/mdx_backend.hpp"
#include "rpcmp/player/player_session.hpp"

namespace rpcmp::platform::pocket {

// These bindings describe APF PAD, not engine controls. They can be replaced
// independently of rendering, sequencing and the semantic UI actions.
[[nodiscard]] ui::v2::InputBindings pocket_input_bindings() noexcept;
[[nodiscard]] ui::v2::InputSample pocket_input_sample(std::uint32_t key) noexcept;

class PocketClock final : public SoundClock {
public:
  PocketClock(IMmio32& mmio, std::uint32_t cpu_hz) noexcept;
  [[nodiscard]] std::uint64_t now_us() noexcept override;
  [[nodiscard]] bool valid() const noexcept { return divisor_ != 0 && !failed_; }

private:
  IMmio32& mmio_;
  std::uint64_t last_{};
  std::uint32_t divisor_{};
  bool failed_{};
};

struct DrawSurface {
  std::uint8_t* pixels{};
  std::size_t bytes{};
  std::uint32_t stride{};
};
enum class PresentResult : std::uint8_t { Presented, Busy, Failed };
class PlayerDisplay {
public:
  virtual ~PlayerDisplay() = default;
  // Retain this surface until Presented; Busy neither consumes nor changes it.
  [[nodiscard]] virtual DrawSurface acquire() noexcept = 0;
  [[nodiscard]] virtual PresentResult present() noexcept = 0;
};

struct ApplicationMetrics {
  std::uint64_t service_calls{}, frames{}, max_service_gap_us{}, max_record_us{}, max_pump_us{},
      max_present_us{};
};
enum class ApplicationFailure : std::uint8_t { None, SoundSetup, Clock, Canvas, Display };

// Target composition root. Keep this large owner in BSS, not on the stack.
// The caller polls held input and calls step continuously, without vblank waits.
class PlayerApplication {
public:
  PlayerApplication(const player::CatalogSession& catalog, IMmio32& mmio, SoundClock& clock,
                    PlayerDisplay& display, player::RandomSource& random) noexcept;
  [[nodiscard]] bool initialize() noexcept;
  void step(ui::v2::InputSample input);
  [[nodiscard]] ApplicationFailure failure() const noexcept { return failure_; }
  [[nodiscard]] const ApplicationMetrics& metrics() const noexcept { return metrics_; }
  [[nodiscard]] const ui::v2::PlayerView& view() const noexcept { return ui_.view(); }

private:
  [[nodiscard]] std::uint64_t time() noexcept;
  void service();
  void fail(ApplicationFailure reason) noexcept;
  SoundClock& clock_;
  PlayerDisplay& display_;
  SoundMmioClient client_;
  PocketMdxBackend backend_;
  player::PlayerSession player_;
  ui::v2::PlayerUi ui_;
  BitmapCanvas canvas_;
  DrawSurface surface_{};
  ApplicationMetrics metrics_{};
  std::uint64_t last_time_{}, last_service_{}, last_publication_{}, last_frame_{};
  ApplicationFailure failure_{ApplicationFailure::None};
  bool initialized_{}, published_{}, drawing_{}, framed_{};
};

class LibrarySlot {
public:
  virtual ~LibrarySlot() = default;
  [[nodiscard]] virtual bool size(std::uint32_t& bytes) noexcept = 0;
  [[nodiscard]] virtual bool read(std::uint32_t offset, std::uint8_t* output,
                                  std::uint32_t bytes) noexcept = 0;
};
inline constexpr std::uint32_t kLibraryReadChunk = 64 * 1024;
// Startup only, before the application and its audio mailbox owner exist.
[[nodiscard]] player::CatalogChangeResult load_player_library(LibrarySlot& slot,
                                                              std::uint8_t* storage,
                                                              std::size_t capacity,
                                                              player::CatalogSession& catalog);

} // namespace rpcmp::platform::pocket
#endif
