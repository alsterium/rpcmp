#include "rpcmp/library/container.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace rpcmp::library {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic{'R', 'P', 'C', 'M', 'L', 'I', 'B', 0};
constexpr std::array<std::uint32_t, 6> kRequiredTags{0x4B415254U, 0x424F4C42U, 0x53525453U,
                                                     0x53504544U, 0x58444E49U, 0x4D555343U};

std::uint16_t read_u16(const std::uint8_t* const bytes) {
  return static_cast<std::uint16_t>(bytes[0]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U);
}

std::uint32_t read_u32(const std::uint8_t* const bytes) {
  return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

std::uint64_t read_u64(const std::uint8_t* const bytes) {
  return static_cast<std::uint64_t>(read_u32(bytes)) |
         (static_cast<std::uint64_t>(read_u32(bytes + 4)) << 32U);
}

bool checked_range(const std::uint64_t offset, const std::uint64_t length,
                   const std::uint64_t file_size) {
  return offset <= file_size && length <= file_size - offset;
}

bool ranges_overlap(const std::uint64_t first_offset, const std::uint64_t first_length,
                    const std::uint64_t second_offset, const std::uint64_t second_length) {
  if (first_length == 0 || second_length == 0) {
    return false;
  }
  return first_offset < second_offset + second_length &&
         second_offset < first_offset + first_length;
}

bool is_power_of_two(const std::uint32_t value) {
  return value != 0 && (value & (value - 1U)) == 0;
}

std::size_t required_tag_index(const std::uint32_t tag) {
  for (std::size_t index = 0; index < kRequiredTags.size(); ++index) {
    if (kRequiredTags[index] == tag) {
      return index;
    }
  }
  return kRequiredTags.size();
}

std::uint32_t header_crc32(const ByteView file) {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (std::size_t index = 0; index < kHeaderSize; ++index) {
    const std::uint8_t byte = index >= 72 && index < 76 ? 0 : file.data[index];
    crc ^= byte;
    for (std::uint32_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

} // namespace

std::uint32_t crc32(const ByteView bytes) {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (std::size_t index = 0; index < bytes.size; ++index) {
    crc ^= bytes.data[index];
    for (std::uint32_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

ValidationResult validate_container_envelope(const ByteView file, const ValidationLimits limits) {
  ValidationResult result;
  if (file.size < kHeaderSize || file.data == nullptr) {
    result.error = LibraryError::FileTooSmall;
    return result;
  }
  for (std::size_t index = 0; index < kMagic.size(); ++index) {
    if (file.data[index] != kMagic[index]) {
      result.error = LibraryError::BadMagic;
      return result;
    }
  }
  if (read_u16(file.data + 8) != 1) {
    result.error = LibraryError::UnsupportedMajorVersion;
    return result;
  }
  result.info.format_minor = read_u16(file.data + 10);
  if (read_u32(file.data + 12) != kHeaderSize) {
    result.error = LibraryError::InvalidHeaderSize;
    return result;
  }
  result.info.optional_features = read_u64(file.data + 16);
  if (read_u64(file.data + 24) != 0) {
    result.error = LibraryError::UnsupportedRequiredFeatures;
    return result;
  }
  const std::uint64_t declared_size = read_u64(file.data + 32);
  if (declared_size != file.size || declared_size > limits.max_file_size) {
    result.error = LibraryError::FileSizeMismatch;
    return result;
  }
  if (read_u32(file.data + 72) != header_crc32(file)) {
    result.error = LibraryError::BadHeaderChecksum;
    return result;
  }

  const std::uint64_t directory_offset = read_u64(file.data + 40);
  result.info.section_count = read_u32(file.data + 48);
  if (result.info.section_count > limits.max_sections) {
    result.error = LibraryError::SectionLimitExceeded;
    return result;
  }
  if (read_u32(file.data + 52) != kDirectoryEntrySize) {
    result.error = LibraryError::InvalidDirectoryEntrySize;
    return result;
  }
  if (result.info.section_count > std::numeric_limits<std::uint64_t>::max() / kDirectoryEntrySize) {
    result.error = LibraryError::ArithmeticOverflow;
    return result;
  }
  const std::uint64_t directory_length =
      static_cast<std::uint64_t>(result.info.section_count) * kDirectoryEntrySize;
  if (directory_offset < kHeaderSize ||
      !checked_range(directory_offset, directory_length, declared_size)) {
    result.error = LibraryError::RangeOutsideFile;
    return result;
  }
  for (std::size_t index = 0; index < result.info.build_id.size(); ++index) {
    result.info.build_id[index] = file.data[56 + index];
  }

  std::array<bool, kRequiredTags.size()> found_required{};
  for (std::uint32_t index = 0; index < result.info.section_count; ++index) {
    const std::uint64_t entry_offset =
        directory_offset + static_cast<std::uint64_t>(index) * kDirectoryEntrySize;
    const auto* const entry = file.data + static_cast<std::size_t>(entry_offset);
    const std::uint32_t tag = read_u32(entry);
    const std::uint32_t flags = read_u32(entry + 4);
    const std::uint64_t payload_offset = read_u64(entry + 8);
    const std::uint64_t payload_length = read_u64(entry + 16);
    const std::uint32_t alignment = read_u32(entry + 28);

    if (!is_power_of_two(alignment)) {
      result.error = LibraryError::InvalidAlignment;
      return result;
    }
    if ((payload_offset & (alignment - 1U)) != 0) {
      result.error = LibraryError::MisalignedSection;
      return result;
    }
    if (!checked_range(payload_offset, payload_length, declared_size)) {
      result.error = LibraryError::RangeOutsideFile;
      return result;
    }
    if (ranges_overlap(payload_offset, payload_length, 0, kHeaderSize) ||
        ranges_overlap(payload_offset, payload_length, directory_offset, directory_length)) {
      result.error = LibraryError::SectionOverlap;
      return result;
    }

    for (std::uint32_t previous = 0; previous < index; ++previous) {
      const std::uint64_t previous_entry_offset =
          directory_offset + static_cast<std::uint64_t>(previous) * kDirectoryEntrySize;
      const auto* const previous_entry =
          file.data + static_cast<std::size_t>(previous_entry_offset);
      if (read_u32(previous_entry) == tag) {
        result.error = LibraryError::DuplicateSection;
        return result;
      }
      if (ranges_overlap(payload_offset, payload_length, read_u64(previous_entry + 8),
                         read_u64(previous_entry + 16))) {
        result.error = LibraryError::SectionOverlap;
        return result;
      }
    }

    const std::size_t required_index = required_tag_index(tag);
    if (required_index == kRequiredTags.size()) {
      if ((flags & 1U) != 0) {
        result.error = LibraryError::UnknownRequiredSection;
        return result;
      }
    } else {
      found_required[required_index] = true;
    }
    const ByteView payload{file.data + static_cast<std::size_t>(payload_offset),
                           static_cast<std::size_t>(payload_length)};
    if (read_u32(entry + 24) != crc32(payload)) {
      result.error = LibraryError::BadSectionChecksum;
      return result;
    }
  }

  for (const bool found : found_required) {
    if (!found) {
      result.error = LibraryError::MissingRequiredSection;
      return result;
    }
  }
  return result;
}

} // namespace rpcmp::library
