#include "rpcmp/ui/player_canvas.hpp"

namespace rpcmp::ui::v2 {
namespace {
using contracts::TransportState;
struct Label {
  std::array<char, 32> bytes{};
  std::size_t size{};
  void append(const std::string_view value) noexcept {
    for (const auto ch : value)
      if (size < bytes.size())
        bytes[size++] = ch;
  }
  void number(std::uint64_t value, const unsigned base = 10, const unsigned minimum = 1) noexcept {
    std::array<char, 20> digits{};
    std::size_t count = 0;
    do {
      digits[count++] = "0123456789ABCDEF"[value % base];
      value /= base;
    } while (value != 0);
    while (count < minimum)
      digits[count++] = '0';
    while (count != 0 && size < bytes.size())
      bytes[size++] = digits[--count];
  }
  [[nodiscard]] std::string_view value() const noexcept { return {bytes.data(), size}; }
};
Label note(const std::optional<std::uint8_t> value) noexcept {
  Label result;
  if (!value) {
    result.append("--");
    return result;
  }
  constexpr std::array<std::string_view, 12> names{"C-", "C#", "D-", "D#", "E-", "F-",
                                                   "F#", "G-", "G#", "A-", "A#", "B-"};
  result.append(names[*value % 12]);
  if (*value < 12)
    result.append("-1");
  else
    result.number(*value / 12U - 1U);
  return result;
}
void voice(Label& label, const api::PerformanceChannel& channel) noexcept {
  if (channel.mdx_fm && channel.mdx_fm->voice_number)
    label.number(*channel.mdx_fm->voice_number, 16, 2);
  else
    label.append("--");
}
void border(PlayerCanvas& canvas, const Box box, const Color color) {
  const auto right = static_cast<std::uint16_t>(box.x + box.width - 1);
  const auto bottom = static_cast<std::uint16_t>(box.y + box.height - 1);
  canvas.line({box.x, box.y}, {right, box.y}, color);
  canvas.line({right, box.y}, {right, bottom}, color);
  canvas.line({right, bottom}, {box.x, bottom}, color);
  canvas.line({box.x, bottom}, {box.x, box.y}, color);
}
void metadata(PlayerCanvas& canvas, const Box box, const contracts::CatalogText& value,
              const Color color) {
  canvas.text(box, value.length == 0 ? "--" : std::string_view{value.bytes.data(), value.length},
              color, value.truncated);
}
std::string_view view_name(const View view) noexcept {
  switch (view) {
  case View::Tracker:
    return "TRACKER";
  case View::Keyboard:
    return "KEYBOARD";
  case View::Library:
    return "LIBRARY";
  }
  return "--";
}
std::string_view transport_name(const TransportState state) noexcept {
  switch (state) {
  case TransportState::Empty:
    return "EMPTY";
  case TransportState::Loading:
    return "LOADING";
  case TransportState::Stopped:
    return "STOPPED";
  case TransportState::Playing:
    return "PLAYING";
  case TransportState::Paused:
    return "PAUSED";
  case TransportState::Ended:
    return "ENDED";
  case TransportState::Error:
    return "ERROR";
  }
  return "--";
}
std::string_view rejection_name(const api::CommandReason reason) noexcept {
  switch (reason) {
  case api::CommandReason::ResourceBusy:
    return "Player is busy";
  case api::CommandReason::QueueFull:
    return "Player queue is full";
  case api::CommandReason::UnsupportedCapability:
    return "Control is unavailable";
  case api::CommandReason::LibraryUnavailable:
    return "Library is unavailable";
  case api::CommandReason::StaleLibrary:
    return "Library changed; select again";
  case api::CommandReason::UnknownTrack:
    return "Track is unavailable";
  case api::CommandReason::StaleSnapshotSequence:
    return "Player changed; select again";
  case api::CommandReason::NoNextTrack:
    return "No next track";
  case api::CommandReason::NoPreviousTrack:
    return "No previous track";
  case api::CommandReason::TerminalFailure:
    return "Player must be restarted";
  default:
    return "Command was rejected";
  }
}
std::string_view diagnostic_name(const PlayerView& view) noexcept {
  switch (view.diagnostic) {
  case Diagnostic::None:
    return {};
  case Diagnostic::InvalidSnapshot:
    return "Invalid player state";
  case Diagnostic::InvalidInput:
    return "Invalid button assignment";
  case Diagnostic::InvalidFocus:
    return "Invalid focus assignment";
  case Diagnostic::Unsupported:
    return "Control is unavailable";
  case Diagnostic::Busy:
    return "Waiting for player";
  case Diagnostic::NoSelection:
    return "No available track or action";
  case Diagnostic::CommandRejected:
    return rejection_name(view.rejection);
  case Diagnostic::CommandFailed:
    return "Command could not complete";
  case Diagnostic::Protocol:
    return "Unexpected player response";
  case Diagnostic::CatalogFailure:
    return "Library list is unavailable";
  case Diagnostic::CommandExhausted:
    return "Controller must be restarted";
  case Diagnostic::PolicyQueueFull:
    return "Settings queue is full";
  }
  return "Unknown UI state";
}
std::string_view playback_error(const api::PlaybackError& error) noexcept {
  if (error.terminal) {
    switch (error.code) {
    case api::PlaybackErrorCode::ResetFailed:
      return "Sound reset failed; restart the player";
    case api::PlaybackErrorCode::Protocol:
      return "Playback protocol error; restart the player";
    default:
      return "Playback failed; restart the player";
    }
  }
  switch (error.code) {
  case api::PlaybackErrorCode::Library:
    return "Library could not be read";
  case api::PlaybackErrorCode::Preparation:
  case api::PlaybackErrorCode::PreparationTimeout:
    return "Track could not be loaded";
  case api::PlaybackErrorCode::AudioTimeout:
    return "Audio did not respond";
  default:
    return "Playback failed; select a track to retry";
  }
}
std::string_view history_status(const PlayerView& view) noexcept {
  if (!view.tracker.supported)
    return "Performance display unavailable";
  switch (view.tracker.availability) {
  case api::PerformanceAvailability::Waiting:
    return "Waiting for performance data";
  case api::PerformanceAvailability::Invalid:
    return "Performance data unavailable";
  case api::PerformanceAvailability::Exhausted:
    return "History full / current keys continue";
  default:
    break;
  }
  if (view.tracker.gap && view.tracker.retained_earlier)
    return "History gap / earlier rows omitted";
  if (view.tracker.gap)
    return "History gap";
  if (view.tracker.retained_earlier)
    return "Earlier rows omitted";
  if (view.snapshot.transport == TransportState::Paused)
    return "PAUSED / held channel state";
  return "";
}
bool captured(const PlayerView& view) noexcept {
  return view.tracker.supported &&
         (view.tracker.availability == api::PerformanceAvailability::Available ||
          view.tracker.availability == api::PerformanceAvailability::Degraded ||
          view.tracker.availability == api::PerformanceAvailability::Exhausted);
}

void tracker(const PlayerView& view, PlayerCanvas& canvas) {
  canvas.text({14, 34, 48, 16}, "EVENT", palette::muted);
  for (std::uint16_t channel = 0; channel < api::kPerformanceChannels; ++channel) {
    const auto x = static_cast<std::uint16_t>(64 + channel * 71);
    Label title;
    title.append("FM ");
    title.number(channel + 1U);
    canvas.text({static_cast<std::uint16_t>(x + 4), 34, 64, 16}, title.value(), palette::accent);
    canvas.line({x, 30}, {x, 342}, palette::grid);
  }
  if (captured(view)) {
    for (std::uint16_t i = 0; i < view.tracker.count; ++i) {
      const auto& row = view.tracker.rows[i];
      const auto y = static_cast<std::uint16_t>(54 + i * 18);
      const bool latest = i + 1 == view.tracker.count;
      if (latest)
        canvas.fill({9, y, 622, 18}, palette::selected);
      Label sequence;
      sequence.number(row.first_sequence, 16, 4);
      canvas.text({14, y, 46, 16}, sequence.value(), palette::muted);
      for (std::uint16_t channel = 0; channel < api::kPerformanceChannels; ++channel) {
        const auto& cell = row.cells[channel];
        if (!cell)
          continue;
        Label label;
        if (cell->kind == api::PerformanceKind::PitchChanged)
          label.append("~");
        else if (cell->kind == api::PerformanceKind::InstrumentChanged)
          label.append("@");
        if (cell->kind == api::PerformanceKind::KeyOff)
          label.append("OFF");
        else
          label.append(note(cell->channel.note).value());
        label.append(" ");
        voice(label, cell->channel);
        canvas.text({static_cast<std::uint16_t>(68 + channel * 71), y, 64, 16}, label.value(),
                    latest ? palette::accent : palette::text);
      }
    }
  }
  canvas.text({16, 344, 608, 16}, history_status(view), palette::muted);
}

bool black_key(const unsigned note_value) noexcept {
  const auto pitch = note_value % 12;
  return pitch == 1 || pitch == 3 || pitch == 6 || pitch == 8 || pitch == 10;
}
void keyboard(const PlayerView& view, PlayerCanvas& canvas) {
  canvas.text({14, 34, 80, 16}, "CHANNEL", palette::muted);
  for (std::uint16_t octave = 0; octave < 11; ++octave) {
    Label label;
    label.append("C");
    if (octave == 0)
      label.append("-1");
    else
      label.number(octave - 1U);
    canvas.text({static_cast<std::uint16_t>(100 + octave * 49), 34, 32, 16}, label.value(),
                palette::muted);
  }
  const bool observed = captured(view) && view.snapshot.performance_history.has_value();
  for (std::uint16_t channel = 0; channel < api::kPerformanceChannels; ++channel) {
    const auto y = static_cast<std::uint16_t>(54 + channel * 36);
    api::PerformanceChannel current;
    if (observed)
      current = view.snapshot.performance_history->channels[channel];
    const bool sounding = current.key_on.value_or(false);
    const auto highlight =
        view.snapshot.transport == TransportState::Paused ? palette::warning : palette::accent;
    Label name;
    name.append("FM");
    name.number(channel + 1U);
    canvas.text({14, y, 32, 16}, name.value(), sounding ? highlight : palette::muted);
    const auto current_note = note(current.note);
    canvas.text({48, y, 44, 16},
                !current.key_on   ? "--"
                : *current.key_on ? current_note.value()
                                  : "OFF",
                palette::text);
    Label instrument;
    voice(instrument, current);
    canvas.text({48, static_cast<std::uint16_t>(y + 16), 44, 16}, instrument.value(),
                palette::muted);
    unsigned white = 0;
    for (unsigned key = 0; key < 128; ++key) {
      if (black_key(key))
        continue;
      const auto x = static_cast<std::uint16_t>(100 + white++ * 7);
      const bool active = sounding && current.note && *current.note == key;
      canvas.fill({x, y, 7, 26}, active ? highlight : palette::white_key);
      canvas.line({static_cast<std::uint16_t>(x + 6), y},
                  {static_cast<std::uint16_t>(x + 6), static_cast<std::uint16_t>(y + 25)},
                  palette::grid);
    }
    white = 0;
    for (unsigned key = 0; key < 128; ++key) {
      if (!black_key(key)) {
        ++white;
        continue;
      }
      const auto x = static_cast<std::uint16_t>(100 + white * 7 - 2);
      const bool active = sounding && current.note && *current.note == key;
      canvas.fill({x, y, 4, 16}, active ? highlight : palette::black_key);
    }
  }
  canvas.text({16, 344, 608, 16}, history_status(view), palette::muted);
}

void library(const PlayerView& view, PlayerCanvas& canvas) {
  const auto& browser = view.browser;
  if (!browser.valid) {
    canvas.text({16, 34, 440, 16}, "ALBUMS", palette::accent);
    canvas.text({24, 70, 580, 16},
                view.snapshot.library.phase == contracts::CatalogPhase::Empty
                    ? "No library loaded"
                    : "Library list unavailable",
                palette::muted);
    return;
  }
  const bool albums = browser.level == BrowseLevel::Albums;
  if (albums || !browser.album)
    canvas.text({16, 34, 480, 16}, "ALBUMS", palette::accent);
  else
    metadata(canvas, {16, 34, 480, 16}, browser.album->name, palette::accent);
  const auto& header = albums ? browser.albums.header : browser.tracks.header;
  Label position;
  position.number(browser.cursor + 1U);
  position.append(" / ");
  position.number(header.total);
  canvas.text({520, 34, 104, 16}, position.value(), palette::muted);
  for (std::uint16_t i = 0; i < header.count; ++i) {
    const auto ordinal = header.query.start_ordinal + i;
    const auto y = static_cast<std::uint16_t>(54 + i * 18);
    const bool selected = ordinal == browser.cursor;
    if (selected)
      canvas.fill({9, y, 622, 18}, palette::selected);
    Label number;
    number.number(ordinal + 1U, 10, 3);
    canvas.text({16, y, 32, 16}, number.value(), palette::muted);
    const auto& title = albums ? browser.albums.items[i].name : browser.tracks.items[i].title;
    metadata(canvas, {60, y, 508, 16}, title, selected ? palette::accent : palette::text);
    if (albums) {
      Label count;
      count.number(browser.albums.items[i].track_count);
      canvas.text({580, y, 36, 16}, count.value(), palette::muted);
    } else if (view.snapshot.track &&
               browser.tracks.items[i].track_id == view.snapshot.track->item.track_id) {
      canvas.fill({612, static_cast<std::uint16_t>(y + 4), 6, 6}, palette::accent);
    }
  }
  canvas.text({16, 344, 608, 16}, albums ? "A: open album" : "A: play selected track",
              palette::muted);
}

struct Control {
  std::string_view hint;
  bool enabled{};
};
Control control(const PlayerView& view, Focus focus) noexcept {
  if (focus == Focus::ViewSwitch)
    return {"A: switch display", true};
  if (focus == Focus::Main && view.main == View::Library)
    return {view.browser.level == BrowseLevel::Albums
                ? "A: open album / Right: controls"
                : "A: play track / B: albums / Right: controls",
            view.browser.valid};
  if (focus == Focus::Main)
    focus = Focus::PlayPause;
  const auto& snapshot = view.snapshot;
  if (!view.valid_snapshot || (snapshot.capabilities.bits & api::kTransportCommands) == 0)
    return {"Player controls unavailable", false};
  if (snapshot.error && snapshot.error->terminal)
    return {"Player must be restarted", false};
  if (focus != Focus::Stop && snapshot.settings &&
      snapshot.settings->restore == api::SettingsRestore::Reading)
    return {"Restoring settings", false};
  switch (focus) {
  case Focus::PlayPause:
    if (!snapshot.track || snapshot.error)
      return {"Select a track to play", false};
    if (view.transport_pending || snapshot.pending_intent || snapshot.preparation ||
        snapshot.audio_control)
      return {"Waiting for player", false};
    if (snapshot.transport == TransportState::Playing ||
        snapshot.transport == TransportState::Paused)
      return {snapshot.transport == TransportState::Playing ? "A: pause" : "A: resume",
              (snapshot.capabilities.bits & api::kStatePreservingPause) != 0};
    return {"A: play", snapshot.transport == TransportState::Stopped ||
                           snapshot.transport == TransportState::Ended};
  case Focus::Stop:
    return {"A: stop", snapshot.track.has_value() && !snapshot.error};
  case Focus::Previous:
    return {"A: previous track", snapshot.navigation && snapshot.navigation->can_previous};
  case Focus::Next:
    return {"A: next track", snapshot.navigation && snapshot.navigation->can_next};
  case Focus::Repeat:
  case Focus::Shuffle: {
    const auto needed = api::kRepeatControl | api::kPolicyCommandResults;
    return {focus == Focus::Repeat ? "A: 2 / 3 / 5 / infinite" : "A: toggle whole-library shuffle",
            (snapshot.capabilities.bits & needed) == needed &&
                (focus != Focus::Shuffle || snapshot.navigation)};
  }
  default:
    return {"", false};
  }
}
void triangle(PlayerCanvas& canvas, const std::uint16_t x, const std::uint16_t y, const bool right,
              const Color color) {
  for (std::uint16_t i = 0; i < 9; ++i) {
    const auto offset = right ? i : 8 - i;
    const auto xx = static_cast<std::uint16_t>(x + offset);
    canvas.line({xx, static_cast<std::uint16_t>(y + i / 2)},
                {xx, static_cast<std::uint16_t>(y + 8 - i / 2)}, color);
  }
}
void icon(const PlayerView& view, PlayerCanvas& canvas, const Focus focus, const Box box) {
  const auto color = control(view, focus).enabled ? palette::text : palette::muted;
  canvas.fill(box, view.focus == focus ? palette::selected : palette::panel);
  border(canvas, box, view.focus == focus ? palette::accent : palette::grid);
  const auto x = static_cast<std::uint16_t>(box.x + 16);
  const auto y = static_cast<std::uint16_t>(box.y + 6);
  switch (focus) {
  case Focus::Previous:
    canvas.fill({static_cast<std::uint16_t>(x - 3), y, 2, 9}, color);
    triangle(canvas, x, y, false, color);
    break;
  case Focus::Next:
    triangle(canvas, x, y, true, color);
    canvas.fill({static_cast<std::uint16_t>(x + 11), y, 2, 9}, color);
    break;
  case Focus::PlayPause:
    if (view.valid_snapshot && view.snapshot.transport == TransportState::Playing) {
      canvas.fill({x, y, 3, 9}, color);
      canvas.fill({static_cast<std::uint16_t>(x + 6), y, 3, 9}, color);
    } else
      triangle(canvas, x, y, true, color);
    break;
  case Focus::Stop:
    canvas.fill({x, y, 9, 9}, color);
    break;
  case Focus::ViewSwitch:
    border(canvas, {static_cast<std::uint16_t>(x - 2), static_cast<std::uint16_t>(y - 1), 14, 11},
           color);
    canvas.line({x, static_cast<std::uint16_t>(y + 3)},
                {static_cast<std::uint16_t>(x + 9), static_cast<std::uint16_t>(y + 3)}, color);
    canvas.line({x, static_cast<std::uint16_t>(y + 6)},
                {static_cast<std::uint16_t>(x + 9), static_cast<std::uint16_t>(y + 6)}, color);
    break;
  case Focus::Repeat: {
    Label count;
    if (view.valid_snapshot && view.snapshot.policy) {
      const auto& policy = view.snapshot.policy->desired;
      if (policy.repeat == api::RepeatMode::RepeatOne)
        count.append("∞");
      else if (policy.repeat == api::RepeatMode::Default)
        count.number(2);
      else if (policy.count)
        count.number(*policy.count);
      else
        count.append("--");
    } else
      count.append("--");
    canvas.text(
        {static_cast<std::uint16_t>(box.x + 12), static_cast<std::uint16_t>(box.y + 3), 30, 16},
        count.value(), color);
    canvas.line({static_cast<std::uint16_t>(box.x + 4), static_cast<std::uint16_t>(box.y + 6)},
                {static_cast<std::uint16_t>(box.x + 4), static_cast<std::uint16_t>(box.y + 15)},
                color);
    canvas.line({static_cast<std::uint16_t>(box.x + 4), static_cast<std::uint16_t>(box.y + 6)},
                {static_cast<std::uint16_t>(box.x + 8), static_cast<std::uint16_t>(box.y + 3)},
                color);
    break;
  }
  case Focus::Shuffle: {
    const auto active = view.valid_snapshot && view.snapshot.policy &&
                        view.snapshot.policy->desired.order == api::PlaybackOrder::ShuffleLibrary;
    const auto ink = active ? palette::accent : color;
    canvas.line({static_cast<std::uint16_t>(x - 3), y},
                {static_cast<std::uint16_t>(x + 12), static_cast<std::uint16_t>(y + 9)}, ink);
    canvas.line({static_cast<std::uint16_t>(x - 3), static_cast<std::uint16_t>(y + 9)},
                {static_cast<std::uint16_t>(x + 12), y}, ink);
    canvas.line({static_cast<std::uint16_t>(x + 8), y}, {static_cast<std::uint16_t>(x + 12), y},
                ink);
    canvas.line({static_cast<std::uint16_t>(x + 12), y},
                {static_cast<std::uint16_t>(x + 12), static_cast<std::uint16_t>(y + 4)}, ink);
    break;
  }
  case Focus::List:
    break;
  }
}
std::string_view save_name(const api::PlaybackSettingsObservation& settings) noexcept {
  if (settings.restore == api::SettingsRestore::Reading)
    return "READ";
  switch (settings.save) {
  case api::SettingsSave::Saved:
    return "SAVED";
  case api::SettingsSave::Pending:
  case api::SettingsSave::Writing:
    return "...";
  case api::SettingsSave::Failed:
    return "FAIL";
  case api::SettingsSave::Unavailable:
    return "N/A";
  case api::SettingsSave::NotSaved:
    return "--";
  }
  return "--";
}
void information(const PlayerView& view, PlayerCanvas& canvas) {
  canvas.fill({8, 368, 624, 80}, palette::panel);
  border(canvas, {8, 368, 624, 80}, palette::grid);
  canvas.line({376, 376}, {376, 440}, palette::grid);
  canvas.text({16, 372, 344, 16}, "TRACK", palette::muted);
  canvas.text({16, 420, 48, 16}, "ALBUM", palette::muted);
  if (view.valid_snapshot && view.snapshot.track) {
    metadata(canvas, {16, 392, 344, 20}, view.snapshot.track->item.title, palette::accent);
    metadata(canvas, {72, 420, 288, 16}, view.snapshot.track->album_name, palette::text);
  } else {
    canvas.text({16, 392, 344, 20}, view.valid_snapshot ? "No track selected" : "--",
                palette::muted);
    canvas.text({72, 420, 288, 16}, "--", palette::muted);
  }
  constexpr std::array<Focus, 4> top{Focus::Previous, Focus::PlayPause, Focus::Stop, Focus::Next};
  constexpr std::array<Focus, 3> bottom{Focus::ViewSwitch, Focus::Repeat, Focus::Shuffle};
  for (std::size_t i = 0; i < top.size(); ++i)
    icon(view, canvas, top[i], {static_cast<std::uint16_t>(390 + i * 60), 376, 44, 22});
  for (std::size_t i = 0; i < bottom.size(); ++i)
    icon(view, canvas, bottom[i], {static_cast<std::uint16_t>(390 + i * 60), 402, 44, 22});
  const auto& settings = view.snapshot.settings;
  canvas.text({570, 405, 48, 16}, view.valid_snapshot && settings ? save_name(*settings) : "--",
              settings && settings->error ? palette::warning : palette::muted);
  Label elapsed;
  if (view.valid_snapshot) {
    const auto seconds = view.snapshot.position_frames / view.snapshot.frame_rate;
    if (seconds >= 3600) {
      elapsed.number(seconds / 3600);
      elapsed.append(":");
    }
    elapsed.number(seconds / 60 % 60, 10, 2);
    elapsed.append(":");
    elapsed.number(seconds % 60, 10, 2);
  } else
    elapsed.append("--:--");
  canvas.text({390, 430, 76, 16},
              view.valid_snapshot ? transport_name(view.snapshot.transport) : "--", palette::text);
  canvas.text({466, 430, 152, 16}, elapsed.value(), palette::accent);
}
void status(const PlayerView& view, PlayerCanvas& canvas) {
  canvas.line({8, 456}, {631, 456}, palette::grid);
  auto message = view.valid_snapshot ? diagnostic_name(view) : "Invalid player state";
  if (view.valid_snapshot && view.snapshot.error)
    message = playback_error(*view.snapshot.error);
  auto color = message.empty() ? palette::muted : palette::warning;
  if (message.empty() && view.snapshot.settings && view.snapshot.settings->error) {
    message = "Settings could not be saved";
    color = palette::warning;
  }
  if (message.empty())
    message = control(view, view.focus).hint;
  canvas.text({16, 460, 608, 16}, message, color);
}
} // namespace

void render_player(const PlayerView& view, PlayerCanvas& canvas) {
  canvas.fill({0, 0, kCanvasWidth, kCanvasHeight}, palette::background);
  canvas.fill({0, 0, kCanvasWidth, 24}, palette::panel);
  canvas.text({12, 4, 300, 16}, "RPCMP / FM PLAYER", palette::accent);
  canvas.text({528, 4, 100, 16}, view_name(view.main), palette::text);
  border(canvas, {8, 28, 624, 332}, view.focus == Focus::List ? palette::accent : palette::grid);
  if (!view.valid_snapshot) {
    canvas.text({24, 52, 584, 16}, "Invalid player state", palette::warning);
  } else {
    switch (view.main) {
    case View::Tracker:
      tracker(view, canvas);
      break;
    case View::Keyboard:
      keyboard(view, canvas);
      break;
    case View::Library:
      library(view, canvas);
      break;
    }
  }
  information(view, canvas);
  status(view, canvas);
}

} // namespace rpcmp::ui::v2
