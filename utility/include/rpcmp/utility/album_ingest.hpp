#ifndef RPCMP_UTILITY_ALBUM_INGEST_HPP
#define RPCMP_UTILITY_ALBUM_INGEST_HPP

#include "rpcmp/library/container.hpp"
#include "rpcmp/utility/mdx_ingest.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rpcmp::utility {

inline constexpr std::size_t kAlbumMaxSourceEntries = 4096;
inline constexpr std::size_t kAlbumMaxSourceText = std::size_t{1024} * 1024;
inline constexpr std::size_t kAlbumMaxSourceDepth = 64;

enum class AlbumEntryKind : std::uint8_t { File, Directory, Link, Other };
struct AlbumSourceEntry {
  std::string relative_path;
  std::uint64_t size{};
  AlbumEntryKind kind{AlbumEntryKind::File};
};
struct AlbumSourceManifest {
  std::string root_name;
  std::vector<AlbumSourceEntry> entries;
};
class AlbumReadPort {
public:
  virtual ~AlbumReadPort() = default;
  // Fill exactly size bytes from the unchanged regular file at this entry.
  [[nodiscard]] virtual bool read_file(std::size_t entry, std::uint8_t* bytes,
                                       std::size_t size) = 0;
};

enum class AlbumIngestStatus : std::uint8_t { Failed, Complete, WithExclusions };
enum class AlbumIngestError : std::uint8_t {
  None,
  InvalidSource,
  Capacity,
  NameCollision,
  Read,
  NoTracks,
  DuplicateTrack,
  Writer
};
enum class AlbumDiagnosticKind : std::uint8_t { Accepted, Excluded, NotMdx, Link, Other, Error };
struct AlbumDiagnostic {
  std::string path;
  AlbumDiagnosticKind kind{AlbumDiagnosticKind::Accepted};
  MdxIngestError stage{MdxIngestError::None};
  runtime::mdx::ParseResult parse{};
  runtime::mdx::DecodeResult admission{};
  TextError metadata{TextError::None};
  TitleFallback fallback{TitleFallback::None};
};
struct AlbumIngestResult {
  AlbumIngestStatus status{AlbumIngestStatus::Failed};
  AlbumIngestError error{AlbumIngestError::None};
  WriterError writer{WriterError::None};
  std::string error_path;
  std::string conflicting_path;
  std::uint32_t accepted{};
  std::uint32_t excluded{};
  std::uint32_t skipped{};
  std::vector<AlbumDiagnostic> diagnostics;
  std::vector<std::uint8_t> bytes;
  [[nodiscard]] bool ok() const noexcept { return status != AlbumIngestStatus::Failed; }
};

[[nodiscard]] AlbumIngestResult ingest_album_sources(const AlbumSourceManifest& source,
                                                     AlbumReadPort& reader,
                                                     MdxIngestWorkspace& workspace);

// A transaction owns only its exclusive temporary file until publish succeeds.
class AlbumOutputPort {
public:
  virtual ~AlbumOutputPort() = default;
  [[nodiscard]] virtual bool begin() = 0;
  [[nodiscard]] virtual bool write(library::ByteView bytes) = 0;
  [[nodiscard]] virtual bool finish() = 0; // flush and close
  [[nodiscard]] virtual bool publish() = 0;
  [[nodiscard]] virtual bool discard() = 0;
};
enum class AlbumOutputError : std::uint8_t { None, InvalidSize, Begin, Write, Finish, Publish };
struct AlbumOutputResult {
  AlbumOutputError error{AlbumOutputError::None};
  bool cleanup_failed{};
  [[nodiscard]] bool ok() const noexcept { return error == AlbumOutputError::None; }
};
[[nodiscard]] AlbumOutputResult publish_album(library::ByteView bytes, AlbumOutputPort& output);

} // namespace rpcmp::utility

#endif // RPCMP_UTILITY_ALBUM_INGEST_HPP
