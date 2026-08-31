#include "rpcmp/library/container.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using rpcmp::library::ByteView;
using rpcmp::library::LibraryError;

constexpr std::array<std::array<std::uint8_t, 4>, 6> kTags{{
    {{'T', 'R', 'A', 'K'}},
    {{'B', 'L', 'O', 'B'}},
    {{'S', 'T', 'R', 'S'}},
    {{'D', 'E', 'P', 'S'}},
    {{'I', 'N', 'D', 'X'}},
    {{'C', 'S', 'U', 'M'}},
}};

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

ByteView view(const std::vector<std::uint8_t>& bytes) { return {bytes.data(), bytes.size()}; }

void update_header_crc(std::vector<std::uint8_t>& bytes) {
  write_u32(bytes, 72, 0);
  write_u32(bytes, 72, rpcmp::library::crc32({bytes.data(), 80}));
}

std::vector<std::uint8_t> minimal_file() {
  constexpr std::size_t directory_offset = 80;
  std::vector<std::uint8_t> bytes(directory_offset + kTags.size() * 40, 0);
  const std::array<std::uint8_t, 8> magic{'R', 'P', 'C', 'M', 'L', 'I', 'B', 0};
  for (std::size_t index = 0; index < magic.size(); ++index) {
    bytes[index] = magic[index];
  }
  write_u16(bytes, 8, 1);
  write_u16(bytes, 10, 0);
  write_u32(bytes, 12, 80);
  write_u64(bytes, 32, bytes.size());
  write_u64(bytes, 40, directory_offset);
  write_u32(bytes, 48, static_cast<std::uint32_t>(kTags.size()));
  write_u32(bytes, 52, 40);
  for (std::size_t index = 0; index < 16; ++index) {
    bytes[56 + index] = static_cast<std::uint8_t>(index);
  }
  for (std::size_t index = 0; index < kTags.size(); ++index) {
    const std::size_t entry = directory_offset + index * 40;
    for (std::size_t byte = 0; byte < 4; ++byte) {
      bytes[entry + byte] = kTags[index][byte];
    }
    write_u32(bytes, entry + 4, 1);
    write_u64(bytes, entry + 8, bytes.size());
    write_u64(bytes, entry + 16, 0);
    write_u32(bytes, entry + 24, 0);
    write_u32(bytes, entry + 28, 8);
  }
  update_header_crc(bytes);
  return bytes;
}

std::vector<std::uint8_t> file_with_payload() {
  auto bytes = minimal_file();
  constexpr std::size_t old_directory_offset = 80;
  constexpr std::size_t new_directory_offset = 88;
  bytes.resize(bytes.size() + (new_directory_offset - old_directory_offset));
  std::copy_backward(bytes.begin() + old_directory_offset,
                     bytes.begin() + old_directory_offset + kTags.size() * 40, bytes.end());
  write_u64(bytes, 32, bytes.size());
  write_u64(bytes, 40, new_directory_offset);
  write_u64(bytes, new_directory_offset + 8, old_directory_offset);
  write_u64(bytes, new_directory_offset + 16, 4);
  write_u32(bytes, new_directory_offset + 24,
            rpcmp::library::crc32({bytes.data() + old_directory_offset, 4}));
  update_header_crc(bytes);
  return bytes;
}

LibraryError validate(const std::vector<std::uint8_t>& bytes) {
  return rpcmp::library::validate_container_envelope(view(bytes)).error;
}

} // namespace

int main() {
  rpcmp::test::Suite suite;

  auto bytes = minimal_file();
  const auto valid = rpcmp::library::validate_container_envelope(view(bytes));
  RPCMP_CHECK(suite, valid.ok());
  RPCMP_CHECK(suite, valid.info.section_count == 6);
  RPCMP_CHECK(suite, valid.info.build_id[15] == 15);
  RPCMP_CHECK(suite, rpcmp::library::crc32({nullptr, 0}) == 0);

  auto payload_file = file_with_payload();
  RPCMP_CHECK(suite, validate(payload_file) == LibraryError::None);

  auto corrupted = bytes;
  corrupted.resize(79);
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::FileTooSmall);

  corrupted = bytes;
  corrupted[0] = 'X';
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::BadMagic);

  corrupted = bytes;
  write_u16(corrupted, 8, 2);
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::UnsupportedMajorVersion);

  corrupted = bytes;
  write_u64(corrupted, 24, 1);
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::UnsupportedRequiredFeatures);

  corrupted = bytes;
  corrupted[56] ^= 1U;
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::BadHeaderChecksum);

  corrupted = bytes;
  write_u32(corrupted, 48, 65);
  update_header_crc(corrupted);
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::SectionLimitExceeded);

  corrupted = bytes;
  write_u32(corrupted, 80 + 28, 3);
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::InvalidAlignment);

  corrupted = payload_file;
  write_u64(corrupted, 88 + 8, 81);
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::MisalignedSection);

  corrupted = payload_file;
  write_u64(corrupted, 88 + 40 + 8, 80);
  write_u64(corrupted, 88 + 40 + 16, 4);
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::SectionOverlap);

  corrupted = payload_file;
  write_u64(corrupted, 88 + 8, corrupted.size());
  write_u64(corrupted, 88 + 16, 1);
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::RangeOutsideFile);

  corrupted = bytes;
  for (std::size_t index = 0; index < 4; ++index) {
    corrupted[120 + index] = corrupted[80 + index];
  }
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::DuplicateSection);

  corrupted = bytes;
  corrupted[80] = 'X';
  write_u32(corrupted, 84, 0);
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::MissingRequiredSection);

  corrupted = bytes;
  corrupted[80] = 'X';
  write_u32(corrupted, 84, 1);
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::UnknownRequiredSection);

  corrupted = bytes;
  write_u32(corrupted, 80 + 24, 1);
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::BadSectionChecksum);

  corrupted = payload_file;
  corrupted[80] ^= 1U;
  RPCMP_CHECK(suite, validate(corrupted) == LibraryError::BadSectionChecksum);

  return suite.finish("library container envelope");
}
