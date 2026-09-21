#include "rpcmp/library/prepared_playlist.hpp"

#include <algorithm>

namespace rpcmp::library {
namespace {
namespace api = contracts::minimal;
std::uint32_t word(const std::uint8_t* p) noexcept {
  return p[0] | (std::uint32_t{p[1]} << 8U) | (std::uint32_t{p[2]} << 16U) |
         (std::uint32_t{p[3]} << 24U);
}
bool read_all(PlaylistSource& source, std::uint32_t offset, std::uint8_t* out, std::uint32_t size) {
  while (size != 0) {
    const auto chunk = std::min(size, 65536U);
    if (!source.read(offset, out, chunk))
      return false;
    offset += chunk;
    out += chunk;
    size -= chunk;
  }
  return true;
}
contracts::CatalogText text(const std::uint8_t* p, const std::uint32_t length) noexcept {
  contracts::CatalogText result{};
  if (length <= result.bytes.size()) {
    result.length = static_cast<std::uint16_t>(length);
    std::copy_n(p, length, result.bytes.begin());
  }
  return result;
}
bool valid_text(const std::uint8_t* p, const std::uint32_t length) {
  if (length == 0 || length > 96)
    return false;
  return contracts::valid_catalog_text(text(p, length)) &&
         std::none_of(p, p + length, [](std::uint8_t c) { return c < 32 || c == 127; }) &&
         std::all_of(p + length, p + 96, [](std::uint8_t c) { return c == 0; });
}
} // namespace
bool PreparedPlaylist::open(PlaylistSource& source) {
  source_ = nullptr;
  count_ = playlists_ = 0;
  std::uint32_t size{};
  std::array<std::uint8_t, 48> header{};
  if (!source.size(size) || size < header.size() || size > kPreparedFileLimit ||
      !source.read(0, header.data(), static_cast<std::uint32_t>(header.size())))
    return false;
  const auto* h = header.data();
  const auto lists = word(h + 16);
  const auto count = word(h + 20);
  if (word(h) != 0x324C5048 || word(h + 4) != 2 || word(h + 8) != 112 || word(h + 12) != 128 ||
      lists == 0 || lists > api::kMaxPlaylists || count == 0 || count > api::kMaxTracks ||
      word(h + 24) != size || word(h + 32) != crc32({h, 32}) || word(h + 36) != 0 ||
      word(h + 40) != 0 || word(h + 44) != 0)
    return false;
  const auto bytes = lists * 112 + count * 128;
  if (bytes > size - 48 || !read_all(source, 48, index_.data(), bytes) ||
      crc32({index_.data(), bytes}) != word(h + 28))
    return false;
  std::uint32_t next = 1;
  for (std::uint32_t i = 0; i < lists; ++i) {
    const auto* p = index_.data() + static_cast<std::size_t>(i) * 112;
    const auto length = word(p + 4);
    if (word(p) != next || length == 0 || length > api::kMaxPlaylistTracks ||
        length > count - (next - 1) || !valid_text(p + 12, word(p + 8)) || word(p + 108) != 0)
      return false;
    next += length;
  }
  if (next != count + 1)
    return false;
  const auto payload = 48 + bytes;
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto* p = index_.data() + std::size_t{lists} * 112 + static_cast<std::size_t>(i) * 128;
    const auto a = word(p + 4);
    const auto b = word(p + 16);
    if (a < 10 || a > kPreparedPairLimit || b > kPreparedPairLimit - a || word(p) < payload ||
        word(p) > size || a > size - word(p) || !valid_text(p + 28, word(p + 24)) ||
        word(p + 124) != 0)
      return false;
    if (b == 0) {
      if (word(p + 12) != 0 || word(p + 20) != 0)
        return false;
    } else if (b < 10 || word(p + 12) < payload || word(p + 12) > size || b > size - word(p + 12)) {
      return false;
    }
  }
  source_ = &source;
  count_ = count;
  playlists_ = lists;
  return true;
}
const std::uint8_t* PreparedPlaylist::entry(const contracts::TrackId id) const noexcept {
  return id.value > 0 && id.value <= count_ ? index_.data() + std::size_t{playlists_} * 112 +
                                                  static_cast<std::size_t>(id.value - 1) * 128
                                            : nullptr;
}
contracts::CatalogText PreparedPlaylist::title(const contracts::TrackId id) const noexcept {
  const auto* p = entry(id);
  return p ? text(p + 28, word(p + 24)) : contracts::CatalogText{};
}
api::Playlist PreparedPlaylist::playlist(const api::PlaylistId id) const noexcept {
  if (id.value == 0 || id.value > playlists_)
    return {};
  const auto* p = index_.data() + static_cast<std::size_t>(id.value - 1) * 112;
  return {id, {word(p)}, word(p + 4), text(p + 12, word(p + 8))};
}
api::PlaylistId PreparedPlaylist::playlist_for(const contracts::TrackId id) const noexcept {
  if (id.value == 0 || id.value > count_)
    return {};
  for (std::uint32_t i = 0; i < playlists_; ++i) {
    const auto* p = index_.data() + static_cast<std::size_t>(i) * 112;
    if (id.value >= word(p) && id.value - word(p) < word(p + 4))
      return {i + 1};
  }
  return {};
}
bool PreparedPlaylist::sizes(const contracts::TrackId id, std::uint32_t& mdx,
                             std::uint32_t& pdx) const noexcept {
  const auto* p = entry(id);
  mdx = p ? word(p + 4) : 0;
  pdx = p ? word(p + 16) : 0;
  return p != nullptr;
}
bool PreparedPlaylist::load(const contracts::TrackId id, const PreparedBuffer mdx,
                            const PreparedBuffer pdx) {
  std::uint32_t a{}, b{};
  if (!sizes(id, a, b) || !mdx.data || mdx.size < a + 16U ||
      (b != 0 && (!pdx.data || pdx.size < b + 16U)))
    return false;
  const auto* p = entry(id);
  if (!read_all(*source_, word(p), mdx.data, a) || crc32({mdx.data, a}) != word(p + 8))
    return false;
  constexpr std::array<std::uint8_t, 10> fm{0, 0, 255, 255, 0, 10, 0, 8, 0, 0};
  constexpr std::array<std::uint8_t, 10> mixed{0, 0, 0, 0, 0, 10, 0, 8, 0, 0};
  constexpr std::array<std::uint8_t, 10> samples{0, 0, 0, 0, 0, 10, 0, 2, 0, 0};
  const auto& expected = b == 0 ? fm : mixed;
  if (!std::equal(expected.begin(), expected.end(), mdx.data))
    return false;
  if (b != 0 &&
      (!read_all(*source_, word(p + 12), pdx.data, b) || crc32({pdx.data, b}) != word(p + 20) ||
       !std::equal(samples.begin(), samples.end(), pdx.data)))
    return false;
  std::fill_n(mdx.data + a, 16, std::uint8_t{0});
  if (b != 0)
    std::fill_n(pdx.data + b, 16, std::uint8_t{0});
  return true;
}
} // namespace rpcmp::library
