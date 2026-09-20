#ifndef RPCMP_UI_PLAYER_UI_HPP
#define RPCMP_UI_PLAYER_UI_HPP

#include "rpcmp/contracts/player_command_v2.hpp"

namespace rpcmp::ui::v2 {

namespace api = contracts::v2;
enum class Action : std::uint8_t { None, Back, Previous, Next, Confirm, Up, Down, Left, Right };
struct InputSample {
  std::uint16_t down{};
  bool connected{true};
};
struct InputBindings {
  // Back, Previous, Next, Confirm, Up, Down, Left, Right; platform supplies real bits.
  std::array<std::uint16_t, 8> masks{2, 4, 8, 1, 16, 32, 64, 128};
  std::uint64_t repeat_delay_us{350'000};
  std::uint64_t repeat_period_us{80'000};
};
class InputMapper {
public:
  explicit InputMapper(InputBindings bindings = {}) noexcept;
  void rebind(InputBindings bindings) noexcept;
  [[nodiscard]] Action sample(InputSample input, std::uint64_t now_us) noexcept;
  [[nodiscard]] bool valid() const noexcept { return valid_; }

private:
  InputBindings bindings_{};
  std::uint16_t blocked_{}, previous_{};
  Action direction_{Action::None};
  std::uint64_t last_time_{}, repeat_time_{};
  bool valid_{}, connected_{}, repeating_{};
};

enum class View : std::uint8_t { Tracker, Keyboard, Library };
enum class Focus : std::uint8_t {
  Main,
  List = Main, // Compatibility name for the original Library-only focus.
  Previous,
  PlayPause,
  Stop,
  Next,
  ViewSwitch,
  Repeat,
  Shuffle
};
using FocusTable = std::array<std::array<Focus, 4>, 8>; // Up, Down, Left, Right
[[nodiscard]] FocusTable default_focus_table() noexcept;
enum class Diagnostic : std::uint8_t {
  None,
  InvalidSnapshot,
  InvalidInput,
  InvalidFocus,
  Unsupported,
  Busy,
  NoSelection,
  CommandRejected,
  CommandFailed,
  Protocol,
  CatalogFailure,
  CommandExhausted,
  PolicyQueueFull
};
enum class BrowseLevel : std::uint8_t { Albums, Tracks };
struct BrowserState {
  contracts::LibraryGeneration generation{};
  BrowseLevel level{BrowseLevel::Albums};
  std::uint32_t cursor{};
  std::optional<contracts::CatalogAlbumItem> album;
  contracts::CatalogAlbumPage albums{};
  contracts::CatalogTrackPage tracks{};
  bool valid{};
};
inline constexpr std::uint16_t kTrackerRows = 16;
struct TrackerRow {
  std::uint64_t first_sequence{}, at_frame{};
  std::array<std::optional<api::PerformanceChange>, api::kPerformanceChannels> cells{};
};
struct TrackerState {
  std::array<TrackerRow, kTrackerRows> rows{};
  std::uint16_t count{};
  bool supported{}, gap{}, retained_earlier{};
  api::PerformanceAvailability availability{api::PerformanceAvailability::Waiting};
};
struct PlayerView {
  api::PlayerSnapshot snapshot{};
  View main{View::Tracker};
  Focus focus{Focus::PlayPause};
  BrowserState browser{};
  TrackerState tracker{};
  Diagnostic diagnostic{Diagnostic::None};
  api::CommandReason rejection{api::CommandReason::None};
  std::uint8_t queued_policy_actions{};
  bool valid_snapshot{}, transport_pending{};
};
struct UiConfiguration {
  InputBindings bindings{};
  FocusTable focus{default_focus_table()};
  std::uint64_t first_command_id{1};
};

// Single UI owner. Rendering sees const copied observations, never playback objects.
class PlayerUi {
public:
  PlayerUi(api::CommandIngress& commands, const contracts::CatalogReader& catalog,
           UiConfiguration configuration = {}) noexcept;
  void update(const api::PlayerSnapshot& snapshot);
  void input(InputSample sample, std::uint64_t now_us);
  void rebind(InputBindings bindings) noexcept;
  // Semantic actions are fresh presses/repeats already arbitrated by the input adapter.
  void dispatch(Action action);
  [[nodiscard]] const PlayerView& view() const noexcept { return view_; }

private:
  struct PendingTransport {
    api::CommandKind kind{api::CommandKind::Stop};
    std::uint64_t sequence{}, generation{};
    bool new_generation{true};
  };
  enum class PolicyAction : std::uint8_t { RepeatStep, ToggleShuffle };
  void move(Action direction);
  void confirm();
  void back();
  void switch_view();
  void play_pause();
  void neighbour(bool next);
  void stop();
  void transport(api::CommandKind kind,
                 std::optional<api::TrackSelection> selection = std::nullopt);
  [[nodiscard]] std::optional<api::CommandResult> send(api::PlayerCommand command);
  void enqueue_policy(PolicyAction action);
  void pump_policy();
  void pop_policy() noexcept;
  void observe_commands();
  void observe_history();
  void reset_browser();
  [[nodiscard]] bool load_page();
  [[nodiscard]] bool ready_to_send();
  void diagnose(Diagnostic diagnostic) noexcept;
  api::CommandIngress& commands_;
  const contracts::CatalogReader& catalog_;
  InputMapper input_;
  FocusTable focus_;
  PlayerView view_{};
  View last_monitor_{View::Tracker};
  std::uint64_t next_command_id_{};
  std::optional<PendingTransport> pending_transport_;
  std::array<PolicyAction, 8> policy_queue_{};
  std::optional<std::uint64_t> pending_policy_;
  std::uint64_t policy_observed_sequence_{};
  std::uint64_t history_next_{1};
  bool focus_valid_{}, observed_{}, history_seen_{};
};

} // namespace rpcmp::ui::v2

#endif // RPCMP_UI_PLAYER_UI_HPP
