#include "rpcmp/player/mdx_library_session.hpp"
#include "rpcmp/utility/mdx_ingest.hpp"
#include "rpcmp/utility/stable_id.hpp"
#include "test_support.hpp"

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

std::vector<std::uint8_t> fixture() {
  std::ifstream input(std::string{RPCMP_SOURCE_DIR} + "/tests/fixtures/mdx/oracle-fm.mdx.hex");
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

rpcmp::library::ByteView utf8_bytes(const std::string& value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

bool equal(const rpcmp::runtime::mdx::TimedYm2151Batch& left,
           const rpcmp::runtime::mdx::TimedYm2151Batch& right) {
  if (left.count != right.count)
    return false;
  for (std::size_t index = 0; index < left.count; ++index) {
    const auto& lhs = left.writes[index];
    const auto& rhs = right.writes[index];
    if (lhs.at_tick != rhs.at_tick || lhs.write.address != rhs.write.address ||
        lhs.write.value != rhs.write.value)
      return false;
  }
  return true;
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  const auto source = fixture();
  const rpcmp::utility::MdxIngestMetadata metadata{"Self-authored FM fixture", "RPCMP"};
  static rpcmp::utility::MdxIngestWorkspace ingest_workspace{};
  const auto ingested =
      rpcmp::utility::ingest_single_mdx({source.data(), source.size()}, metadata, ingest_workspace);
  RPCMP_CHECK(suite, ingested.ok());

  rpcmp::library::LogicalLibrary library;
  RPCMP_CHECK(suite, rpcmp::library::LogicalLibrary::open(
                         {ingested.library_bytes.data(), ingested.library_bytes.size()}, library) ==
                         rpcmp::library::LibraryError::None);

  const auto blob_id =
      rpcmp::utility::stable_blob_id(rpcmp::library::kMdxFourcc, {source.data(), source.size()});
  const auto title_id = rpcmp::utility::stable_string_id(utf8_bytes(metadata.title));
  const auto artist_id = rpcmp::utility::stable_string_id(utf8_bytes(metadata.artist));
  const rpcmp::utility::TrackIdentity identity{
      rpcmp::library::kMdxFourcc, blob_id, nullptr, 0, title_id, artist_id, {}, {}, {}};
  const auto track_id = rpcmp::utility::stable_track_id(identity);

  static rpcmp::player::MdxLibrarySessionWorkspace session_workspace{};
  rpcmp::player::MdxLibrarySession session;
  const auto prepared =
      rpcmp::player::prepare_mdx_library_session(library, track_id, session, session_workspace);
  RPCMP_CHECK(suite, prepared.ok());
  RPCMP_CHECK(suite, session.track_id == track_id);
  RPCMP_CHECK(suite, session.primary_blob_id == blob_id);

  rpcmp::runtime::mdx::MdxDocument direct_document;
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::parse({source.data(), source.size()}, direct_document).ok());
  static rpcmp::runtime::mdx::MdxEngineScratch direct_scratch{};
  rpcmp::runtime::mdx::DocumentValidation direct_validation;
  RPCMP_CHECK(suite, rpcmp::runtime::mdx::prepare_mdx_playback(direct_document, direct_validation,
                                                               direct_scratch)
                         .ok());
  rpcmp::runtime::mdx::MdxEngineState direct_state;
  for (std::size_t tick = 0; tick < 32; ++tick) {
    rpcmp::runtime::mdx::TimedYm2151Batch via_library;
    rpcmp::runtime::mdx::TimedYm2151Batch direct;
    RPCMP_CHECK(suite,
                rpcmp::runtime::mdx::advance_mdx_tick(session.document, 48'000, session.state,
                                                      via_library, session_workspace.engine)
                    .ok());
    RPCMP_CHECK(suite, rpcmp::runtime::mdx::advance_mdx_tick(direct_document, 48'000, direct_state,
                                                             direct, direct_scratch)
                           .ok());
    RPCMP_CHECK(suite, equal(via_library, direct));
  }

  rpcmp::player::MdxLibrarySession unchanged = session;
  const auto missing = rpcmp::player::prepare_mdx_library_session(
      library, rpcmp::contracts::TrackId{track_id.value + 1}, session, session_workspace);
  RPCMP_CHECK(suite, missing.error == rpcmp::player::MdxSessionError::TrackNotFound);
  RPCMP_CHECK(suite, session.track_id == unchanged.track_id);
  RPCMP_CHECK(suite,
              session.state.timeline.scheduler_tick == unchanged.state.timeline.scheduler_tick);

  return suite.finish("MDX logical-library session");
}
