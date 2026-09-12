#include "rpcmp/player/playback_navigation.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace rpcmp::player;
namespace api = rpcmp::contracts;
template <typename T> T required(const std::optional<T>& value) {
  if (!value)
    throw std::logic_error("missing authored navigation value");
  return *value;
}
api::CatalogText label() {
  api::CatalogText text;
  text.length = 1;
  text.bytes[0] = 'x';
  return text;
}
api::CatalogPageHeader header(const api::CatalogPageQuery& query, std::uint32_t total) {
  api::CatalogPageHeader result;
  result.query = query;
  result.total = total;
  if (query.start_ordinal > total) {
    result.error = api::CatalogQueryError::InvalidStart;
    return result;
  }
  result.count =
      static_cast<std::uint16_t>(std::min<std::uint32_t>(query.limit, total - query.start_ordinal));
  const auto next = query.start_ordinal + result.count;
  if (next < total)
    result.next_start = next;
  return result;
}
// Generated metadata only. Track n belongs to the single album and has ID n+1.
class Catalog final : public api::CatalogReader {
public:
  api::CatalogStatus status() const noexcept override {
    return {1, {7}, api::CatalogPhase::Ready, api::CatalogFailure::None, 1, count};
  }
  api::CatalogAlbumPage albums(const api::CatalogPageQuery& query) const override {
    api::CatalogAlbumPage page;
    page.header = header(query, 1);
    if (page.header.count != 0)
      page.items[0] = {{1}, 0, label(), count};
    return page;
  }
  api::CatalogTrackPage tracks(api::AlbumId album,
                               const api::CatalogPageQuery& query) const override {
    api::CatalogTrackPage page;
    page.header = header(query, count);
    page.album_id = album;
    for (std::uint16_t i = 0; i < page.header.count; ++i) {
      const auto ordinal = query.start_ordinal + i;
      page.items[i] = {{duplicate ? 1 : ordinal + 1}, album, ordinal, label()};
    }
    return page;
  }
  std::uint32_t count{4};
  bool duplicate{};
};
class Random final : public RandomSource {
public:
  explicit Random(std::vector<std::uint32_t> script) : values(std::move(script)) {}
  bool next(std::uint32_t& value) override {
    ++calls;
    if (position == values.size())
      return false;
    value = values[position++];
    return true;
  }
  std::vector<std::uint32_t> values;
  std::size_t position{};
  unsigned calls{};
};

void authored_order(rpcmp::test::Suite& suite) {
  Catalog catalog;
  NavigationIndex index;
  RPCMP_CHECK(suite, index.load(catalog) && index.size() == 4 && index.generation().value == 7);
  RPCMP_CHECK(suite, !index.adjacent({1}, false) && !index.adjacent({4}, true));
  RPCMP_CHECK(suite, required(index.adjacent({2}, false)).value == 1 &&
                         required(index.adjacent({2}, true)).value == 3);
  Random random({0, 1});
  ShuffleCycle cycle;
  RPCMP_CHECK(suite, cycle.begin(index, {1}, true, random, 0) == NavigationFailure::None &&
                         cycle.id() == 1);
  for (const std::uint64_t expected : {4ULL, 3ULL, 2ULL}) {
    const auto track = required(cycle.next(index, {1}, true));
    RPCMP_CHECK(suite, track.value == expected && cycle.mark_started(index, track));
  }
  RPCMP_CHECK(suite, cycle.started() == 4 && !cycle.next(index, {2}, true) && random.calls == 2);
  RPCMP_CHECK(suite, required(cycle.previous(index, {3})).value == 4 &&
                         required(cycle.next(index, {4}, false)).value == 3);
  RPCMP_CHECK(suite, cycle.mark_started(index, {4}) && cycle.started() == 4 &&
                         !cycle.previous(index, {1}));
  cycle.stop();
  RPCMP_CHECK(suite, !cycle.active() && required(cycle.previous(index, {2})).value == 3);
  RPCMP_CHECK(suite, !cycle.mark_started(index, {1}));
}

void sampling_and_transactions(rpcmp::test::Suite& suite) {
  Catalog catalog;
  NavigationIndex index;
  RPCMP_CHECK(suite, index.load(catalog));
  ShuffleCycle cycle;
  Random initial({0, 1});
  RPCMP_CHECK(suite, cycle.begin(index, {1}, true, initial, 8) == NavigationFailure::None);
  Random rejected(std::vector<std::uint32_t>(32, std::numeric_limits<std::uint32_t>::max()));
  RPCMP_CHECK(suite,
              cycle.begin(index, {2}, false, rejected, 9) == NavigationFailure::RandomUnavailable &&
                  rejected.calls == 32);
  RPCMP_CHECK(suite, cycle.id() == 9 && cycle.started() == 1 &&
                         required(cycle.next(index, {1}, true)).value == 4);
  std::vector<std::uint32_t> boundary(31, std::numeric_limits<std::uint32_t>::max());
  boundary.push_back(0);
  boundary.push_back(1);
  Random accepted(std::move(boundary));
  RPCMP_CHECK(suite, cycle.begin(index, {1}, true, accepted, 9) == NavigationFailure::None &&
                         accepted.calls == 33);
  RPCMP_CHECK(suite, cycle.id() == 10 && required(cycle.next(index, {1}, true)).value == 4);
  Random unavailable({});
  RPCMP_CHECK(suite, cycle.begin(index, {1}, false, unavailable, 10) ==
                             NavigationFailure::RandomUnavailable &&
                         unavailable.calls == 1);
  Random untouched({0, 1});
  RPCMP_CHECK(suite,
              cycle.begin(index, {0}, true, untouched, 10) == NavigationFailure::InvalidSelection &&
                  untouched.calls == 0);
  RPCMP_CHECK(suite,
              cycle.begin(index, {1}, true, untouched, std::numeric_limits<std::uint64_t>::max()) ==
                      NavigationFailure::CycleExhausted &&
                  untouched.calls == 0);
  RPCMP_CHECK(suite, cycle.id() == 10);
  catalog.duplicate = true;
  RPCMP_CHECK(suite, !index.load(catalog) && index.size() == 4 && required(index.at(3)).value == 4);
  catalog.duplicate = false;
  catalog.count = 301;
  RPCMP_CHECK(suite, !index.load(catalog) && index.size() == 4);
  index.clear();
  RPCMP_CHECK(suite, !cycle.matches(index) && !cycle.next(index, {1}, true) &&
                         !cycle.mark_started(index, {1}));
}

void catalog_bounds(rpcmp::test::Suite& suite) {
  for (const std::uint32_t total : {1U, 300U}) {
    Catalog catalog;
    catalog.count = total;
    NavigationIndex index;
    RPCMP_CHECK(suite, index.load(catalog) && index.size() == total);
    Random random(std::vector<std::uint32_t>(total == 1 ? 0 : total - 2, 0));
    ShuffleCycle cycle;
    const api::TrackId first{total == 1 ? 1U : 150U};
    RPCMP_CHECK(suite, cycle.begin(index, first, false, random,
                                   std::numeric_limits<std::uint64_t>::max() - 1) ==
                           NavigationFailure::None);
    RPCMP_CHECK(suite, cycle.started() == 0 && cycle.mark_started(index, first));
    std::array<bool, 301> observed{};
    observed[first.value] = true;
    for (std::uint32_t i = 1; i < total; ++i) {
      const auto next = required(cycle.next(index, first, true));
      if (next.value == 0 || next.value > total)
        throw std::logic_error("out of range navigation result");
      RPCMP_CHECK(suite, !observed[next.value] && cycle.mark_started(index, next));
      observed[next.value] = true;
    }
    for (std::uint32_t i = 1; i <= total; ++i)
      RPCMP_CHECK(suite, observed[i]);
    RPCMP_CHECK(suite, cycle.started() == total && !cycle.next(index, first, true) &&
                           random.calls == (total == 1 ? 0 : total - 2) &&
                           cycle.id() == std::numeric_limits<std::uint64_t>::max());
  }
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  authored_order(suite);
  sampling_and_transactions(suite);
  catalog_bounds(suite);
  return suite.finish("Playback navigation order and bounds");
}
