#include "rpcmp/library/formats.hpp"
#include "rpcmp/spike/m5_library_loader.hpp"
#include "rpcmp/utility/writer.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

class Slot final : public rpcmp::spike::IM5LibrarySlot {
public:
  std::vector<std::uint8_t> bytes;
  std::uint32_t reported_size{};
  std::uint32_t calls{};
  std::uint32_t next_offset{};
  std::uint32_t last_length{};
  std::uint32_t fail_call{};
  bool size_ok{true};
  bool bounded{true};

  bool size(std::uint32_t& value) noexcept override {
    value = reported_size;
    return size_ok;
  }
  bool read(const std::uint32_t offset, std::uint8_t* const destination,
            const std::uint32_t length) noexcept override {
    ++calls;
    bounded = bounded && offset == next_offset && length != 0 &&
              length <= rpcmp::spike::kM5LibraryReadChunk && offset <= reported_size &&
              length <= reported_size - offset;
    if (!bounded || calls == fail_call || offset > bytes.size() || length > bytes.size() - offset)
      return false;
    std::copy_n(bytes.data() + offset, length, destination);
    next_offset += length;
    last_length = length;
    return true;
  }
};

Slot fixture(const std::size_t blob_bytes, const bool two_tracks = false) {
  rpcmp::utility::NormalizedLibrary source;
  source.blobs.push_back({rpcmp::library::kMdxFourcc, std::vector<std::uint8_t>(blob_bytes, 42)});
  rpcmp::utility::NormalizedTrack track;
  track.format = rpcmp::library::kMdxFourcc;
  track.title = "Authored loader test";
  source.tracks.push_back(track);
  if (two_tracks) {
    track.title = "Second authored test";
    source.tracks.push_back(track);
  }
  Slot slot;
  slot.bytes = rpcmp::utility::write_rpcmlib(source).bytes;
  slot.reported_size = static_cast<std::uint32_t>(slot.bytes.size());
  return slot;
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  using rpcmp::spike::load_m5_library;
  using rpcmp::spike::M5LoadError;
  std::vector<std::uint8_t> storage(rpcmp::spike::kM5LibraryMaximumBytes);
  auto slot = fixture(9000);
  const auto result = load_m5_library(slot, storage.data(), storage.size());
  RPCMP_CHECK(suite, result.ok());
  RPCMP_CHECK(suite, result.library.track_count() == 1);
  RPCMP_CHECK(suite, result.bytes.size == slot.bytes.size());
  RPCMP_CHECK(suite, std::equal(slot.bytes.begin(), slot.bytes.end(), storage.begin()));
  RPCMP_CHECK(suite, slot.bounded && slot.calls == 3 && slot.last_length < 4096);

  for (const auto size : {0U, 79U, rpcmp::spike::kM5LibraryMaximumBytes + 1U}) {
    slot = fixture(8);
    slot.reported_size = size;
    const auto rejected = load_m5_library(slot, storage.data(), storage.size());
    RPCMP_CHECK(suite, rejected.error == M5LoadError::Size);
    RPCMP_CHECK(suite, rejected.bytes.data == nullptr && rejected.library.track_count() == 0);
    RPCMP_CHECK(suite, slot.calls == 0);
  }
  slot = fixture(8);
  slot.size_ok = false;
  RPCMP_CHECK(suite,
              load_m5_library(slot, storage.data(), storage.size()).error == M5LoadError::Size);
  RPCMP_CHECK(suite, slot.calls == 0);
  slot.size_ok = true;
  RPCMP_CHECK(suite, load_m5_library(slot, nullptr, storage.size()).error == M5LoadError::Capacity);
  RPCMP_CHECK(suite, load_m5_library(slot, storage.data(), 80).error == M5LoadError::Capacity);
  RPCMP_CHECK(suite, slot.calls == 0);

  slot = fixture(9000);
  slot.fail_call = 2;
  auto rejected = load_m5_library(slot, storage.data(), storage.size());
  RPCMP_CHECK(suite, rejected.error == M5LoadError::Read && slot.calls == 2);
  RPCMP_CHECK(suite, rejected.bytes.data == nullptr && rejected.library.track_count() == 0);
  slot = fixture(9000);
  slot.bytes.pop_back();
  RPCMP_CHECK(suite,
              load_m5_library(slot, storage.data(), storage.size()).error == M5LoadError::Read);
  slot = fixture(8);
  slot.bytes[0] ^= 1U;
  rejected = load_m5_library(slot, storage.data(), storage.size());
  RPCMP_CHECK(suite, rejected.error == M5LoadError::Library && rejected.bytes.data == nullptr);
  slot = fixture(8, true);
  RPCMP_CHECK(suite,
              load_m5_library(slot, storage.data(), storage.size()).error == M5LoadError::Profile);

  // Exact transport ceiling is read in full, then rejected as an invalid envelope.
  slot = fixture(8);
  slot.bytes.resize(storage.size());
  slot.reported_size = static_cast<std::uint32_t>(storage.size());
  rejected = load_m5_library(slot, storage.data(), storage.size());
  RPCMP_CHECK(suite, rejected.error == M5LoadError::Library);
  RPCMP_CHECK(suite, slot.bounded && slot.calls == 512 && slot.last_length == 4096);
  RPCMP_CHECK(suite, rejected.bytes.data == nullptr && rejected.library.track_count() == 0);
  slot = fixture(1024U * 1024U + 1U);
  rejected = load_m5_library(slot, storage.data(), storage.size());
  RPCMP_CHECK(suite, rejected.error == M5LoadError::Library);
  RPCMP_CHECK(suite,
              rejected.library_error == rpcmp::library::LibraryError::DecodedSizeLimitExceeded);
  return suite.finish("m5_library_loader");
}
