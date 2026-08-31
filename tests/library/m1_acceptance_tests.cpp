#include "rpcmp/library/logical_library.hpp"
#include "rpcmp/utility/stable_id.hpp"
#include "rpcmp/utility/writer.hpp"
#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using rpcmp::library::ByteView;
using rpcmp::library::LibraryError;

constexpr std::uint32_t kMdx = 0x2058444DU;
constexpr std::uint32_t kPdx = 0x20584450U;
constexpr std::uint32_t kTrackTag = 0x4B415254U;
constexpr std::uint32_t kBlobTag = 0x424F4C42U;
constexpr std::uint32_t kStringTag = 0x53525453U;

std::uint32_t read_u32(const std::uint8_t* const bytes) {
  return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

std::uint64_t read_u64(const std::uint8_t* const bytes) {
  return static_cast<std::uint64_t>(read_u32(bytes)) |
         (static_cast<std::uint64_t>(read_u32(bytes + 4)) << 32U);
}

void write_u32(std::vector<std::uint8_t>& bytes, const std::size_t offset,
               const std::uint32_t value) {
  for (std::size_t index = 0; index < 4; ++index) {
    bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
  }
}

void write_u64(std::vector<std::uint8_t>& bytes, const std::size_t offset,
               const std::uint64_t value) {
  for (std::size_t index = 0; index < 8; ++index) {
    bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
  }
}

std::size_t directory_entry(const std::vector<std::uint8_t>& bytes, const std::uint32_t tag) {
  const std::size_t directory = static_cast<std::size_t>(read_u64(bytes.data() + 40));
  const std::uint32_t count = read_u32(bytes.data() + 48);
  for (std::uint32_t index = 0; index < count; ++index) {
    const std::size_t entry = directory + static_cast<std::size_t>(index) * 40;
    if (read_u32(bytes.data() + entry) == tag) {
      return entry;
    }
  }
  return bytes.size();
}

std::size_t section_offset(const std::vector<std::uint8_t>& bytes, const std::uint32_t tag) {
  return static_cast<std::size_t>(read_u64(bytes.data() + directory_entry(bytes, tag) + 8));
}

void refresh_section_crc(std::vector<std::uint8_t>& bytes, const std::uint32_t tag) {
  const std::size_t entry = directory_entry(bytes, tag);
  const std::size_t offset = static_cast<std::size_t>(read_u64(bytes.data() + entry + 8));
  const std::size_t length = static_cast<std::size_t>(read_u64(bytes.data() + entry + 16));
  write_u32(bytes, entry + 24, rpcmp::library::crc32({bytes.data() + offset, length}));
}

rpcmp::utility::NormalizedLibrary multi_track_fixture() {
  rpcmp::utility::NormalizedLibrary input;
  input.blobs = {{kMdx, {1, 2, 3}}, {kPdx, {4, 5}}};
  rpcmp::utility::NormalizedTrack first;
  first.format = kMdx;
  first.title = "First";
  first.artist = "Artist";
  first.dependencies.push_back({kPdx, 1});
  rpcmp::utility::NormalizedTrack second = first;
  second.title = "Second";
  second.dependencies.clear();
  input.tracks = {first, second};
  return input;
}

LibraryError open(const std::vector<std::uint8_t>& bytes,
                  const rpcmp::library::ValidationLimits limits = {}) {
  rpcmp::library::LogicalLibrary library;
  return rpcmp::library::LogicalLibrary::open({bytes.data(), bytes.size()}, library, limits);
}

ByteView string_bytes(const std::string& value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

void check_corruption(rpcmp::test::Suite& suite) {
  const auto written = rpcmp::utility::write_rpcmlib(multi_track_fixture());
  RPCMP_CHECK(suite, written.ok());
  RPCMP_CHECK(suite, open(written.bytes) == LibraryError::None);

  auto corrupted = written.bytes;
  corrupted.pop_back();
  RPCMP_CHECK(suite, open(corrupted) == LibraryError::FileSizeMismatch);

  corrupted = written.bytes;
  const std::size_t tracks = section_offset(corrupted, kTrackTag);
  write_u64(corrupted, tracks + 8 + 96, read_u64(corrupted.data() + tracks + 8));
  refresh_section_crc(corrupted, kTrackTag);
  RPCMP_CHECK(suite, open(corrupted) == LibraryError::InvalidRecordOrder);

  corrupted = written.bytes;
  const std::size_t blobs = section_offset(corrupted, kBlobTag);
  write_u64(corrupted, blobs + 16 + 48, read_u64(corrupted.data() + blobs + 16));
  refresh_section_crc(corrupted, kBlobTag);
  RPCMP_CHECK(suite, open(corrupted) == LibraryError::InvalidRecordOrder);

  corrupted = written.bytes;
  const std::size_t strings = section_offset(corrupted, kStringTag);
  write_u64(corrupted, strings + 16 + 16, read_u64(corrupted.data() + strings + 16));
  refresh_section_crc(corrupted, kStringTag);
  RPCMP_CHECK(suite, open(corrupted) == LibraryError::InvalidRecordOrder);

  corrupted = written.bytes;
  write_u32(corrupted, strings + 16 + 16 + 8, 1);
  refresh_section_crc(corrupted, kStringTag);
  RPCMP_CHECK(suite, open(corrupted) == LibraryError::InvalidRecordOrder);

  corrupted = written.bytes;
  const std::size_t string_data =
      strings + static_cast<std::size_t>(read_u64(corrupted.data() + strings + 8));
  corrupted[string_data] = 0;
  refresh_section_crc(corrupted, kStringTag);
  RPCMP_CHECK(suite, open(corrupted) == LibraryError::EmbeddedNul);

  rpcmp::library::ValidationLimits limits;
  limits.max_dependencies_per_track = 0;
  RPCMP_CHECK(suite, open(written.bytes, limits) == LibraryError::DependencyLimitExceeded);
  limits = {};
  limits.max_total_decoded_bytes = 4;
  RPCMP_CHECK(suite, open(written.bytes, limits) == LibraryError::DecodedSizeLimitExceeded);
}

void check_thousand_track_lookup(rpcmp::test::Suite& suite) {
  constexpr std::size_t kTrackCount = 1'000;
  rpcmp::utility::NormalizedLibrary input;
  input.blobs = {{kMdx, {0x4D, 0x44, 0x58}}};
  input.tracks.reserve(kTrackCount);
  for (std::size_t index = 0; index < kTrackCount; ++index) {
    rpcmp::utility::NormalizedTrack track;
    track.format = kMdx;
    track.title = "Track " + std::to_string(index);
    track.artist = "Synthetic";
    input.tracks.push_back(std::move(track));
  }

  const auto written = rpcmp::utility::write_rpcmlib(input);
  RPCMP_CHECK(suite, written.ok());
  rpcmp::library::LogicalLibrary library;
  RPCMP_CHECK(suite,
              rpcmp::library::LogicalLibrary::open({written.bytes.data(), written.bytes.size()},
                                                   library) == LibraryError::None);
  RPCMP_CHECK(suite, library.track_count() == static_cast<std::uint32_t>(kTrackCount));

  const auto blob_id = rpcmp::utility::stable_blob_id(
      kMdx, {input.blobs[0].bytes.data(), input.blobs[0].bytes.size()});
  const auto artist_id = rpcmp::utility::stable_string_id(string_bytes(input.tracks[0].artist));
  constexpr std::array<std::size_t, 8> kLookupOrdinals{0, 1, 17, 127, 499, 731, 998, 999};
  for (const std::size_t ordinal : kLookupOrdinals) {
    const auto title_id =
        rpcmp::utility::stable_string_id(string_bytes(input.tracks[ordinal].title));
    const rpcmp::utility::TrackIdentity identity{kMdx,      blob_id, nullptr, 0, title_id,
                                                 artist_id, {},      {},      {}};
    const auto track_id = rpcmp::utility::stable_track_id(identity);
    rpcmp::library::TrackView track;
    RPCMP_CHECK(suite, library.find_track(track_id, track));
    RPCMP_CHECK(suite, std::string_view(track.title.data, track.title.size) ==
                           input.tracks[ordinal].title);
    rpcmp::library::BlobView blob;
    RPCMP_CHECK(suite, library.find_blob(track.primary_blob_id, blob));
    RPCMP_CHECK(suite, blob.bytes.size == 3);
  }
  rpcmp::library::TrackView missing;
  RPCMP_CHECK(suite, !library.find_track(rpcmp::contracts::TrackId{1}, missing));
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  check_corruption(suite);
  check_thousand_track_lookup(suite);
  return suite.finish("M1 acceptance");
}
