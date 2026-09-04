#include "rpcmp/player/mdx_library_session.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <type_traits>

extern "C" {
extern const std::uint8_t _binary_m5_library_rpcmlib_start[];
void rpcmp_pocket_terminal_init();
[[noreturn]] void rpcmp_pocket_hold_result();
}

namespace {

constexpr rpcmp::contracts::TrackId kFixtureTrack{0x050E01724BD96C75ULL};
constexpr std::size_t kFixtureLibraryBytes = 752;
constexpr std::uint64_t kExpectedWrites = 33;
constexpr std::uint64_t kExpectedDigest = 0xf1f04f5a8695a112ULL;

volatile std::uint64_t result_digest{};

template <typename T> T& construct_in_zero_backed_storage() noexcept {
  static_assert(std::is_trivially_destructible_v<T>);
  alignas(T) static std::byte storage[sizeof(T)]{};
  return *::new (static_cast<void*>(storage)) T{};
}

} // namespace

int main() {
  rpcmp_pocket_terminal_init();
  std::printf("RPCMP M5 BSS probe\n\n");

  rpcmp::library::LogicalLibrary library;
  if (rpcmp::library::LogicalLibrary::open({_binary_m5_library_rpcmlib_start, kFixtureLibraryBytes},
                                           library) != rpcmp::library::LibraryError::None) {
    std::printf("M5 BSS: FAIL 1\n");
    rpcmp_pocket_hold_result();
  }

  auto& session = construct_in_zero_backed_storage<rpcmp::player::MdxLibrarySession>();
  auto& workspace = construct_in_zero_backed_storage<rpcmp::player::MdxLibrarySessionWorkspace>();
  if (!rpcmp::player::prepare_mdx_library_session(library, kFixtureTrack, session, workspace)
           .ok()) {
    std::printf("M5 BSS: FAIL 2\n");
    rpcmp_pocket_hold_result();
  }

  std::uint64_t writes{};
  std::uint64_t digest{};
  static rpcmp::runtime::mdx::TimedYm2151Batch batch;
  for (std::size_t tick = 0; tick < 32; ++tick) {
    if (!rpcmp::runtime::mdx::advance_mdx_tick(session.document, 48'000, session.state, batch,
                                               workspace.engine)
             .ok()) {
      std::printf("M5 BSS: FAIL 3\n");
      rpcmp_pocket_hold_result();
    }
    writes += batch.count;
    for (std::size_t index = 0; index < batch.count; ++index) {
      digest = (digest * 1099511628211ULL) ^ batch.writes[index].at_tick;
      digest = (digest * 1099511628211ULL) ^ batch.writes[index].write.address;
      digest = (digest * 1099511628211ULL) ^ batch.writes[index].write.value;
    }
  }
  result_digest = digest;
  if (writes != kExpectedWrites || digest != kExpectedDigest) {
    std::printf("M5 BSS: FAIL 4\n");
    rpcmp_pocket_hold_result();
  }
  std::printf("M5 BSS: PASS\n");
  std::printf("W=%llu D=%016llx\n", static_cast<unsigned long long>(writes),
              static_cast<unsigned long long>(digest));
  std::printf("Open Pocket menu to exit.\n");
  rpcmp_pocket_hold_result();
}
