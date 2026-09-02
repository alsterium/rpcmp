#include "rpcmp/player/mdx_library_session.hpp"

#include <cstddef>
#include <cstdint>

extern "C" {
extern const std::uint8_t _binary_m5_library_rpcmlib_start[];
}

namespace {

constexpr rpcmp::contracts::TrackId kFixtureTrack{0x050E01724BD96C75ULL};
constexpr std::size_t kFixtureLibraryBytes = 752;

volatile std::uint64_t result_digest{};

} // namespace

int main() {
  rpcmp::library::LogicalLibrary library;
  if (rpcmp::library::LogicalLibrary::open({_binary_m5_library_rpcmlib_start, kFixtureLibraryBytes},
                                           library) != rpcmp::library::LibraryError::None) {
    return 1;
  }

  static rpcmp::player::MdxLibrarySession session;
  static rpcmp::player::MdxLibrarySessionWorkspace workspace;
  if (!rpcmp::player::prepare_mdx_library_session(library, kFixtureTrack, session, workspace)
           .ok()) {
    return 2;
  }

  std::uint64_t digest{};
  static rpcmp::runtime::mdx::TimedYm2151Batch batch;
  for (std::size_t tick = 0; tick < 32; ++tick) {
    if (!rpcmp::runtime::mdx::advance_mdx_tick(session.document, 48'000, session.state, batch,
                                               workspace.engine)
             .ok()) {
      return 3;
    }
    for (std::size_t index = 0; index < batch.count; ++index) {
      digest = (digest * 1099511628211ULL) ^ batch.writes[index].at_tick;
      digest = (digest * 1099511628211ULL) ^ batch.writes[index].write.address;
      digest = (digest * 1099511628211ULL) ^ batch.writes[index].write.value;
    }
  }
  result_digest = digest;
  return digest == 0 ? 4 : 0;
}
