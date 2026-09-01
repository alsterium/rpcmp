#ifndef RPCMP_RUNTIME_FIXED_YM2151_SEQUENCE_HPP
#define RPCMP_RUNTIME_FIXED_YM2151_SEQUENCE_HPP

#include "rpcmp/contracts/device_op.hpp"

#include <cstdint>

namespace rpcmp::runtime {

inline constexpr contracts::DeviceId kFixedYm2151DeviceId{1};
inline constexpr std::uint32_t kFixedYm2151TickRate = 1'000;
inline constexpr std::uint64_t kFixedYm2151EndTick = 3'500;

contracts::DeviceOpStream make_fixed_ym2151_sequence();

} // namespace rpcmp::runtime

#endif // RPCMP_RUNTIME_FIXED_YM2151_SEQUENCE_HPP
