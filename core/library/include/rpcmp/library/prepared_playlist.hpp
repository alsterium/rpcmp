#ifndef RPCMP_LIBRARY_PREPARED_PLAYLIST_HPP
#define RPCMP_LIBRARY_PREPARED_PLAYLIST_HPP

#include "rpcmp/contracts/minimal_player.hpp"
#include "rpcmp/library/container.hpp"

namespace rpcmp::library {
inline constexpr std::uint32_t kPreparedPairLimit = 16U * 1024 * 1024;
inline constexpr std::uint32_t kPreparedFileLimit = 512U * 1024 * 1024;
struct PreparedBuffer {
  std::uint8_t* data{};
  std::size_t size{};
};
class PlaylistSource {
public:
  virtual ~PlaylistSource() = default;
  virtual bool size(std::uint32_t& bytes) = 0;
  virtual bool read(std::uint32_t offset, std::uint8_t* output, std::uint32_t bytes) = 0;
};
class PreparedPlaylist final : public contracts::minimal::TrackList {
public:
  bool open(PlaylistSource& source);
  [[nodiscard]] std::uint32_t count() const noexcept override { return count_; }
  [[nodiscard]] contracts::CatalogText title(contracts::TrackId id) const noexcept override;
  // Caller owns zero-padded storage; no offsets escape this adapter.
  bool sizes(contracts::TrackId id, std::uint32_t& mdx, std::uint32_t& pdx) const noexcept;
  bool load(contracts::TrackId id, PreparedBuffer mdx, PreparedBuffer pdx);

private:
  struct Entry {
    std::uint32_t mdx_offset{}, mdx_size{}, mdx_crc{}, pdx_offset{}, pdx_size{}, pdx_crc{};
    contracts::CatalogText title{};
  };
  std::array<Entry, 300> entries_{};
  std::array<std::uint8_t, std::size_t{300} * 128> index_{};
  PlaylistSource* source_{};
  std::uint32_t count_{};
};
} // namespace rpcmp::library
#endif
