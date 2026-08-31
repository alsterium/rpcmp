#include "rpcmp/utility/stable_id.hpp"
#include "test_support.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace {

rpcmp::library::ByteView bytes(const std::string_view value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

} // namespace

int main() {
  rpcmp::test::Suite suite;

  constexpr std::array<std::uint8_t, 32> kEmptyDigest{
      0xE3, 0xB0, 0xC4, 0x42, 0x98, 0xFC, 0x1C, 0x14, 0x9A, 0xFB, 0xF4,
      0xC8, 0x99, 0x6F, 0xB9, 0x24, 0x27, 0xAE, 0x41, 0xE4, 0x64, 0x9B,
      0x93, 0x4C, 0xA4, 0x95, 0x99, 0x1B, 0x78, 0x52, 0xB8, 0x55};
  RPCMP_CHECK(suite, rpcmp::utility::sha256({nullptr, 0}) == kEmptyDigest);

  constexpr std::uint32_t kMdx = 0x2058444DU;
  const std::array<std::uint8_t, 3> payload{1, 2, 3};
  const auto blob = rpcmp::utility::stable_blob_id(kMdx, {payload.data(), payload.size()});
  const auto title = rpcmp::utility::stable_string_id(bytes("Title"));
  const auto artist = rpcmp::utility::stable_string_id(bytes("Artist"));
  RPCMP_CHECK(suite, blob.value == 0xA75C21A0C642594AULL);
  RPCMP_CHECK(suite, title.value == 0x693B325E565DAF71ULL);
  RPCMP_CHECK(suite,
              !(rpcmp::utility::stable_blob_id(kMdx, {payload.data(), payload.size()}, 1) == blob));

  const rpcmp::utility::TrackIdentity identity{kMdx, blob, nullptr, 0, title, artist, {}, {}, {}};
  RPCMP_CHECK(suite, rpcmp::utility::stable_track_id(identity).value == 0x3AED84079591D01DULL);
  RPCMP_CHECK(suite, !(rpcmp::utility::stable_track_id(identity, 1) ==
                       rpcmp::utility::stable_track_id(identity)));

  return suite.finish("stable IDs");
}
