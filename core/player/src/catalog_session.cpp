#include "rpcmp/player/catalog_session.hpp"

#include <algorithm>
#include <limits>

namespace rpcmp::player {
namespace {

using namespace contracts;
static_assert(kCatalogMaxItems == library::kAlbumMaxAlbums);
static_assert(kCatalogMaxItems == library::kAlbumMaxTracks);

CatalogText copy_text(const library::Utf8View source) {
  CatalogText result;
  auto length = std::min(source.size, result.bytes.size());
  if (length < source.size) {
    while (length != 0 && (static_cast<unsigned char>(source.data[length]) & 0xC0U) == 0x80U)
      --length;
  }
  if (length != 0)
    std::copy_n(source.data, length, result.bytes.data());
  result.length = static_cast<std::uint16_t>(length);
  result.truncated = length != source.size;
  return result;
}

void set_page_size(CatalogPageHeader& header, const std::uint32_t total) {
  header.total = total;
  header.count = static_cast<std::uint16_t>(
      std::min<std::uint32_t>(header.query.limit, total - header.query.start_ordinal));
  const auto end = header.query.start_ordinal + header.count;
  if (end < total)
    header.next_start = end;
}

} // namespace

CatalogSession::CatalogSession(const LibraryGeneration last_issued) noexcept {
  status_.generation = last_issued;
}

CatalogChangeResult CatalogSession::invalidate(const CatalogPhase phase) {
  catalog_ = {};
  status_.album_count = 0;
  status_.track_count = 0;
  if (status_.generation.value == std::numeric_limits<std::uint64_t>::max()) {
    status_.phase = CatalogPhase::Error;
    status_.failure = CatalogFailure::GenerationExhausted;
    return {CatalogChangeError::GenerationExhausted, status_.generation, {}};
  }
  ++status_.generation.value;
  status_.phase = phase;
  status_.failure = CatalogFailure::None;
  return {CatalogChangeError::None, status_.generation, {}};
}

CatalogChangeResult CatalogSession::begin_open() { return invalidate(CatalogPhase::Loading); }
CatalogChangeResult CatalogSession::close() { return invalidate(CatalogPhase::Empty); }

CatalogChangeError CatalogSession::completion_error(const LibraryGeneration ticket) const {
  if (ticket != status_.generation || ticket.value == 0)
    return CatalogChangeError::StaleLibrary;
  if (status_.failure == CatalogFailure::GenerationExhausted)
    return CatalogChangeError::GenerationExhausted;
  return status_.phase == CatalogPhase::Loading ? CatalogChangeError::None
                                                : CatalogChangeError::NotLoading;
}

CatalogChangeResult CatalogSession::complete_open(const LibraryGeneration ticket,
                                                  const library::ByteView file) {
  const auto error = completion_error(ticket);
  if (error != CatalogChangeError::None)
    return {error, ticket, {}};
  const auto validation = library::AlbumCatalog::open(file, catalog_);
  if (!validation.ok()) {
    status_.phase = CatalogPhase::Error;
    status_.failure = CatalogFailure::InvalidLibrary;
    return {CatalogChangeError::InvalidLibrary, ticket, validation};
  }
  status_.phase = CatalogPhase::Ready;
  status_.album_count = catalog_.album_count();
  status_.track_count = catalog_.track_count();
  return {CatalogChangeError::None, ticket, validation};
}

CatalogChangeResult CatalogSession::fail_open(const LibraryGeneration ticket) {
  const auto error = completion_error(ticket);
  if (error != CatalogChangeError::None)
    return {error, ticket, {}};
  status_.phase = CatalogPhase::Error;
  status_.failure = CatalogFailure::ReadFailed;
  return {CatalogChangeError::ReadFailed, ticket, {}};
}

CatalogQueryError CatalogSession::query_error(const CatalogPageQuery& query) const {
  if (query.schema_version != kCatalogSchemaVersion)
    return CatalogQueryError::UnsupportedSchema;
  if (status_.phase != CatalogPhase::Ready)
    return CatalogQueryError::Unavailable;
  if (query.generation != status_.generation)
    return CatalogQueryError::StaleLibrary;
  if (query.limit == 0 || query.limit > kCatalogPageCapacity)
    return CatalogQueryError::InvalidLimit;
  return CatalogQueryError::None;
}

CatalogAlbumPage CatalogSession::albums(const CatalogPageQuery& query) const {
  CatalogAlbumPage page;
  page.header.query = query;
  page.header.error = query_error(query);
  if (!page.header.ok())
    return page;
  if (query.start_ordinal > catalog_.album_count()) {
    page.header.error = CatalogQueryError::InvalidStart;
    return page;
  }
  set_page_size(page.header, catalog_.album_count());
  for (std::uint16_t index = 0; index < page.header.count; ++index) {
    library::AlbumView album;
    if (!catalog_.album_at(query.start_ordinal + index, album)) {
      page = {};
      page.header.query = query;
      page.header.error = CatalogQueryError::Unavailable;
      return page;
    }
    page.items[index] = {album.album_id, album.display_ordinal, copy_text(album.name),
                         album.track_count};
  }
  return page;
}

CatalogTrackPage CatalogSession::tracks(const AlbumId album_id,
                                        const CatalogPageQuery& query) const {
  CatalogTrackPage page;
  page.album_id = album_id;
  page.header.query = query;
  page.header.error = query_error(query);
  if (!page.header.ok())
    return page;
  library::AlbumView album;
  if (!catalog_.find_album(album_id, album)) {
    page.header.error = CatalogQueryError::UnknownAlbum;
    return page;
  }
  if (query.start_ordinal > album.track_count) {
    page.header.error = CatalogQueryError::InvalidStart;
    return page;
  }
  set_page_size(page.header, album.track_count);
  for (std::uint16_t index = 0; index < page.header.count; ++index) {
    library::TrackView track;
    const auto ordinal = query.start_ordinal + index;
    if (!catalog_.track_at(album_id, ordinal, track)) {
      page = {};
      page.album_id = album_id;
      page.header.query = query;
      page.header.error = CatalogQueryError::Unavailable;
      return page;
    }
    page.items[index] = {track.track_id, album_id, ordinal, copy_text(track.title)};
  }
  return page;
}

CatalogTrackError CatalogSession::resolve_track(const LibraryGeneration generation,
                                                const TrackId track,
                                                library::TrackView& output) const {
  if (status_.phase != CatalogPhase::Ready)
    return CatalogTrackError::Unavailable;
  if (generation != status_.generation)
    return CatalogTrackError::StaleLibrary;
  return catalog_.library().find_track(track, output) ? CatalogTrackError::None
                                                      : CatalogTrackError::UnknownTrack;
}

const library::LogicalLibrary*
CatalogSession::borrow_library(const LibraryGeneration generation) const noexcept {
  return status_.phase == CatalogPhase::Ready && generation == status_.generation
             ? &catalog_.library()
             : nullptr;
}

} // namespace rpcmp::player
