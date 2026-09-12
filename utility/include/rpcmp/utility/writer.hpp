#ifndef RPCMP_UTILITY_WRITER_HPP
#define RPCMP_UTILITY_WRITER_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rpcmp::utility {

struct NormalizedBlob {
  std::uint32_t kind{};
  std::vector<std::uint8_t> bytes;
};

struct NormalizedDependency {
  std::uint32_t role{};
  std::size_t blob_index{};
};

struct NormalizedTrack {
  std::uint32_t format{};
  std::uint32_t flags{};
  std::size_t primary_blob_index{};
  std::string title;
  std::string artist;
  std::optional<std::string> album;
  std::optional<std::string> composer;
  std::optional<std::string> system;
  std::optional<std::uint64_t> duration_ticks;
  std::optional<std::uint64_t> loop_start_ticks;
  std::vector<NormalizedDependency> dependencies;
  std::optional<std::uint16_t> year;
  std::uint8_t estimate_confidence{};
};

struct NormalizedLibrary {
  std::vector<NormalizedBlob> blobs;
  std::vector<NormalizedTrack> tracks;
};

struct NormalizedAlbum {
  std::string folder_key;
  std::string name;
  std::vector<std::size_t> track_indices;
};

enum class WriterError : std::uint8_t {
  None = 0,
  InvalidInput,
  InvalidUtf8,
  EmbeddedNul,
  DuplicateTrack,
  SizeLimitExceeded,
  IdentifierExhausted,
};

struct WriterResult {
  WriterError error{WriterError::None};
  std::vector<std::uint8_t> bytes;

  [[nodiscard]] bool ok() const { return error == WriterError::None; }
};

[[nodiscard]] WriterResult write_rpcmlib(const NormalizedLibrary& input);
[[nodiscard]] WriterResult write_album_rpcmlib(const NormalizedLibrary& input,
                                               const std::vector<NormalizedAlbum>& albums);

} // namespace rpcmp::utility

#endif // RPCMP_UTILITY_WRITER_HPP
