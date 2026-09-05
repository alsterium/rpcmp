#include "rpcmp/library/formats.hpp"
#include "rpcmp/spike/m5_library_loader.hpp"
#include "rpcmp/spike/m5_playback.hpp"
#include "rpcmp/utility/stable_id.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> read_file(const char* path, const std::size_t maximum) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  const auto size = input.tellg();
  if (!input || size <= 0 || size > static_cast<std::streamoff>(maximum))
    return {};
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  input.seekg(0, std::ios::beg);
  if (!input.read(reinterpret_cast<char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size())))
    return {};
  return bytes;
}

class Slot final : public rpcmp::spike::IM5LibrarySlot {
public:
  explicit Slot(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
  bool size(std::uint32_t& value) noexcept override {
    value = static_cast<std::uint32_t>(bytes_.size());
    return true;
  }
  bool read(const std::uint32_t offset, std::uint8_t* destination,
            const std::uint32_t length) noexcept override {
    if (offset > bytes_.size() || length > bytes_.size() - offset)
      return false;
    std::copy_n(bytes_.data() + offset, length, destination);
    return true;
  }

private:
  const std::vector<std::uint8_t>& bytes_;
};

class Device final : public rpcmp::spike::IM5PlaybackDevice {
public:
  std::uint64_t time{};
  std::uint64_t due{};
  std::uint64_t keyons{};
  unsigned pan{};
  bool now(std::uint64_t& value) noexcept override {
    value = time;
    return true;
  }
  bool status(bool& empty) noexcept override {
    empty = time >= due;
    return true;
  }
  rpcmp::spike::M5Submit submit(const std::uint64_t tick,
                                const rpcmp::runtime::mdx::Ym2151Write& write) noexcept override {
    if (tick < due)
      return rpcmp::spike::M5Submit::Fault;
    due = tick;
    if (write.address == 8 && (write.value & 0x78U) != 0)
      ++keyons;
    if (write.address >= 0x20 && write.address <= 0x27)
      pan |= write.value & 0xc0U;
    return rpcmp::spike::M5Submit::Accepted;
  }
  bool reset() noexcept override { return true; }
};

rpcmp::library::ByteView text_bytes(const std::string& text) {
  return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

} // namespace

int main(const int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "usage: rpcmp_m5_file_preflight <source.mdx> <music.rpcmlib> <title>\n";
    return 1;
  }
  const auto source = read_file(argv[1], rpcmp::runtime::mdx::kMdxMaxInputBytes);
  const auto container = read_file(argv[2], rpcmp::spike::kM5LibraryMaximumBytes);
  if (source.empty() || container.empty())
    return 2;
  const auto blob_id =
      rpcmp::utility::stable_blob_id(rpcmp::library::kMdxFourcc, {source.data(), source.size()});
  const std::string title{argv[3]};
  const std::string artist;
  const rpcmp::utility::TrackIdentity identity{rpcmp::library::kMdxFourcc,
                                               blob_id,
                                               nullptr,
                                               0,
                                               rpcmp::utility::stable_string_id(text_bytes(title)),
                                               rpcmp::utility::stable_string_id(text_bytes(artist)),
                                               {},
                                               {},
                                               {}};
  const auto id = rpcmp::utility::stable_track_id(identity);
  Slot slot{container};
  std::vector<std::uint8_t> storage(container.size());
  const auto loaded = rpcmp::spike::load_m5_library(slot, storage.data(), storage.size());
  if (!loaded.ok())
    return 3;
  rpcmp::library::BlobView blob;
  if (!loaded.library.find_blob(blob_id, blob) || blob.bytes.size != source.size() ||
      !std::equal(source.begin(), source.end(), blob.bytes.data))
    return 4;
  static rpcmp::player::MdxLibrarySession session;
  static rpcmp::player::MdxLibrarySessionWorkspace workspace;
  if (!rpcmp::player::prepare_mdx_library_session(loaded.library, id, session, workspace).ok())
    return 5;
  Device device;
  auto pump = std::make_unique<rpcmp::spike::M5PlaybackPump>(session, workspace, device);
  for (device.time = 0;
       device.time < 60ULL * 48'000ULL && pump->state() == rpcmp::spike::M5PlaybackState::Running;
       device.time += 48)
    static_cast<void>(pump->service());
  if (pump->state() == rpcmp::spike::M5PlaybackState::Fault ||
      pump->state() == rpcmp::spike::M5PlaybackState::ResetFailed) {
    std::cerr << "playback_error=" << static_cast<unsigned>(pump->error()) << '\n';
    return 6;
  }
  static rpcmp::runtime::mdx::MdxDocument direct_document;
  if (!rpcmp::runtime::mdx::parse({source.data(), source.size()}, direct_document).ok())
    return 7;
  static rpcmp::runtime::mdx::MdxEngineState direct_state;
  static rpcmp::runtime::mdx::TimedYm2151Batch batch;
  std::uint64_t writes{};
  std::uint64_t digest{};
  unsigned ticks{};
  while (direct_state.timeline.scheduler_tick < session.state.timeline.scheduler_tick) {
    if (++ticks > 1'000'000 || !rpcmp::runtime::mdx::advance_mdx_tick(
                                    direct_document, 48'000, direct_state, batch, workspace.engine)
                                    .ok())
      return 8;
    for (std::size_t index = 0; index < batch.count; ++index) {
      const auto& write = batch.writes[index];
      ++writes;
      digest = (digest * 1099511628211ULL) ^ write.at_tick;
      digest = (digest * 1099511628211ULL) ^ write.write.address;
      digest = (digest * 1099511628211ULL) ^ write.write.value;
    }
  }
  if (writes != pump->writes() || digest != pump->digest() || device.keyons == 0 ||
      device.pan != 0xc0)
    return 9;
  // This local output includes generated identity/trace evidence; do not commit it.
  std::cout << "track_id=" << std::hex << std::setw(16) << std::setfill('0') << id.value << '\n'
            << "digest=" << std::setw(16) << digest << std::dec << '\n'
            << "writes=" << writes << " keyons=" << device.keyons << " driver_ticks=" << ticks
            << '\n'
            << "result=PASS\n";
  return 0;
}
