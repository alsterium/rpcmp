#include "rpcmp/library/logical_library.hpp"

#include "rpcmp/library/album_catalog.hpp"
#include "rpcmp/library/formats.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace rpcmp::library {
namespace {

constexpr std::uint32_t kTrackTag = 0x4B415254U;
constexpr std::uint32_t kBlobTag = 0x424F4C42U;
constexpr std::uint32_t kStringTag = 0x53525453U;
constexpr std::uint32_t kDependencyTag = 0x53504544U;
constexpr std::uint32_t kIndexTag = 0x58444E49U;
constexpr std::uint32_t kChecksumTag = 0x4D555343U;
constexpr std::uint32_t kTrackEntity = 1;
constexpr std::uint32_t kBlobEntity = 2;

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
                   const std::uint64_t size) {
  return offset <= size && length <= size - offset;
}

bool checked_array(const std::uint32_t count, const std::uint32_t record_size,
                   const std::uint64_t header_size, const std::uint64_t section_size,
                   std::uint64_t& end) {
  if (record_size != 0 &&
      count > (std::numeric_limits<std::uint64_t>::max() - header_size) / record_size) {
    return false;
  }
  end = header_size + static_cast<std::uint64_t>(count) * record_size;
  return end <= section_size;
}

bool is_power_of_two(const std::uint32_t value) {
  return value != 0 && (value & (value - 1U)) == 0;
}

bool valid_utf8(const std::uint8_t* data, const std::size_t size) {
  std::size_t index = 0;
  while (index < size) {
    const std::uint8_t first = data[index++];
    if (first < 0x80U) {
      continue;
    }
    std::uint32_t codepoint{};
    std::size_t remaining{};
    if (first >= 0xC2U && first <= 0xDFU) {
      codepoint = first & 0x1FU;
      remaining = 1;
    } else if (first >= 0xE0U && first <= 0xEFU) {
      codepoint = first & 0x0FU;
      remaining = 2;
    } else if (first >= 0xF0U && first <= 0xF4U) {
      codepoint = first & 0x07U;
      remaining = 3;
    } else {
      return false;
    }
    if (remaining > size - index) {
      return false;
    }
    for (std::size_t continuation = 0; continuation < remaining; ++continuation) {
      const std::uint8_t byte = data[index++];
      if ((byte & 0xC0U) != 0x80U) {
        return false;
      }
      codepoint = (codepoint << 6U) | (byte & 0x3FU);
    }
    if ((remaining == 2 && codepoint < 0x800U) || (remaining == 3 && codepoint < 0x10000U) ||
        codepoint > 0x10FFFFU || (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
      return false;
    }
  }
  return true;
}

const std::uint8_t* find_index_record(const std::uint8_t* const section, const std::uint32_t count,
                                      const std::uint32_t kind, const std::uint64_t id) {
  std::uint32_t first = 0;
  std::uint32_t last = count;
  while (first < last) {
    const std::uint32_t middle = first + (last - first) / 2U;
    const auto* const record = section + 8 + static_cast<std::size_t>(middle) * 24;
    const std::uint32_t record_kind = read_u32(record);
    const std::uint64_t record_id = read_u64(record + 8);
    if (record_kind < kind || (record_kind == kind && record_id < id)) {
      first = middle + 1U;
    } else {
      last = middle;
    }
  }
  if (first == count) {
    return nullptr;
  }
  const auto* const record = section + 8 + static_cast<std::size_t>(first) * 24;
  return read_u32(record) == kind && read_u64(record + 8) == id ? record : nullptr;
}

const std::uint8_t* find_string_record(const std::uint8_t* const section, const std::uint32_t count,
                                       const std::uint64_t id) {
  std::uint32_t first = 0;
  std::uint32_t last = count;
  while (first < last) {
    const std::uint32_t middle = first + (last - first) / 2U;
    const auto* const record = section + 16 + static_cast<std::size_t>(middle) * 16;
    if (read_u64(record) < id) {
      first = middle + 1U;
    } else {
      last = middle;
    }
  }
  if (first == count) {
    return nullptr;
  }
  const auto* const record = section + 16 + static_cast<std::size_t>(first) * 16;
  return read_u64(record) == id ? record : nullptr;
}

Utf8View string_view(const std::uint8_t* const section, const std::uint32_t count,
                     const std::uint64_t id) {
  const auto* const record = find_string_record(section, count, id);
  const std::uint64_t data_offset = read_u64(section + 8);
  return {reinterpret_cast<const char*>(section + data_offset + read_u32(record + 8)),
          read_u32(record + 12)};
}

} // namespace

LibraryError LogicalLibrary::open(const ByteView file, LogicalLibrary& output,
                                  const ValidationLimits limits) {
  const ValidationResult envelope = validate_container_envelope(file, limits);
  if (!envelope.ok()) {
    return envelope.error;
  }

  LogicalLibrary candidate;
  candidate.file_ = file;
  const std::uint64_t directory_offset = read_u64(file.data + 40);
  for (std::uint32_t index = 0; index < envelope.info.section_count; ++index) {
    const auto* const entry =
        file.data + directory_offset + static_cast<std::uint64_t>(index) * kDirectoryEntrySize;
    Section section{file.data + read_u64(entry + 8),
                    static_cast<std::size_t>(read_u64(entry + 16))};
    switch (read_u32(entry)) {
    case kTrackTag:
      candidate.tracks_ = section;
      break;
    case kBlobTag:
      candidate.blobs_ = section;
      break;
    case kStringTag:
      candidate.strings_ = section;
      break;
    case kDependencyTag:
      candidate.dependencies_ = section;
      break;
    case kIndexTag:
      candidate.index_ = section;
      break;
    case kChecksumTag:
      candidate.checksums_ = section;
      break;
    default:
      break;
    }
  }

  if (candidate.tracks_.size < 8 || candidate.blobs_.size < 16 || candidate.strings_.size < 16 ||
      candidate.dependencies_.size < 8 || candidate.index_.size < 8 ||
      candidate.checksums_.size < 8) {
    return LibraryError::InvalidSectionHeader;
  }
  candidate.track_count_ = read_u32(candidate.tracks_.data);
  candidate.blob_count_ = read_u32(candidate.blobs_.data);
  candidate.string_count_ = read_u32(candidate.strings_.data);
  candidate.dependency_count_ = read_u32(candidate.dependencies_.data);
  const std::uint32_t index_count = read_u32(candidate.index_.data);
  const std::uint32_t checksum_count = read_u32(candidate.checksums_.data);
  if (candidate.track_count_ > limits.max_records_per_section ||
      candidate.blob_count_ > limits.max_records_per_section ||
      candidate.string_count_ > limits.max_records_per_section ||
      candidate.dependency_count_ > limits.max_records_per_section ||
      index_count > limits.max_records_per_section ||
      checksum_count > limits.max_records_per_section) {
    return LibraryError::RecordLimitExceeded;
  }

  std::uint64_t tracks_end{};
  std::uint64_t blobs_records_end{};
  std::uint64_t strings_records_end{};
  std::uint64_t dependencies_end{};
  std::uint64_t index_end{};
  std::uint64_t checksums_end{};
  if (read_u32(candidate.tracks_.data + 4) != 96 ||
      !checked_array(candidate.track_count_, 96, 8, candidate.tracks_.size, tracks_end) ||
      tracks_end != candidate.tracks_.size || read_u32(candidate.blobs_.data + 4) != 48 ||
      !checked_array(candidate.blob_count_, 48, 16, candidate.blobs_.size, blobs_records_end) ||
      read_u32(candidate.strings_.data + 4) != 16 ||
      !checked_array(candidate.string_count_, 16, 16, candidate.strings_.size,
                     strings_records_end) ||
      read_u32(candidate.dependencies_.data + 4) != 24 ||
      !checked_array(candidate.dependency_count_, 24, 8, candidate.dependencies_.size,
                     dependencies_end) ||
      dependencies_end != candidate.dependencies_.size ||
      read_u32(candidate.index_.data + 4) != 24 ||
      !checked_array(index_count, 24, 8, candidate.index_.size, index_end) ||
      index_end != candidate.index_.size || read_u32(candidate.checksums_.data + 4) != 24 ||
      !checked_array(checksum_count, 24, 8, candidate.checksums_.size, checksums_end) ||
      checksums_end != candidate.checksums_.size) {
    return LibraryError::InvalidSectionHeader;
  }
  const std::uint64_t blob_data_offset = read_u64(candidate.blobs_.data + 8);
  const std::uint64_t string_data_offset = read_u64(candidate.strings_.data + 8);
  if (blob_data_offset < blobs_records_end || blob_data_offset > candidate.blobs_.size ||
      string_data_offset < strings_records_end || string_data_offset > candidate.strings_.size) {
    return LibraryError::InvalidSectionHeader;
  }

  std::uint64_t previous_id{};
  std::uint64_t previous_string_end = string_data_offset;
  for (std::uint32_t ordinal = 0; ordinal < candidate.string_count_; ++ordinal) {
    const auto* const record =
        candidate.strings_.data + 16 + static_cast<std::size_t>(ordinal) * 16;
    const std::uint64_t id = read_u64(record);
    const std::uint32_t offset = read_u32(record + 8);
    const std::uint32_t length = read_u32(record + 12);
    if (offset > candidate.strings_.size - string_data_offset) {
      return LibraryError::RangeOutsideFile;
    }
    const std::uint64_t absolute_offset = string_data_offset + offset;
    if (id == 0 || id <= previous_id || absolute_offset < previous_string_end) {
      return LibraryError::InvalidRecordOrder;
    }
    if (length > limits.max_string_bytes) {
      return LibraryError::StringTooLong;
    }
    if (!checked_range(string_data_offset + offset, length, candidate.strings_.size)) {
      return LibraryError::RangeOutsideFile;
    }
    const auto* const data = candidate.strings_.data + string_data_offset + offset;
    for (std::uint32_t byte = 0; byte < length; ++byte) {
      if (data[byte] == 0) {
        return LibraryError::EmbeddedNul;
      }
    }
    if (!valid_utf8(data, length)) {
      return LibraryError::InvalidUtf8;
    }
    previous_string_end = absolute_offset + length;
    previous_id = id;
  }

  previous_id = 0;
  std::uint64_t previous_payload_end = blob_data_offset;
  std::uint64_t total_decoded{};
  for (std::uint32_t ordinal = 0; ordinal < candidate.blob_count_; ++ordinal) {
    const auto* const record = candidate.blobs_.data + 16 + static_cast<std::size_t>(ordinal) * 48;
    const std::uint64_t id = read_u64(record);
    const std::uint16_t codec = read_u16(record + 12);
    const std::uint64_t decoded_size = read_u64(record + 16);
    const std::uint64_t stored_size = read_u64(record + 24);
    const std::uint64_t payload_offset = read_u64(record + 32);
    const std::uint32_t alignment = read_u32(record + 40);
    if (id == 0 || id <= previous_id || payload_offset < previous_payload_end) {
      return LibraryError::InvalidRecordOrder;
    }
    if (codec != 0 || decoded_size != stored_size) {
      return LibraryError::UnsupportedCodec;
    }
    if (!is_power_of_two(alignment) || (payload_offset & (alignment - 1U)) != 0 ||
        payload_offset < blob_data_offset) {
      return LibraryError::InvalidAlignment;
    }
    if (!checked_range(payload_offset, stored_size, candidate.blobs_.size)) {
      return LibraryError::RangeOutsideFile;
    }
    if (read_u32(record + 44) !=
        crc32({candidate.blobs_.data + payload_offset, static_cast<std::size_t>(stored_size)})) {
      return LibraryError::BadSectionChecksum;
    }
    if (decoded_size > limits.max_total_decoded_bytes - total_decoded) {
      return LibraryError::DecodedSizeLimitExceeded;
    }
    total_decoded += decoded_size;
    previous_payload_end = payload_offset + stored_size;
    previous_id = id;
  }

  previous_id = 0;
  std::uint32_t next_dependency = 0;
  for (std::uint32_t ordinal = 0; ordinal < candidate.track_count_; ++ordinal) {
    const auto* const record = candidate.tracks_.data + 8 + static_cast<std::size_t>(ordinal) * 96;
    const std::uint64_t id = read_u64(record);
    const std::uint32_t first_dependency = read_u32(record + 80);
    const std::uint32_t dependency_count = read_u32(record + 84);
    if (id == 0 || id <= previous_id) {
      return LibraryError::InvalidRecordOrder;
    }
    if (dependency_count > limits.max_dependencies_per_track) {
      return LibraryError::DependencyLimitExceeded;
    }
    if (first_dependency != next_dependency ||
        dependency_count > candidate.dependency_count_ - next_dependency) {
      return LibraryError::InvalidReference;
    }
    next_dependency += dependency_count;
    if (record[90] > 2) {
      return LibraryError::InvalidReference;
    }
    previous_id = id;
  }
  if (next_dependency != candidate.dependency_count_) {
    return LibraryError::InvalidReference;
  }

  if (static_cast<std::uint64_t>(index_count) !=
      static_cast<std::uint64_t>(candidate.track_count_) + candidate.blob_count_) {
    return LibraryError::InvalidIndex;
  }
  std::uint32_t previous_kind{};
  previous_id = 0;
  for (std::uint32_t entry = 0; entry < index_count; ++entry) {
    const auto* const record = candidate.index_.data + 8 + static_cast<std::size_t>(entry) * 24;
    const std::uint32_t kind = read_u32(record);
    const std::uint64_t id = read_u64(record + 8);
    const std::uint32_t ordinal = read_u32(record + 16);
    if ((kind != kTrackEntity && kind != kBlobEntity) || kind < previous_kind ||
        (kind == previous_kind && id <= previous_id)) {
      return LibraryError::InvalidIndex;
    }
    const std::uint32_t count =
        kind == kTrackEntity ? candidate.track_count_ : candidate.blob_count_;
    const std::size_t header = kind == kTrackEntity ? 8 : 16;
    const std::size_t record_size = kind == kTrackEntity ? 96 : 48;
    const auto* const section =
        kind == kTrackEntity ? candidate.tracks_.data : candidate.blobs_.data;
    if (ordinal >= count ||
        read_u64(section + header + static_cast<std::size_t>(ordinal) * record_size) != id) {
      return LibraryError::InvalidIndex;
    }
    previous_kind = kind;
    previous_id = id;
  }

  for (std::uint32_t ordinal = 0; ordinal < candidate.track_count_; ++ordinal) {
    const auto* const track = candidate.tracks_.data + 8 + static_cast<std::size_t>(ordinal) * 96;
    if (find_index_record(candidate.index_.data, index_count, kBlobEntity, read_u64(track + 16)) ==
            nullptr ||
        read_u64(track + 24) == 0 || read_u64(track + 32) == 0) {
      return LibraryError::InvalidReference;
    }
    for (const std::size_t field :
         {std::size_t{24}, std::size_t{32}, std::size_t{40}, std::size_t{48}, std::size_t{56}}) {
      const std::uint64_t string_id = read_u64(track + field);
      if (string_id != 0 && find_string_record(candidate.strings_.data, candidate.string_count_,
                                               string_id) == nullptr) {
        return LibraryError::InvalidReference;
      }
    }
    const std::uint32_t first_dependency = read_u32(track + 80);
    const std::uint32_t dependency_count = read_u32(track + 84);
    for (std::uint32_t position = 0; position < dependency_count; ++position) {
      const auto* const dependency = candidate.dependencies_.data + 8 +
                                     static_cast<std::size_t>(first_dependency + position) * 24;
      if (read_u64(dependency) != read_u64(track) ||
          find_index_record(candidate.index_.data, index_count, kBlobEntity,
                            read_u64(dependency + 16)) == nullptr) {
        return LibraryError::InvalidReference;
      }
    }
  }

  if (checksum_count != candidate.blob_count_) {
    return LibraryError::InvalidChecksumRecord;
  }
  for (std::uint32_t ordinal = 0; ordinal < checksum_count; ++ordinal) {
    const auto* const checksum =
        candidate.checksums_.data + 8 + static_cast<std::size_t>(ordinal) * 24;
    const auto* const blob = candidate.blobs_.data + 16 + static_cast<std::size_t>(ordinal) * 48;
    if (read_u32(checksum) != kBlobEntity || read_u64(checksum + 8) != read_u64(blob) ||
        read_u32(checksum + 4) != read_u32(blob + 44) ||
        read_u64(checksum + 16) != read_u64(blob + 16)) {
      return LibraryError::InvalidChecksumRecord;
    }
  }

  output = candidate;
  return LibraryError::None;
}

bool LogicalLibrary::find_track(const contracts::TrackId id, TrackView& output) const {
  const std::uint32_t index_count = read_u32(index_.data);
  const auto* const index_record =
      find_index_record(index_.data, index_count, kTrackEntity, id.value);
  if (index_record == nullptr) {
    return false;
  }
  const auto* const record =
      tracks_.data + 8 + static_cast<std::size_t>(read_u32(index_record + 16)) * 96;
  output.track_id = id;
  output.format = read_u32(record + 8);
  output.flags = read_u32(record + 12);
  output.primary_blob_id = contracts::BlobId{read_u64(record + 16)};
  output.title = string_view(strings_.data, string_count_, read_u64(record + 24));
  output.artist = string_view(strings_.data, string_count_, read_u64(record + 32));
  const auto optional_string = [this, record](const std::size_t offset) -> std::optional<Utf8View> {
    const std::uint64_t string_id = read_u64(record + offset);
    if (string_id == 0) {
      return std::nullopt;
    }
    return string_view(strings_.data, string_count_, string_id);
  };
  output.album = optional_string(40);
  output.composer = optional_string(48);
  output.system = optional_string(56);
  const std::uint64_t duration = read_u64(record + 64);
  const std::uint64_t loop_start = read_u64(record + 72);
  output.duration_ticks = duration == std::numeric_limits<std::uint64_t>::max()
                              ? std::nullopt
                              : std::optional<std::uint64_t>{duration};
  output.loop_start_ticks = loop_start == std::numeric_limits<std::uint64_t>::max()
                                ? std::nullopt
                                : std::optional<std::uint64_t>{loop_start};
  output.dependency_count = read_u32(record + 84);
  const std::uint16_t year = read_u16(record + 88);
  output.year = year == 0 ? std::nullopt : std::optional<std::uint16_t>{year};
  output.estimate_confidence = record[90];
  return true;
}

bool LogicalLibrary::find_blob(const contracts::BlobId id, BlobView& output) const {
  const auto* const index_record =
      find_index_record(index_.data, read_u32(index_.data), kBlobEntity, id.value);
  if (index_record == nullptr) {
    return false;
  }
  const auto* const record =
      blobs_.data + 16 + static_cast<std::size_t>(read_u32(index_record + 16)) * 48;
  output.blob_id = id;
  output.kind = read_u32(record + 8);
  output.bytes = {blobs_.data + read_u64(record + 32),
                  static_cast<std::size_t>(read_u64(record + 24))};
  return true;
}

bool LogicalLibrary::dependency(const contracts::TrackId track_id, const std::uint32_t position,
                                DependencyView& output) const {
  const auto* const index_record =
      find_index_record(index_.data, read_u32(index_.data), kTrackEntity, track_id.value);
  if (index_record == nullptr) {
    return false;
  }
  const auto* const track =
      tracks_.data + 8 + static_cast<std::size_t>(read_u32(index_record + 16)) * 96;
  const std::uint32_t count = read_u32(track + 84);
  if (position >= count) {
    return false;
  }
  const auto* const record =
      dependencies_.data + 8 + static_cast<std::size_t>(read_u32(track + 80) + position) * 24;
  output.role = read_u32(record + 8);
  output.blob_id = contracts::BlobId{read_u64(record + 16)};
  return true;
}

bool valid_album_key(const Utf8View key) noexcept {
  if (key.data == nullptr || key.size == 0 || key.size > 4096)
    return false;
  const std::string_view text{key.data, key.size};
  if (text == ".")
    return true;
  if (text.front() == '/' || text.back() == '/' || text.find('\\') != std::string_view::npos ||
      text.find('\0') != std::string_view::npos)
    return false;
  if (text.size() >= 2 && text[1] == ':' &&
      ((text[0] >= 'a' && text[0] <= 'z') || (text[0] >= 'A' && text[0] <= 'Z')))
    return false;
  std::size_t start = 0;
  while (start < text.size()) {
    const auto end = text.find('/', start);
    const std::string_view component{
        text.data() + start, end == std::string_view::npos ? text.size() - start : end - start};
    if (component.empty() || component == "." || component == "..")
      return false;
    if (end == std::string_view::npos)
      break;
    start = end + 1;
  }
  return true;
}

CatalogResult AlbumCatalog::open(const ByteView file, AlbumCatalog& output) {
  if (file.size > kAlbumMaxFileBytes)
    return {CatalogError::Capacity};
  AlbumCatalog candidate;
  ValidationLimits limits;
  limits.max_sections = 16;
  limits.max_file_size = kAlbumMaxFileBytes;
  limits.max_records_per_section = 4096;
  limits.max_total_decoded_bytes = kAlbumMaxFileBytes;
  const auto result = LogicalLibrary::open(file, candidate.library_, limits);
  if (result != LibraryError::None)
    return {CatalogError::Library, result};
  const auto& logical = candidate.library_;
  if (logical.track_count_ == 0 || logical.track_count_ > kAlbumMaxTracks ||
      logical.blob_count_ == 0 || logical.blob_count_ > kAlbumMaxTracks ||
      logical.string_count_ > 4096)
    return {CatalogError::Capacity};
  std::uint64_t string_bytes = 0;
  for (std::uint32_t i = 0; i < logical.string_count_; ++i) {
    string_bytes += read_u32(logical.strings_.data + 16 + static_cast<std::size_t>(i) * 16 + 12);
    if (string_bytes > kAlbumMaxStringBytes)
      return {CatalogError::Capacity};
  }
  for (std::uint32_t i = 0; i < logical.blob_count_; ++i) {
    const auto* record = logical.blobs_.data + 16 + static_cast<std::size_t>(i) * 48;
    if (read_u64(record + 16) > kAlbumMaxBlobBytes)
      return {CatalogError::Capacity};
    if (read_u32(record + 8) != kMdxFourcc)
      return {CatalogError::InvalidReference};
  }
  for (std::uint32_t i = 0; i < logical.track_count_; ++i) {
    const auto* record = logical.tracks_.data + 8 + static_cast<std::size_t>(i) * 96;
    if (read_u32(record + 8) != kMdxFourcc || read_u32(record + 84) != 0)
      return {CatalogError::InvalidReference};
  }
  ByteView payload;
  const auto directory = read_u64(file.data + 40);
  const auto sections = read_u32(file.data + 48);
  for (std::uint32_t i = 0; i < sections; ++i) {
    const auto* entry = file.data + directory + static_cast<std::size_t>(i) * 40;
    if (read_u32(entry) == 0x4d424c41U) {
      if (read_u32(entry + 4) != 0 || read_u32(entry + 28) != 8)
        return {CatalogError::InvalidHeader};
      payload = {file.data + read_u64(entry + 8), static_cast<std::size_t>(read_u64(entry + 16))};
    }
  }
  if (payload.data == nullptr)
    return {CatalogError::MissingSection};
  if (payload.size < 32)
    return {CatalogError::InvalidHeader};
  if (read_u16(payload.data) != 1 || read_u16(payload.data + 2) != 0)
    return {CatalogError::UnsupportedVersion};
  if (read_u16(payload.data + 4) != 32 || read_u16(payload.data + 6) != 40 ||
      read_u16(payload.data + 8) != 16)
    return {CatalogError::InvalidHeader};
  const auto albums = read_u32(payload.data + 12);
  const auto members = read_u32(payload.data + 16);
  if (albums == 0 || albums > kAlbumMaxAlbums || members == 0 || members > kAlbumMaxTracks)
    return {CatalogError::Capacity};
  const auto member_offset = 32ULL + 40ULL * albums;
  if (read_u64(payload.data + 24) != member_offset ||
      payload.size != member_offset + 16ULL * members)
    return {CatalogError::InvalidHeader};
  if (members != logical.track_count_)
    return {CatalogError::InvalidMembership};
  candidate.album_count_ = albums;
  candidate.albums_ = payload.data + 32;
  candidate.members_ = payload.data + member_offset;
  std::array<bool, kAlbumMaxAlbums> ordinals{};
  std::array<std::string_view, kAlbumMaxAlbums> keys{};
  std::array<std::uint64_t, kAlbumMaxTracks> track_ids{};
  std::uint64_t previous_id = 0;
  std::uint32_t next_member = 0;
  for (std::uint32_t i = 0; i < albums; ++i) {
    const auto* record = candidate.albums_ + static_cast<std::size_t>(i) * 40;
    const auto id = read_u64(record);
    if (id <= previous_id)
      return {CatalogError::InvalidAlbum};
    previous_id = id;
    const auto name_id = read_u64(record + 8);
    const auto key_id = read_u64(record + 16);
    if (name_id == 0 || key_id == 0 ||
        find_string_record(logical.strings_.data, logical.string_count_, name_id) == nullptr ||
        find_string_record(logical.strings_.data, logical.string_count_, key_id) == nullptr)
      return {CatalogError::InvalidReference};
    const auto name = string_view(logical.strings_.data, logical.string_count_, name_id);
    const auto key = string_view(logical.strings_.data, logical.string_count_, key_id);
    if (name.size == 0 || !valid_album_key(key))
      return {CatalogError::InvalidAlbum};
    const std::string_view key_text{key.data, key.size};
    keys[i] = key_text;
    const auto slash = key_text.find_last_of('/');
    const auto component_start = slash == std::string_view::npos ? 0 : slash + 1;
    const std::string_view component{key_text.data() + component_start,
                                     key_text.size() - component_start};
    if (key_text != "." && component != std::string_view{name.data, name.size})
      return {CatalogError::InvalidAlbum};
    const auto display = read_u32(record + 24);
    if (display >= albums || ordinals[display])
      return {CatalogError::InvalidOrder};
    ordinals[display] = true;
    candidate.display_indices_[display] = static_cast<std::uint16_t>(i);
    const auto count = read_u32(record + 32);
    if (read_u32(record + 28) != next_member || count == 0 || count > members - next_member)
      return {CatalogError::InvalidMembership};
    for (std::uint32_t j = 0; j < count; ++j) {
      const auto* member = candidate.members_ + static_cast<std::size_t>(next_member + j) * 16;
      if (read_u32(member + 8) != j)
        return {CatalogError::InvalidOrder};
      const auto track_id = read_u64(member);
      const auto* index = find_index_record(logical.index_.data, read_u32(logical.index_.data),
                                            kTrackEntity, track_id);
      if (index == nullptr)
        return {CatalogError::InvalidReference};
      const auto* track =
          logical.tracks_.data + 8 + static_cast<std::size_t>(read_u32(index + 16)) * 96;
      if (read_u64(track + 40) != name_id)
        return {CatalogError::InvalidReference};
      track_ids[next_member + j] = track_id;
    }
    next_member += count;
  }
  if (next_member != members)
    return {CatalogError::InvalidMembership};
  std::sort(keys.begin(), keys.begin() + albums);
  if (std::adjacent_find(keys.begin(), keys.begin() + albums) != keys.begin() + albums)
    return {CatalogError::InvalidAlbum};
  std::sort(track_ids.begin(), track_ids.begin() + members);
  if (std::adjacent_find(track_ids.begin(), track_ids.begin() + members) !=
      track_ids.begin() + members)
    return {CatalogError::InvalidMembership};
  output = candidate;
  return {};
}

const std::uint8_t* AlbumCatalog::album_record(const contracts::AlbumId id) const {
  std::uint32_t first = 0;
  std::uint32_t last = album_count_;
  while (first < last) {
    const auto middle = first + (last - first) / 2;
    const auto* record = albums_ + static_cast<std::size_t>(middle) * 40;
    if (read_u64(record) < id.value)
      first = middle + 1;
    else
      last = middle;
  }
  if (first == album_count_)
    return nullptr;
  const auto* record = albums_ + static_cast<std::size_t>(first) * 40;
  return read_u64(record) == id.value ? record : nullptr;
}

void AlbumCatalog::album_view(const std::uint8_t* record, AlbumView& output) const {
  output = {{read_u64(record)},
            read_u32(record + 24),
            string_view(library_.strings_.data, library_.string_count_, read_u64(record + 8)),
            read_u32(record + 32)};
}

bool AlbumCatalog::album_at(const std::uint32_t display_ordinal, AlbumView& output) const {
  if (display_ordinal >= album_count_)
    return false;
  album_view(albums_ + static_cast<std::size_t>(display_indices_[display_ordinal]) * 40, output);
  return true;
}

bool AlbumCatalog::find_album(const contracts::AlbumId id, AlbumView& output) const {
  const auto* record = album_record(id);
  if (record == nullptr)
    return false;
  album_view(record, output);
  return true;
}

bool AlbumCatalog::track_at(const contracts::AlbumId album, const std::uint32_t ordinal,
                            TrackView& output) const {
  const auto* record = album_record(album);
  if (record == nullptr || ordinal >= read_u32(record + 32))
    return false;
  const auto member = read_u32(record + 28) + ordinal;
  return library_.find_track({read_u64(members_ + static_cast<std::size_t>(member) * 16)}, output);
}

} // namespace rpcmp::library
