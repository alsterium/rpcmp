#ifndef RPCMP_PLAYER_PLAYBACK_NAVIGATION_HPP
#define RPCMP_PLAYER_PLAYBACK_NAVIGATION_HPP

#include "rpcmp/contracts/catalog.hpp"

namespace rpcmp::player {

class RandomSource {
public:
  virtual ~RandomSource() = default;
  [[nodiscard]] virtual bool next(std::uint32_t& value) = 0;
};
struct NavigationTrack {
  contracts::TrackId track{};
  contracts::AlbumId album{};
};
class NavigationIndex {
public:
  [[nodiscard]] bool load(const contracts::CatalogReader& catalog);
  void clear() noexcept;
  [[nodiscard]] contracts::LibraryGeneration generation() const noexcept { return generation_; }
  [[nodiscard]] std::uint16_t size() const noexcept { return count_; }
  [[nodiscard]] std::optional<std::uint16_t> find(contracts::TrackId track) const noexcept;
  [[nodiscard]] std::optional<contracts::TrackId> at(std::uint16_t ordinal) const noexcept;
  [[nodiscard]] std::optional<contracts::TrackId> adjacent(contracts::TrackId track,
                                                           bool next) const noexcept;

private:
  std::array<NavigationTrack, contracts::kCatalogMaxItems> tracks_{};
  contracts::LibraryGeneration generation_{};
  std::uint16_t count_{};
};
enum class NavigationFailure : std::uint8_t {
  None,
  InvalidSelection,
  RandomUnavailable,
  CycleExhausted
};
class ShuffleCycle {
public:
  [[nodiscard]] NavigationFailure begin(const NavigationIndex& index, contracts::TrackId selected,
                                        bool already_started, RandomSource& random,
                                        std::uint64_t last_cycle_id);
  [[nodiscard]] bool mark_started(const NavigationIndex& index, contracts::TrackId track) noexcept;
  void stop() noexcept { active_ = false; }
  void clear() noexcept;
  [[nodiscard]] bool matches(const NavigationIndex& index) const noexcept;
  [[nodiscard]] std::optional<contracts::TrackId>
  next(const NavigationIndex& index, contracts::TrackId selected, bool automatic) const noexcept;
  [[nodiscard]] std::optional<contracts::TrackId>
  previous(const NavigationIndex& index, contracts::TrackId selected) const noexcept;
  [[nodiscard]] std::uint64_t id() const noexcept { return id_; }
  [[nodiscard]] std::uint16_t total() const noexcept { return total_; }
  [[nodiscard]] std::uint16_t started() const noexcept { return started_count_; }
  [[nodiscard]] bool active() const noexcept { return active_; }

private:
  [[nodiscard]] std::optional<contracts::TrackId>
  first_unstarted(const NavigationIndex& index) const noexcept;
  std::array<std::uint16_t, contracts::kCatalogMaxItems> permutation_{};
  std::array<std::uint16_t, contracts::kCatalogMaxItems> history_{};
  std::array<bool, contracts::kCatalogMaxItems> started_{};
  contracts::LibraryGeneration generation_{};
  std::uint64_t id_{};
  std::uint16_t total_{};
  std::uint16_t started_count_{};
  bool active_{};
};

} // namespace rpcmp::player

#endif // RPCMP_PLAYER_PLAYBACK_NAVIGATION_HPP
