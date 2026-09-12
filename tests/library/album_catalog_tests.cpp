#include "rpcmp/library/album_catalog.hpp"
#include "rpcmp/library/formats.hpp"
#include "rpcmp/utility/writer.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::uint32_t u32(const std::vector<std::uint8_t>& bytes, const std::size_t at) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < 4; ++i)
    value |= static_cast<std::uint32_t>(bytes[at + i]) << (8 * i);
  return value;
}
std::uint64_t u64(const std::vector<std::uint8_t>& bytes, const std::size_t at) {
  return u32(bytes, at) | (static_cast<std::uint64_t>(u32(bytes, at + 4)) << 32U);
}
void set32(std::vector<std::uint8_t>& bytes, const std::size_t at, const std::uint32_t value) {
  for (std::size_t i = 0; i < 4; ++i)
    bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
void set64(std::vector<std::uint8_t>& bytes, const std::size_t at, const std::uint64_t value) {
  set32(bytes, at, static_cast<std::uint32_t>(value));
  set32(bytes, at + 4, static_cast<std::uint32_t>(value >> 32U));
}
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
rpcmp::utility::NormalizedLibrary model() {
  rpcmp::utility::NormalizedLibrary input;
  input.blobs = {{rpcmp::library::kMdxFourcc, {1, 2, 3}}};
  for (const auto* title : {"A1", "A2", "B1", "B2"}) {
    rpcmp::utility::NormalizedTrack track;
    track.format = rpcmp::library::kMdxFourcc;
    track.title = title;
    track.album = title[0] == 'A' ? "Alpha" : "Beta";
    input.tracks.push_back(track);
  }
  return input;
}
std::vector<rpcmp::utility::NormalizedAlbum> albums() {
  return {{"2/Beta", "Beta", {3, 2}}, {"1/Alpha", "Alpha", {1, 0}}};
}
std::string_view text(const rpcmp::library::Utf8View value) { return {value.data, value.size}; }

void repair(std::vector<std::uint8_t>& bytes, const std::size_t entry) {
  const auto offset = static_cast<std::size_t>(u64(bytes, entry + 8));
  const auto size = static_cast<std::size_t>(u64(bytes, entry + 16));
  set32(bytes, entry + 24, rpcmp::library::crc32({bytes.data() + offset, size}));
  set32(bytes, 72, 0);
  set32(bytes, 72, rpcmp::library::crc32({bytes.data(), 80}));
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  using rpcmp::library::AlbumCatalog;
  using rpcmp::library::CatalogError;
  using rpcmp::utility::WriterError;
  const auto golden = fixture();
  RPCMP_CHECK(suite, golden.size() == 1360);
  AlbumCatalog catalog;
  rpcmp::library::AlbumView album;
  rpcmp::library::TrackView track;
  RPCMP_CHECK(suite, !catalog.album_at(0, album));
  RPCMP_CHECK(suite, !catalog.find_album({1}, album));
  RPCMP_CHECK(suite, !catalog.track_at({1}, 0, track));
  RPCMP_CHECK(suite, AlbumCatalog::open({golden.data(), golden.size()}, catalog).ok());
  RPCMP_CHECK(suite, catalog.album_count() == 2 && catalog.track_count() == 4);
  RPCMP_CHECK(suite,
              catalog.album_at(0, album) && text(album.name) == "Beta" && album.track_count == 2);
  RPCMP_CHECK(suite, catalog.track_at(album.album_id, 0, track) && text(track.title) == "B2");
  RPCMP_CHECK(suite, catalog.track_at(album.album_id, 1, track) && text(track.title) == "B1");
  rpcmp::library::BlobView blob;
  RPCMP_CHECK(suite, catalog.library().find_blob(track.primary_blob_id, blob));
  RPCMP_CHECK(suite, blob.kind == rpcmp::library::kMdxFourcc && blob.bytes.size == 3 &&
                         blob.bytes.data[0] == 1 && blob.bytes.data[1] == 2 &&
                         blob.bytes.data[2] == 3);
  RPCMP_CHECK(suite, !catalog.track_at(album.album_id, 2, track));
  RPCMP_CHECK(suite, catalog.album_at(1, album) && text(album.name) == "Alpha");
  RPCMP_CHECK(suite, catalog.find_album(album.album_id, album) && album.display_ordinal == 1);
  RPCMP_CHECK(suite, catalog.track_at(album.album_id, 0, track) && text(track.title) == "A2");
  RPCMP_CHECK(suite, !catalog.album_at(2, album));
  RPCMP_CHECK(suite, !catalog.find_album({0}, album));

  const auto written = rpcmp::utility::write_album_rpcmlib(model(), albums());
  RPCMP_CHECK(suite, written.ok() && written.bytes == golden);
  auto reordered = model();
  std::reverse(reordered.tracks.begin(), reordered.tracks.end());
  auto membership = albums();
  for (auto& item : membership)
    for (auto& index : item.track_indices)
      index = 3 - index;
  RPCMP_CHECK(suite, rpcmp::utility::write_album_rpcmlib(reordered, membership).bytes == golden);
  membership = albums();
  std::reverse(membership.begin(), membership.end());
  const auto changed_order = rpcmp::utility::write_album_rpcmlib(model(), membership);
  RPCMP_CHECK(suite, changed_order.ok() && changed_order.bytes != golden);
  RPCMP_CHECK(suite, std::equal(golden.begin() + 56, golden.begin() + 72,
                                changed_order.bytes.begin() + 56));
  rpcmp::library::LogicalLibrary legacy;
  RPCMP_CHECK(suite, rpcmp::library::LogicalLibrary::open({golden.data(), golden.size()}, legacy) ==
                         rpcmp::library::LibraryError::None);
  const auto old_file = rpcmp::utility::write_rpcmlib(model());
  RPCMP_CHECK(suite,
              AlbumCatalog::open({old_file.bytes.data(), old_file.bytes.size()}, catalog).error ==
                  CatalogError::MissingSection);
  RPCMP_CHECK(suite, catalog.album_count() == 2); // failed open preserves prior view

  const auto entry = static_cast<std::size_t>(u64(golden, 40)) + std::size_t{6} * 40;
  const auto section = static_cast<std::size_t>(u64(golden, entry + 8));
  const auto record = section + 32;
  const auto members = section + 112;
  const auto reject = [&](std::vector<std::uint8_t> bytes,
                          const CatalogError expected = CatalogError::None) {
    repair(bytes, entry);
    AlbumCatalog rejected;
    const auto result = AlbumCatalog::open({bytes.data(), bytes.size()}, rejected);
    RPCMP_CHECK(suite, !result.ok());
    if (expected != CatalogError::None)
      RPCMP_CHECK(suite, result.error == expected);
    RPCMP_CHECK(suite, rejected.album_count() == 0);
  };
  // Mutate one condition at a time, with a correct CRC, not just damaged bytes.
  for (const auto offset :
       {std::size_t{0}, std::size_t{2}, std::size_t{4}, std::size_t{6}, std::size_t{8}}) {
    auto bad = golden;
    bad[section + offset] ^= 1;
    reject(bad);
  }
  for (const auto count : {0U, 301U, 0xffffffffU}) {
    auto bad = golden;
    set32(bad, section + 12, count);
    reject(bad);
  }
  for (const auto offset : {record, record + 8, record + 16, members}) {
    auto bad = golden;
    set64(bad, offset, 0);
    reject(bad);
  }
  auto bad = golden;
  set64(bad, record + 40, u64(bad, record));
  reject(bad); // duplicate AlbumId
  bad = golden;
  set32(bad, record + 40 + 24, u32(bad, record + 24));
  reject(bad); // display hole
  bad = golden;
  set32(bad, record + 28, 1);
  reject(bad); // range gap
  bad = golden;
  set32(bad, record + 32, 0);
  reject(bad); // empty album
  bad = golden;
  set32(bad, members + 8, 1);
  reject(bad); // member ordinal
  bad = golden;
  set64(bad, members + 16, u64(bad, members));
  reject(bad); // duplicate track
  bad = golden;
  set64(bad, record + 8, u64(bad, record + 40 + 8));
  reject(bad); // key/name mismatch
  // Keep valid ALBM keys and names; change only the referenced TRAK owner.
  const auto track_entry = static_cast<std::size_t>(u64(golden, 40));
  const auto track_record = static_cast<std::size_t>(u64(golden, track_entry + 8)) + 8;
  bad = golden;
  const auto original_owner = u64(bad, track_record + 40);
  const auto other_owner =
      original_owner == u64(bad, record + 8) ? u64(bad, record + 48) : u64(bad, record + 8);
  set64(bad, track_record + 40, other_owner);
  repair(bad, track_entry);
  reject(bad, CatalogError::InvalidReference);
  // Duplicate a key with matching names/references, isolating key uniqueness.
  bad = golden;
  set64(bad, record + 48, u64(bad, record + 8));
  set64(bad, record + 56, u64(bad, record + 16));
  for (std::size_t i = 0; i < 4; ++i)
    set64(bad, track_record + i * 96 + 40, u64(bad, record + 8));
  repair(bad, track_entry);
  reject(bad, CatalogError::InvalidAlbum);
  bad = golden;
  set64(bad, section + 24, 0xffffffffffffffffULL);
  reject(bad);
  bad = golden;
  set64(bad, entry + 16, 175);
  reject(bad); // truncated payload, valid envelope
  bad = golden;
  set32(bad, entry + 28, 4);
  reject(bad);
  bad = golden;
  bad[section + 10] = 7;
  bad[record + 36] = 9;
  bad[members + 12] = 11;
  repair(bad, entry);
  AlbumCatalog reserved;
  RPCMP_CHECK(suite, AlbumCatalog::open({bad.data(), bad.size()}, reserved).ok());
  RPCMP_CHECK(suite, AlbumCatalog::open({nullptr, static_cast<std::size_t>(
                                                      rpcmp::library::kAlbumMaxFileBytes + 1)},
                                        reserved)
                             .error == CatalogError::Capacity);

  for (const auto* key : {"", "/", "a/", "./a", "a/../b", "a//b", "a\\b", "C:/b"})
    RPCMP_CHECK(suite,
                !rpcmp::library::valid_album_key({key, std::char_traits<char>::length(key)}));
  auto input = model();
  input.tracks[1] = input.tracks[0];
  RPCMP_CHECK(suite, rpcmp::utility::write_album_rpcmlib(input, albums()).error ==
                         WriterError::DuplicateTrack);
  RPCMP_CHECK(suite, rpcmp::utility::write_rpcmlib(input).ok()); // existing API unchanged
  membership = albums();
  membership[0].track_indices[1] = 3;
  RPCMP_CHECK(suite, rpcmp::utility::write_album_rpcmlib(model(), membership).error ==
                         WriterError::InvalidInput);
  input = model();
  input.blobs[0].bytes.resize(1048577);
  RPCMP_CHECK(suite, rpcmp::utility::write_album_rpcmlib(input, albums()).error ==
                         WriterError::SizeLimitExceeded);
  input = model();
  input.tracks.clear();
  membership = {{".", "Root", {}}};
  for (std::size_t i = 0; i < 300; ++i) {
    rpcmp::utility::NormalizedTrack item;
    item.format = rpcmp::library::kMdxFourcc;
    item.title = std::to_string(i);
    item.album = "Root";
    input.tracks.push_back(item);
    membership[0].track_indices.push_back(i);
  }
  const auto maximum = rpcmp::utility::write_album_rpcmlib(input, membership);
  AlbumCatalog large;
  RPCMP_CHECK(suite,
              maximum.ok() &&
                  AlbumCatalog::open({maximum.bytes.data(), maximum.bytes.size()}, large).ok());
  RPCMP_CHECK(suite, large.track_count() == 300);
  auto many_album_input = input;
  std::vector<rpcmp::utility::NormalizedAlbum> many_albums;
  for (std::size_t i = 0; i < 300; ++i) {
    const auto name = "Album" + std::to_string(i);
    many_album_input.tracks[i].album = name;
    many_albums.push_back({name, name, {i}});
  }
  const auto maximum_albums = rpcmp::utility::write_album_rpcmlib(many_album_input, many_albums);
  AlbumCatalog many_catalog;
  RPCMP_CHECK(suite, maximum_albums.ok() && AlbumCatalog::open({maximum_albums.bytes.data(),
                                                                maximum_albums.bytes.size()},
                                                               many_catalog)
                                                .ok());
  RPCMP_CHECK(suite, many_catalog.album_count() == 300 && many_catalog.album_at(299, album) &&
                         text(album.name) == "Album299" &&
                         many_catalog.track_at(album.album_id, 0, track) &&
                         text(track.title) == "299");
  many_albums.push_back({"Overflow", "Overflow", {0}});
  RPCMP_CHECK(suite, rpcmp::utility::write_album_rpcmlib(many_album_input, many_albums).error ==
                         WriterError::SizeLimitExceeded);
  input.tracks.push_back(input.tracks[0]);
  membership[0].track_indices.push_back(300);
  RPCMP_CHECK(suite, rpcmp::utility::write_album_rpcmlib(input, membership).error ==
                         WriterError::SizeLimitExceeded);
  // Whole-file cap, including legal envelope padding: accept exactly 32 MiB,
  // reject the next byte before dereferencing an over-limit input view.
  auto padded = golden;
  padded.resize(static_cast<std::size_t>(rpcmp::library::kAlbumMaxFileBytes));
  set64(padded, 32, padded.size());
  repair(padded, entry);
  AlbumCatalog padded_catalog;
  RPCMP_CHECK(suite, AlbumCatalog::open({padded.data(), padded.size()}, padded_catalog).ok());
  padded.push_back(0);
  RPCMP_CHECK(suite, AlbumCatalog::open({padded.data(), padded.size()}, padded_catalog).error ==
                         CatalogError::Capacity);

  // 255*4096 + 4091 title bytes + "Root" + "." = exactly 1 MiB of unique STRS data.
  input.tracks.resize(256);
  membership[0].track_indices.resize(256);
  for (std::size_t i = 0; i < input.tracks.size(); ++i) {
    input.tracks[i].title = std::string(i == 255 ? 4091 : 4096, 'x');
    const auto prefix = std::to_string(i);
    input.tracks[i].title.replace(0, prefix.size(), prefix);
  }
  const auto strings_max = rpcmp::utility::write_album_rpcmlib(input, membership);
  AlbumCatalog strings_catalog;
  RPCMP_CHECK(suite, strings_max.ok() &&
                         AlbumCatalog::open({strings_max.bytes.data(), strings_max.bytes.size()},
                                            strings_catalog)
                             .ok());
  input.tracks.back().title.push_back('x');
  RPCMP_CHECK(suite, rpcmp::utility::write_album_rpcmlib(input, membership).error ==
                         WriterError::SizeLimitExceeded);
  // 32 tracks/blobs, 35 strings (32 three-byte titles, empty artist, Root, .),
  // one album: aligned v1 sections + ALBM + header/directory add 8584 bytes.
  // Keep all blob lengths aligned so this calculation does not depend on IDs.
  input = {};
  membership = {{".", "Root", {}}};
  for (std::size_t i = 0; i < 32; ++i) {
    input.blobs.push_back({rpcmp::library::kMdxFourcc,
                           std::vector<std::uint8_t>(1048576, static_cast<std::uint8_t>(i))});
    rpcmp::utility::NormalizedTrack item;
    item.format = rpcmp::library::kMdxFourcc;
    item.primary_blob_index = i;
    item.title = std::string{"t"} + (i < 10 ? "0" : "") + std::to_string(i);
    item.album = "Root";
    input.tracks.push_back(item);
    membership[0].track_indices.push_back(i);
  }
  input.blobs.back().bytes.resize(1048576 - 8584);
  const auto file_max = rpcmp::utility::write_album_rpcmlib(input, membership);
  RPCMP_CHECK(suite, file_max.ok() && file_max.bytes.size() == rpcmp::library::kAlbumMaxFileBytes);
  input.blobs.back().bytes.push_back(31);
  const auto file_over = rpcmp::utility::write_album_rpcmlib(input, membership);
  RPCMP_CHECK(suite, file_over.error == WriterError::SizeLimitExceeded && file_over.bytes.empty());
  return suite.finish("album catalog");
}
