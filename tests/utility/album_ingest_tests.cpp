#include "rpcmp/library/album_catalog.hpp"
#include "rpcmp/utility/album_ingest.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <string_view>

namespace {
using namespace rpcmp::utility;

std::vector<std::uint8_t> mdx(const std::string_view title) {
  std::ifstream input(std::string{RPCMP_SOURCE_DIR} + "/tests/fixtures/mdx/oracle-fm.mdx.hex");
  std::string hex;
  std::vector<std::uint8_t> fixture;
  while (input >> hex)
    for (std::size_t i = 0; i < hex.size(); i += 2)
      fixture.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
  // The literal ASCII title "RPCMP ORACLE" is 12 bytes, then CR LF 1A and empty PDX.
  std::vector<std::uint8_t> result(title.begin(), title.end());
  result.insert(result.end(), fixture.begin() + 12, fixture.end());
  return result;
}
class MemoryInput final : public AlbumReadPort {
public:
  AlbumSourceManifest source{"Root", {}};
  std::vector<std::vector<std::uint8_t>> data;
  bool fail{};
  std::size_t reads{};
  void add(std::string path, std::vector<std::uint8_t> bytes = {},
           const AlbumEntryKind kind = AlbumEntryKind::File) {
    source.entries.push_back({std::move(path), bytes.size(), kind});
    data.push_back(std::move(bytes));
  }
  bool read_file(const std::size_t entry, std::uint8_t* const bytes,
                 const std::size_t size) override {
    ++reads;
    if (fail || entry >= data.size() || data[entry].size() != size)
      return false;
    if (size != 0)
      std::copy(data[entry].begin(), data[entry].end(), bytes);
    return true;
  }
};
std::string text(const rpcmp::library::Utf8View value) { return {value.data, value.size}; }
std::vector<std::string> titles(const AlbumIngestResult& result, const std::uint32_t ordinal) {
  rpcmp::library::AlbumCatalog catalog;
  rpcmp::library::AlbumView album;
  std::vector<std::string> output;
  if (!rpcmp::library::AlbumCatalog::open({result.bytes.data(), result.bytes.size()}, catalog)
           .ok() ||
      !catalog.album_at(ordinal, album))
    return output;
  for (std::uint32_t i = 0; i < album.track_count; ++i) {
    rpcmp::library::TrackView track;
    if (!catalog.track_at(album.album_id, i, track))
      return {};
    output.push_back(text(track.title));
  }
  return output;
}
class Output final : public AlbumOutputPort {
public:
  int fail_step{};
  int step{};
  bool fail_cleanup{};
  bool discarded{};
  std::vector<std::uint8_t> destination{9, 8, 7};
  std::vector<std::uint8_t> temporary;
  bool begin() override { return ++step != fail_step; }
  bool write(const rpcmp::library::ByteView bytes) override {
    temporary.assign(bytes.data, bytes.data + bytes.size);
    return ++step != fail_step;
  }
  bool finish() override { return ++step != fail_step; }
  bool publish() override {
    if (++step == fail_step)
      return false;
    destination = temporary;
    temporary.clear();
    return true;
  }
  bool discard() override {
    discarded = true;
    if (fail_cleanup)
      return false;
    temporary.clear();
    return true;
  }
};
} // namespace

int main() {
  rpcmp::test::Suite suite;
  static MdxIngestWorkspace workspace;
  MemoryInput input;
  for (const auto* directory : {"10", "10/Game", "2", "2/Game"})
    input.add(directory, {}, AlbumEntryKind::Directory);
  input.add("2/Game/10.mdx", mdx("alpha"));
  input.add("2/Game/02.mdx", mdx("omega"));
  input.add("2/Game/2.mdx", mdx("beta"));
  input.add("2/Game/1.MDX", mdx("zeta"));
  input.add("00.mdx", mdx("root"));
  input.add("10/Game/1.mdx", mdx("tail"));
  input.add("readme.txt", {1, 2});
  input.add(".mdx", {1});
  input.add("linked", {}, AlbumEntryKind::Link);
  const auto first = ingest_album_sources(input.source, input, workspace);
  RPCMP_CHECK(suite, first.status == AlbumIngestStatus::Complete && first.accepted == 6 &&
                         first.skipped == 3 && input.reads == 6);
  RPCMP_CHECK(suite, titles(first, 0) == std::vector<std::string>{"root"});
  const std::vector<std::string> expected{"zeta", "omega", "beta", "alpha"};
  RPCMP_CHECK(suite, titles(first, 1) == expected);
  RPCMP_CHECK(suite, titles(first, 2) == std::vector<std::string>{"tail"});
  std::reverse(input.source.entries.begin(), input.source.entries.end());
  std::reverse(input.data.begin(), input.data.end());
  const auto reversed = ingest_album_sources(input.source, input, workspace);
  RPCMP_CHECK(suite, reversed.ok() && reversed.bytes == first.bytes);
  RPCMP_CHECK(suite, std::is_sorted(reversed.diagnostics.begin(), reversed.diagnostics.end(),
                                    [](const AlbumDiagnostic& a, const AlbumDiagnostic& b) {
                                      return a.path < b.path;
                                    }));

  MemoryInput numeric;
  numeric.add("01a10.mdx", mdx("last"));
  numeric.add("1a2.mdx", mdx("first"));
  numeric.add(std::string(100, '9') + ".mdx", mdx("100-digit"));
  numeric.add("1" + std::string(100, '0') + ".mdx", mdx("101-digit"));
  numeric.add("000.mdx", mdx("zeros"));
  numeric.add("0.mdx", mdx("zero"));
  const std::vector<std::string> numeric_expected{"zero", "zeros",     "first",
                                                  "last", "100-digit", "101-digit"};
  RPCMP_CHECK(suite, titles(ingest_album_sources(numeric.source, numeric, workspace), 0) ==
                         numeric_expected);

  MemoryInput mixed;
  mixed.add("01.mdx", mdx(""));
  mixed.add("broken.mdx", {1, 2, 3});
  auto pcm = mdx("pcm");
  // BASE follows title, three-byte delimiter and empty PDX. P's offset is 0x30
  // in the independent fixture, with f1 00 changed to unsupported PCM opcode e8.
  pcm[3 + 4 + 0x30] = 0xe8;
  mixed.add("pcm.mdx", pcm);
  const auto partial = ingest_album_sources(mixed.source, mixed, workspace);
  RPCMP_CHECK(suite, partial.status == AlbumIngestStatus::WithExclusions && partial.accepted == 1 &&
                         partial.excluded == 2 &&
                         partial.diagnostics[0].fallback == TitleFallback::Empty);
  RPCMP_CHECK(suite, titles(partial, 0) == std::vector<std::string>{"01"});
  RPCMP_CHECK(suite, partial.diagnostics[2].admission.error ==
                         rpcmp::runtime::mdx::DecodeError::UnsupportedPcm);

  MemoryInput duplicate;
  duplicate.add("1.mdx", mdx("same"));
  duplicate.add("2.mdx", mdx("same"));
  const auto collision = ingest_album_sources(duplicate.source, duplicate, workspace);
  RPCMP_CHECK(suite, collision.error == AlbumIngestError::DuplicateTrack &&
                         collision.bytes.empty() && collision.error_path == "2.mdx" &&
                         collision.conflicting_path == "1.mdx");
  MemoryInput names;
  names.add(u8"が", {}, AlbumEntryKind::Directory);
  names.add(u8"か\u3099", {}, AlbumEntryKind::Directory);
  RPCMP_CHECK(suite, ingest_album_sources(names.source, names, workspace).error ==
                             AlbumIngestError::NameCollision &&
                         names.reads == 0);
  names.source.entries.resize(1);
  names.source.entries[0].relative_path = "../outside.mdx";
  RPCMP_CHECK(suite, ingest_album_sources(names.source, names, workspace).error ==
                         AlbumIngestError::InvalidSource);
  names.source.entries[0].relative_path = "missing/1.mdx";
  RPCMP_CHECK(suite, ingest_album_sources(names.source, names, workspace).error ==
                         AlbumIngestError::InvalidSource);
  names.source.entries[0].relative_path = "\xff.mdx";
  RPCMP_CHECK(suite, ingest_album_sources(names.source, names, workspace).error ==
                         AlbumIngestError::InvalidSource);

  MemoryInput empty;
  empty.add("bad.mdx", {});
  RPCMP_CHECK(suite, ingest_album_sources(empty.source, empty, workspace).error ==
                         AlbumIngestError::NoTracks);
  empty.source.entries[0].size = 1048577;
  empty.reads = 0;
  RPCMP_CHECK(suite, ingest_album_sources(empty.source, empty, workspace).error ==
                             AlbumIngestError::Capacity &&
                         empty.reads == 0);
  empty.source.entries.resize(kAlbumMaxSourceEntries + 1);
  RPCMP_CHECK(suite, ingest_album_sources(empty.source, empty, workspace).error ==
                         AlbumIngestError::Capacity);
  duplicate.fail = true;
  RPCMP_CHECK(suite, ingest_album_sources(duplicate.source, duplicate, workspace).error ==
                         AlbumIngestError::Read);

  MemoryInput title_limit;
  std::string japanese_title;
  for (std::size_t i = 0; i < 1366; ++i)
    japanese_title += "\x82\xa0"; // 2732 CP932 bytes become 4098 UTF-8 bytes.
  title_limit.add("1.mdx", mdx(japanese_title));
  const auto too_long = ingest_album_sources(title_limit.source, title_limit, workspace);
  RPCMP_CHECK(suite, too_long.error == AlbumIngestError::Capacity && too_long.excluded == 0 &&
                         too_long.bytes.empty() && too_long.diagnostics.size() == 1 &&
                         too_long.diagnostics[0].kind == AlbumDiagnosticKind::Error);
  title_limit.source.entries[0].relative_path = std::string(4097, 'x');
  title_limit.reads = 0;
  const auto oversized_path = ingest_album_sources(title_limit.source, title_limit, workspace);
  RPCMP_CHECK(suite, oversized_path.error == AlbumIngestError::Capacity && title_limit.reads == 0);
  RPCMP_CHECK(suite, oversized_path.error_path.empty());
  title_limit.source.entries[0].relative_path.clear();
  for (std::size_t i = 0; i < 64; ++i)
    title_limit.source.entries[0].relative_path += "d/";
  title_limit.source.entries[0].relative_path += "1.mdx";
  RPCMP_CHECK(suite, ingest_album_sources(title_limit.source, title_limit, workspace).error ==
                             AlbumIngestError::Capacity &&
                         title_limit.reads == 0);

  MemoryInput maximum;
  for (std::size_t i = 0; i < 300; ++i)
    maximum.add(std::to_string(i) + ".mdx", mdx(""));
  const auto admitted = ingest_album_sources(maximum.source, maximum, workspace);
  RPCMP_CHECK(suite,
              admitted.ok() && admitted.accepted == 300 && titles(admitted, 0).size() == 300);
  maximum.add("300.mdx", mdx(""));
  const auto over = ingest_album_sources(maximum.source, maximum, workspace);
  RPCMP_CHECK(suite,
              over.error == AlbumIngestError::Capacity && over.bytes.empty() && over.excluded == 0);

  const std::array<std::uint8_t, 3> bytes{1, 2, 3};
  const std::array<AlbumOutputError, 4> errors{AlbumOutputError::Begin, AlbumOutputError::Write,
                                               AlbumOutputError::Finish, AlbumOutputError::Publish};
  for (int step = 1; step <= 4; ++step) {
    Output output;
    output.fail_step = step;
    const auto rejected = publish_album({bytes.data(), bytes.size()}, output);
    const std::vector<std::uint8_t> old{9, 8, 7};
    RPCMP_CHECK(suite, rejected.error == errors[static_cast<std::size_t>(step - 1)] &&
                           !rejected.cleanup_failed && output.discarded &&
                           output.temporary.empty() && output.destination == old &&
                           output.step == step);
  }
  Output failed_cleanup;
  failed_cleanup.fail_step = 2;
  failed_cleanup.fail_cleanup = true;
  RPCMP_CHECK(suite, publish_album({bytes.data(), bytes.size()}, failed_cleanup).cleanup_failed);
  Output success;
  RPCMP_CHECK(suite,
              publish_album({bytes.data(), bytes.size()}, success).ok() && !success.discarded &&
                  success.destination == std::vector<std::uint8_t>(bytes.begin(), bytes.end()));
  Output invalid;
  RPCMP_CHECK(suite, publish_album({}, invalid).error == AlbumOutputError::InvalidSize &&
                         invalid.step == 0);
  return suite.finish("album ingestion");
}
