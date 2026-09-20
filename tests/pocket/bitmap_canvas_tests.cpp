#include "rpcmp/platform/pocket/bitmap_canvas.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {
namespace ui = rpcmp::ui::v2;
using namespace rpcmp::platform::pocket;
constexpr std::uint32_t stride = 648;
constexpr std::size_t bytes = static_cast<std::size_t>(stride) * ui::kCanvasHeight;
using Surface = std::vector<std::uint8_t>;
void drain(rpcmp::test::Suite& suite, BitmapCanvas& canvas, Surface& buffer,
           const std::uint32_t budget) {
  RPCMP_CHECK(suite, canvas.seal());
  std::size_t calls{};
  while (!canvas.complete() && !canvas.failed() && calls++ < 2'000'000) {
    const auto used = canvas.pump(buffer.data() + 1, bytes, stride, budget);
    RPCMP_CHECK(suite, used <= budget);
  }
  RPCMP_CHECK(suite, canvas.complete());
  RPCMP_CHECK(suite, buffer.front() == 0xEE && buffer.back() == 0xEE);
  for (std::size_t y = 0; y < ui::kCanvasHeight; ++y)
    for (std::size_t x = ui::kCanvasWidth; x < stride; ++x)
      RPCMP_CHECK(suite, buffer[1 + y * stride + x] == 0xEE);
}
void reference_glyph(Surface& buffer, const unsigned x, const unsigned y,
                     const std::uint32_t scalar, const unsigned height = 16) {
  const auto glyph = bitmap_glyph(scalar);
  for (unsigned row = 0; row < height; ++row)
    for (unsigned col = 0; col < glyph.width; ++col)
      if ((glyph.rows[row * (glyph.width / 8) + col / 8] & (0x80U >> (col % 8))) != 0)
        buffer[1 + (y + row) * stride + x + col] = 5;
}
void text_cases(rpcmp::test::Suite& suite) {
  auto canvas = std::make_unique<BitmapCanvas>();
  Surface actual(bytes + 2, 0xEE), expected = actual;
  canvas->begin();
  // The pinned font's ellipsis is 8 px. A 32 px box keeps A + あ + ellipsis.
  canvas->text({2, 3, 32, 16}, "AあB漢", ui::palette::accent);
  reference_glyph(expected, 2, 3, 'A');
  reference_glyph(expected, 10, 3, 0x3042);
  reference_glyph(expected, 26, 3, 0x2026);
  canvas->text({2, 24, 16, 8}, "AB", ui::palette::accent, true);
  reference_glyph(expected, 2, 24, 'A', 8);
  reference_glyph(expected, 10, 24, 0x2026, 8);
  canvas->text({2, 40, 7, 16}, "A", ui::palette::accent, true); // no whole ellipsis fits
  canvas->text({2, 58, 32, 16}, "🎵", ui::palette::accent);
  reference_glyph(expected, 2, 58, 0xFFFD);
  std::array<char, 3> temporary{'A', 'B', 'C'};
  canvas->text({2, 78, 24, 16}, {temporary.data(), temporary.size()}, ui::palette::accent);
  temporary.fill('Z');
  reference_glyph(expected, 2, 78, 'A');
  reference_glyph(expected, 10, 78, 'B');
  reference_glyph(expected, 18, 78, 'C');
  std::string maximum;
  for (unsigned i = 0; i < 32; ++i)
    maximum += "あ";
  canvas->text({0, 100, 640, 16}, maximum, ui::palette::accent, true);
  for (unsigned i = 0; i < 32; ++i)
    reference_glyph(expected, i * 16, 100, 0x3042);
  reference_glyph(expected, 512, 100, 0x2026);
  RPCMP_CHECK(suite, canvas->missing_glyphs());
  drain(suite, *canvas, actual, 7);
  RPCMP_CHECK(suite, actual == expected);
  RPCMP_CHECK(suite, bitmap_glyph(0xFF76).width == 8);
  RPCMP_CHECK(suite, bitmap_glyph(0x6F22).width == 16);
  RPCMP_CHECK(suite, !bitmap_has_glyph(0x1F3B5));
  RPCMP_CHECK(suite, bitmap_glyph(0x110000).rows == bitmap_glyph(0xFFFD).rows);
}
void primitives(rpcmp::test::Suite& suite) {
  auto canvas = std::make_unique<BitmapCanvas>();
  Surface actual(bytes + 2, 0xEE), expected = actual;
  canvas->fill({637, 477, 3, 3}, ui::palette::accent);
  for (unsigned y = 477; y < 480; ++y)
    for (unsigned x = 637; x < 640; ++x)
      expected[1 + y * stride + x] = 5;
  canvas->line({4, 4}, {1, 1}, ui::palette::accent);
  for (unsigned n = 1; n <= 4; ++n)
    expected[1 + n * stride + n] = 5;
  canvas->line({7, 2}, {7, 4}, ui::palette::accent);
  for (unsigned y = 2; y <= 4; ++y)
    expected[1 + y * stride + 7] = 5;
  drain(suite, *canvas, actual, 1);
  RPCMP_CHECK(suite, actual == expected);
}
bool save_ppm(const Surface& surface, const std::string& path) {
  std::ofstream output{path, std::ios::binary};
  output << "P6\n640 480\n255\n";
  for (unsigned y = 0; y < ui::kCanvasHeight; ++y)
    for (unsigned x = 0; x < ui::kCanvasWidth; ++x) {
      const auto color = kBitmapPalette.at(surface[1 + y * stride + x]);
      const std::array<char, 3> pixel{static_cast<char>(color >> 16), static_cast<char>(color >> 8),
                                      static_cast<char>(color)};
      output.write(pixel.data(), static_cast<std::streamsize>(pixel.size()));
    }
  output.close();
  return !output.fail();
}
rpcmp::contracts::CatalogText label(const std::string_view text) {
  rpcmp::contracts::CatalogText result;
  std::copy(text.begin(), text.end(), result.bytes.begin());
  result.length = static_cast<std::uint16_t>(text.size());
  return result;
}
void bounded_views(rpcmp::test::Suite& suite, const std::string& output_prefix) {
  auto canvas = std::make_unique<BitmapCanvas>();
  ui::PlayerView view;
  view.tracker.supported = true;
  view.tracker.availability = rpcmp::contracts::v2::PerformanceAvailability::Available;
  view.tracker.count = ui::kTrackerRows;
  std::uint64_t sequence = 1;
  for (auto& row : view.tracker.rows) {
    row.first_sequence = sequence++;
    for (std::size_t i = 0; i < row.cells.size(); ++i) {
      rpcmp::contracts::v2::PerformanceChannel channel;
      channel.channel_id = static_cast<std::uint16_t>(i);
      channel.note = static_cast<std::uint8_t>(48 + i + sequence);
      channel.key_on = true;
      row.cells[i] = rpcmp::contracts::v2::PerformanceChange{
          0, channel, rpcmp::contracts::v2::PerformanceKind::KeyOn};
    }
  }
  view.snapshot.track = rpcmp::contracts::v2::SelectedTrack{
      {{1}, {1}, 0, label("夜の街を歩く / 01 Opening")}, label("自作サンプル・アルバム")};
  view.valid_snapshot = true;
  view.browser.valid = true;
  view.browser.albums.header.count = 3;
  view.browser.albums.header.total = 3;
  for (auto& album : view.browser.albums.items) {
    album.name = label("作品名サンプル / Album");
    album.track_count = 12;
  }
  for (const auto mode : {ui::View::Tracker, ui::View::Keyboard, ui::View::Library}) {
    view.main = mode;
    Surface small(bytes + 2, 0xEE), large = small;
    canvas->begin();
    ui::render_player(view, *canvas);
    RPCMP_CHECK(suite, canvas->command_count() < BitmapCanvas::kCommandCapacity);
    drain(suite, *canvas, small, 113);
    canvas->begin();
    ui::render_player(view, *canvas);
    drain(suite, *canvas, large, 1'000'000);
    RPCMP_CHECK(suite, small == large);
    if (!output_prefix.empty())
      RPCMP_CHECK(suite, save_ppm(small, output_prefix +
                                             std::to_string(static_cast<unsigned>(mode)) + ".ppm"));
  }
}
void rejection(rpcmp::test::Suite& suite) {
  auto canvas = std::make_unique<BitmapCanvas>();
  Surface buffer(bytes + 2, 0xEE);
  canvas->fill({639, 479, 2, 1}, 0);
  RPCMP_CHECK(suite, !canvas->seal());
  RPCMP_CHECK(suite, canvas->pump(buffer.data() + 1, bytes, stride, 64) == 0);
  canvas->begin();
  canvas->text({0, 0, 20, 16}, "\xC0\xAF", 0);
  RPCMP_CHECK(suite, !canvas->seal());
  canvas->begin();
  canvas->text({0, 0, 640, 16}, std::string(97, 'A'), 0);
  RPCMP_CHECK(suite, !canvas->seal());
  canvas->begin();
  for (std::size_t i = 0; i <= BitmapCanvas::kCommandCapacity; ++i)
    canvas->fill({0, 0, 1, 1}, 0);
  RPCMP_CHECK(suite, !canvas->seal());
  canvas->begin();
  canvas->fill({0, 0, 2, 2}, 0);
  RPCMP_CHECK(suite, canvas->seal());
  RPCMP_CHECK(suite, canvas->pump(buffer.data(), 10, stride, 64) == 0);
  RPCMP_CHECK(suite, canvas->failed());
  RPCMP_CHECK(suite,
              std::all_of(buffer.begin(), buffer.end(), [](const auto b) { return b == 0xEE; }));
  canvas->begin();
  canvas->fill({0, 0, 2, 2}, 0);
  RPCMP_CHECK(suite, canvas->seal());
  RPCMP_CHECK(suite, canvas->pump(buffer.data() + 1, bytes, stride, 1) == 1);
  RPCMP_CHECK(suite, canvas->pump(buffer.data(), bytes, stride, 1) == 0);
  RPCMP_CHECK(suite, canvas->failed());
}
} // namespace
int main(const int argc, char** argv) {
  rpcmp::test::Suite suite;
  text_cases(suite);
  primitives(suite);
  bounded_views(suite, argc == 2 ? argv[1] : "");
  rejection(suite);
  return suite.finish("bitmap canvas");
}
