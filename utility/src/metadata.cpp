#include "rpcmp/utility/metadata.hpp"

#include <algorithm>
#include <array>
#include <utf8proc.h>
#include <utility>
#include <vector>

namespace rpcmp::utility {
namespace {

constexpr auto kNfc = static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPOSE);
struct Cp932Entry {
  std::uint16_t code;
  std::uint16_t scalar;
};
#include "cp932_table.inc"

TextResult error(const TextError value) { return {value, {}}; }

TextResult normalize(const std::string_view input, const std::size_t input_limit) {
  if (input.size() > input_limit)
    return error(TextError::SizeLimit);
  if (input.find('\0') != std::string_view::npos)
    return error(TextError::EmbeddedNul);
  if (input.empty())
    return {};
  const auto* bytes = reinterpret_cast<const utf8proc_uint8_t*>(input.data());
  const auto length = static_cast<utf8proc_ssize_t>(input.size());
  const auto count = utf8proc_decompose(bytes, length, nullptr, 0, kNfc);
  if (count < 0)
    return error(count == UTF8PROC_ERROR_INVALIDUTF8 ? TextError::InvalidEncoding
                                                     : TextError::Normalization);
  // Reserve input, final output and the reencoder's terminating byte as well
  // as the counted UTF-32 workspace. All arithmetic is bounded before allocation.
  const auto max_elements =
      (kMetadataWorkBytes - 3 * kMetadataMaxBytes - kMetadataMaxBytes) / sizeof(utf8proc_int32_t);
  if (static_cast<std::size_t>(count) >= max_elements)
    return error(TextError::SizeLimit);
  std::vector<utf8proc_int32_t> workspace(static_cast<std::size_t>(count) + 1);
  if (utf8proc_decompose(bytes, length, workspace.data(), count, kNfc) != count)
    return error(TextError::Normalization);
  const auto written = utf8proc_reencode(workspace.data(), count, kNfc);
  if (written < 0)
    return error(TextError::Normalization);
  if (static_cast<std::size_t>(written) > kMetadataMaxBytes)
    return error(TextError::SizeLimit);
  return {TextError::None,
          {reinterpret_cast<const char*>(workspace.data()), static_cast<std::size_t>(written)}};
}

void append_scalar(std::string& text, const utf8proc_int32_t scalar) {
  std::array<utf8proc_uint8_t, 4> bytes{};
  const auto count = utf8proc_encode_char(scalar, bytes.data());
  text.append(reinterpret_cast<const char*>(bytes.data()), static_cast<std::size_t>(count));
}

TextResult decode_cp932(const std::string_view input) {
  if (input.size() > kMetadataMaxBytes)
    return error(TextError::SizeLimit);
  std::string decoded;
  decoded.reserve(input.size() * 3);
  for (std::size_t offset = 0; offset < input.size();) {
    std::uint16_t code = static_cast<unsigned char>(input[offset++]);
    if ((code >= 0x81 && code <= 0x9f) || (code >= 0xe0 && code <= 0xfc)) {
      if (offset == input.size())
        return error(TextError::InvalidEncoding);
      code = static_cast<std::uint16_t>((code << 8U) | static_cast<unsigned char>(input[offset++]));
    }
    const auto found = std::lower_bound(
        kCp932.begin(), kCp932.end(), code,
        [](const Cp932Entry& entry, const std::uint16_t key) { return entry.code < key; });
    if (found == kCp932.end() || found->code != code)
      return error(TextError::InvalidEncoding);
    append_scalar(decoded, found->scalar);
  }
  return {TextError::None, std::move(decoded)};
}

TextResult sanitize_title(TextResult decoded) {
  if (!decoded.ok())
    return decoded;
  auto& text = decoded.text;
  std::size_t first = text.size();
  std::size_t last = 0;
  for (std::size_t offset = 0; offset < text.size();) {
    utf8proc_int32_t scalar = 0;
    const auto count =
        utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(text.data() + offset),
                         static_cast<utf8proc_ssize_t>(text.size() - offset), &scalar);
    if (count < 0)
      return error(TextError::InvalidEncoding);
    if (scalar == '\r' || scalar == '\n' || scalar == '\t') {
      text[offset] = ' ';
      scalar = ' ';
    } else if (scalar < 0x20 || (scalar >= 0x7f && scalar <= 0x9f)) {
      return error(TextError::ControlCharacter);
    }
    if (scalar != 0x20 && scalar != 0x3000) {
      first = std::min(first, offset);
      last = offset + static_cast<std::size_t>(count);
    }
    offset += static_cast<std::size_t>(count);
  }
  if (last == 0)
    return error(TextError::Empty);
  // Erase in place so decoded input and a second display copy do not coexist.
  text.erase(last);
  text.erase(0, first);
  return decoded;
}

} // namespace

TextResult normalize_utf8(const std::string_view input) {
  return normalize(input, kMetadataMaxBytes);
}

TextResult utf16_to_utf8(const std::u16string_view input) {
  if (input.size() > kMetadataMaxBytes)
    return error(TextError::SizeLimit);
  std::string text;
  text.reserve(kMetadataMaxBytes);
  for (std::size_t offset = 0; offset < input.size();) {
    auto scalar = static_cast<utf8proc_int32_t>(input[offset++]);
    if (scalar >= 0xd800 && scalar <= 0xdbff) {
      if (offset == input.size())
        return error(TextError::InvalidEncoding);
      const auto low = static_cast<utf8proc_int32_t>(input[offset++]);
      if (low < 0xdc00 || low > 0xdfff)
        return error(TextError::InvalidEncoding);
      scalar = 0x10000 + ((scalar - 0xd800) << 10) + low - 0xdc00;
    } else if (scalar >= 0xdc00 && scalar <= 0xdfff) {
      return error(TextError::InvalidEncoding);
    }
    if (scalar == 0)
      return error(TextError::EmbeddedNul);
    const std::size_t count = scalar <= 0x7f ? 1 : scalar <= 0x7ff ? 2 : scalar <= 0xffff ? 3 : 4;
    if (count > kMetadataMaxBytes - text.size())
      return error(TextError::SizeLimit);
    append_scalar(text, scalar);
  }
  return {TextError::None, std::move(text)};
}

MdxTitleResult select_mdx_title(const std::string_view raw_title,
                                const std::string_view filename_stem) {
  auto title = sanitize_title(decode_cp932(raw_title));
  TitleFallback fallback = TitleFallback::None;
  switch (title.error) {
  case TextError::InvalidEncoding:
    fallback = TitleFallback::InvalidEncoding;
    break;
  case TextError::Empty:
    fallback = TitleFallback::Empty;
    break;
  case TextError::ControlCharacter:
    fallback = TitleFallback::ControlCharacter;
    break;
  default:
    break;
  }
  if (fallback != TitleFallback::None) {
    // NFC validates the fallback before display sanitation; NUL is a fatal
    // filename error, while NUL in the embedded title is a fallback reason.
    title = sanitize_title(normalize_utf8(filename_stem));
  } else if (title.ok()) {
    title = normalize(title.text, kMetadataMaxBytes * 3);
  }
  return {std::move(title), fallback};
}

const char* title_fallback_name(const TitleFallback reason) noexcept {
  switch (reason) {
  case TitleFallback::None:
    return "none";
  case TitleFallback::InvalidEncoding:
    return "invalid-encoding";
  case TitleFallback::Empty:
    return "empty";
  case TitleFallback::ControlCharacter:
    return "control-character";
  }
  return "unknown";
}

} // namespace rpcmp::utility
