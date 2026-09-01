#include "rpcmp/runtime/fixed_ym2151_sequence.hpp"

namespace rpcmp::runtime {
namespace {

contracts::DeviceOp reset_at(const std::uint64_t at_tick) {
  return contracts::DeviceOp{at_tick, kFixedYm2151DeviceId, contracts::ResetDevice{}};
}

contracts::DeviceOp write_at(const std::uint64_t at_tick, const std::uint8_t address,
                             const std::uint8_t value) {
  return contracts::DeviceOp{at_tick, kFixedYm2151DeviceId,
                             contracts::WriteRegister{address, value}};
}

} // namespace

contracts::DeviceOpStream make_fixed_ym2151_sequence() {
  contracts::DeviceOpStream stream;
  stream.tick_rate = kFixedYm2151TickRate;
  stream.operations = {
      reset_at(0),
      write_at(0, 0x20, 0xC7),
      write_at(0, 0x28, 0x3C),
      write_at(0, 0x30, 0x00),
      write_at(0, 0x40, 0x01),
      write_at(0, 0x60, 0x00),
      write_at(0, 0x80, 0x1F),
      write_at(0, 0xA0, 0x00),
      write_at(0, 0xC0, 0x00),
      write_at(0, 0xE0, 0x0F),
      write_at(0, 0x68, 0x7F),
      write_at(0, 0x70, 0x7F),
      write_at(0, 0x78, 0x7F),
      write_at(1'000, 0x08, 0x08),
      write_at(2'000, 0x28, 0x40),
      write_at(3'000, 0x08, 0x00),
      reset_at(kFixedYm2151EndTick),
  };
  return stream;
}

} // namespace rpcmp::runtime
