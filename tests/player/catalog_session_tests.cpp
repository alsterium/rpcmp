#include "rpcmp/library/formats.hpp"
#include "rpcmp/player/catalog_session.hpp"
#include "rpcmp/utility/writer.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using namespace rpcmp::contracts;
using namespace rpcmp::player;

static_assert(std::is_trivially_copyable_v<CatalogAlbumPage>);
static_assert(std::is_trivially_copyable_v<CatalogTrackPage>);
static_assert(!std::is_copy_constructible_v<CatalogSession>);

// Literal IDs from the independently encoded committed fixture, not the writer.
constexpr AlbumId kBeta{0x072692aa06258cebULL};
constexpr AlbumId kAlpha{0xcba9c5a08b5a78e4ULL};
constexpr TrackId kB2{0x47305869eca5f89aULL};
constexpr TrackId kB1{0x1379269c029ea5fcULL};
constexpr TrackId kA2{0x00dc3d0692d52f4dULL};
constexpr TrackId kA1{0xdfa1842e795ddea1ULL};

std::vector<std::uint8_t> fixture() {
  std::ifstream input(std::string{RPCMP_SOURCE_DIR} + "/tests/fixtures/rpcmlib/album.rpcmlib.hex");
  std::string pair;
  std::vector<std::uint8_t> bytes;
  char c = 0;
  while (input.get(c)) {
    if (c == '\r' || c == '\n')
      continue;
    pair += c;
    if (pair.size() == 2) {
      bytes.push_back(static_cast<std::uint8_t>(std::stoul(pair, nullptr, 16)));
      pair.clear();
    }
  }
  return bytes;
}
std::string_view text(const CatalogText& value) { return {value.bytes.data(), value.length}; }
rpcmp::library::ByteView view(const std::vector<std::uint8_t>& bytes) {
  return {bytes.data(), bytes.size()};
}
CatalogPageQuery query(const LibraryGeneration generation, const std::uint32_t start = 0,
                       const std::uint16_t limit = 16) {
  return {kCatalogSchemaVersion, generation, start, limit};
}

void lifecycle(rpcmp::test::Suite& suite) {
  auto bytes = fixture();
  RPCMP_CHECK(suite, bytes.size() == 1360);
  CatalogSession session;
  const CatalogReader& reader = session;
  RPCMP_CHECK(suite, valid_catalog_status(reader.status()));
  RPCMP_CHECK(suite, reader.status().generation.value == 0);
  RPCMP_CHECK(suite, reader.albums(query({0})).header.error == CatalogQueryError::Unavailable);
  RPCMP_CHECK(suite, session.complete_open({0}, {}).error == CatalogChangeError::StaleLibrary);
  auto bad_schema = query({0});
  bad_schema.schema_version = 2;
  RPCMP_CHECK(suite,
              reader.albums(bad_schema).header.error == CatalogQueryError::UnsupportedSchema);
  const auto first = session.begin_open();
  RPCMP_CHECK(suite, first.ok() && first.generation.value == 1);
  RPCMP_CHECK(suite, reader.status().phase == CatalogPhase::Loading);
  RPCMP_CHECK(suite, valid_catalog_status(reader.status()));
  RPCMP_CHECK(suite, reader.albums(query(first.generation)).header.error ==
                         CatalogQueryError::Unavailable);
  RPCMP_CHECK(suite, session.complete_open(first.generation, view(bytes)).ok());
  RPCMP_CHECK(suite, valid_catalog_status(reader.status()));
  RPCMP_CHECK(suite, reader.status().album_count == 2 && reader.status().track_count == 4);
  const auto albums = reader.albums(query(first.generation));
  RPCMP_CHECK(suite, valid_catalog_page(albums) && albums.header.count == 2);
  RPCMP_CHECK(suite, albums.items[0].album_id == kBeta && text(albums.items[0].name) == "Beta");
  RPCMP_CHECK(suite, albums.items[1].album_id == kAlpha && text(albums.items[1].name) == "Alpha");
  const auto beta = reader.tracks(kBeta, query(first.generation));
  const auto alpha = reader.tracks(kAlpha, query(first.generation));
  RPCMP_CHECK(suite, valid_catalog_page(beta) && valid_catalog_page(alpha));
  RPCMP_CHECK(suite, beta.items[0].track_id == kB2 && text(beta.items[0].title) == "B2");
  RPCMP_CHECK(suite, beta.items[1].track_id == kB1 && text(beta.items[1].title) == "B1");
  RPCMP_CHECK(suite, alpha.items[0].track_id == kA2 && text(alpha.items[0].title) == "A2");
  RPCMP_CHECK(suite, alpha.items[1].track_id == kA1 && text(alpha.items[1].title) == "A1");
  RPCMP_CHECK(suite,
              session.complete_open(first.generation, {}).error == CatalogChangeError::NotLoading);
  RPCMP_CHECK(suite, session.fail_open(first.generation).error == CatalogChangeError::NotLoading);

  rpcmp::library::TrackView selected;
  RPCMP_CHECK(suite,
              session.resolve_track(first.generation, kB2, selected) == CatalogTrackError::None);
  RPCMP_CHECK(suite, selected.track_id == kB2);
  RPCMP_CHECK(suite, session.resolve_track({0}, kB1, selected) == CatalogTrackError::StaleLibrary);
  RPCMP_CHECK(suite, session.resolve_track(first.generation, {0}, selected) ==
                         CatalogTrackError::UnknownTrack);
  RPCMP_CHECK(suite, selected.track_id == kB2 && reader.status().generation == first.generation);
  RPCMP_CHECK(suite, reader.albums(query(first.generation)).header.ok());

  for (const auto limit : {std::uint16_t{0}, std::uint16_t{17}, std::uint16_t{65535}}) {
    const auto page = reader.albums(query(first.generation, 0, limit));
    RPCMP_CHECK(suite,
                page.header.error == CatalogQueryError::InvalidLimit && valid_catalog_page(page));
    RPCMP_CHECK(suite, reader.tracks({0}, query(first.generation, 0, limit)).header.error ==
                           CatalogQueryError::InvalidLimit);
  }
  const auto unknown = reader.tracks({0}, query(first.generation, 3));
  RPCMP_CHECK(suite, unknown.header.error == CatalogQueryError::UnknownAlbum &&
                         valid_catalog_page(unknown));
  for (const auto start : {3U, std::numeric_limits<std::uint32_t>::max()}) {
    const auto a = reader.albums(query(first.generation, start));
    const auto t = reader.tracks(kBeta, query(first.generation, start));
    RPCMP_CHECK(suite, a.header.error == CatalogQueryError::InvalidStart && valid_catalog_page(a));
    RPCMP_CHECK(suite, t.header.error == CatalogQueryError::InvalidStart && valid_catalog_page(t));
  }
  RPCMP_CHECK(suite, reader.albums(query(first.generation, 2)).header.count == 0);
  RPCMP_CHECK(suite, valid_catalog_page(reader.albums(query(first.generation, 2))));
  RPCMP_CHECK(suite, valid_catalog_page(reader.tracks(kBeta, query(first.generation, 2))));
  const auto one = reader.tracks(kBeta, query(first.generation, 0, 1));
  RPCMP_CHECK(suite,
              one.header.count == 1 && one.header.next_start == 1U && valid_catalog_page(one));
  RPCMP_CHECK(suite, reader.tracks(kBeta, query(first.generation, 1, 1)).items[0].track_id == kB1);

  const auto second = session.begin_open();
  RPCMP_CHECK(suite, second.generation.value == 2 && reader.status().album_count == 0);
  RPCMP_CHECK(suite, session.resolve_track(first.generation, kB2, selected) ==
                         CatalogTrackError::Unavailable);
  // A late success/failure cannot overwrite the current Loading operation.
  RPCMP_CHECK(suite, session.complete_open(first.generation, view(bytes)).error ==
                         CatalogChangeError::StaleLibrary);
  RPCMP_CHECK(suite, session.fail_open(first.generation).error == CatalogChangeError::StaleLibrary);
  RPCMP_CHECK(suite, reader.status().phase == CatalogPhase::Loading);
  RPCMP_CHECK(suite, session.complete_open(second.generation, {}).error ==
                         CatalogChangeError::InvalidLibrary);
  RPCMP_CHECK(suite, reader.status().failure == CatalogFailure::InvalidLibrary &&
                         valid_catalog_status(reader.status()));
  RPCMP_CHECK(suite, reader.albums(query(first.generation)).header.error ==
                         CatalogQueryError::Unavailable);
  const auto third = session.begin_open();
  RPCMP_CHECK(suite, session.fail_open(third.generation).error == CatalogChangeError::ReadFailed);
  RPCMP_CHECK(suite, reader.status().failure == CatalogFailure::ReadFailed &&
                         valid_catalog_status(reader.status()));
  const auto fourth = session.begin_open();
  RPCMP_CHECK(suite, session.complete_open(fourth.generation, view(bytes)).ok());
  const auto stale = reader.tracks(kBeta, query(first.generation, 0, 0));
  RPCMP_CHECK(suite,
              stale.header.error == CatalogQueryError::StaleLibrary && valid_catalog_page(stale));
  RPCMP_CHECK(suite, stale.header.query.generation == first.generation && stale.album_id == kBeta);
  RPCMP_CHECK(suite, session.close().generation.value == 5);
  RPCMP_CHECK(suite, reader.status().phase == CatalogPhase::Empty &&
                         valid_catalog_status(reader.status()));
  RPCMP_CHECK(suite, session.complete_open(fourth.generation, view(bytes)).error ==
                         CatalogChangeError::StaleLibrary);
  std::vector<std::uint8_t>().swap(bytes); // Release backing storage after invalidation.
  RPCMP_CHECK(suite, text(beta.items[0].title) == "B2" && text(albums.items[0].name) == "Beta");
  RPCMP_CHECK(suite, valid_catalog_page(beta) && valid_catalog_page(albums));

  const auto pending = session.begin_open();
  RPCMP_CHECK(suite, session.close().generation.value == pending.generation.value + 1);
  RPCMP_CHECK(suite,
              session.fail_open(pending.generation).error == CatalogChangeError::StaleLibrary);
}

rpcmp::utility::WriterResult library_with(const std::uint32_t album_count,
                                          const std::uint32_t tracks_per_album,
                                          const std::string& name = "Album",
                                          const std::string& title = "Track") {
  rpcmp::utility::NormalizedLibrary input;
  input.blobs = {{rpcmp::library::kMdxFourcc, {1, 2, 3}}};
  std::vector<rpcmp::utility::NormalizedAlbum> albums;
  for (std::uint32_t a = 0; a < album_count; ++a) {
    rpcmp::utility::NormalizedAlbum album{
        album_count == 1 ? "." : std::to_string(a) + "/" + name, name, {}};
    for (std::uint32_t t = 0; t < tracks_per_album; ++t) {
      rpcmp::utility::NormalizedTrack track;
      track.format = rpcmp::library::kMdxFourcc;
      track.title = title;
      track.artist =
          std::to_string(a * tracks_per_album + t); // Unique identity, unchanged display.
      track.album = name;
      album.track_indices.push_back(input.tracks.size());
      input.tracks.push_back(track);
    }
    albums.push_back(album);
  }
  return rpcmp::utility::write_album_rpcmlib(input, albums);
}

void pagination_and_text(rpcmp::test::Suite& suite) {
  for (const bool many_albums : {false, true}) {
    const auto file = library_with(many_albums ? 300 : 1, many_albums ? 1 : 300);
    RPCMP_CHECK(suite, file.ok());
    CatalogSession session;
    const auto opened = session.begin_open();
    RPCMP_CHECK(suite, session.complete_open(opened.generation, view(file.bytes)).ok());
    const auto album = session.albums(query(opened.generation)).items[0].album_id;
    for (std::uint32_t start = 0; start <= 300; start += 16) {
      const auto page_query = query(opened.generation, start);
      const auto a = session.albums(page_query);
      const auto t = session.tracks(album, page_query);
      const auto& header = many_albums ? a.header : t.header;
      RPCMP_CHECK(suite, many_albums ? valid_catalog_page(a) : valid_catalog_page(t));
      RPCMP_CHECK(suite, header.total == 300 && header.count == (start == 288 ? 12 : 16));
      RPCMP_CHECK(suite,
                  start == 288 ? !header.next_start.has_value() : header.next_start == start + 16);
    }
    RPCMP_CHECK(suite,
                many_albums
                    ? valid_catalog_page(session.albums(query(opened.generation, 300)))
                    : valid_catalog_page(session.tracks(album, query(opened.generation, 300))));
  }
  struct TextCase {
    std::string source;
    std::string expected;
    bool truncated;
  };
  const std::vector<TextCase> cases = {
      {"", "", false},
      {std::string(96, 'x'), std::string(96, 'x'), false},
      {std::string(97, 'x'), std::string(96, 'x'), true},
      {std::string(93, 'x') + "日", std::string(93, 'x') + "日", false},
      {std::string(94, 'x') + "日", std::string(94, 'x'), true},
      {std::string(95, 'x') + "日", std::string(95, 'x'), true},
      {std::string(93, 'x') + "😀", std::string(93, 'x'), true},
      {std::string(92, 'x') + "😀Z", std::string(92, 'x') + "😀", true},
      {std::string(95, 'x') + "é", std::string(95, 'x'), true},
      {std::string(4096, 'x'), std::string(96, 'x'), true}};
  for (const auto& test : cases) {
    // Album names are nonempty in the storage profile; empty titles are valid.
    const auto file = library_with(1, 1, test.source.empty() ? "Album" : test.source, test.source);
    RPCMP_CHECK(suite, file.ok());
    CatalogSession session;
    const auto opened = session.begin_open();
    RPCMP_CHECK(suite, session.complete_open(opened.generation, view(file.bytes)).ok());
    const auto a = session.albums(query(opened.generation));
    const auto t = session.tracks(a.items[0].album_id, query(opened.generation));
    RPCMP_CHECK(suite, valid_catalog_page(a) && valid_catalog_page(t));
    RPCMP_CHECK(suite, text(t.items[0].title) == test.expected &&
                           t.items[0].title.truncated == test.truncated);
    if (!test.source.empty())
      RPCMP_CHECK(suite, text(a.items[0].name) == test.expected &&
                             a.items[0].name.truncated == test.truncated);
  }
}

void generation_limits(rpcmp::test::Suite& suite) {
  const auto file = fixture();
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  CatalogSession session({maximum - 1});
  const auto last = session.begin_open();
  RPCMP_CHECK(suite, last.ok() && last.generation.value == maximum);
  RPCMP_CHECK(suite, session.complete_open(last.generation, view(file)).ok());
  RPCMP_CHECK(suite, session.begin_open().error == CatalogChangeError::GenerationExhausted);
  RPCMP_CHECK(suite, valid_catalog_status(session.status()) &&
                         session.status().generation.value == maximum);
  RPCMP_CHECK(suite, session.albums(query(last.generation)).header.error ==
                         CatalogQueryError::Unavailable);
  RPCMP_CHECK(suite, session.complete_open(last.generation, view(file)).error ==
                         CatalogChangeError::GenerationExhausted);
  RPCMP_CHECK(suite,
              session.fail_open(last.generation).error == CatalogChangeError::GenerationExhausted);
  RPCMP_CHECK(suite, session.close().error == CatalogChangeError::GenerationExhausted);
  CatalogSession closing({maximum});
  RPCMP_CHECK(suite, closing.close().error == CatalogChangeError::GenerationExhausted);
  RPCMP_CHECK(suite, valid_catalog_status(closing.status()));
}

void reordered_reload(rpcmp::test::Suite& suite) {
  const auto original = fixture();
  rpcmp::utility::NormalizedLibrary model;
  model.blobs = {{rpcmp::library::kMdxFourcc, {1, 2, 3}}};
  for (const auto* title : {"A1", "A2", "B1", "B2"}) {
    rpcmp::utility::NormalizedTrack track;
    track.format = rpcmp::library::kMdxFourcc;
    track.title = title;
    track.album = title[0] == 'A' ? "Alpha" : "Beta";
    model.tracks.push_back(track);
  }
  const auto changed = rpcmp::utility::write_album_rpcmlib(
      model, {{"1/Alpha", "Alpha", {1, 0}}, {"2/Beta", "Beta", {3, 2}}});
  RPCMP_CHECK(suite, changed.ok() && changed.bytes != original);
  RPCMP_CHECK(suite,
              std::equal(original.begin() + 56, original.begin() + 72, changed.bytes.begin() + 56));
  CatalogSession session;
  const auto first = session.begin_open();
  RPCMP_CHECK(suite, session.complete_open(first.generation, view(original)).ok());
  const auto second = session.begin_open();
  RPCMP_CHECK(suite, session.complete_open(second.generation, view(changed.bytes)).ok());
  RPCMP_CHECK(suite, session.albums(query(first.generation)).header.error ==
                         CatalogQueryError::StaleLibrary);
  RPCMP_CHECK(suite, session.albums(query(second.generation)).items[0].album_id == kAlpha);
  rpcmp::library::TrackView output;
  RPCMP_CHECK(suite, session.resolve_track(first.generation, kA2, output) ==
                         CatalogTrackError::StaleLibrary);
  RPCMP_CHECK(suite,
              session.resolve_track(second.generation, kA2, output) == CatalogTrackError::None);
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  lifecycle(suite);
  pagination_and_text(suite);
  generation_limits(suite);
  reordered_reload(suite);
  return suite.finish("catalog session");
}
