#ifndef RPCMP_LIBRARY_CONTAINER_HPP
#define RPCMP_LIBRARY_CONTAINER_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace rpcmp::library {

constexpr std::size_t kHeaderSize = 80;
constexpr std::size_t kDirectoryEntrySize = 40;

struct ByteView {
  const std::uint8_t* data{};
  std::size_t size{};
};

enum class LibraryError : std::uint8_t {
  None = 0,
  FileTooSmall,
  BadMagic,
  UnsupportedMajorVersion,
  InvalidHeaderSize,
  UnsupportedRequiredFeatures,
  FileSizeMismatch,
  BadHeaderChecksum,
  SectionLimitExceeded,
  InvalidDirectoryEntrySize,
  ArithmeticOverflow,
  RangeOutsideFile,
  InvalidAlignment,
  MisalignedSection,
  SectionOverlap,
  DuplicateSection,
  UnknownRequiredSection,
  MissingRequiredSection,
  BadSectionChecksum,
  InvalidSectionHeader,
  RecordLimitExceeded,
  InvalidRecordOrder,
  InvalidUtf8,
  StringTooLong,
  EmbeddedNul,
  UnsupportedCodec,
  InvalidReference,
  InvalidIndex,
  InvalidChecksumRecord,
  DependencyLimitExceeded,
  DecodedSizeLimitExceeded,
};

struct ValidationLimits {
  std::uint32_t max_sections{64};
  std::uint64_t max_file_size{4ULL * 1024ULL * 1024ULL * 1024ULL};
  std::uint32_t max_records_per_section{1'000'000};
  std::uint32_t max_string_bytes{4 * 1024};
  std::uint32_t max_dependencies_per_track{64};
  std::uint64_t max_total_decoded_bytes{8ULL * 1024ULL * 1024ULL * 1024ULL};
};

struct EnvelopeInfo {
  std::uint16_t format_minor{};
  std::uint64_t optional_features{};
  std::array<std::uint8_t, 16> build_id{};
  std::uint32_t section_count{};
};

struct ValidationResult {
  LibraryError error{LibraryError::None};
  EnvelopeInfo info{};

  [[nodiscard]] bool ok() const { return error == LibraryError::None; }
};

[[nodiscard]] std::uint32_t crc32(ByteView bytes);
[[nodiscard]] ValidationResult
validate_container_envelope(ByteView file, ValidationLimits limits = ValidationLimits{});

} // namespace rpcmp::library

#endif // RPCMP_LIBRARY_CONTAINER_HPP
