#include "rpcmp/contracts/device_op.hpp"

namespace rpcmp::contracts {

bool operator==(const DeviceOp& left, const DeviceOp& right) noexcept {
  return left.at_tick == right.at_tick && left.device_id == right.device_id &&
         left.operation == right.operation;
}

bool operator==(const DeviceOpStream& left, const DeviceOpStream& right) noexcept {
  return left.version == right.version && left.tick_rate == right.tick_rate &&
         left.operations == right.operations;
}

} // namespace rpcmp::contracts
