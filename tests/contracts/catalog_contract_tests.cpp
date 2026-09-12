#include "rpcmp/contracts/catalog.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <limits>
#include <string_view>

namespace {
using namespace rpcmp::contracts;

CatalogText text(const std::string_view source) {
  CatalogText result;
  std::copy(source.begin(), source.end(), result.bytes.begin());
  result.length = static_cast<std::uint16_t>(source.size());
  return result;
}

// Authored response independent of Core, library storage and writer code.
CatalogAlbumPage mock_albums() {
  CatalogAlbumPage page;
  page.header.query = {1, {42}, 16, 2};
  page.header.total = 20;
  page.header.count = 2;
  page.header.next_start = 18;
  page.items[0] = {{9}, 16, text("日本語"), 3};
  page.items[1] = {{5}, 17, text("Album"), 1};
  return page;
}
CatalogTrackPage mock_tracks() {
  CatalogTrackPage page;
  page.header.query = {1, {42}, 1, 2};
  page.header.total = 3;
  page.header.count = 2;
  page.album_id = {9};
  page.items[0] = {{51}, {9}, 1, text("曲２")};
  page.items[1] = {{17}, {9}, 2, text("曲３")};
  return page;
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  RPCMP_CHECK(suite, valid_catalog_page(mock_albums()));
  RPCMP_CHECK(suite, valid_catalog_page(mock_tracks()));
  const auto reject_album = [&](const auto& mutate) {
    auto page = mock_albums();
    mutate(page);
    RPCMP_CHECK(suite, !valid_catalog_page(page));
  };
  reject_album([](auto& p) { p.header.schema_version = 2; });
  reject_album([](auto& p) { p.header.query.schema_version = 2; });
  reject_album([](auto& p) { p.header.query.generation = {}; });
  reject_album([](auto& p) { p.header.query.limit = 0; });
  reject_album([](auto& p) { p.header.query.limit = 17; });
  reject_album([](auto& p) { p.header.total = 301; });
  reject_album([](auto& p) { p.header.total = 0; });
  reject_album([](auto& p) { p.header.count = 65535; });
  reject_album([](auto& p) { p.header.count = 1; });
  reject_album([](auto& p) { p.header.next_start.reset(); });
  reject_album([](auto& p) { p.header.next_start = 19; });
  reject_album(
      [](auto& p) { p.header.query.start_ordinal = std::numeric_limits<std::uint32_t>::max(); });
  reject_album([](auto& p) { p.items[1].album_id = p.items[0].album_id; });
  reject_album([](auto& p) { p.items[0].album_id = {}; });
  reject_album([](auto& p) { p.items[0].display_ordinal = 0; });
  reject_album([](auto& p) { p.items[0].track_count = 0; });
  reject_album([](auto& p) { p.items[0].track_count = 301; });
  reject_album([](auto& p) { p.items[0].name.length = 65535; });
  reject_album([](auto& p) { p.items[0].name.length = 0; });
  reject_album([](auto& p) { p.items[0].name.truncated = true; });
  for (const std::string_view invalid :
       {"\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE6\x97", "\x80"})
    reject_album([&](auto& p) { p.items[0].name = text(invalid); });
  auto album = mock_albums();
  album.items[0].name = text(std::string_view{"a\0b", 3});
  RPCMP_CHECK(suite, !valid_catalog_page(album)); // Storage forbids embedded NUL.
  album = mock_albums();
  album.items[15].name.length = 65535;
  RPCMP_CHECK(suite, valid_catalog_page(album)); // Unused entries are never read.

  const auto reject_track = [&](const auto& mutate) {
    auto page = mock_tracks();
    mutate(page);
    RPCMP_CHECK(suite, !valid_catalog_page(page));
  };
  reject_track([](auto& p) { p.album_id = {}; });
  reject_track([](auto& p) { p.items[0].album_id = {8}; });
  reject_track([](auto& p) { p.items[0].track_id = {}; });
  reject_track([](auto& p) { p.items[1].track_id = p.items[0].track_id; });
  reject_track([](auto& p) { p.items[1].track_ordinal = 1; });
  reject_track([](auto& p) { p.items[0].title = text("\xFF"); });
  reject_track([](auto& p) { p.header.next_start = 3; });
  auto end = mock_tracks();
  end.header.query.start_ordinal = 3;
  end.header.count = 0;
  RPCMP_CHECK(suite, valid_catalog_page(end));

  CatalogAlbumPage error;
  error.header.error = CatalogQueryError::Unavailable;
  RPCMP_CHECK(suite, valid_catalog_page(error));
  error.header.error = CatalogQueryError::UnsupportedSchema;
  RPCMP_CHECK(suite, !valid_catalog_page(error));
  error.header.query.schema_version = 2;
  RPCMP_CHECK(suite, valid_catalog_page(error));
  error.header.query.schema_version = 1;
  error.header.error = CatalogQueryError::StaleLibrary;
  error.header.query.start_ordinal = std::numeric_limits<std::uint32_t>::max();
  error.header.query.limit = 0;
  RPCMP_CHECK(suite, valid_catalog_page(error));
  error.header.error = CatalogQueryError::InvalidLimit;
  RPCMP_CHECK(suite, !valid_catalog_page(error));
  error.header.query.generation = {42};
  RPCMP_CHECK(suite, valid_catalog_page(error));
  error.header.query.limit = 16;
  RPCMP_CHECK(suite, !valid_catalog_page(error));
  error.header.error = CatalogQueryError::InvalidStart;
  RPCMP_CHECK(suite, valid_catalog_page(error));
  error.header.query.start_ordinal = 0;
  RPCMP_CHECK(suite, !valid_catalog_page(error));
  error.header.error = CatalogQueryError::UnknownAlbum;
  RPCMP_CHECK(suite, !valid_catalog_page(error));
  CatalogTrackPage unknown;
  unknown.header = error.header;
  RPCMP_CHECK(suite, valid_catalog_page(unknown));
  unknown.header.count = 1;
  RPCMP_CHECK(suite, !valid_catalog_page(unknown));
  unknown.header.count = 0;
  unknown.header.total = 1;
  RPCMP_CHECK(suite, !valid_catalog_page(unknown));
  unknown.header.total = 0;
  unknown.header.next_start = 1;
  RPCMP_CHECK(suite, !valid_catalog_page(unknown));
  error.header.error = static_cast<CatalogQueryError>(255);
  RPCMP_CHECK(suite, !valid_catalog_page(error));

  CatalogStatus status;
  RPCMP_CHECK(suite, valid_catalog_status(status));
  status.phase = CatalogPhase::Loading;
  RPCMP_CHECK(suite, !valid_catalog_status(status));
  status.generation = {42};
  RPCMP_CHECK(suite, valid_catalog_status(status));
  status.phase = CatalogPhase::Ready;
  status.album_count = 2;
  status.track_count = 1;
  RPCMP_CHECK(suite, !valid_catalog_status(status));
  status.track_count = 300;
  RPCMP_CHECK(suite, valid_catalog_status(status));
  status.track_count = 301;
  RPCMP_CHECK(suite, !valid_catalog_status(status));
  status.phase = CatalogPhase::Error;
  status.failure = CatalogFailure::ReadFailed;
  RPCMP_CHECK(suite, !valid_catalog_status(status));
  status.album_count = 0;
  status.track_count = 0;
  RPCMP_CHECK(suite, valid_catalog_status(status));
  status.failure = CatalogFailure::GenerationExhausted;
  RPCMP_CHECK(suite, !valid_catalog_status(status));
  status.generation.value = std::numeric_limits<std::uint64_t>::max();
  RPCMP_CHECK(suite, valid_catalog_status(status));
  status.failure = static_cast<CatalogFailure>(255);
  RPCMP_CHECK(suite, !valid_catalog_status(status));
  status.phase = static_cast<CatalogPhase>(255);
  RPCMP_CHECK(suite, !valid_catalog_status(status));
  status = {};
  status.schema_version = 2;
  RPCMP_CHECK(suite, !valid_catalog_status(status));
  return suite.finish("catalog contracts (mock only)");
}
