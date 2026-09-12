#ifndef RPCMP_CONTRACTS_CATALOG_HPP
#define RPCMP_CONTRACTS_CATALOG_HPP

#include "rpcmp/contracts/types.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace rpcmp::contracts {

struct AlbumId {
  std::uint64_t value{};
};
constexpr bool operator==(AlbumId left, AlbumId right) noexcept {
  return left.value == right.value;
}
constexpr bool operator!=(AlbumId left, AlbumId right) noexcept { return !(left == right); }

struct LibraryGeneration {
  std::uint64_t value{};
};
constexpr bool operator==(LibraryGeneration left, LibraryGeneration right) noexcept {
  return left.value == right.value;
}
constexpr bool operator!=(LibraryGeneration left, LibraryGeneration right) noexcept {
  return !(left == right);
}

inline constexpr std::uint16_t kCatalogSchemaVersion = 1;
inline constexpr std::uint16_t kCatalogPageCapacity = 16;
inline constexpr std::uint32_t kCatalogMaxItems = 300;

struct CatalogText {
  std::array<char, kMaxMetadataBytes> bytes{};
  std::uint16_t length{};
  bool truncated{};
};

enum class CatalogPhase : std::uint8_t { Empty, Loading, Ready, Error };
enum class CatalogFailure : std::uint8_t { None, InvalidLibrary, ReadFailed, GenerationExhausted };
struct CatalogStatus {
  std::uint16_t schema_version{kCatalogSchemaVersion};
  LibraryGeneration generation{};
  CatalogPhase phase{CatalogPhase::Empty};
  CatalogFailure failure{CatalogFailure::None};
  std::uint32_t album_count{};
  std::uint32_t track_count{};
};

struct CatalogPageQuery {
  std::uint16_t schema_version{kCatalogSchemaVersion};
  LibraryGeneration generation{};
  std::uint32_t start_ordinal{};
  std::uint16_t limit{kCatalogPageCapacity};
};
enum class CatalogQueryError : std::uint8_t {
  None,
  UnsupportedSchema,
  Unavailable,
  StaleLibrary,
  InvalidLimit,
  UnknownAlbum,
  InvalidStart
};
struct CatalogPageHeader {
  std::uint16_t schema_version{kCatalogSchemaVersion};
  CatalogPageQuery query{};
  CatalogQueryError error{CatalogQueryError::None};
  std::uint32_t total{};
  std::uint16_t count{};
  std::optional<std::uint32_t> next_start;
  [[nodiscard]] bool ok() const noexcept { return error == CatalogQueryError::None; }
};
struct CatalogAlbumItem {
  AlbumId album_id{};
  std::uint32_t display_ordinal{};
  CatalogText name{};
  std::uint32_t track_count{};
};
struct CatalogTrackItem {
  TrackId track_id{};
  AlbumId album_id{};
  std::uint32_t track_ordinal{};
  CatalogText title{};
};
struct CatalogAlbumPage {
  CatalogPageHeader header{};
  std::array<CatalogAlbumItem, kCatalogPageCapacity> items{};
};
struct CatalogTrackPage {
  CatalogPageHeader header{};
  AlbumId album_id{};
  std::array<CatalogTrackItem, kCatalogPageCapacity> items{};
};

// Copies only; the UI cannot obtain lifecycle mutators or borrowed library storage.
class CatalogReader {
public:
  virtual ~CatalogReader() = default;
  [[nodiscard]] virtual CatalogStatus status() const noexcept = 0;
  [[nodiscard]] virtual CatalogAlbumPage albums(const CatalogPageQuery& query) const = 0;
  [[nodiscard]] virtual CatalogTrackPage tracks(AlbumId album,
                                                const CatalogPageQuery& query) const = 0;
};

[[nodiscard]] bool valid_catalog_status(const CatalogStatus& status) noexcept;
[[nodiscard]] bool valid_catalog_page(const CatalogAlbumPage& page) noexcept;
[[nodiscard]] bool valid_catalog_page(const CatalogTrackPage& page) noexcept;

} // namespace rpcmp::contracts

#endif // RPCMP_CONTRACTS_CATALOG_HPP
