#include "rpcmp/contracts/player_command.hpp"
#include "rpcmp/contracts/player_command_v2.hpp"
#include "test_support.hpp"

#include <type_traits>

namespace {
namespace api = rpcmp::contracts::v2;
static_assert(std::is_trivially_copyable_v<api::PlayerCommand>);
static_assert(std::is_trivially_copyable_v<api::CommandResult>);
static_assert(!std::is_same_v<api::PlayerCommand, rpcmp::contracts::PlayerCommand>);
static_assert(api::kCommandQueueCapacity == 32 && api::kCommandHistoryCapacity == 64);
class MockIngress final : public api::CommandIngress {
public:
  api::CommandResult submit(const api::PlayerCommand& command) override {
    recorded = command;
    return {2, command.command_id, api::CommandOutcome::Accepted, api::CommandReason::None,
            7, std::nullopt};
  }
  api::PlayerCommand recorded;
};
} // namespace

int main() {
  rpcmp::test::Suite suite;
  MockIngress mock;
  api::CommandIngress& ingress = mock;
  api::PlayerCommand command{2, 42, 7, api::CommandKind::PlayTrack,
                             api::TrackSelection{{12}, {99}}};
  const auto result = ingress.submit(command);
  command.selection.reset();
  RPCMP_CHECK(suite, result.schema_version == 2 && result.command_id == 42 &&
                         result.outcome == api::CommandOutcome::Accepted && !result.original);
  RPCMP_CHECK(suite, mock.recorded.selection && mock.recorded.selection->track_id.value == 99);
  return suite.finish("Player command schema 2 mock");
}
