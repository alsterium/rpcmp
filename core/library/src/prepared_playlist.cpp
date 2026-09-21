#include "rpcmp/library/prepared_playlist.hpp"

#include <algorithm>

namespace rpcmp::library {
namespace {
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
} // namespace

bool PreparedPlaylist::open(PlaylistSource& source) {
  source_ = nullptr;
  count_ = 0;
  std::uint32_t size{};
  std::array<std::uint8_t, 32> header{};
  if (!source.size(size) || size < header.size() || size > kPreparedFileLimit ||
      !source.read(0, header.data(), static_cast<std::uint32_t>(header.size())))
    return false;
  const auto* h = header.data();
  const auto count = word(h + 12);
  if (word(h) != 0x314C5048 || word(h + 4) != 1 || word(h + 8) != 128 || count == 0 ||
      count > entries_.size() || word(h + 16) != size || word(h + 28) != 0 ||
      word(h + 24) != crc32({h, 24}))
    return false;
  const auto bytes = count * 128;
  if (bytes > size - 32 || !read_all(source, 32, index_.data(), bytes) ||
      crc32({index_.data(), bytes}) != word(h + 20))
    return false;
  std::uint32_t next = 32 + bytes;
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto* p = index_.data() + static_cast<std::size_t>(i) * 128;
    auto& entry = entries_[i];
    entry = {};
    entry.mdx_offset = word(p);
    entry.mdx_size = word(p + 4);
    entry.mdx_crc = word(p + 8);
    entry.pdx_offset = word(p + 12);
    entry.pdx_size = word(p + 16);
    entry.pdx_crc = word(p + 20);
    const auto length = word(p + 24);
    if (length == 0 || length > entry.title.bytes.size() || word(p + 124) != 0 ||
        entry.mdx_size < 10 || entry.mdx_size > kPreparedPairLimit ||
        entry.pdx_size > kPreparedPairLimit - entry.mdx_size || entry.mdx_offset != next ||
        entry.mdx_size > size - next)
      return false;
    next += entry.mdx_size;
    if (entry.pdx_size == 0) {
      if (entry.pdx_offset != 0 || entry.pdx_crc != 0)
        return false;
    } else {
      if (entry.pdx_size < 10 || entry.pdx_offset != next || entry.pdx_size > size - next)
        return false;
      next += entry.pdx_size;
    }
    entry.title.length = static_cast<std::uint16_t>(length);
    std::copy_n(p + 28, length, entry.title.bytes.begin());
    if (!contracts::valid_catalog_text(entry.title) ||
        std::any_of(p + 28, p + 28 + length, [](std::uint8_t c) { return c < 32 || c == 127; }) ||
        std::any_of(p + 28 + length, p + 124, [](std::uint8_t c) { return c != 0; }))
      return false;
  }
  if (next != size)
    return false;
  source_ = &source;
  count_ = count;
  return true;
}
contracts::CatalogText PreparedPlaylist::title(const contracts::TrackId id) const noexcept {
  return id.value > 0 && id.value <= count_ ? entries_[id.value - 1].title
                                            : contracts::CatalogText{};
}
bool PreparedPlaylist::sizes(const contracts::TrackId id, std::uint32_t& mdx,
                             std::uint32_t& pdx) const noexcept {
  mdx = pdx = 0;
  if (id.value == 0 || id.value > count_)
    return false;
  const auto& entry = entries_[id.value - 1];
  mdx = entry.mdx_size;
  pdx = entry.pdx_size;
  return true;
}
bool PreparedPlaylist::load(const contracts::TrackId id, const PreparedBuffer mdx,
                            const PreparedBuffer pdx) {
  std::uint32_t a{}, b{};
  if (!sizes(id, a, b) || !mdx.data || mdx.size < a + 16U ||
      (b != 0 && (!pdx.data || pdx.size < b + 16U)))
    return false;
  const auto& entry = entries_[id.value - 1];
  if (!read_all(*source_, entry.mdx_offset, mdx.data, a) || crc32({mdx.data, a}) != entry.mdx_crc)
    return false;
  constexpr std::array<std::uint8_t, 10> fm{0, 0, 255, 255, 0, 10, 0, 8, 0, 0};
  constexpr std::array<std::uint8_t, 10> mixed{0, 0, 0, 0, 0, 10, 0, 8, 0, 0};
  constexpr std::array<std::uint8_t, 10> samples{0, 0, 0, 0, 0, 10, 0, 2, 0, 0};
  const auto& expected = b == 0 ? fm : mixed;
  if (!std::equal(expected.begin(), expected.end(), mdx.data))
    return false;
  if (b != 0 && (!read_all(*source_, entry.pdx_offset, pdx.data, b) ||
                 crc32({pdx.data, b}) != entry.pdx_crc ||
                 !std::equal(samples.begin(), samples.end(), pdx.data)))
    return false;
  std::fill_n(mdx.data + a, 16, std::uint8_t{0});
  if (b != 0)
    std::fill_n(pdx.data + b, 16, std::uint8_t{0});
  return true;
}
} // namespace rpcmp::library
