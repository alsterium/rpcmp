#ifndef RPCMP_SPIKE_M5_LIBRARY_LOADER_HPP
#define RPCMP_SPIKE_M5_LIBRARY_LOADER_HPP

#include "rpcmp/library/logical_library.hpp"

#include <cstddef>
#include <cstdint>

namespace rpcmp::spike {

inline constexpr std::uint32_t kM5LibraryMaximumBytes = 2U * 1024U * 1024U;
inline constexpr std::uint32_t kM5LibraryReadChunk = 4096;

class IM5LibrarySlot {
public:
  virtual ~IM5LibrarySlot() = default;
  virtual bool size(std::uint32_t& bytes) noexcept = 0;
  // Success means exactly length bytes. Failure must not be retried by callers.
  virtual bool read(std::uint32_t offset, std::uint8_t* destination,
                    std::uint32_t length) noexcept = 0;
};

enum class M5LoadError : std::uint8_t { None, Size, Capacity, Read, Library, Profile };

struct M5LoadResult {
  M5LoadError error{M5LoadError::None};
  library::LibraryError library_error{library::LibraryError::None};
  library::ByteView bytes{};
  library::LogicalLibrary library{};
  [[nodiscard]] bool ok() const noexcept { return error == M5LoadError::None; }
};

// Storage belongs to the caller and must remain immutable/alive after success.
// Failed transfers/validation never publish a partial byte or logical view.
[[nodiscard]] M5LoadResult load_m5_library(IM5LibrarySlot& slot, std::uint8_t* storage,
                                           std::size_t capacity) noexcept;

} // namespace rpcmp::spike

#endif
