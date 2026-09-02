#include "rpcmp/player/mdx_library_session.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr rpcmp::contracts::TrackId kFixtureTrack{0x050E01724BD96C75ULL};

std::uint8_t nibble(const char value) {
  if (value >= '0' && value <= '9')
    return static_cast<std::uint8_t>(value - '0');
  if (value >= 'a' && value <= 'f')
    return static_cast<std::uint8_t>(value - 'a' + 10);
  if (value >= 'A' && value <= 'F')
    return static_cast<std::uint8_t>(value - 'A' + 10);
  return 0xff;
}

std::vector<std::uint8_t> load_hex(const char* const path) {
  std::ifstream input(path);
  const std::string text{std::istreambuf_iterator<char>(input), {}};
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

int main(int argc, char** argv) {
  if (argc != 2)
    return 1;
  const auto bytes = load_hex(argv[1]);
  rpcmp::library::LogicalLibrary library;
  if (rpcmp::library::LogicalLibrary::open({bytes.data(), bytes.size()}, library) !=
      rpcmp::library::LibraryError::None)
    return 2;

  static rpcmp::player::MdxLibrarySession session;
  static rpcmp::player::MdxLibrarySessionWorkspace workspace;
  if (!rpcmp::player::prepare_mdx_library_session(library, kFixtureTrack, session, workspace).ok())
    return 3;

  std::uint64_t writes{};
  std::uint64_t digest{};
  static rpcmp::runtime::mdx::TimedYm2151Batch batch;
  for (std::size_t tick = 0; tick < 32; ++tick) {
    if (!rpcmp::runtime::mdx::advance_mdx_tick(session.document, 48'000, session.state, batch,
                                               workspace.engine)
             .ok())
      return 4;
    writes += batch.count;
    for (std::size_t index = 0; index < batch.count; ++index) {
      digest = (digest * 1099511628211ULL) ^ batch.writes[index].at_tick;
      digest = (digest * 1099511628211ULL) ^ batch.writes[index].write.address;
      digest = (digest * 1099511628211ULL) ^ batch.writes[index].write.value;
    }
  }
  std::printf("M5_SESSION:PASS writes=%llu digest=%016llx\n",
              static_cast<unsigned long long>(writes), static_cast<unsigned long long>(digest));
  return digest == 0 ? 5 : 0;
}
