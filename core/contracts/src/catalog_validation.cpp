#include "rpcmp/contracts/catalog.hpp"
#include "utf8.hpp"

#include <algorithm>
#include <limits>

namespace rpcmp::contracts {

bool valid_catalog_text(const CatalogText& text) noexcept {
  if (text.length > text.bytes.size() || (text.truncated && text.length < kMaxMetadataBytes - 3))
    return false;
  const std::string_view value{text.bytes.data(), text.length};
  return value.find('\0') == std::string_view::npos && detail::valid_utf8(value);
}

namespace {

bool valid_header(const CatalogPageHeader& header) noexcept {
  if (header.schema_version != kCatalogSchemaVersion)
    return false;
  const auto& query = header.query;
  const bool valid_limit = query.limit >= 1 && query.limit <= kCatalogPageCapacity;
  if (!header.ok()) {
    if (header.total != 0 || header.count != 0 || header.next_start.has_value())
      return false;
    if (header.error == CatalogQueryError::UnsupportedSchema)
      return query.schema_version != kCatalogSchemaVersion;
    if (query.schema_version != kCatalogSchemaVersion)
      return false;
    switch (header.error) {
    case CatalogQueryError::Unavailable:
    case CatalogQueryError::StaleLibrary:
      return true;
    case CatalogQueryError::InvalidLimit:
      return query.generation.value != 0 && !valid_limit;
    case CatalogQueryError::UnknownAlbum:
      return query.generation.value != 0 && valid_limit;
    case CatalogQueryError::InvalidStart:
      return query.generation.value != 0 && valid_limit && query.start_ordinal != 0;
    default:
      return false;
    }
  }
  if (query.schema_version != kCatalogSchemaVersion || query.generation.value == 0 ||
      !valid_limit || header.total == 0 || header.total > kCatalogMaxItems ||
      query.start_ordinal > header.total)
    return false;
  const auto expected = std::min<std::uint32_t>(query.limit, header.total - query.start_ordinal);
  if (header.count != expected)
    return false;
  const auto end = query.start_ordinal + expected;
  return end < header.total ? header.next_start == end : !header.next_start.has_value();
}

} // namespace

bool valid_catalog_status(const CatalogStatus& status) noexcept {
  if (status.schema_version != kCatalogSchemaVersion ||
      (status.phase != CatalogPhase::Empty && status.generation.value == 0))
    return false;
  if (status.phase == CatalogPhase::Ready)
    return status.failure == CatalogFailure::None && status.album_count >= 1 &&
           status.album_count <= status.track_count && status.track_count <= kCatalogMaxItems;
  if (status.album_count != 0 || status.track_count != 0)
    return false;
  if (status.phase == CatalogPhase::Empty || status.phase == CatalogPhase::Loading)
    return status.failure == CatalogFailure::None;
  if (status.phase != CatalogPhase::Error)
    return false;
  switch (status.failure) {
  case CatalogFailure::InvalidLibrary:
  case CatalogFailure::ReadFailed:
    return true;
  case CatalogFailure::GenerationExhausted:
    return status.generation.value == std::numeric_limits<std::uint64_t>::max();
  default:
    return false;
  }
}

bool valid_catalog_page(const CatalogAlbumPage& page) noexcept {
  if (!valid_header(page.header) || page.header.error == CatalogQueryError::UnknownAlbum)
    return false;
  for (std::uint16_t index = 0; index < page.header.count; ++index) {
    const auto& item = page.items[index];
    if (item.album_id.value == 0 ||
        item.display_ordinal != page.header.query.start_ordinal + index || item.track_count == 0 ||
        item.track_count > kCatalogMaxItems || item.name.length == 0 ||
        !valid_catalog_text(item.name))
      return false;
    for (std::uint16_t previous = 0; previous < index; ++previous)
      if (item.album_id == page.items[previous].album_id)
        return false;
  }
  return true;
}

bool valid_catalog_page(const CatalogTrackPage& page) noexcept {
  if (!valid_header(page.header) || (page.header.ok() && page.album_id.value == 0) ||
      (page.header.error == CatalogQueryError::InvalidStart && page.album_id.value == 0))
    return false;
  for (std::uint16_t index = 0; index < page.header.count; ++index) {
    const auto& item = page.items[index];
    if (item.track_id.value == 0 || item.album_id != page.album_id ||
        item.track_ordinal != page.header.query.start_ordinal + index ||
        !valid_catalog_text(item.title))
      return false;
    for (std::uint16_t previous = 0; previous < index; ++previous)
      if (item.track_id == page.items[previous].track_id)
        return false;
  }
  return true;
}

} // namespace rpcmp::contracts
