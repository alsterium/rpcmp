#ifndef RPCMP_UTILITY_ALBUM_NATIVE_HPP
#define RPCMP_UTILITY_ALBUM_NATIVE_HPP

#include "rpcmp/utility/album_ingest.hpp"

#include <filesystem>

namespace rpcmp::utility {

struct AlbumScanResult {
  AlbumIngestError error{AlbumIngestError::None};
  std::string path;
  [[nodiscard]] bool ok() const noexcept { return error == AlbumIngestError::None; }
};
class NativeAlbumInput final : public AlbumReadPort {
public:
  [[nodiscard]] AlbumScanResult scan(const std::filesystem::path& root);
  [[nodiscard]] const AlbumSourceManifest& manifest() const noexcept { return manifest_; }
  [[nodiscard]] bool read_file(std::size_t entry, std::uint8_t* bytes, std::size_t size) override;
  // Existing MDX aliases/hard links count as conflicts. Errors fail closed.
  [[nodiscard]] bool output_conflicts(const std::filesystem::path& destination) const;

private:
  std::filesystem::path root_;
  AlbumSourceManifest manifest_;
  std::vector<std::filesystem::path> paths_;
};

class NativeAlbumOutput final : public AlbumOutputPort {
public:
  explicit NativeAlbumOutput(std::filesystem::path destination);
  NativeAlbumOutput(const NativeAlbumOutput&) = delete;
  NativeAlbumOutput& operator=(const NativeAlbumOutput&) = delete;
  ~NativeAlbumOutput() override;
  [[nodiscard]] bool begin() override;
  [[nodiscard]] bool write(library::ByteView bytes) override;
  [[nodiscard]] bool finish() override;
  [[nodiscard]] bool publish() override;
  [[nodiscard]] bool discard() override;

private:
  std::filesystem::path destination_;
  std::filesystem::path temporary_;
#ifdef _WIN32
  void* handle_{};
#else
  int handle_{-1};
#endif
  bool owned_{};
};

} // namespace rpcmp::utility

#endif // RPCMP_UTILITY_ALBUM_NATIVE_HPP
