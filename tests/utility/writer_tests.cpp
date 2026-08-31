#include "rpcmp/library/logical_library.hpp"
#include "rpcmp/utility/writer.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t kMdx = 0x2058444DU;
constexpr std::uint32_t kPdx = 0x20584450U;
constexpr std::uint32_t kSamples = 0x20584450U;

rpcmp::utility::NormalizedLibrary fixture() {
  rpcmp::utility::NormalizedLibrary input;
  input.blobs = {{kMdx, {1, 2, 3}}, {kPdx, {4, 5}}};
  rpcmp::utility::NormalizedTrack first;
  first.format = kMdx;
  first.primary_blob_index = 0;
  first.title = "First";
  first.artist = "Artist";
  first.dependencies.push_back({kSamples, 1});
  first.year = static_cast<std::uint16_t>(1990);
  first.estimate_confidence = 2;
  rpcmp::utility::NormalizedTrack second;
  second.format = kMdx;
  second.primary_blob_index = 0;
  second.title = "Second";
  second.artist = "Artist";
  input.tracks = {first, second};
  return input;
}

rpcmp::utility::NormalizedLibrary minimal_fixture() {
  rpcmp::utility::NormalizedLibrary input;
  input.blobs = {{kMdx, {1, 2, 3}}};
  rpcmp::utility::NormalizedTrack track;
  track.format = kMdx;
  track.title = "Title";
  track.artist = "Artist";
  input.tracks = {track};
  return input;
}

std::uint8_t hex_nibble(const char value) {
  if (value >= '0' && value <= '9') {
    return static_cast<std::uint8_t>(value - '0');
  }
  if (value >= 'a' && value <= 'f') {
    return static_cast<std::uint8_t>(value - 'a' + 10);
  }
  return 0xFFU;
}

std::vector<std::uint8_t> read_golden(const char* const filename) {
  const std::string path = std::string{RPCMP_SOURCE_DIR} + "/tests/fixtures/rpcmlib/" + filename;
  std::ifstream input(path);
  std::string hex;
  std::string line;
  while (std::getline(input, line)) {
    hex += line;
  }
  std::vector<std::uint8_t> bytes;
  if ((hex.size() & 1U) != 0) {
    return bytes;
  }
  bytes.reserve(hex.size() / 2);
  for (std::size_t index = 0; index < hex.size(); index += 2) {
    const std::uint8_t high = hex_nibble(hex[index]);
    const std::uint8_t low = hex_nibble(hex[index + 1]);
    if (high == 0xFFU || low == 0xFFU) {
      return {};
    }
    bytes.push_back(static_cast<std::uint8_t>((high << 4U) | low));
  }
  return bytes;
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  auto input = fixture();
  const auto first = rpcmp::utility::write_rpcmlib(input);
  const auto second = rpcmp::utility::write_rpcmlib(input);
  RPCMP_CHECK(suite, first.ok());
  RPCMP_CHECK(suite, first.bytes == second.bytes);
  RPCMP_CHECK(suite, first.bytes == read_golden("multi-track.rpcmlib.hex"));

  const auto minimal = rpcmp::utility::write_rpcmlib(minimal_fixture());
  RPCMP_CHECK(suite, minimal.ok());
  RPCMP_CHECK(suite, minimal.bytes == read_golden("minimal.rpcmlib.hex"));

  auto reordered = fixture();
  std::reverse(reordered.blobs.begin(), reordered.blobs.end());
  for (auto& track : reordered.tracks) {
    track.primary_blob_index = 1;
  }
  reordered.tracks[0].dependencies[0].blob_index = 0;
  std::reverse(reordered.tracks.begin(), reordered.tracks.end());
  RPCMP_CHECK(suite, rpcmp::utility::write_rpcmlib(reordered).bytes == first.bytes);

  rpcmp::library::LogicalLibrary library;
  RPCMP_CHECK(suite,
              rpcmp::library::LogicalLibrary::open({first.bytes.data(), first.bytes.size()},
                                                   library) == rpcmp::library::LibraryError::None);
  RPCMP_CHECK(suite, library.track_count() == 2);
  RPCMP_CHECK(suite, library.blob_count() == 2);

  input.blobs.push_back(input.blobs.front());
  input.tracks[0].primary_blob_index = 2;
  const auto deduplicated = rpcmp::utility::write_rpcmlib(input);
  RPCMP_CHECK(suite, deduplicated.ok());
  RPCMP_CHECK(suite, deduplicated.bytes == first.bytes);

  auto invalid = fixture();
  invalid.tracks[0].title = std::string(1, static_cast<char>(0xFF));
  RPCMP_CHECK(suite, rpcmp::utility::write_rpcmlib(invalid).error ==
                         rpcmp::utility::WriterError::InvalidUtf8);
  invalid = fixture();
  invalid.tracks[0].primary_blob_index = 99;
  RPCMP_CHECK(suite, rpcmp::utility::write_rpcmlib(invalid).error ==
                         rpcmp::utility::WriterError::InvalidInput);

  return suite.finish("rpcmlib writer");
}
