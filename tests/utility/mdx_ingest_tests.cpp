#include "rpcmp/library/logical_library.hpp"
#include "rpcmp/utility/mdx_ingest.hpp"
#include "rpcmp/utility/stable_id.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::uint8_t nibble(const char value) {
  if (value >= '0' && value <= '9')
    return static_cast<std::uint8_t>(value - '0');
  if (value >= 'a' && value <= 'f')
    return static_cast<std::uint8_t>(value - 'a' + 10);
  if (value >= 'A' && value <= 'F')
    return static_cast<std::uint8_t>(value - 'A' + 10);
  return 0xff;
}

std::vector<std::uint8_t> fixture(const char* const name) {
  std::ifstream input(std::string{RPCMP_SOURCE_DIR} + "/tests/fixtures/mdx/" + name);
  std::string text{std::istreambuf_iterator<char>(input), {}};
  std::vector<std::uint8_t> bytes;
  std::uint8_t high = 0xff;
  for (const char value : text) {
    const auto digit = nibble(value);
    if (digit == 0xff)
      continue;
    if (high == 0xff)
      high = digit;
    else {
      bytes.push_back(static_cast<std::uint8_t>((high << 4U) | digit));
      high = 0xff;
    }
  }
  return bytes;
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  const auto source = fixture("oracle-fm.mdx.hex");
  static rpcmp::utility::MdxIngestWorkspace workspace{};
  const rpcmp::utility::MdxIngestMetadata metadata{"Self-authored FM fixture", "RPCMP"};
  const auto first =
      rpcmp::utility::ingest_single_mdx({source.data(), source.size()}, metadata, workspace);
  const auto second =
      rpcmp::utility::ingest_single_mdx({source.data(), source.size()}, metadata, workspace);
  RPCMP_CHECK(suite, first.ok());
  RPCMP_CHECK(suite, first.library_bytes == second.library_bytes);

  rpcmp::library::LogicalLibrary library;
  RPCMP_CHECK(suite, rpcmp::library::LogicalLibrary::open(
                         {first.library_bytes.data(), first.library_bytes.size()}, library) ==
                         rpcmp::library::LibraryError::None);
  RPCMP_CHECK(suite, library.track_count() == 1);
  RPCMP_CHECK(suite, library.blob_count() == 1);
  const auto blob_id =
      rpcmp::utility::stable_blob_id(rpcmp::utility::kMdxFourcc, {source.data(), source.size()});
  rpcmp::library::BlobView blob;
  RPCMP_CHECK(suite, library.find_blob(blob_id, blob));
  RPCMP_CHECK(suite, blob.kind == rpcmp::utility::kMdxFourcc);
  RPCMP_CHECK(suite, blob.bytes.size == source.size());
  RPCMP_CHECK(suite, std::equal(source.begin(), source.end(), blob.bytes.data,
                                blob.bytes.data + blob.bytes.size));

  auto malformed = source;
  malformed.resize(3);
  const auto rejected =
      rpcmp::utility::ingest_single_mdx({malformed.data(), malformed.size()}, metadata, workspace);
  RPCMP_CHECK(suite, rejected.error == rpcmp::utility::MdxIngestError::Parse);
  RPCMP_CHECK(suite, rejected.library_bytes.empty());

  rpcmp::runtime::mdx::MdxDocument parsed;
  RPCMP_CHECK(suite, rpcmp::runtime::mdx::parse({source.data(), source.size()}, parsed).ok());
  auto active_pcm = source;
  RPCMP_CHECK(suite, parsed.tracks[8].source.size > 0);
  active_pcm[parsed.tracks[8].source_offset] = 0xe8;
  const auto pcm_rejected = rpcmp::utility::ingest_single_mdx(
      {active_pcm.data(), active_pcm.size()}, metadata, workspace);
  RPCMP_CHECK(suite, pcm_rejected.error == rpcmp::utility::MdxIngestError::Admission);
  RPCMP_CHECK(suite,
              pcm_rejected.admission.error == rpcmp::runtime::mdx::DecodeError::UnsupportedPcm);
  RPCMP_CHECK(suite, pcm_rejected.library_bytes.empty());

  auto unsupported = source;
  RPCMP_CHECK(suite, parsed.tracks[0].source.size > 0);
  unsupported[parsed.tracks[0].source_offset] = 0xe0;
  const auto unsupported_rejected = rpcmp::utility::ingest_single_mdx(
      {unsupported.data(), unsupported.size()}, metadata, workspace);
  RPCMP_CHECK(suite, unsupported_rejected.error == rpcmp::utility::MdxIngestError::Admission);
  RPCMP_CHECK(suite, unsupported_rejected.admission.error ==
                         rpcmp::runtime::mdx::DecodeError::UnsupportedExtension);
  RPCMP_CHECK(suite, unsupported_rejected.library_bytes.empty());

  auto invalid_metadata = metadata;
  invalid_metadata.title.assign(1, static_cast<char>(0xff));
  const auto bad_title = rpcmp::utility::ingest_single_mdx({source.data(), source.size()},
                                                           invalid_metadata, workspace);
  RPCMP_CHECK(suite, bad_title.error == rpcmp::utility::MdxIngestError::Write);
  RPCMP_CHECK(suite, bad_title.writer == rpcmp::utility::WriterError::InvalidUtf8);
  RPCMP_CHECK(suite, bad_title.library_bytes.empty());

  return suite.finish("MDX ingestion");
}
