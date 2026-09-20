#ifndef RPCMP_PLAYER_CATALOG_SESSION_HPP
#define RPCMP_PLAYER_CATALOG_SESSION_HPP

#include "rpcmp/contracts/catalog.hpp"
#include "rpcmp/library/album_catalog.hpp"

namespace rpcmp::player {

enum class CatalogChangeError : std::uint8_t {
  None,
  StaleLibrary,
  NotLoading,
  InvalidLibrary,
  ReadFailed,
  GenerationExhausted
};
struct CatalogChangeResult {
  CatalogChangeError error{CatalogChangeError::None};
  contracts::LibraryGeneration generation{};
  library::CatalogResult validation{};
  [[nodiscard]] bool ok() const noexcept { return error == CatalogChangeError::None; }
};
enum class CatalogTrackError : std::uint8_t { None, Unavailable, StaleLibrary, UnknownTrack };

// Core control context only. Ready borrows immutable bytes until invalidation;
// published pages own their contents. UI receives only const CatalogReader&.
class CatalogSession final : public contracts::CatalogReader {
public:
  explicit CatalogSession(contracts::LibraryGeneration last_issued = {}) noexcept;
  CatalogSession(const CatalogSession&) = delete;
  CatalogSession& operator=(const CatalogSession&) = delete;
  CatalogSession(CatalogSession&&) = delete;
  CatalogSession& operator=(CatalogSession&&) = delete;
  ~CatalogSession() override = default;

  [[nodiscard]] CatalogChangeResult begin_open();
  [[nodiscard]] CatalogChangeResult close();
  [[nodiscard]] CatalogChangeResult complete_open(contracts::LibraryGeneration ticket,
                                                  library::ByteView file);
  [[nodiscard]] CatalogChangeResult fail_open(contracts::LibraryGeneration ticket);
  [[nodiscard]] contracts::CatalogStatus status() const noexcept override { return status_; }
  [[nodiscard]] contracts::CatalogAlbumPage
  albums(const contracts::CatalogPageQuery& query) const override;
  [[nodiscard]] contracts::CatalogTrackPage
  tracks(contracts::AlbumId album, const contracts::CatalogPageQuery& query) const override;

  // Validates identity before later transport work; never changes playback.
  [[nodiscard]] CatalogTrackError resolve_track(contracts::LibraryGeneration generation,
                                                contracts::TrackId track,
                                                library::TrackView& output) const;
  // Core-only borrow; the owner retains bytes through playback quiescence.
  [[nodiscard]] const library::LogicalLibrary*
  borrow_library(contracts::LibraryGeneration generation) const noexcept;

private:
  [[nodiscard]] CatalogChangeResult invalidate(contracts::CatalogPhase phase);
  [[nodiscard]] CatalogChangeError completion_error(contracts::LibraryGeneration ticket) const;
  [[nodiscard]] contracts::CatalogQueryError
  query_error(const contracts::CatalogPageQuery& query) const;
  library::AlbumCatalog catalog_{};
  contracts::CatalogStatus status_{};
};

} // namespace rpcmp::player

#endif // RPCMP_PLAYER_CATALOG_SESSION_HPP
