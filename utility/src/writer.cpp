#include "rpcmp/utility/writer.hpp"

#include "rpcmp/library/album_catalog.hpp"
#include "rpcmp/library/container.hpp"
#include "rpcmp/library/formats.hpp"
#include "rpcmp/utility/stable_id.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rpcmp::utility {
namespace {

constexpr std::uint32_t kTrackTag = 0x4B415254U;
constexpr std::uint32_t kBlobTag = 0x424F4C42U;
constexpr std::uint32_t kStringTag = 0x53525453U;
constexpr std::uint32_t kDependencyTag = 0x53504544U;
constexpr std::uint32_t kIndexTag = 0x58444E49U;
constexpr std::uint32_t kChecksumTag = 0x4D555343U;

void append_u16(std::vector<std::uint8_t>& out, const std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value));
  out.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void append_u32(std::vector<std::uint8_t>& out, const std::uint32_t value) {
  for (std::uint32_t shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

void append_u64(std::vector<std::uint8_t>& out, const std::uint64_t value) {
  for (std::uint32_t shift = 0; shift < 64; shift += 8) {
    out.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

void write_u32(std::vector<std::uint8_t>& out, const std::size_t offset,
               const std::uint32_t value) {
  for (std::size_t index = 0; index < 4; ++index) {
    out[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
  }
}

void write_u64(std::vector<std::uint8_t>& out, const std::size_t offset,
               const std::uint64_t value) {
  for (std::size_t index = 0; index < 8; ++index) {
    out[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
  }
}

void align(std::vector<std::uint8_t>& out, const std::size_t alignment) {
  while ((out.size() & (alignment - 1U)) != 0) {
    out.push_back(0);
  }
}

bool valid_utf8(const std::string& value) {
  std::size_t index = 0;
  while (index < value.size()) {
    const auto first = static_cast<std::uint8_t>(value[index++]);
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
    if (remaining > value.size() - index) {
      return false;
    }
    for (std::size_t count = 0; count < remaining; ++count) {
      const auto byte = static_cast<std::uint8_t>(value[index++]);
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

library::ByteView view(const std::string& value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

struct BlobRecord {
  contracts::BlobId id;
  std::uint32_t kind{};
  std::vector<std::uint8_t> bytes;
};

struct StringRecord {
  contracts::StringId id;
  std::string value;
};

struct TrackRecord {
  contracts::TrackId id;
  const NormalizedTrack* source{};
  contracts::BlobId primary;
  std::array<contracts::StringId, 5> strings{};
  std::vector<DependencyIdentity> dependencies;
};

struct AlbumRecord {
  contracts::AlbumId id;
  const NormalizedAlbum* source{};
  contracts::StringId name;
  contracts::StringId key;
  std::uint32_t ordinal{};
};

bool same_track_identity(const TrackRecord& left, const TrackRecord& right) {
  return left.source->format == right.source->format && left.primary == right.primary &&
         left.strings == right.strings && left.dependencies.size() == right.dependencies.size() &&
         std::equal(left.dependencies.begin(), left.dependencies.end(), right.dependencies.begin(),
                    [](const DependencyIdentity& a, const DependencyIdentity& b) {
                      return a.role == b.role && a.blob_id == b.blob_id;
                    });
}

template <typename Id, typename MakeId>
Id unique_id(const std::set<std::uint64_t>& occupied, MakeId make_id) {
  std::uint32_t counter = 0;
  while (true) {
    const Id id = make_id(counter);
    if (id.value != 0 && occupied.count(id.value) == 0) {
      return id;
    }
    if (counter == std::numeric_limits<std::uint32_t>::max())
      return {};
    ++counter;
  }
}

WriterResult write_library(const NormalizedLibrary& input,
                           const std::vector<NormalizedAlbum>* catalog) {
  WriterResult result;
  constexpr std::size_t kMaxRecords = 1'000'000;
  constexpr std::size_t kMaxStringBytes = std::size_t{4} * 1024;
  constexpr std::size_t kMaxDependencies = 64;
  if (input.blobs.size() > kMaxRecords || input.tracks.size() > kMaxRecords) {
    result.error = WriterError::SizeLimitExceeded;
    return result;
  }

  if (catalog != nullptr) {
    if (input.blobs.empty() || input.blobs.size() > library::kAlbumMaxTracks ||
        input.tracks.empty() || input.tracks.size() > library::kAlbumMaxTracks ||
        catalog->empty() || catalog->size() > library::kAlbumMaxAlbums) {
      result.error = WriterError::SizeLimitExceeded;
      return result;
    }
    std::array<bool, library::kAlbumMaxTracks> owned{};
    std::set<std::string_view> keys;
    for (const auto& album : *catalog) {
      if (album.folder_key.size() > kMaxStringBytes || album.name.size() > kMaxStringBytes ||
          album.track_indices.size() > library::kAlbumMaxTracks) {
        result.error = WriterError::SizeLimitExceeded;
        return result;
      }
      const std::string_view key{album.folder_key};
      const auto slash = key.find_last_of('/');
      const auto component = slash == std::string_view::npos ? key : key.substr(slash + 1);
      if (!library::valid_album_key({key.data(), key.size()}) || album.name.empty() ||
          (key != "." && component != album.name) || !keys.insert(key).second ||
          album.track_indices.empty()) {
        result.error = WriterError::InvalidInput;
        return result;
      }
      for (const auto index : album.track_indices) {
        if (index >= input.tracks.size() || owned[index] || !input.tracks[index].album ||
            *input.tracks[index].album != album.name) {
          result.error = WriterError::InvalidInput;
          return result;
        }
        owned[index] = true;
      }
    }
    const auto owned_end = owned.begin() + static_cast<std::ptrdiff_t>(input.tracks.size());
    if (std::find(owned.begin(), owned_end, false) != owned_end) {
      result.error = WriterError::InvalidInput;
      return result;
    }
  }

  std::vector<BlobRecord> blobs;
  std::vector<contracts::BlobId> input_blob_ids(input.blobs.size());
  std::set<std::uint64_t> blob_ids;
  std::uint64_t decoded_bytes = 0;
  for (std::size_t index = 0; index < input.blobs.size(); ++index) {
    const auto& source = input.blobs[index];
    if (catalog != nullptr && source.bytes.size() > library::kAlbumMaxBlobBytes) {
      result.error = WriterError::SizeLimitExceeded;
      return result;
    }
    if (catalog != nullptr && source.kind != library::kMdxFourcc) {
      result.error = WriterError::InvalidInput;
      return result;
    }
    auto duplicate = std::find_if(blobs.begin(), blobs.end(), [&source](const BlobRecord& blob) {
      return blob.kind == source.kind && blob.bytes == source.bytes;
    });
    if (duplicate != blobs.end()) {
      input_blob_ids[index] = duplicate->id;
      continue;
    }
    if (catalog != nullptr) {
      if (source.bytes.size() > library::kAlbumMaxFileBytes - decoded_bytes) {
        result.error = WriterError::SizeLimitExceeded;
        return result;
      }
      decoded_bytes += source.bytes.size();
    }
    const library::ByteView bytes{source.bytes.data(), source.bytes.size()};
    const auto id = unique_id<contracts::BlobId>(blob_ids, [&](const std::uint32_t counter) {
      return stable_blob_id(source.kind, bytes, counter);
    });
    if (id.value == 0) {
      result.error = WriterError::IdentifierExhausted;
      return result;
    }
    blob_ids.insert(id.value);
    input_blob_ids[index] = id;
    blobs.push_back({id, source.kind, source.bytes});
  }
  std::sort(blobs.begin(), blobs.end(), [](const BlobRecord& left, const BlobRecord& right) {
    return left.id.value < right.id.value;
  });

  std::vector<StringRecord> strings;
  std::set<std::uint64_t> string_ids;
  std::map<std::string, contracts::StringId> string_lookup;
  std::uint64_t string_bytes = 0;
  const auto intern = [&](const std::string& value, contracts::StringId& output) -> WriterError {
    if (value.size() > kMaxStringBytes) {
      return WriterError::SizeLimitExceeded;
    }
    if (value.find('\0') != std::string::npos) {
      return WriterError::EmbeddedNul;
    }
    if (!valid_utf8(value)) {
      return WriterError::InvalidUtf8;
    }
    const auto existing = string_lookup.find(value);
    if (existing != string_lookup.end()) {
      output = existing->second;
      return WriterError::None;
    }
    if (catalog != nullptr &&
        (strings.size() >= 4096 || value.size() > library::kAlbumMaxStringBytes - string_bytes))
      return WriterError::SizeLimitExceeded;
    output = unique_id<contracts::StringId>(string_ids, [&](const std::uint32_t counter) {
      return stable_string_id(view(value), counter);
    });
    if (output.value == 0)
      return WriterError::IdentifierExhausted;
    if (catalog != nullptr)
      string_bytes += value.size();
    string_ids.insert(output.value);
    string_lookup.emplace(value, output);
    strings.push_back({output, value});
    return WriterError::None;
  };

  std::vector<TrackRecord> tracks;
  std::vector<contracts::TrackId> input_track_ids;
  std::set<std::uint64_t> track_ids;
  for (const auto& source : input.tracks) {
    if (catalog != nullptr &&
        (source.format != library::kMdxFourcc || !source.dependencies.empty())) {
      result.error = WriterError::InvalidInput;
      return result;
    }
    if (source.primary_blob_index >= input_blob_ids.size() || source.estimate_confidence > 2 ||
        source.dependencies.size() > kMaxDependencies) {
      result.error = WriterError::InvalidInput;
      return result;
    }
    TrackRecord record;
    record.source = &source;
    record.primary = input_blob_ids[source.primary_blob_index];
    const std::array<const std::optional<std::string>*, 3> optional{&source.album, &source.composer,
                                                                    &source.system};
    result.error = intern(source.title, record.strings[0]);
    if (result.error != WriterError::None) {
      return result;
    }
    result.error = intern(source.artist, record.strings[1]);
    if (result.error != WriterError::None) {
      return result;
    }
    for (std::size_t index = 0; index < optional.size(); ++index) {
      if ((*optional[index]).has_value()) {
        result.error = intern(**optional[index], record.strings[index + 2]);
        if (result.error != WriterError::None) {
          return result;
        }
      }
    }
    for (const auto& dependency : source.dependencies) {
      if (dependency.blob_index >= input_blob_ids.size()) {
        result.error = WriterError::InvalidInput;
        return result;
      }
      record.dependencies.push_back({dependency.role, input_blob_ids[dependency.blob_index]});
    }
    const TrackIdentity identity{source.format,
                                 record.primary,
                                 record.dependencies.data(),
                                 record.dependencies.size(),
                                 record.strings[0],
                                 record.strings[1],
                                 record.strings[2],
                                 record.strings[3],
                                 record.strings[4]};
    if (catalog != nullptr &&
        std::any_of(tracks.begin(), tracks.end(), [&](const TrackRecord& previous) {
          return same_track_identity(previous, record);
        })) {
      result.error = WriterError::DuplicateTrack;
      return result;
    }
    record.id = unique_id<contracts::TrackId>(
        track_ids, [&](const std::uint32_t counter) { return stable_track_id(identity, counter); });
    if (record.id.value == 0) {
      result.error = WriterError::IdentifierExhausted;
      return result;
    }
    if (!track_ids.insert(record.id.value).second) {
      result.error = WriterError::DuplicateTrack;
      return result;
    }
    if (catalog != nullptr)
      input_track_ids.push_back(record.id);
    tracks.push_back(std::move(record));
  }
  std::vector<AlbumRecord> albums;
  if (catalog != nullptr) {
    std::vector<std::size_t> key_order;
    key_order.reserve(catalog->size());
    for (std::size_t i = 0; i < catalog->size(); ++i)
      key_order.push_back(i);
    std::sort(key_order.begin(), key_order.end(), [&](const std::size_t a, const std::size_t b) {
      return (*catalog)[a].folder_key < (*catalog)[b].folder_key;
    });
    std::set<std::uint64_t> album_ids;
    for (const auto i : key_order) {
      const auto& source = (*catalog)[i];
      AlbumRecord album;
      album.source = &source;
      album.ordinal = static_cast<std::uint32_t>(i);
      result.error = intern(source.name, album.name);
      if (result.error != WriterError::None)
        return result;
      result.error = intern(source.folder_key, album.key);
      if (result.error != WriterError::None)
        return result;
      album.id = unique_id<contracts::AlbumId>(album_ids, [&](const std::uint32_t counter) {
        return stable_album_id(view(source.folder_key), counter);
      });
      if (album.id.value == 0) {
        result.error = WriterError::IdentifierExhausted;
        return result;
      }
      album_ids.insert(album.id.value);
      albums.push_back(album);
    }
    std::sort(albums.begin(), albums.end(),
              [](const AlbumRecord& a, const AlbumRecord& b) { return a.id.value < b.id.value; });
  }
  std::sort(strings.begin(), strings.end(),
            [](const StringRecord& left, const StringRecord& right) {
              return left.id.value < right.id.value;
            });
  std::sort(tracks.begin(), tracks.end(), [](const TrackRecord& left, const TrackRecord& right) {
    return left.id.value < right.id.value;
  });

  if (catalog != nullptr) {
    const auto aligned_size = [](const std::uint64_t size) { return (size + 7) & ~7ULL; };
    std::uint64_t blob_payload_bytes = 0;
    for (const auto& blob : blobs)
      blob_payload_bytes += aligned_size(blob.bytes.size());
    // All counts/data sizes have already passed the small profile limits.
    // Include padding and directory entries before allocating serialization.
    const auto file_bytes = 80 + aligned_size(8 + 96 * tracks.size()) + 16 + 48 * blobs.size() +
                            blob_payload_bytes +
                            aligned_size(16 + 16 * strings.size() + string_bytes) + 8 +
                            aligned_size(8 + 24 * (tracks.size() + blobs.size())) +
                            aligned_size(8 + 24 * blobs.size()) +
                            aligned_size(32 + 40 * albums.size() + 16 * tracks.size()) + 7ULL * 40;
    if (file_bytes > library::kAlbumMaxFileBytes) {
      result.error = WriterError::SizeLimitExceeded;
      return result;
    }
  }

  std::vector<std::uint8_t> track_section;
  append_u32(track_section, static_cast<std::uint32_t>(tracks.size()));
  append_u32(track_section, 96);
  std::uint32_t dependency_ordinal = 0;
  for (const auto& track : tracks) {
    append_u64(track_section, track.id.value);
    append_u32(track_section, track.source->format);
    append_u32(track_section, track.source->flags);
    append_u64(track_section, track.primary.value);
    for (const auto id : track.strings)
      append_u64(track_section, id.value);
    append_u64(track_section,
               track.source->duration_ticks.value_or(std::numeric_limits<std::uint64_t>::max()));
    append_u64(track_section,
               track.source->loop_start_ticks.value_or(std::numeric_limits<std::uint64_t>::max()));
    append_u32(track_section, dependency_ordinal);
    append_u32(track_section, static_cast<std::uint32_t>(track.dependencies.size()));
    append_u16(track_section, track.source->year.value_or(0));
    track_section.push_back(track.source->estimate_confidence);
    track_section.insert(track_section.end(), 5, 0);
    dependency_ordinal += static_cast<std::uint32_t>(track.dependencies.size());
  }

  std::vector<std::uint8_t> blob_section;
  append_u32(blob_section, static_cast<std::uint32_t>(blobs.size()));
  append_u32(blob_section, 48);
  append_u64(blob_section, 0);
  blob_section.resize(16 + blobs.size() * 48, 0);
  align(blob_section, 8);
  write_u64(blob_section, 8, blob_section.size());
  for (std::size_t index = 0; index < blobs.size(); ++index) {
    align(blob_section, 8);
    const std::size_t record = 16 + index * 48;
    const std::size_t payload = blob_section.size();
    write_u64(blob_section, record, blobs[index].id.value);
    write_u32(blob_section, record + 8, blobs[index].kind);
    write_u64(blob_section, record + 16, blobs[index].bytes.size());
    write_u64(blob_section, record + 24, blobs[index].bytes.size());
    write_u64(blob_section, record + 32, payload);
    write_u32(blob_section, record + 40, 8);
    write_u32(blob_section, record + 44,
              library::crc32({blobs[index].bytes.data(), blobs[index].bytes.size()}));
    blob_section.insert(blob_section.end(), blobs[index].bytes.begin(), blobs[index].bytes.end());
  }

  std::vector<std::uint8_t> string_section;
  append_u32(string_section, static_cast<std::uint32_t>(strings.size()));
  append_u32(string_section, 16);
  append_u64(string_section, 16 + strings.size() * 16);
  string_section.resize(16 + strings.size() * 16, 0);
  std::uint32_t string_offset = 0;
  for (std::size_t index = 0; index < strings.size(); ++index) {
    const std::size_t record = 16 + index * 16;
    write_u64(string_section, record, strings[index].id.value);
    write_u32(string_section, record + 8, string_offset);
    write_u32(string_section, record + 12, static_cast<std::uint32_t>(strings[index].value.size()));
    string_section.insert(string_section.end(), strings[index].value.begin(),
                          strings[index].value.end());
    string_offset += static_cast<std::uint32_t>(strings[index].value.size());
  }

  std::vector<std::uint8_t> dependency_section;
  append_u32(dependency_section, dependency_ordinal);
  append_u32(dependency_section, 24);
  for (const auto& track : tracks) {
    for (const auto& dependency : track.dependencies) {
      append_u64(dependency_section, track.id.value);
      append_u32(dependency_section, dependency.role);
      append_u32(dependency_section, 0);
      append_u64(dependency_section, dependency.blob_id.value);
    }
  }

  std::vector<std::uint8_t> index_section;
  append_u32(index_section, static_cast<std::uint32_t>(tracks.size() + blobs.size()));
  append_u32(index_section, 24);
  for (std::size_t ordinal = 0; ordinal < tracks.size(); ++ordinal) {
    append_u32(index_section, 1);
    append_u32(index_section, 0);
    append_u64(index_section, tracks[ordinal].id.value);
    append_u32(index_section, static_cast<std::uint32_t>(ordinal));
    append_u32(index_section, 0);
  }
  for (std::size_t ordinal = 0; ordinal < blobs.size(); ++ordinal) {
    append_u32(index_section, 2);
    append_u32(index_section, 0);
    append_u64(index_section, blobs[ordinal].id.value);
    append_u32(index_section, static_cast<std::uint32_t>(ordinal));
    append_u32(index_section, 0);
  }

  std::vector<std::uint8_t> checksum_section;
  append_u32(checksum_section, static_cast<std::uint32_t>(blobs.size()));
  append_u32(checksum_section, 24);
  for (const auto& blob : blobs) {
    append_u32(checksum_section, 2);
    append_u32(checksum_section, library::crc32({blob.bytes.data(), blob.bytes.size()}));
    append_u64(checksum_section, blob.id.value);
    append_u64(checksum_section, blob.bytes.size());
  }

  std::vector<std::uint8_t> album_section;
  if (catalog != nullptr) {
    for (const auto value : std::array<std::uint16_t, 6>{1, 0, 32, 40, 16, 0})
      append_u16(album_section, value);
    append_u32(album_section, static_cast<std::uint32_t>(albums.size()));
    append_u32(album_section, static_cast<std::uint32_t>(tracks.size()));
    append_u32(album_section, 0);
    append_u64(album_section, 32 + 40 * albums.size());
    std::uint32_t member = 0;
    for (const auto& album : albums) {
      append_u64(album_section, album.id.value);
      append_u64(album_section, album.name.value);
      append_u64(album_section, album.key.value);
      append_u32(album_section, album.ordinal);
      append_u32(album_section, member);
      append_u32(album_section, static_cast<std::uint32_t>(album.source->track_indices.size()));
      append_u32(album_section, 0);
      member += static_cast<std::uint32_t>(album.source->track_indices.size());
    }
    for (const auto& album : albums) {
      std::uint32_t ordinal = 0;
      for (const auto index : album.source->track_indices) {
        append_u64(album_section, input_track_ids[index].value);
        append_u32(album_section, ordinal++);
        append_u32(album_section, 0);
      }
    }
  }

  std::array<std::pair<std::uint32_t, std::vector<std::uint8_t>*>, 7> sections{
      {{kTrackTag, &track_section},
       {kBlobTag, &blob_section},
       {kStringTag, &string_section},
       {kDependencyTag, &dependency_section},
       {kIndexTag, &index_section},
       {kChecksumTag, &checksum_section},
       {0x4d424c41U, &album_section}}};
  const std::size_t section_count = catalog == nullptr ? 6 : 7;
  std::vector<std::uint8_t> build_material;
  for (std::size_t i = 0; i < 6; ++i)
    build_material.insert(build_material.end(), sections[i].second->begin(),
                          sections[i].second->end());
  const auto build_digest = sha256({build_material.data(), build_material.size()});

  result.bytes.resize(80, 0);
  std::array<std::uint64_t, 7> offsets{};
  for (std::size_t index = 0; index < section_count; ++index) {
    align(result.bytes, 8);
    offsets[index] = result.bytes.size();
    result.bytes.insert(result.bytes.end(), sections[index].second->begin(),
                        sections[index].second->end());
  }
  align(result.bytes, 8);
  const std::uint64_t directory_offset = result.bytes.size();
  for (std::size_t index = 0; index < section_count; ++index) {
    append_u32(result.bytes, sections[index].first);
    append_u32(result.bytes, index < 6 ? 1 : 0);
    append_u64(result.bytes, offsets[index]);
    append_u64(result.bytes, sections[index].second->size());
    append_u32(result.bytes,
               library::crc32({sections[index].second->data(), sections[index].second->size()}));
    append_u32(result.bytes, 8);
    append_u64(result.bytes, 0);
  }
  const std::array<std::uint8_t, 8> magic{'R', 'P', 'C', 'M', 'L', 'I', 'B', 0};
  std::copy(magic.begin(), magic.end(), result.bytes.begin());
  result.bytes[8] = 1;
  write_u32(result.bytes, 12, 80);
  write_u64(result.bytes, 32, result.bytes.size());
  write_u64(result.bytes, 40, directory_offset);
  write_u32(result.bytes, 48, static_cast<std::uint32_t>(section_count));
  write_u32(result.bytes, 52, 40);
  std::copy_n(build_digest.begin(), 16, result.bytes.begin() + 56);
  write_u32(result.bytes, 72, library::crc32({result.bytes.data(), 80}));
  return result;
}

} // namespace

WriterResult write_rpcmlib(const NormalizedLibrary& input) { return write_library(input, nullptr); }

WriterResult write_album_rpcmlib(const NormalizedLibrary& input,
                                 const std::vector<NormalizedAlbum>& albums) {
  return write_library(input, &albums);
}

} // namespace rpcmp::utility
