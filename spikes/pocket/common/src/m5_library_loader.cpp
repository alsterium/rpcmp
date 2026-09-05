#include "rpcmp/spike/m5_library_loader.hpp"

#include <algorithm>

namespace rpcmp::spike {

M5LoadResult load_m5_library(IM5LibrarySlot& slot, std::uint8_t* const storage,
                             const std::size_t capacity) noexcept {
  M5LoadResult result;
  std::uint32_t size{};
  if (!slot.size(size) || size < library::kHeaderSize || size > kM5LibraryMaximumBytes) {
    result.error = M5LoadError::Size;
    return result;
  }
  if (storage == nullptr || size > capacity) {
    result.error = M5LoadError::Capacity;
    return result;
  }
  std::uint32_t offset{};
  while (offset < size) {
    const auto length = std::min(kM5LibraryReadChunk, size - offset);
    if (!slot.read(offset, storage + offset, length)) {
      result.error = M5LoadError::Read;
      return result;
    }
    offset += length;
  }
  library::ValidationLimits limits;
  limits.max_sections = 16;
  limits.max_file_size = kM5LibraryMaximumBytes;
  limits.max_records_per_section = 64;
  limits.max_string_bytes = 4096;
  limits.max_dependencies_per_track = 4;
  limits.max_total_decoded_bytes = 1024ULL * 1024ULL;
  library::LogicalLibrary candidate;
  result.library_error = library::LogicalLibrary::open({storage, size}, candidate, limits);
  if (result.library_error != library::LibraryError::None) {
    result.error = M5LoadError::Library;
    return result;
  }
  if (candidate.track_count() != 1 || candidate.blob_count() != 1) {
    result.error = M5LoadError::Profile;
    return result;
  }
  result.bytes = {storage, size};
  result.library = candidate;
  return result;
}

} // namespace rpcmp::spike
