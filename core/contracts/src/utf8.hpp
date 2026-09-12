#ifndef RPCMP_CONTRACTS_INTERNAL_UTF8_HPP
#define RPCMP_CONTRACTS_INTERNAL_UTF8_HPP

#include <cstdint>
#include <string_view>

namespace rpcmp::contracts::detail {

inline bool valid_utf8(const std::string_view value) noexcept {
  std::size_t index = 0;
  while (index < value.size()) {
    const auto first = static_cast<std::uint8_t>(value[index]);
    std::size_t continuation_count = 0;
    std::uint32_t code_point = 0;
    if (first <= 0x7FU) {
      ++index;
      continue;
    }
    if ((first & 0xE0U) == 0xC0U) {
      continuation_count = 1;
      code_point = first & 0x1FU;
      if (code_point == 0) {
        return false;
      }
    } else if ((first & 0xF0U) == 0xE0U) {
      continuation_count = 2;
      code_point = first & 0x0FU;
    } else if ((first & 0xF8U) == 0xF0U) {
      continuation_count = 3;
      code_point = first & 0x07U;
    } else {
      return false;
    }
    if (index + continuation_count >= value.size()) {
      return false;
    }
    for (std::size_t offset = 1; offset <= continuation_count; ++offset) {
      const auto next = static_cast<std::uint8_t>(value[index + offset]);
      if ((next & 0xC0U) != 0x80U) {
        return false;
      }
      code_point = (code_point << 6U) | (next & 0x3FU);
    }
    const bool overlong = (continuation_count == 1 && code_point < 0x80U) ||
                          (continuation_count == 2 && code_point < 0x800U) ||
                          (continuation_count == 3 && code_point < 0x10000U);
    if (overlong || code_point > 0x10FFFFU || (code_point >= 0xD800U && code_point <= 0xDFFFU)) {
      return false;
    }
    index += continuation_count + 1;
  }
  return true;
}

} // namespace rpcmp::contracts::detail

#endif // RPCMP_CONTRACTS_INTERNAL_UTF8_HPP
