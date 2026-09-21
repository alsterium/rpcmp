#include "rpcmp/platform/pocket/minimal_display.hpp"

#include "rpcmp/platform/pocket/bitmap_font.hpp"

#include <algorithm>
#include <cstdio>
#include <string_view>

namespace rpcmp::platform::pocket {
namespace {
// Catalog strings have already passed strict UTF-8 validation.
void text_row(std::uint8_t* out, const std::string_view text, const unsigned row,
              const std::uint8_t color) {
  unsigned x = 16;
  for (std::size_t pos = 0; pos < text.size();) {
    const auto first = static_cast<std::uint8_t>(text[pos++]);
    const unsigned count = first < 128 ? 1 : first < 224 ? 2 : first < 240 ? 3 : 4;
    if (count - 1 > text.size() - pos)
      break;
    std::uint32_t scalar = first & (count == 1 ? 127U : count == 2 ? 31U : count == 3 ? 15U : 7U);
    for (unsigned i = 1; i < count; ++i)
      scalar = (scalar << 6U) | (static_cast<std::uint8_t>(text[pos++]) & 63U);
    auto glyph = bitmap_glyph(scalar);
    const bool elide = x + glyph.width > 608;
    if (elide)
      glyph = bitmap_glyph(0x2026);
    for (unsigned bit = 0; bit < glyph.width; ++bit)
      if ((glyph.rows[row * (glyph.width / 8) + bit / 8] & (128U >> (bit % 8))) != 0)
        out[x + bit] = color;
    x += glyph.width;
    if (elide)
      break;
  }
}
std::string_view title(const contracts::CatalogText& value) {
  return {value.bytes.data(), value.length};
}
} // namespace
void MinimalDisplay::begin(const ui::minimal::View view, const MinimalTimings timings) {
  view_ = view;
  timings_ = timings;
  row_ = 0;
}
bool MinimalDisplay::pump(std::uint8_t* surface, const std::size_t bytes) {
  if (!surface || bytes < std::size_t{640} * 480)
    return false;
  for (unsigned slice = 0; slice < 4 && row_ < 480; ++slice, ++row_) {
    auto* out = surface + static_cast<std::size_t>(row_) * 640;
    std::fill_n(out, 640, std::uint8_t{0});
    if (row_ >= 16 && row_ < 32)
      text_row(out, "RPCMP  MDX / PCM", row_ - 16, 3);
    if (row_ >= 48 && row_ < 64)
      text_row(out, "上下:選曲 左右:ページ A:再生 B:停止 X:一時停止/再開", row_ - 48, 1);
    if (row_ >= 80 && row_ < 80 + ui::minimal::kRows * 24) {
      const auto line = (row_ - 80) / 24;
      const auto within = (row_ - 80) % 24;
      const auto index = view_.first + line;
      if (index < view_.count) {
        const bool playing = (view_.playback.state == contracts::minimal::State::Playing ||
                              view_.playback.state == contracts::minimal::State::Paused) &&
                             index + 1 == view_.playback.track.value;
        if (index == view_.selected)
          std::fill_n(out + 8, 624, std::uint8_t{2});
        if (playing && within >= 8 && within < 16)
          std::fill_n(out + 2, 4, std::uint8_t{3});
        if (within >= 4 && within < 20)
          text_row(out, title(view_.titles[line]), within - 4, playing ? 3 : 1);
      }
    }
    if (row_ >= 400 && row_ < 416) {
      using contracts::minimal::State;
      const auto state = view_.playback.state;
      const char* status = state == State::Playing     ? "再生中"
                           : state == State::Paused    ? "一時停止中"
                           : state == State::Advancing ? "次曲へ"
                           : state == State::Ended     ? "再生終了"
                           : view_.playback.error == contracts::minimal::Error::Reset
                               ? "音声リセット失敗: コアを再起動"
                           : state == State::Error ? "エラー: Aで再試行"
                                                   : "停止中";
      std::array<char, 96> buffer{};
      std::snprintf(buffer.data(), buffer.size(), "%s   %lu / %lu", status,
                    static_cast<unsigned long>(view_.playback.track.value != 0
                                                   ? view_.playback.track.value
                                               : view_.count ? view_.selected + 1
                                                             : 0),
                    static_cast<unsigned long>(view_.count));
      if (state == State::Error) {
        std::snprintf(buffer.data(), buffer.size(), "%s  E%u", status,
                      static_cast<unsigned>(view_.playback.error));
      } else if (view_.playback.skipped_count != 0) {
        std::snprintf(buffer.data(), buffer.size(), "%s  SKIP %lu  #%lu E%u", status,
                      static_cast<unsigned long>(view_.playback.skipped_count),
                      static_cast<unsigned long>(view_.playback.skipped_track.value),
                      static_cast<unsigned>(view_.playback.skipped_error));
      }
      text_row(out, buffer.data(), row_ - 400, state == State::Error ? 4 : 3);
    }
    if (row_ >= 432 && row_ < 448)
      text_row(out, title(view_.playing_title), row_ - 432, 1);
    if (row_ >= 456 && row_ < 472) {
      std::array<char, 96> buffer{};
      std::snprintf(
          buffer.data(), buffer.size(), "R%lu F%lu D%lu V%lu us  Q%lu",
          static_cast<unsigned long>(timings_.render), static_cast<unsigned long>(timings_.feed),
          static_cast<unsigned long>(timings_.draw), static_cast<unsigned long>(timings_.flip),
          static_cast<unsigned long>(timings_.queue));
      text_row(out, buffer.data(), row_ - 456, 1);
    }
  }
  return true;
}
} // namespace rpcmp::platform::pocket
