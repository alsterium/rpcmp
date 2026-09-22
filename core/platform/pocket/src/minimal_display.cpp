#include "rpcmp/platform/pocket/minimal_display.hpp"

#include "rpcmp/platform/pocket/bitmap_font.hpp"

#include <algorithm>
#include <cstdio>
#include <string_view>

namespace rpcmp::platform::pocket {
namespace {
namespace ui = rpcmp::ui::minimal;
using contracts::minimal::State;
std::string_view title(const contracts::CatalogText& value) {
  return {value.bytes.data(), std::min<std::size_t>(value.length, value.bytes.size())};
}
BitmapGlyph next_glyph(const std::string_view text, std::size_t& pos) {
  const auto first = static_cast<std::uint8_t>(text[pos++]);
  const unsigned count = first < 128 ? 1 : first < 224 ? 2 : first < 240 ? 3 : 4;
  if (count - 1 > text.size() - pos) {
    pos = text.size();
    return bitmap_glyph(0xfffd);
  }
  std::uint32_t scalar = first & (count == 1 ? 127U : count == 2 ? 31U : count == 3 ? 15U : 7U);
  for (unsigned i = 1; i < count; ++i)
    scalar = (scalar << 6U) | (static_cast<std::uint8_t>(text[pos++]) & 63U);
  return bitmap_glyph(scalar);
}
std::uint32_t width(const std::string_view text) {
  std::uint32_t result = 0;
  for (std::size_t pos = 0; pos < text.size();)
    result += next_glyph(text, pos).width;
  return result;
}
std::uint32_t scroll_offset(const std::string_view text, const unsigned available,
                            const std::uint32_t tick) {
  const auto pixels = width(text);
  if (pixels <= available)
    return 0;
  const auto distance = pixels - available;
  const auto travel_ticks = (distance * 5 + 7) / 8;
  const auto phase = tick % (40 + travel_ticks);
  return phase < 20 ? 0 : std::min(distance, (phase - 20) * 8 / 5);
}
void text_row(std::uint8_t* out, const std::string_view text, const unsigned row,
              const unsigned left, const unsigned right, const std::uint8_t color,
              const unsigned offset = 0, const bool elide = true) {
  int x = static_cast<int>(left) - static_cast<int>(offset);
  const bool needs_elision = elide && width(text) > right - left;
  for (std::size_t pos = 0; pos < text.size();) {
    auto glyph = next_glyph(text, pos);
    bool last = false;
    if (needs_elision &&
        (x + static_cast<int>(glyph.width) > static_cast<int>(right) ||
         (pos < text.size() && x + static_cast<int>(glyph.width) + 16 > static_cast<int>(right)))) {
      glyph = bitmap_glyph(0x2026);
      last = true;
    }
    for (unsigned bit = 0; bit < glyph.width; ++bit) {
      const int pixel = x + static_cast<int>(bit);
      if (pixel >= static_cast<int>(left) && pixel < static_cast<int>(right) &&
          (glyph.rows[row * (glyph.width / 8) + bit / 8] & (128U >> (bit % 8))) != 0)
        out[pixel] = color;
    }
    x += static_cast<int>(glyph.width);
    if (last || x >= static_cast<int>(right))
      break;
  }
}
void frame(std::uint8_t* out, unsigned row, unsigned x, unsigned y, unsigned w, unsigned h,
           std::uint8_t color, std::uint8_t fill = 0) {
  if (row < y || row >= y + h)
    return;
  std::fill_n(out + x, w, fill);
  if (row == y || row == y + h - 1)
    std::fill_n(out + x, w, color);
  else {
    out[x] = color;
    out[x + w - 1] = color;
  }
}
void triangle(std::uint8_t* out, unsigned row, unsigned x, unsigned y, bool right,
              std::uint8_t color) {
  if (row < y || row >= y + 14)
    return;
  const auto half = row - y < 7 ? row - y : 13 - (row - y);
  const auto start = right ? x : x + 7 - half;
  std::fill_n(out + start, half + 1, color);
}
void loop_row(std::uint8_t* out, unsigned row, unsigned x, unsigned y, std::uint8_t color) {
  if (row < y || row >= y + 24)
    return;
  const auto local = row - y;
  const bool bottom = local >= 12;
  const auto r = bottom ? 23 - local : local;
  if (r > 10)
    return;
  for (unsigned p = 0; p < 24; ++p) {
    const bool head = p >= 17 && p < 17 + (r < 6 ? r + 1 : 11 - r);
    const bool stem = r >= 4 && r <= 6 && p >= 3 && p < 18;
    const bool bend = (r == 5 && p >= 1 && p < 3) || (r >= 6 && p < 3);
    if (head || stem || bend)
      out[x + (bottom ? 23 - p : p)] = color;
  }
}
void icon_row(std::uint8_t* out, unsigned row, const ui::View& view, unsigned index) {
  const auto icon = static_cast<ui::Icon>(index);
  const unsigned x = 450 + (index < 3 ? index : index - 3) * 58;
  const unsigned y = index < 3 ? 378 : 412;
  const unsigned w = icon == ui::Icon::Repeat ? 108 : 50;
  const bool active = view.panel == ui::Panel::Controls;
  const bool selected = view.icon == icon;
  const auto color = static_cast<std::uint8_t>(view.enabled[index] ? 1 : 5);
  frame(out, row, x, y, w, 28, selected && active ? 3 : 5, selected ? (active ? 2 : 6) : 0);
  if (icon == ui::Icon::Repeat) {
    loop_row(out, row, x + 10, y + 2, color);
    return;
  }
  if (row < y + 7 || row >= y + 21)
    return;
  if (icon == ui::Icon::Previous || icon == ui::Icon::Next) {
    const bool next = icon == ui::Icon::Next;
    triangle(out, row, x + 21, y + 7, next, color);
    std::fill_n(out + x + (next ? 30 : 17), 2, color);
  } else if (icon == ui::Icon::PlayPause) {
    if (view.playback.state == State::Playing) {
      std::fill_n(out + x + 18, 4, color);
      std::fill_n(out + x + 27, 4, color);
    } else {
      triangle(out, row, x + 21, y + 7, true, color);
    }
  } else if (icon == ui::Icon::Stop) {
    std::fill_n(out + x + 19, 12, color);
  }
}
} // namespace
bool MinimalDisplay::marquee_needed(const ui::View& view) {
  return (view.panel == ui::Panel::List && view.selected_row < view.rows &&
          view.selected_row < view.titles.size() &&
          width(title(view.titles[view.selected_row])) > 536) ||
         (view.playing_number != 0 && width(title(view.playing_title)) > 400) ||
         width(title(view.playing_list)) > 400;
}
void MinimalDisplay::begin(const ui::View view, const MinimalTimings timings) {
  view_ = view;
  timings_ = timings;
  row_ = scroll_pixels_ = 0;
  if (view_.panel == ui::Panel::List && view_.selected_row < view_.rows &&
      view_.selected_row < view_.titles.size())
    scroll_pixels_ = scroll_offset(title(view_.titles[view_.selected_row]), 536, view_.scroll_tick);
  title_scroll_ = view_.playing_number != 0
                      ? scroll_offset(title(view_.playing_title), 400, view_.info_scroll_tick)
                      : 0;
  list_scroll_ = scroll_offset(title(view_.playing_list), 400, view_.info_scroll_tick);
}
bool MinimalDisplay::pump(std::uint8_t* surface, const std::size_t bytes) {
  if (!surface || bytes < std::size_t{640} * 480)
    return false;
  for (unsigned slice = 0; slice < 4 && row_ < 480; ++slice, ++row_) {
    auto* out = surface + static_cast<std::size_t>(row_) * 640;
    std::fill_n(out, 640, std::uint8_t{0});
    const bool list_focus = view_.panel == ui::Panel::List;
    frame(out, row_, 8, 40, 624, 320, list_focus ? 3 : 5);
    frame(out, row_, 8, 370, 424, 76, 5);
    frame(out, row_, 440, 370, 192, 76, list_focus ? 5 : 3);
    if (row_ >= 16 && row_ < 32) {
      text_row(out, "RPCMP", row_ - 16, 16, 120, 3);
      text_row(out, list_focus ? "X:パネル  A:決定  B:戻る" : "X:パネル  A:操作  B:停止", row_ - 16,
               200, 624, 1);
    }
    if (row_ >= 48 && row_ < 64) {
      text_row(out, title(view_.list_title), row_ - 48, 20, 424, 3);
      std::array<char, 48> buffer{};
      std::snprintf(buffer.data(), buffer.size(), "P%lu/%lu  %lu/%lu",
                    static_cast<unsigned long>(view_.count ? view_.first / ui::kRows + 1 : 0),
                    static_cast<unsigned long>((view_.count + ui::kRows - 1) / ui::kRows),
                    static_cast<unsigned long>(view_.count ? view_.selected + 1 : 0),
                    static_cast<unsigned long>(view_.count));
      text_row(out, buffer.data(), row_ - 48, 448, 624, 5);
    }
    if (row_ >= 70 && row_ < 70 + ui::kRows * 22) {
      const auto line = (row_ - 70) / 22;
      const auto within = (row_ - 70) % 22;
      if (line < view_.rows && line < view_.titles.size()) {
        const bool selected = line == view_.selected_row;
        if (selected)
          std::fill_n(out + 14, 612, static_cast<std::uint8_t>(list_focus ? 2 : 6));
        if (view_.playing[line]) {
          if (view_.playback.state == State::Paused && within >= 6 && within < 16) {
            std::fill_n(out + 18, 2, std::uint8_t{7});
            std::fill_n(out + 23, 2, std::uint8_t{7});
          } else if (view_.playback.state == State::Playing) {
            triangle(out, row_, 18, 70 + line * 22 + 4, true, 7);
          }
        }
        if (within >= 3 && within < 19) {
          std::array<char, 16> buffer{};
          std::snprintf(buffer.data(), buffer.size(), "%03lu",
                        static_cast<unsigned long>(view_.first) + line + 1UL);
          text_row(out, buffer.data(), within - 3, 32, 64, 5);
          text_row(out, title(view_.titles[line]), within - 3, 72, 608, 1,
                   selected && list_focus ? scroll_pixels_ : 0, !(selected && list_focus));
        }
      }
    }
    if (row_ >= 378 && row_ < 394)
      text_row(out, view_.playing_number ? title(view_.playing_title) : "曲を選択してください",
               row_ - 378, 20, 420, 1, title_scroll_, false);
    if (row_ >= 402 && row_ < 418)
      text_row(out, title(view_.playing_list), row_ - 402, 20, 420, 5, list_scroll_, false);
    if (row_ >= 426 && row_ < 442) {
      const auto state = view_.playback.state;
      const char* status = state == State::Playing     ? "再生中"
                           : state == State::Paused    ? "一時停止中"
                           : state == State::Advancing ? "次曲へ"
                           : state == State::Ended     ? "再生終了"
                           : state == State::Error     ? "エラー"
                                                       : "停止中";
      const auto seconds = view_.playback.elapsed_seconds;
      std::array<char, 96> buffer{};
      std::snprintf(buffer.data(), buffer.size(), "%lu/%lu  %02llu:%02llu  %s",
                    static_cast<unsigned long>(view_.playing_number),
                    static_cast<unsigned long>(view_.playing_count),
                    static_cast<unsigned long long>(seconds / 60),
                    static_cast<unsigned long long>(seconds % 60), status);
      text_row(out, buffer.data(), row_ - 426, 20, 420, 3);
    }
    for (unsigned i = 0; i < 5; ++i)
      icon_row(out, row_, view_, i);
    if (row_ >= 418 && row_ < 434) {
      using contracts::minimal::RepeatMode;
      const char* mode = view_.playback.repeat == RepeatMode::Two     ? "2周"
                         : view_.playback.repeat == RepeatMode::Three ? "3周"
                         : view_.playback.repeat == RepeatMode::Five  ? "5周"
                                                                      : "無限";
      text_row(out, mode, row_ - 418, 544, 610, view_.enabled[4] ? 1 : 5);
    }
    if (row_ >= 448 && row_ < 464) {
      using contracts::minimal::Error;
      std::array<char, 112> buffer{};
      const auto error = view_.playback.error;
      const char* message;
      if (error == Error::Reset)
        message = "音声リセット失敗: コアを再起動してください";
      else if (view_.playback.state == State::Error) {
        const char* reason = error == Error::Load       ? "読込失敗"
                             : error == Error::Renderer ? "音源処理失敗"
                             : error == Error::Audio    ? "音声出力失敗"
                                                        : "応答待ち失敗";
        std::snprintf(buffer.data(), buffer.size(), "E%u %s: 一覧で曲を選び直してください",
                      static_cast<unsigned>(error), reason);
        message = buffer.data();
      } else if (view_.playback.skipped_count != 0) {
        std::snprintf(buffer.data(), buffer.size(), "読込・再生失敗: %lu曲スキップ  #%lu E%u",
                      static_cast<unsigned long>(view_.playback.skipped_count),
                      static_cast<unsigned long>(view_.playback.skipped_track.value),
                      static_cast<unsigned>(view_.playback.skipped_error));
        message = buffer.data();
      } else if (!list_focus) {
        constexpr std::array<const char*, 5> labels{"前の曲", "再生 / 一時停止", "次の曲", "停止",
                                                    "ループ回数"};
        const auto index = static_cast<std::size_t>(view_.icon);
        std::snprintf(buffer.data(), buffer.size(), "%s%s", labels[index],
                      view_.enabled[index] ? "" : " : 現在は使用できません");
        message = buffer.data();
      } else {
        message = "上下:選択  左右:ページ";
      }
      text_row(out, message, row_ - 448, 16, 624,
               error != Error::None || view_.playback.skipped_count != 0 ? 4 : 5);
    }
    if (row_ >= 464) {
      std::array<char, 96> buffer{};
      std::snprintf(
          buffer.data(), buffer.size(), "R%lu F%lu D%lu V%lu us Q%lu I%lu ms",
          static_cast<unsigned long>(timings_.render), static_cast<unsigned long>(timings_.feed),
          static_cast<unsigned long>(timings_.draw), static_cast<unsigned long>(timings_.flip),
          static_cast<unsigned long>(timings_.queue),
          static_cast<unsigned long>(timings_.catalog_load / 1000));
      text_row(out, buffer.data(), row_ - 464, 16, 624, 5);
    }
  }
  return true;
}
} // namespace rpcmp::platform::pocket
