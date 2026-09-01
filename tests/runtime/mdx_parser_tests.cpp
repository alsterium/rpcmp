#include "rpcmp/runtime/mdx_parser.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

using rpcmp::runtime::mdx::ByteView;
using rpcmp::runtime::mdx::MdxDocument;
using rpcmp::runtime::mdx::MdxError;
using rpcmp::runtime::mdx::ParseLimits;
using rpcmp::runtime::mdx::TrackTarget;

std::vector<std::uint8_t> decode_hex(const std::string& text) {
  std::vector<std::uint8_t> bytes;
  int high = -1;
  for (const char character : text) {
    int nibble = -1;
    if (character >= '0' && character <= '9') {
      nibble = character - '0';
    } else if (character >= 'a' && character <= 'f') {
      nibble = character - 'a' + 10;
    } else if (character == '\n' || character == '\r' || character == ' ' || character == '\t') {
      continue;
    }
    if (nibble < 0) {
      return {};
    }
    if (high < 0) {
      high = nibble;
    } else {
      bytes.push_back(static_cast<std::uint8_t>((high << 4) | nibble));
      high = -1;
    }
  }
  return high < 0 ? bytes : std::vector<std::uint8_t>{};
}

std::vector<std::uint8_t> fixture() {
  const std::string path = std::string(RPCMP_SOURCE_DIR) + "/tests/fixtures/mdx/minimal-fm.mdx.hex";
  std::ifstream stream(path, std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  return decode_hex(text);
}

ByteView view(const std::vector<std::uint8_t>& bytes) { return {bytes.data(), bytes.size()}; }

void write_u16be(std::vector<std::uint8_t>& bytes, const std::size_t offset,
                 const std::uint16_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value >> 8U);
  bytes[offset + 1] = static_cast<std::uint8_t>(value);
}

MdxError parse_error(const std::vector<std::uint8_t>& bytes,
                     const ParseLimits limits = ParseLimits{}) {
  MdxDocument output{};
  return rpcmp::runtime::mdx::parse(view(bytes), output, limits).error;
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  const auto valid_bytes = fixture();
  RPCMP_CHECK(suite, valid_bytes.size() == 77);

  MdxDocument document{};
  const auto valid = rpcmp::runtime::mdx::parse(view(valid_bytes), document);
  RPCMP_CHECK(suite, valid.ok());
  RPCMP_CHECK(suite, document.input.data == valid_bytes.data());
  RPCMP_CHECK(suite, document.original_title.size == 8);
  RPCMP_CHECK(suite, document.pdx_reference.size == 0);
  RPCMP_CHECK(suite, document.voice_count == 1);
  RPCMP_CHECK(suite, document.voices[1].present);
  RPCMP_CHECK(suite, document.voices[1].slot_mask == 0x0f);
  for (std::size_t index = 0; index < document.tracks.size(); ++index) {
    RPCMP_CHECK(suite, document.tracks[index].logical_channel == index);
    RPCMP_CHECK(suite, document.tracks[index].source.size == 2);
    RPCMP_CHECK(suite, document.tracks[index].source.data[0] == 0xf1);
    RPCMP_CHECK(suite, document.tracks[index].source.data[1] == 0x00);
    RPCMP_CHECK(suite, document.tracks[index].target ==
                           (index < 8 ? TrackTarget::Ym2151 : TrackTarget::LegacyAdpcm));
  }

  MdxDocument unchanged{};
  unchanged.voice_count = 99;
  auto truncated = valid_bytes;
  truncated.resize(3);
  const auto rejected = rpcmp::runtime::mdx::parse(view(truncated), unchanged);
  RPCMP_CHECK(suite, rejected.error == MdxError::MissingTitleTerminator);
  RPCMP_CHECK(suite, unchanged.voice_count == 99);

  for (std::size_t size = 0; size < valid_bytes.size(); ++size) {
    auto prefix = valid_bytes;
    prefix.resize(size);
    RPCMP_CHECK(suite, parse_error(prefix) != MdxError::None);
  }

  auto corrupted = valid_bytes;
  corrupted[8] = 0;
  RPCMP_CHECK(suite, parse_error(corrupted) == MdxError::MissingTitleTerminator);

  corrupted = valid_bytes;
  corrupted[11] = 'x';
  corrupted.resize(12);
  RPCMP_CHECK(suite, parse_error(corrupted) == MdxError::MissingPdxTerminator);

  corrupted = valid_bytes;
  write_u16be(corrupted, 14, 34);
  RPCMP_CHECK(suite, parse_error(corrupted) == MdxError::UnsupportedPcm8Layout);

  corrupted.resize(30);
  RPCMP_CHECK(suite, parse_error(corrupted) == MdxError::RangeOutsideInput);

  corrupted = valid_bytes;
  write_u16be(corrupted, 14, 0xffff);
  RPCMP_CHECK(suite, parse_error(corrupted) == MdxError::InvalidTrackTable);

  corrupted = valid_bytes;
  write_u16be(corrupted, 16, 20);
  RPCMP_CHECK(suite, parse_error(corrupted) == MdxError::OverlappingRegions);

  corrupted = valid_bytes;
  write_u16be(corrupted, 12, 0xffff);
  RPCMP_CHECK(suite, parse_error(corrupted) == MdxError::RangeOutsideInput);

  corrupted = valid_bytes;
  corrupted[51] = 0xc0;
  RPCMP_CHECK(suite, parse_error(corrupted) == MdxError::InvalidVoiceRecord);

  auto duplicate_voice = valid_bytes;
  duplicate_voice.insert(duplicate_voice.end(), valid_bytes.begin() + 50, valid_bytes.end());
  RPCMP_CHECK(suite, parse_error(duplicate_voice) == MdxError::DuplicateVoice);

  ParseLimits limits{};
  limits.max_input_bytes = valid_bytes.size() - 1;
  RPCMP_CHECK(suite, parse_error(valid_bytes, limits) == MdxError::InputTooLarge);
  limits = {};
  limits.max_title_bytes = 7;
  RPCMP_CHECK(suite, parse_error(valid_bytes, limits) == MdxError::TitleTooLong);
  limits = {};
  limits.max_voice_records = 0;
  RPCMP_CHECK(suite, parse_error(valid_bytes, limits) == MdxError::InvalidVoiceRecord);
  limits = {};
  limits.max_input_bytes = rpcmp::runtime::mdx::kMdxMaxInputBytes + 1;
  RPCMP_CHECK(suite, parse_error(valid_bytes, limits) == MdxError::InvalidLimits);

  std::vector<std::uint8_t> invalid_pointer(1, 0);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::parse({nullptr, invalid_pointer.size()}, document).error ==
                  MdxError::RangeOutsideInput);

  return suite.finish("mdx structural parser");
}
