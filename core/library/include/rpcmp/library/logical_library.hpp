#ifndef RPCMP_LIBRARY_LOGICAL_LIBRARY_HPP
#define RPCMP_LIBRARY_LOGICAL_LIBRARY_HPP

#include "rpcmp/contracts/types.hpp"
#include "rpcmp/library/container.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace rpcmp::library {

struct Utf8View {
  const char* data{};
  std::size_t size{};
};

struct BlobView {
  contracts::BlobId blob_id{};
  std::uint32_t kind{};
  ByteView bytes{};
};

struct DependencyView {
  std::uint32_t role{};
  contracts::BlobId blob_id{};
};

struct TrackView {
  contracts::TrackId track_id{};
  std::uint32_t format{};
  std::uint32_t flags{};
  contracts::BlobId primary_blob_id{};
  Utf8View title{};
  Utf8View artist{};
  std::optional<Utf8View> album;
  std::optional<Utf8View> composer;
  std::optional<Utf8View> system;
  std::optional<std::uint64_t> duration_ticks;
  std::optional<std::uint64_t> loop_start_ticks;
  std::uint32_t dependency_count{};
  std::optional<std::uint16_t> year;
  std::uint8_t estimate_confidence{};
};

// The source bytes must outlive this immutable view.
class LogicalLibrary {
public:
  [[nodiscard]] static LibraryError open(ByteView file, LogicalLibrary& output,
                                         ValidationLimits limits = ValidationLimits{});

  [[nodiscard]] std::uint32_t track_count() const { return track_count_; }
  [[nodiscard]] std::uint32_t blob_count() const { return blob_count_; }
  [[nodiscard]] bool find_track(contracts::TrackId id, TrackView& output) const;
  [[nodiscard]] bool find_blob(contracts::BlobId id, BlobView& output) const;
  [[nodiscard]] bool dependency(contracts::TrackId track_id, std::uint32_t position,
                                DependencyView& output) const;

private:
  struct Section {
    const std::uint8_t* data{};
    std::size_t size{};
  };

  ByteView file_{};
  Section tracks_{};
  Section blobs_{};
  Section strings_{};
  Section dependencies_{};
  Section index_{};
  Section checksums_{};
  std::uint32_t track_count_{};
  std::uint32_t blob_count_{};
  std::uint32_t string_count_{};
  std::uint32_t dependency_count_{};
};

} // namespace rpcmp::library

#endif // RPCMP_LIBRARY_LOGICAL_LIBRARY_HPP
