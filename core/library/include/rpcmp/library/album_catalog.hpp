#ifndef RPCMP_LIBRARY_ALBUM_CATALOG_HPP
#define RPCMP_LIBRARY_ALBUM_CATALOG_HPP

#include "rpcmp/contracts/catalog.hpp"
#include "rpcmp/library/logical_library.hpp"

#include <array>
#include <cstdint>

namespace rpcmp::library {

inline constexpr std::uint32_t kAlbumMaxTracks = 300;
inline constexpr std::uint32_t kAlbumMaxAlbums = 300;
inline constexpr std::uint64_t kAlbumMaxFileBytes = 32ULL * 1024 * 1024;
inline constexpr std::uint64_t kAlbumMaxBlobBytes = 1024ULL * 1024;
inline constexpr std::uint64_t kAlbumMaxStringBytes = 1024ULL * 1024;

enum class CatalogError : std::uint8_t {
  None = 0,
  Library,
  MissingSection,
  UnsupportedVersion,
  InvalidHeader,
  Capacity,
  InvalidAlbum,
  InvalidOrder,
  InvalidMembership,
  InvalidReference
};
struct CatalogResult {
  CatalogError error{CatalogError::None};
  LibraryError library_error{LibraryError::None};
  [[nodiscard]] bool ok() const noexcept { return error == CatalogError::None; }
};
struct AlbumView {
  contracts::AlbumId album_id{};
  std::uint32_t display_ordinal{};
  Utf8View name{};
  std::uint32_t track_count{};
};

// UTF-8/NFC validation is the producer's separate metadata responsibility;
// this helper checks the portable relative-key syntax only.
[[nodiscard]] bool valid_album_key(Utf8View key) noexcept;

// Immutable borrowed storage. An unsuccessful open never replaces output.
class AlbumCatalog {
public:
  [[nodiscard]] static CatalogResult open(ByteView file, AlbumCatalog& output);
  [[nodiscard]] std::uint32_t album_count() const noexcept { return album_count_; }
  [[nodiscard]] std::uint32_t track_count() const noexcept { return library_.track_count(); }
  [[nodiscard]] bool album_at(std::uint32_t display_ordinal, AlbumView& output) const;
  [[nodiscard]] bool find_album(contracts::AlbumId id, AlbumView& output) const;
  [[nodiscard]] bool track_at(contracts::AlbumId album, std::uint32_t ordinal,
                              TrackView& output) const;
  [[nodiscard]] const LogicalLibrary& library() const noexcept { return library_; }

private:
  [[nodiscard]] const std::uint8_t* album_record(contracts::AlbumId id) const;
  void album_view(const std::uint8_t* record, AlbumView& output) const;
  LogicalLibrary library_{};
  const std::uint8_t* albums_{};
  const std::uint8_t* members_{};
  std::uint32_t album_count_{};
  std::array<std::uint16_t, kAlbumMaxAlbums> display_indices_{};
};

} // namespace rpcmp::library

#endif // RPCMP_LIBRARY_ALBUM_CATALOG_HPP
