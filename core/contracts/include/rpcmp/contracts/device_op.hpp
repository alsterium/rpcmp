#ifndef RPCMP_CONTRACTS_DEVICE_OP_HPP
#define RPCMP_CONTRACTS_DEVICE_OP_HPP

#include "rpcmp/contracts/types.hpp"

#include <cstdint>
#include <variant>
#include <vector>

namespace rpcmp::contracts {

inline constexpr std::uint16_t kDeviceOpStreamVersion = 1;

struct ResetDevice {};

struct WriteRegister {
  std::uint8_t address{};
  std::uint8_t value{};
};

using DeviceOperation = std::variant<ResetDevice, WriteRegister>;

struct DeviceOp {
  std::uint64_t at_tick{};
  DeviceId device_id{};
  DeviceOperation operation;
};

struct DeviceOpStream {
  std::uint16_t version{kDeviceOpStreamVersion};
  std::uint32_t tick_rate{};
  std::vector<DeviceOp> operations;
};

constexpr bool operator==(ResetDevice, ResetDevice) noexcept { return true; }

constexpr bool operator==(WriteRegister left, WriteRegister right) noexcept {
  return left.address == right.address && left.value == right.value;
}

bool operator==(const DeviceOp& left, const DeviceOp& right) noexcept;
bool operator==(const DeviceOpStream& left, const DeviceOpStream& right) noexcept;

} // namespace rpcmp::contracts

#endif // RPCMP_CONTRACTS_DEVICE_OP_HPP
