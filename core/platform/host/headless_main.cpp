#include "rpcmp/runtime/mock_core.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <utility>

namespace {

using namespace rpcmp;

contracts::PlayerCommand command(const std::uint64_t id, contracts::Command payload) {
  return {contracts::kSchemaVersion, id, std::nullopt, std::move(payload)};
}

class SnapshotCounter final : public runtime::ISnapshotObserver {
public:
  void published(const contracts::PlayerSnapshot&) override { ++count; }
  std::size_t count{};
};

} // namespace

int main() {
  SnapshotCounter snapshots;
  rpcmp::runtime::MockCore core(nullptr, &snapshots);
  core.submit(command(1, rpcmp::contracts::OpenLibrary{"m0:library"}));
  core.submit(command(2, rpcmp::contracts::LoadTrack{rpcmp::contracts::TrackId{1}}));
  core.submit(command(3, rpcmp::contracts::Play{}));
  if (core.advance_to(120'000) != rpcmp::runtime::AdvanceResult::Ok) {
    return 1;
  }
  const auto snapshot = core.latest();
  std::cout << "snapshots=" << snapshots.count << " sequence=" << snapshot.sequence
            << " position=" << snapshot.position.position_ticks << '\n';
  return snapshots.count >= 120 && snapshot.channels.size() == 8U ? 0 : 2;
}
