#ifndef RPCMP_UTILITY_STABLE_ID_HPP
#define RPCMP_UTILITY_STABLE_ID_HPP

#include "rpcmp/contracts/types.hpp"
#include "rpcmp/library/container.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rpcmp::utility {

struct DependencyIdentity {
  std::uint32_t role{};
  contracts::BlobId blob_id{};
};

struct TrackIdentity {
  std::uint32_t format{};
  contracts::BlobId primary_blob_id{};
  const DependencyIdentity* dependencies{};
  std::size_t dependency_count{};
  contracts::StringId title_id{};
  contracts::StringId artist_id{};
  contracts::StringId album_id{};
  contracts::StringId composer_id{};
  contracts::StringId system_id{};
};

[[nodiscard]] std::array<std::uint8_t, 32> sha256(library::ByteView bytes);
[[nodiscard]] contracts::BlobId stable_blob_id(std::uint32_t kind, library::ByteView bytes,
                                               std::uint32_t collision_counter = 0);
[[nodiscard]] contracts::StringId stable_string_id(library::ByteView normalized_utf8,
                                                   std::uint32_t collision_counter = 0);
[[nodiscard]] contracts::TrackId stable_track_id(const TrackIdentity& track,
                                                 std::uint32_t collision_counter = 0);

} // namespace rpcmp::utility

#endif // RPCMP_UTILITY_STABLE_ID_HPP
