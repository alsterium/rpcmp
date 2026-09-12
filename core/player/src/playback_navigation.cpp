#include "rpcmp/player/playback_navigation.hpp"

#include <algorithm>
#include <limits>

namespace rpcmp::player {
namespace {
std::optional<std::uint32_t> uniform(RandomSource& random, const std::uint32_t bound) {
  const auto range = 1ULL << 32U;
  const auto limit = range - range % bound;
  for (unsigned attempt = 0; attempt < 32; ++attempt) {
    std::uint32_t value{};
    if (!random.next(value))
      return std::nullopt;
    if (value < limit)
      return value % bound;
  }
  return std::nullopt;
}
} // namespace

void NavigationIndex::clear() noexcept { *this = {}; }

bool NavigationIndex::load(const contracts::CatalogReader& catalog) {
  const auto status = catalog.status();
  if (!contracts::valid_catalog_status(status) || status.phase != contracts::CatalogPhase::Ready)
    return false;
  NavigationIndex candidate;
  candidate.generation_ = status.generation;
  for (std::uint32_t start = 0; start < status.album_count;
       start += contracts::kCatalogPageCapacity) {
    const auto albums = catalog.albums({1, status.generation, start});
    if (!contracts::valid_catalog_page(albums) || !albums.header.ok() ||
        albums.header.query.generation != status.generation ||
        albums.header.query.start_ordinal != start || albums.header.total != status.album_count ||
        albums.header.count !=
            std::min<std::uint32_t>(contracts::kCatalogPageCapacity, status.album_count - start))
      return false;
    for (std::uint16_t a = 0; a < albums.header.count; ++a) {
      const auto& album = albums.items[a];
      if (album.display_ordinal != start + a ||
          album.track_count > status.track_count - candidate.count_)
        return false;
      for (std::uint32_t first = 0; first < album.track_count;
           first += contracts::kCatalogPageCapacity) {
        const auto tracks = catalog.tracks(album.album_id, {1, status.generation, first});
        if (!contracts::valid_catalog_page(tracks) || !tracks.header.ok() ||
            tracks.album_id != album.album_id ||
            tracks.header.query.generation != status.generation ||
            tracks.header.query.start_ordinal != first ||
            tracks.header.total != album.track_count ||
            tracks.header.count !=
                std::min<std::uint32_t>(contracts::kCatalogPageCapacity, album.track_count - first))
          return false;
        for (std::uint16_t t = 0; t < tracks.header.count; ++t) {
          const auto& track = tracks.items[t];
          if (track.track_ordinal != first + t || track.album_id != album.album_id ||
              candidate.find(track.track_id))
            return false;
          candidate.tracks_[candidate.count_++] = {track.track_id, track.album_id};
        }
      }
    }
  }
  if (candidate.count_ != status.track_count || catalog.status() != status)
    return false;
  *this = candidate;
  return true;
}

std::optional<std::uint16_t> NavigationIndex::find(const contracts::TrackId track) const noexcept {
  for (std::uint16_t i = 0; i < count_; ++i)
    if (tracks_[i].track == track)
      return i;
  return std::nullopt;
}

std::optional<contracts::TrackId> NavigationIndex::at(const std::uint16_t ordinal) const noexcept {
  if (ordinal >= count_)
    return std::nullopt;
  return tracks_[ordinal].track;
}

std::optional<contracts::TrackId> NavigationIndex::adjacent(const contracts::TrackId track,
                                                            const bool next) const noexcept {
  const auto ordinal = find(track);
  if (!ordinal || (next ? *ordinal + 1 >= count_ : *ordinal == 0))
    return std::nullopt;
  const auto neighbour = static_cast<std::uint16_t>(next ? *ordinal + 1 : *ordinal - 1);
  if (tracks_[neighbour].album != tracks_[*ordinal].album)
    return std::nullopt;
  return tracks_[neighbour].track;
}

void ShuffleCycle::clear() noexcept { *this = {}; }

bool ShuffleCycle::matches(const NavigationIndex& index) const noexcept {
  return id_ != 0 && total_ != 0 && total_ == index.size() && generation_ == index.generation();
}

NavigationFailure ShuffleCycle::begin(const NavigationIndex& index,
                                      const contracts::TrackId selected, const bool already_started,
                                      RandomSource& random, const std::uint64_t last_cycle_id) {
  const auto selected_index = index.find(selected);
  if (!selected_index)
    return NavigationFailure::InvalidSelection;
  if (last_cycle_id == std::numeric_limits<std::uint64_t>::max())
    return NavigationFailure::CycleExhausted;
  ShuffleCycle candidate;
  candidate.id_ = last_cycle_id + 1;
  candidate.generation_ = index.generation();
  candidate.total_ = index.size();
  candidate.active_ = true;
  candidate.permutation_[0] = *selected_index;
  std::uint16_t written = 1;
  for (std::uint16_t i = 0; i < index.size(); ++i)
    if (i != *selected_index)
      candidate.permutation_[written++] = i;
  for (std::uint16_t tail = static_cast<std::uint16_t>(index.size() - 1); tail > 1; --tail) {
    const auto chosen = uniform(random, tail);
    if (!chosen)
      return NavigationFailure::RandomUnavailable;
    std::swap(candidate.permutation_[tail], candidate.permutation_[1 + *chosen]);
  }
  if (already_started && !candidate.mark_started(index, selected))
    return NavigationFailure::InvalidSelection;
  *this = candidate;
  return NavigationFailure::None;
}

bool ShuffleCycle::mark_started(const NavigationIndex& index,
                                const contracts::TrackId track) noexcept {
  const auto ordinal = index.find(track);
  if (!active_ || !matches(index) || !ordinal)
    return false;
  if (started_[*ordinal])
    return true;
  if (started_count_ == total_)
    return false;
  started_[*ordinal] = true;
  history_[started_count_++] = *ordinal;
  return true;
}

std::optional<contracts::TrackId>
ShuffleCycle::first_unstarted(const NavigationIndex& index) const noexcept {
  for (std::uint16_t i = 0; i < total_; ++i)
    if (!started_[permutation_[i]])
      return index.at(permutation_[i]);
  return std::nullopt;
}

std::optional<contracts::TrackId> ShuffleCycle::next(const NavigationIndex& index,
                                                     const contracts::TrackId selected,
                                                     const bool automatic) const noexcept {
  const auto ordinal = index.find(selected);
  if (!matches(index) || !ordinal)
    return std::nullopt;
  if (automatic)
    return first_unstarted(index);
  if (started_[*ordinal]) {
    for (std::uint16_t i = 0; i + 1 < started_count_; ++i)
      if (history_[i] == *ordinal)
        return index.at(history_[i + 1]);
    return first_unstarted(index);
  }
  // A Loading candidate has no history entry. Move past it, retaining it for
  // later automatic playback if this request cancels its preparation.
  std::uint16_t position = 0;
  while (position < total_ && permutation_[position] != *ordinal)
    ++position;
  for (std::uint16_t distance = 1; distance < total_; ++distance) {
    const auto next_position = static_cast<std::uint16_t>((position + distance) % total_);
    if (!started_[permutation_[next_position]])
      return index.at(permutation_[next_position]);
  }
  return std::nullopt;
}

std::optional<contracts::TrackId>
ShuffleCycle::previous(const NavigationIndex& index,
                       const contracts::TrackId selected) const noexcept {
  const auto ordinal = index.find(selected);
  if (!matches(index) || !ordinal || started_count_ == 0)
    return std::nullopt;
  if (!started_[*ordinal])
    return index.at(history_[started_count_ - 1]);
  for (std::uint16_t i = 1; i < started_count_; ++i)
    if (history_[i] == *ordinal)
      return index.at(history_[i - 1]);
  return std::nullopt;
}

} // namespace rpcmp::player
