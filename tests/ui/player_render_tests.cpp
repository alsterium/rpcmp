#include "rpcmp/platform/host/svg_canvas.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace c = rpcmp::contracts;
namespace api = c::v2;
using namespace rpcmp::ui::v2;
template <typename T> T& required(std::optional<T>& value) {
  if (!value)
    throw std::logic_error("missing authored value");
  return *value;
}
c::CatalogText text(const std::string_view value) {
  c::CatalogText result;
  if (value.size() > result.bytes.size())
    throw std::logic_error("oversized authored text");
  std::copy(value.begin(), value.end(), result.bytes.begin());
  result.length = static_cast<std::uint16_t>(value.size());
  return result;
}
PlayerView fixture() {
  PlayerView view;
  auto& s = view.snapshot;
  s.sequence = 1;
  s.library = {1, {1}, c::CatalogPhase::Ready, c::CatalogFailure::None, 1, 3};
  s.capabilities.bits |= api::kTransportCommands | api::kStatePreservingPause |
                         api::kPolicyObservations | api::kRepeatControl |
                         api::kPolicyCommandResults | api::kPerformanceHistory |
                         api::kPlaybackNavigation;
  s.transport = s.projected = c::TransportState::Playing;
  s.track = api::SelectedTrack{{{101}, {10}, 1, text("Current title")}, text("Current album")};
  s.play_generation = 1;
  s.position_frames = 96'000;
  s.prepared = true;
  s.policy = api::PlaybackPolicyObservation{};
  s.policy_commands = api::PolicyCommandObservation{};
  s.navigation = api::PlaybackNavigationObservation{true, true, std::nullopt};
  api::PerformanceHistorySnapshot history;
  history.play_generation = 1;
  history.observed_through_frame = s.position_frames;
  history.channels = api::unknown_performance_channels();
  const api::PerformanceChannel on{0, true, std::uint8_t{60}, std::nullopt,
                                   api::MdxFmV1{std::uint8_t{0x2A}}};
  const api::PerformanceChannel off{1, false, std::uint8_t{61}, std::nullopt, std::nullopt};
  history.events[0] = {1, {100, on, api::PerformanceKind::KeyOn}};
  history.events[1] = {2, {100, off, api::PerformanceKind::KeyOff}};
  history.channels[0] = on;
  history.channels[0].note = std::uint8_t{63}; // current D#4 differs from retained C-4
  history.channels[1] = off;
  history.count = 2;
  history.next_sequence = 3;
  history.availability = api::PerformanceAvailability::Available;
  s.performance_history = history;
  view.tracker.supported = true;
  view.tracker.availability = api::PerformanceAvailability::Available;
  view.tracker.count = 1;
  view.tracker.rows[0] = {1, 100, {}};
  view.tracker.rows[0].cells[0] = history.events[0].change;
  view.tracker.rows[0].cells[1] = history.events[1].change;
  auto& browser = view.browser;
  browser.generation = {1};
  browser.level = BrowseLevel::Tracks;
  browser.cursor = 2;
  browser.album = c::CatalogAlbumItem{{10}, 0, text("Current album"), 3};
  browser.tracks.album_id = {10};
  browser.tracks.header = {1, {1, {1}, 0, 16}, c::CatalogQueryError::None, 3, 3, std::nullopt};
  browser.tracks.items[0] = {{100}, {10}, 0, text("First title")};
  browser.tracks.items[1] = required(s.track).item;
  browser.tracks.items[2] = {{102}, {10}, 2, text("Browsing candidate")};
  browser.valid = true;
  view.valid_snapshot = true;
  return view;
}
struct TextCall {
  Box box;
  std::string value;
  Color ink{};
  bool truncated{};
};
struct FillCall {
  Box box;
  Color ink{};
};
class Canvas final : public PlayerCanvas {
public:
  explicit Canvas(rpcmp::test::Suite& checks) : suite(checks) {}
  void fill(const Box box, const Color ink) override {
    bounds(box);
    fills.push_back({box, ink});
  }
  void line(const Point a, const Point b, Color) override {
    RPCMP_CHECK(suite, a.x < 640 && a.y < 480 && b.x < 640 && b.y < 480);
    ++lines;
  }
  void text(const Box box, const std::string_view value, const Color ink,
            const bool truncated) override {
    bounds(box);
    texts.push_back({box, std::string(value), ink, truncated});
  }
  bool contains(const std::string_view value) const {
    return std::any_of(texts.begin(), texts.end(),
                       [&](const auto& call) { return call.value == value; });
  }
  bool fill_at(const Box box, const Color ink) const {
    return std::any_of(fills.begin(), fills.end(), [&](const auto& call) {
      return call.box.x == box.x && call.box.y == box.y && call.box.width == box.width &&
             call.box.height == box.height && call.ink == ink;
    });
  }
  void bounds(const Box box) {
    RPCMP_CHECK(suite, box.width != 0 && box.height != 0 && box.x + box.width <= 640 &&
                           box.y + box.height <= 480);
  }
  rpcmp::test::Suite& suite;
  std::vector<TextCall> texts;
  std::vector<FillCall> fills;
  unsigned lines{};
};
void model_rendering(rpcmp::test::Suite& suite) {
  auto view = fixture();
  RPCMP_CHECK(suite, api::valid_player_snapshot(view.snapshot) &&
                         c::valid_catalog_page(view.browser.tracks));
  Canvas tracker(suite);
  render_player(view, tracker);
  RPCMP_CHECK(suite, tracker.contains("OFF")); // Shuffle state must be readable without color.
  required(view.snapshot.policy).desired.order = api::PlaybackOrder::ShuffleLibrary;
  Canvas shuffled(suite);
  render_player(view, shuffled);
  RPCMP_CHECK(suite, shuffled.contains("ON"));
  required(view.snapshot.policy).desired.order = api::PlaybackOrder::AlbumOrder;
  RPCMP_CHECK(suite,
              tracker.contains("C-4 2A") && tracker.contains("OFF --") && tracker.contains("0001"));
  RPCMP_CHECK(suite,
              tracker.contains("FM 1") && tracker.contains("FM 8") && tracker.contains("00:02"));
  RPCMP_CHECK(suite, !tracker.contains("BPM") && !tracker.contains("0:00 / 0:00"));
  view.main = View::Keyboard;
  Canvas keyboard(suite);
  render_player(view, keyboard);
  RPCMP_CHECK(suite, keyboard.fill_at({357, 54, 4, 16}, palette::accent)); // D#4, not retained C-4
  RPCMP_CHECK(suite,
              !keyboard.fill_at({345, 54, 7, 26}, palette::accent) && keyboard.contains("D#4"));
  RPCMP_CHECK(suite,
              std::any_of(keyboard.texts.begin(), keyboard.texts.end(), [](const auto& call) {
                return call.box.x == 48 && call.box.y == 90 && call.value == "OFF";
              }));
  view.snapshot.transport = view.snapshot.projected = c::TransportState::Paused;
  Canvas paused(suite);
  render_player(view, paused);
  RPCMP_CHECK(suite, paused.fill_at({357, 54, 4, 16}, palette::warning) &&
                         paused.contains("PAUSED / held channel state"));
  view.snapshot.transport = view.snapshot.projected = c::TransportState::Playing;
  view.main = View::Library;
  view.focus = Focus::List;
  Canvas library(suite);
  render_player(view, library);
  RPCMP_CHECK(suite, library.contains("Browsing candidate") &&
                         library.fill_at({9, 90, 622, 18}, palette::selected));
  RPCMP_CHECK(suite, library.fill_at({612, 76, 6, 6}, palette::accent));
  for (const auto* canvas : {&tracker, &keyboard, &library}) {
    const auto current = std::find_if(canvas->texts.begin(), canvas->texts.end(),
                                      [](const auto& call) { return call.box.y == 392; });
    RPCMP_CHECK(suite, current != canvas->texts.end() && current->value == "Current title" &&
                           current->box.x == 16 && current->box.width == 344);
    RPCMP_CHECK(suite, canvas->contains("Current album") && canvas->contains("00:02"));
  }
  required(view.snapshot.track).item.title =
      text("あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほABCDEF");
  required(view.snapshot.track).item.title.truncated = true;
  Canvas truncated(suite);
  render_player(view, truncated);
  RPCMP_CHECK(suite,
              std::any_of(truncated.texts.begin(), truncated.texts.end(),
                          [](const auto& call) { return call.box.y == 392 && call.truncated; }));
  view.snapshot.position_frames = std::numeric_limits<std::uint64_t>::max();
  required(view.snapshot.performance_history).observed_through_frame =
      view.snapshot.position_frames;
  Canvas maximum(suite);
  render_player(view, maximum);
  RPCMP_CHECK(suite,
              maximum.contains("106751991167:18:02")); // independently: floor(UINT64_MAX / 48000)
}
void missing_and_failures(rpcmp::test::Suite& suite) {
  auto view = fixture();
  view.main = View::Keyboard;
  view.tracker.supported = false;
  view.snapshot.performance_history.reset();
  view.snapshot.capabilities.bits &= ~api::kPerformanceHistory;
  Canvas unsupported(suite);
  render_player(view, unsupported);
  RPCMP_CHECK(suite, unsupported.contains("Performance display unavailable") &&
                         !unsupported.fill_at({357, 54, 4, 16}, palette::accent));
  view = fixture();
  view.main = View::Tracker;
  view.tracker.gap = true;
  view.tracker.retained_earlier = true;
  Canvas gap(suite);
  render_player(view, gap);
  RPCMP_CHECK(suite, gap.contains("History gap / earlier rows omitted"));
  for (const auto available :
       {api::PerformanceAvailability::Waiting, api::PerformanceAvailability::Invalid,
        api::PerformanceAvailability::Exhausted}) {
    view.tracker.availability = available;
    Canvas state(suite);
    render_player(view, state);
    RPCMP_CHECK(suite, state.contains(available == api::PerformanceAvailability::Waiting
                                          ? "Waiting for performance data"
                                      : available == api::PerformanceAvailability::Invalid
                                          ? "Performance data unavailable"
                                          : "History full / current keys continue"));
    if (available != api::PerformanceAvailability::Exhausted)
      RPCMP_CHECK(suite, !state.contains("C-4 2A"));
  }
  view = fixture();
  view.snapshot.capabilities.bits |= api::kPlaybackSettings;
  view.snapshot.settings =
      api::PlaybackSettingsObservation{1, std::nullopt, api::SettingsRestore::Missing,
                                       api::SettingsSave::Failed, api::SettingsError::Io};
  Canvas save(suite);
  render_player(view, save);
  RPCMP_CHECK(suite, save.contains("FAIL") && save.contains("Settings could not be saved") &&
                         save.contains("PLAYING"));
  view.diagnostic = Diagnostic::CommandRejected;
  view.rejection = api::CommandReason::StaleLibrary;
  Canvas rejected(suite);
  render_player(view, rejected);
  RPCMP_CHECK(suite, rejected.contains("Library changed; select again"));
  view.snapshot.transport = view.snapshot.projected = c::TransportState::Error;
  view.snapshot.error = api::PlaybackError{api::PlaybackErrorCode::ResetFailed, true};
  Canvas reset(suite);
  render_player(view, reset);
  RPCMP_CHECK(suite, reset.contains("Sound reset failed; restart the player"));
  view.snapshot.error->code = api::PlaybackErrorCode::Protocol;
  Canvas protocol(suite);
  render_player(view, protocol);
  RPCMP_CHECK(suite, protocol.contains("Playback protocol error; restart the player"));
  view.valid_snapshot = false;
  Canvas invalid(suite);
  render_player(view, invalid);
  RPCMP_CHECK(suite, invalid.contains("Invalid player state") && invalid.contains("--:--") &&
                         !invalid.contains("Current title") && !invalid.contains("00:02"));
}
std::string svg(const PlayerView& view) {
  std::ostringstream output;
  rpcmp::platform::host::SvgCanvas canvas(output);
  render_player(view, canvas);
  if (!canvas.finish())
    throw std::logic_error("mock rendering failed");
  return output.str();
}
void svg_contract(rpcmp::test::Suite& suite) {
  const auto view = fixture();
  const auto first = svg(view);
  RPCMP_CHECK(suite, first == svg(view));
  std::ostringstream output;
  rpcmp::platform::host::SvgCanvas canvas(output);
  canvas.text({0, 0, 32, 16}, "ABCDE", palette::text);
  canvas.text({0, 20, 48, 16}, "あいうえ", palette::text);
  canvas.text({0, 40, 64, 16}, "AB", palette::text, true);
  canvas.text({0, 60, 160, 16}, "A<&>\"'あ", palette::text);
  canvas.text({0, 80, 64, 16},
              "A\x1B"
              "B",
              palette::text);
  canvas.text({0, 100, 8, 16}, "ｶ", palette::text);
  canvas.text({0, 120, 64, 16},
              "X\xEF\xBF\xBE\xEF\xBF\xBF"
              "Y",
              palette::text);
  RPCMP_CHECK(suite, canvas.finish());
  const auto xml = output.str();
  RPCMP_CHECK(suite, xml.find(">AB…</text>") != std::string::npos &&
                         xml.find(">あい…</text>") != std::string::npos);
  RPCMP_CHECK(suite, xml.find("A&lt;&amp;&gt;&quot;&apos;あ") != std::string::npos &&
                         xml.find("A�B") != std::string::npos);
  RPCMP_CHECK(suite, xml.find(">ｶ</text>") != std::string::npos);
  RPCMP_CHECK(suite, xml.find("X��Y") != std::string::npos &&
                         xml.find("\xEF\xBF\xBE") == std::string::npos &&
                         xml.find("\xEF\xBF\xBF") == std::string::npos);
  const auto closed = output.str();
  RPCMP_CHECK(suite, canvas.finish() && output.str() == closed);
  canvas.fill({0, 0, 1, 1}, palette::text);
  RPCMP_CHECK(suite, !canvas.finish());
  for (unsigned failure = 0; failure < 5; ++failure) {
    std::ostringstream bad_output;
    rpcmp::platform::host::SvgCanvas bad(bad_output);
    switch (failure) {
    case 0:
      bad.text({0, 0, 64, 16}, "\xC3(", palette::text);
      break;
    case 1:
      bad.text({0, 0, 64, 16}, std::string(97, 'x'), palette::text);
      break;
    case 2:
      bad.fill({639, 0, 2, 1}, palette::text);
      break;
    case 3:
      bad.line({0, 0}, {640, 0}, palette::text);
      break;
    case 4:
      bad.fill({0, 0, 1, 1}, 0x1000000);
      break;
    default:
      throw std::logic_error("unknown negative canvas case");
    }
    RPCMP_CHECK(suite, !bad.finish());
  }
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  model_rendering(suite);
  missing_and_failures(suite);
  svg_contract(suite);
  return suite.finish("Player canvas and SVG rendering");
}
