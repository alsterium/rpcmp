#include "rpcmp/library/logical_library.hpp"
#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace {

using rpcmp::library::ByteView;
using rpcmp::library::LibraryError;

void write_u16(std::vector<std::uint8_t>& bytes, const std::size_t offset,
               const std::uint16_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void write_u32(std::vector<std::uint8_t>& bytes, const std::size_t offset,
               const std::uint32_t value) {
  for (std::size_t index = 0; index < 4; ++index) {
    bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
  }
}

void write_u64(std::vector<std::uint8_t>& bytes, const std::size_t offset,
               const std::uint64_t value) {
  for (std::size_t index = 0; index < 8; ++index) {
    bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
  }
}

std::size_t align_eight(const std::size_t value) { return (value + 7U) & ~std::size_t{7}; }

struct SectionFixture {
  std::array<std::uint8_t, 4> tag{};
  std::vector<std::uint8_t> payload;
  std::size_t file_offset{};
};

std::vector<std::uint8_t> strings_section() {
  std::vector<std::uint8_t> section(59, 0);
  write_u32(section, 0, 2);
  write_u32(section, 4, 16);
  write_u64(section, 8, 48);
  write_u64(section, 16, 10);
  write_u32(section, 24, 0);
  write_u32(section, 28, 5);
  write_u64(section, 32, 20);
  write_u32(section, 40, 5);
  write_u32(section, 44, 6);
  const std::string_view text{"TitleArtist"};
  for (std::size_t index = 0; index < text.size(); ++index) {
    section[48 + index] = static_cast<std::uint8_t>(text[index]);
  }
  return section;
}

std::vector<std::uint8_t> blob_section() {
  std::vector<std::uint8_t> section(67, 0);
  write_u32(section, 0, 1);
  write_u32(section, 4, 48);
  write_u64(section, 8, 64);
  write_u64(section, 16, 200);
  section[24] = 'M';
  section[25] = 'D';
  section[26] = 'X';
  section[27] = ' ';
  write_u16(section, 28, 0);
  write_u64(section, 32, 3);
  write_u64(section, 40, 3);
  write_u64(section, 48, 64);
  write_u32(section, 56, 8);
  section[64] = 1;
  section[65] = 2;
  section[66] = 3;
  write_u32(section, 60, rpcmp::library::crc32({section.data() + 64, 3}));
  return section;
}

std::vector<std::uint8_t> track_section() {
  std::vector<std::uint8_t> section(104, 0);
  write_u32(section, 0, 1);
  write_u32(section, 4, 96);
  write_u64(section, 8, 100);
  section[16] = 'M';
  section[17] = 'D';
  section[18] = 'X';
  section[19] = ' ';
  write_u64(section, 24, 200);
  write_u64(section, 32, 10);
  write_u64(section, 40, 20);
  write_u64(section, 72, std::numeric_limits<std::uint64_t>::max());
  write_u64(section, 80, std::numeric_limits<std::uint64_t>::max());
  write_u32(section, 88, 0);
  write_u32(section, 92, 0);
  write_u16(section, 96, 1987);
  section[98] = 1;
  return section;
}

std::vector<std::uint8_t> empty_dependencies_section() {
  std::vector<std::uint8_t> section(8, 0);
  write_u32(section, 4, 24);
  return section;
}

std::vector<std::uint8_t> index_section() {
  std::vector<std::uint8_t> section(56, 0);
  write_u32(section, 0, 2);
  write_u32(section, 4, 24);
  write_u32(section, 8, 1);
  write_u64(section, 16, 100);
  write_u32(section, 24, 0);
  write_u32(section, 32, 2);
  write_u64(section, 40, 200);
  write_u32(section, 48, 0);
  return section;
}

std::vector<std::uint8_t> checksum_section(const std::uint32_t blob_crc) {
  std::vector<std::uint8_t> section(32, 0);
  write_u32(section, 0, 1);
  write_u32(section, 4, 24);
  write_u32(section, 8, 2);
  write_u32(section, 12, blob_crc);
  write_u64(section, 16, 200);
  write_u64(section, 24, 3);
  return section;
}

std::vector<std::uint8_t> valid_file() {
  auto blob = blob_section();
  std::array<SectionFixture, 6> sections{{
      {{{'T', 'R', 'A', 'K'}}, track_section(), 0},
      {{{'B', 'L', 'O', 'B'}}, blob, 0},
      {{{'S', 'T', 'R', 'S'}}, strings_section(), 0},
      {{{'D', 'E', 'P', 'S'}}, empty_dependencies_section(), 0},
      {{{'I', 'N', 'D', 'X'}}, index_section(), 0},
      {{{'C', 'S', 'U', 'M'}}, checksum_section(rpcmp::library::crc32({blob.data() + 64, 3})), 0},
  }};

  std::size_t cursor = 80;
  for (auto& section : sections) {
    cursor = align_eight(cursor);
    section.file_offset = cursor;
    cursor += section.payload.size();
  }
  const std::size_t directory_offset = align_eight(cursor);
  std::vector<std::uint8_t> bytes(directory_offset + sections.size() * 40, 0);
  for (const auto& section : sections) {
    for (std::size_t index = 0; index < section.payload.size(); ++index) {
      bytes[section.file_offset + index] = section.payload[index];
    }
  }
  for (std::size_t index = 0; index < sections.size(); ++index) {
    const std::size_t entry = directory_offset + index * 40;
    for (std::size_t byte = 0; byte < 4; ++byte) {
      bytes[entry + byte] = sections[index].tag[byte];
    }
    write_u32(bytes, entry + 4, 1);
    write_u64(bytes, entry + 8, sections[index].file_offset);
    write_u64(bytes, entry + 16, sections[index].payload.size());
    write_u32(bytes, entry + 24,
              rpcmp::library::crc32(
                  {bytes.data() + sections[index].file_offset, sections[index].payload.size()}));
    write_u32(bytes, entry + 28, 8);
  }

  const std::array<std::uint8_t, 8> magic{'R', 'P', 'C', 'M', 'L', 'I', 'B', 0};
  for (std::size_t index = 0; index < magic.size(); ++index) {
    bytes[index] = magic[index];
  }
  write_u16(bytes, 8, 1);
  write_u32(bytes, 12, 80);
  write_u64(bytes, 32, bytes.size());
  write_u64(bytes, 40, directory_offset);
  write_u32(bytes, 48, static_cast<std::uint32_t>(sections.size()));
  write_u32(bytes, 52, 40);
  write_u32(bytes, 72, rpcmp::library::crc32({bytes.data(), 80}));
  return bytes;
}

std::size_t section_offset(const std::vector<std::uint8_t>& bytes, const char first) {
  const std::size_t directory =
      static_cast<std::size_t>(bytes[40] | (static_cast<std::uint64_t>(bytes[41]) << 8U) |
                               (static_cast<std::uint64_t>(bytes[42]) << 16U) |
                               (static_cast<std::uint64_t>(bytes[43]) << 24U));
  for (std::size_t index = 0; index < 6; ++index) {
    const std::size_t entry = directory + index * 40;
    if (bytes[entry] == static_cast<std::uint8_t>(first)) {
      return static_cast<std::size_t>(bytes[entry + 8] |
                                      (static_cast<std::uint64_t>(bytes[entry + 9]) << 8U) |
                                      (static_cast<std::uint64_t>(bytes[entry + 10]) << 16U) |
                                      (static_cast<std::uint64_t>(bytes[entry + 11]) << 24U));
    }
  }
  return 0;
}

void refresh_section_crc(std::vector<std::uint8_t>& bytes, const char first) {
  const std::size_t directory =
      static_cast<std::size_t>(bytes[40] | (static_cast<std::uint64_t>(bytes[41]) << 8U) |
                               (static_cast<std::uint64_t>(bytes[42]) << 16U) |
                               (static_cast<std::uint64_t>(bytes[43]) << 24U));
  for (std::size_t index = 0; index < 6; ++index) {
    const std::size_t entry = directory + index * 40;
    if (bytes[entry] == static_cast<std::uint8_t>(first)) {
      const std::size_t offset = section_offset(bytes, first);
      const std::size_t length = static_cast<std::size_t>(
          bytes[entry + 16] | (static_cast<std::uint64_t>(bytes[entry + 17]) << 8U) |
          (static_cast<std::uint64_t>(bytes[entry + 18]) << 16U) |
          (static_cast<std::uint64_t>(bytes[entry + 19]) << 24U));
      write_u32(bytes, entry + 24, rpcmp::library::crc32({bytes.data() + offset, length}));
      return;
    }
  }
}

LibraryError open(const std::vector<std::uint8_t>& bytes) {
  rpcmp::library::LogicalLibrary library;
  return rpcmp::library::LogicalLibrary::open({bytes.data(), bytes.size()}, library);
}

} // namespace

int main() {
  rpcmp::test::Suite suite;

  auto bytes = valid_file();
  rpcmp::library::LogicalLibrary library;
  RPCMP_CHECK(suite, rpcmp::library::LogicalLibrary::open({bytes.data(), bytes.size()}, library) ==
                         LibraryError::None);
  RPCMP_CHECK(suite, library.track_count() == 1);
  RPCMP_CHECK(suite, library.blob_count() == 1);

  rpcmp::library::TrackView track;
  RPCMP_CHECK(suite, library.find_track(rpcmp::contracts::TrackId{100}, track));
  RPCMP_CHECK(suite, std::string_view(track.title.data, track.title.size) == "Title");
  RPCMP_CHECK(suite, std::string_view(track.artist.data, track.artist.size) == "Artist");
  RPCMP_CHECK(suite, track.year == 1987);
  RPCMP_CHECK(suite, !track.duration_ticks.has_value());
  RPCMP_CHECK(suite, !library.find_track(rpcmp::contracts::TrackId{999}, track));

  rpcmp::library::BlobView blob;
  RPCMP_CHECK(suite, library.find_blob(rpcmp::contracts::BlobId{200}, blob));
  RPCMP_CHECK(suite, blob.bytes.size == 3);
  RPCMP_CHECK(suite, blob.bytes.data[0] == 1 && blob.bytes.data[2] == 3);
  rpcmp::library::DependencyView dependency;
  RPCMP_CHECK(suite, !library.dependency(rpcmp::contracts::TrackId{100}, 0, dependency));

  auto corrupted = bytes;
  const std::size_t strings = section_offset(corrupted, 'S');
  corrupted[strings + 48] = 0xFFU;
  refresh_section_crc(corrupted, 'S');
  RPCMP_CHECK(suite, open(corrupted) == LibraryError::InvalidUtf8);

  corrupted = bytes;
  const std::size_t tracks = section_offset(corrupted, 'T');
  write_u64(corrupted, tracks + 24, 999);
  refresh_section_crc(corrupted, 'T');
  RPCMP_CHECK(suite, open(corrupted) == LibraryError::InvalidReference);

  corrupted = bytes;
  const std::size_t index = section_offset(corrupted, 'I');
  write_u32(corrupted, index + 24, 1);
  refresh_section_crc(corrupted, 'I');
  RPCMP_CHECK(suite, open(corrupted) == LibraryError::InvalidIndex);

  corrupted = bytes;
  const std::size_t checksums = section_offset(corrupted, 'C');
  write_u32(corrupted, checksums + 12, 1);
  refresh_section_crc(corrupted, 'C');
  RPCMP_CHECK(suite, open(corrupted) == LibraryError::InvalidChecksumRecord);

  corrupted = bytes;
  const std::size_t blobs = section_offset(corrupted, 'B');
  write_u16(corrupted, blobs + 28, 1);
  refresh_section_crc(corrupted, 'B');
  RPCMP_CHECK(suite, open(corrupted) == LibraryError::UnsupportedCodec);

  return suite.finish("logical library");
}
