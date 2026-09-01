#include "rpcmp/runtime/mdx_parser.hpp"

#include <algorithm>
#include <array>

namespace rpcmp::runtime::mdx {
namespace {

constexpr std::array<std::uint8_t, 3> kTitleTerminator{0x0d, 0x0a, 0x1a};
constexpr std::size_t kVoiceRecordBytes = 27;

ParseResult failure(const MdxError error, const std::size_t offset,
                    const std::uint8_t track = 0xff) noexcept {
  return {error, offset, track};
}

bool valid_limits(const ParseLimits& limits) noexcept {
  return limits.max_input_bytes != 0 && limits.max_input_bytes <= kMdxMaxInputBytes &&
         limits.max_title_bytes <= kMdxMaxTitleBytes &&
         limits.max_pdx_reference_bytes <= kMdxMaxPdxReferenceBytes &&
         limits.max_voice_records <= kMdxVoiceSlots;
}

std::size_t find_title_terminator(const ByteView input) noexcept {
  if (input.size < kTitleTerminator.size()) {
    return input.size;
  }
  for (std::size_t offset = 0; offset <= input.size - kTitleTerminator.size(); ++offset) {
    if (std::equal(kTitleTerminator.begin(), kTitleTerminator.end(), input.data + offset)) {
      return offset;
    }
  }
  return input.size;
}

std::size_t find_byte(const ByteView input, const std::size_t begin,
                      const std::uint8_t value) noexcept {
  for (std::size_t offset = begin; offset < input.size; ++offset) {
    if (input.data[offset] == value) {
      return offset;
    }
  }
  return input.size;
}

std::uint16_t read_u16be(const ByteView input, const std::size_t offset) noexcept {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(input.data[offset]) << 8U) |
                                    input.data[offset + 1]);
}

} // namespace

ParseResult parse(const ByteView input, MdxDocument& output, const ParseLimits limits) noexcept {
  if (!valid_limits(limits)) {
    return failure(MdxError::InvalidLimits, 0);
  }
  if ((input.data == nullptr && input.size != 0) || input.size > limits.max_input_bytes) {
    return failure(input.size > limits.max_input_bytes ? MdxError::InputTooLarge
                                                       : MdxError::RangeOutsideInput,
                   0);
  }

  const std::size_t title_end = find_title_terminator(input);
  if (title_end == input.size) {
    return failure(MdxError::MissingTitleTerminator, input.size);
  }
  if (title_end > limits.max_title_bytes) {
    return failure(MdxError::TitleTooLong, title_end);
  }

  const std::size_t pdx_begin = title_end + kTitleTerminator.size();
  const std::size_t pdx_end = find_byte(input, pdx_begin, 0);
  if (pdx_end == input.size) {
    return failure(MdxError::MissingPdxTerminator, input.size);
  }
  if (pdx_end - pdx_begin > limits.max_pdx_reference_bytes) {
    return failure(MdxError::PdxReferenceTooLong, pdx_end);
  }

  const std::size_t base = pdx_end + 1;
  if (base > input.size || input.size - base < 4) {
    return failure(MdxError::RangeOutsideInput, base);
  }
  if (input.size - base >= 7 && input.data[base + 4] == 'L' && input.data[base + 5] == 'Z' &&
      input.data[base + 6] == 'X') {
    return failure(MdxError::UnsupportedCompression, base + 4);
  }

  const std::uint16_t first_track_offset = read_u16be(input, base + 2);
  if (first_track_offset < 2 || ((first_track_offset - 2U) & 1U) != 0) {
    return failure(MdxError::InvalidTrackTable, base + 2);
  }
  const std::size_t track_count = (first_track_offset - 2U) / 2U;
  if (track_count == 16) {
    constexpr std::size_t kPcm8TableBytes = 2 + 16 * 2;
    if (input.size - base < kPcm8TableBytes) {
      return failure(MdxError::RangeOutsideInput, base);
    }
    return failure(MdxError::UnsupportedPcm8Layout, base + 2);
  }
  if (track_count != kMdxTrackCount) {
    return failure(MdxError::InvalidTrackTable, base + 2);
  }

  constexpr std::size_t kObjectCount = kMdxTrackCount + 1;
  constexpr std::size_t kTableBytes = kObjectCount * 2;
  if (input.size - base < kTableBytes) {
    return failure(MdxError::RangeOutsideInput, base);
  }

  std::array<std::size_t, kObjectCount> offsets{};
  for (std::size_t index = 0; index < offsets.size(); ++index) {
    offsets[index] = read_u16be(input, base + index * 2);
    if (offsets[index] < kTableBytes || offsets[index] >= input.size - base) {
      return failure(MdxError::RangeOutsideInput, base + index * 2);
    }
  }
  for (std::size_t left = 0; left < offsets.size(); ++left) {
    for (std::size_t right = left + 1; right < offsets.size(); ++right) {
      if (offsets[left] == offsets[right]) {
        return failure(MdxError::OverlappingRegions, base + right * 2);
      }
    }
  }

  auto sorted_offsets = offsets;
  std::sort(sorted_offsets.begin(), sorted_offsets.end());
  const auto region_end = [&](const std::size_t offset) noexcept {
    const auto next = std::upper_bound(sorted_offsets.begin(), sorted_offsets.end(), offset);
    return next == sorted_offsets.end() ? input.size : base + *next;
  };

  MdxDocument candidate{};
  candidate.input = input;
  candidate.original_title = {input.data, title_end};
  candidate.pdx_reference = {input.data + pdx_begin, pdx_end - pdx_begin};

  const std::size_t voice_start = base + offsets[0];
  const std::size_t voice_end = region_end(offsets[0]);
  const std::size_t voice_bytes = voice_end - voice_start;
  if (voice_bytes % kVoiceRecordBytes != 0) {
    return failure(MdxError::InvalidVoiceRecord, voice_start);
  }
  candidate.voice_count = voice_bytes / kVoiceRecordBytes;
  if (candidate.voice_count > limits.max_voice_records) {
    return failure(MdxError::InvalidVoiceRecord, voice_start);
  }
  for (std::size_t record = 0; record < candidate.voice_count; ++record) {
    const std::size_t record_offset = voice_start + record * kVoiceRecordBytes;
    const std::uint8_t voice_id = input.data[record_offset];
    if (candidate.voices[voice_id].present) {
      return failure(MdxError::DuplicateVoice, record_offset);
    }
    Voice& voice = candidate.voices[voice_id];
    voice.present = true;
    voice.feedback_connection = input.data[record_offset + 1];
    if ((voice.feedback_connection & 0xc0U) != 0) {
      return failure(MdxError::InvalidVoiceRecord, record_offset + 1);
    }
    voice.slot_mask = input.data[record_offset + 2];
    for (std::size_t group = 0; group < voice.operators.size(); ++group) {
      for (std::size_t op = 0; op < voice.operators[group].size(); ++op) {
        voice.operators[group][op] = input.data[record_offset + 3 + group * 4 + op];
      }
    }
  }

  for (std::size_t index = 0; index < candidate.tracks.size(); ++index) {
    const std::size_t start = base + offsets[index + 1];
    const std::size_t end = region_end(offsets[index + 1]);
    if (end <= start) {
      return failure(MdxError::OverlappingRegions, start, static_cast<std::uint8_t>(index));
    }
    candidate.tracks[index] = {
        static_cast<std::uint8_t>(index),
        index < 8 ? TrackTarget::Ym2151 : TrackTarget::LegacyAdpcm,
        start,
        {input.data + start, end - start},
    };
  }

  output = candidate;
  return {};
}

} // namespace rpcmp::runtime::mdx
