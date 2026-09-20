#include "rpcmp/utility/metadata.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <set>
#include <string_view>
#include <utf8proc.h>

// Build-time only: use the same pinned NFC implementation as ingestion, never
// the host OS or Python's potentially different Unicode database.
int main() {
  if (std::string_view{utf8proc_version()} != "2.11.3" ||
      std::string_view{utf8proc_unicode_version()} != "17.0.0")
    return 1;
  std::set<std::uint32_t> points;
  std::uint32_t point{};
  while (std::cin >> point) {
    if (point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF))
      return 2;
    std::array<utf8proc_uint8_t, 4> bytes{};
    const auto size = utf8proc_encode_char(static_cast<utf8proc_int32_t>(point), bytes.data());
    const auto result = rpcmp::utility::normalize_utf8(
        {reinterpret_cast<const char*>(bytes.data()), static_cast<std::size_t>(size)});
    if (!result.ok())
      return 3;
    const auto* normalized = reinterpret_cast<const utf8proc_uint8_t*>(result.text.data());
    const auto written = static_cast<utf8proc_ssize_t>(result.text.size());
    for (utf8proc_ssize_t offset = 0; offset < written;) {
      utf8proc_int32_t scalar{};
      const auto consumed = utf8proc_iterate(normalized + offset, written - offset, &scalar);
      if (consumed <= 0)
        return 5;
      points.insert(static_cast<std::uint32_t>(scalar));
      offset += consumed;
    }
  }
  if (!std::cin.eof())
    return 6;
  for (const auto scalar : points)
    std::cout << scalar << '\n';
  return std::cout ? 0 : 7;
}
