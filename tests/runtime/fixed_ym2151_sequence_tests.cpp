#include "rpcmp/runtime/fixed_ym2151_sequence.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace {

using rpcmp::contracts::DeviceOpStream;

void append_u16_le(std::vector<std::uint8_t>& bytes, const std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value));
  bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void append_u32_le(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
  for (std::uint32_t shift = 0; shift < 32; shift += 8) {
    bytes.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

void append_u64_le(std::vector<std::uint8_t>& bytes, const std::uint64_t value) {
  for (std::uint32_t shift = 0; shift < 64; shift += 8) {
    bytes.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

std::vector<std::uint8_t> encode_golden_fixture(const DeviceOpStream& stream) {
  std::vector<std::uint8_t> bytes{'R', 'D', 'O', '1'};
  append_u16_le(bytes, 1);
  append_u16_le(bytes, stream.version);
  append_u32_le(bytes, stream.tick_rate);
  append_u32_le(bytes, static_cast<std::uint32_t>(stream.operations.size()));

  for (const auto& operation : stream.operations) {
    append_u64_le(bytes, operation.at_tick);
    append_u16_le(bytes, operation.device_id.value);
    if (const auto* write = std::get_if<rpcmp::contracts::WriteRegister>(&operation.operation)) {
      bytes.push_back(1);
      bytes.push_back(write->address);
      bytes.push_back(write->value);
    } else {
      bytes.push_back(0);
      bytes.push_back(0);
      bytes.push_back(0);
    }
    bytes.push_back(0);
  }
  return bytes;
}

std::string to_hex(const std::vector<std::uint8_t>& bytes) {
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (const auto byte : bytes) {
    output << std::setw(2) << static_cast<unsigned int>(byte);
  }
  return output.str();
}

constexpr auto kExpectedGoldenHex =
    "52444f3101000100e803000011000000000000000000000001000000000000000000000000000100"
    "0120c7000000000000000000010001283c0000000000000000000100013000000000000000000000"
    "01000140010000000000000000000100016000000000000000000000010001801f00000000000000"
    "0000010001a000000000000000000000010001c000000000000000000000010001e00f0000000000"
    "00000000010001687f000000000000000000010001707f000000000000000000010001787f00e803"
    "000000000000010001080800d007000000000000010001284000b80b000000000000010001080000"
    "ac0d000000000000010000000000";

} // namespace

int main() {
  rpcmp::test::Suite suite;

  const auto first = rpcmp::runtime::make_fixed_ym2151_sequence();
  const auto second = rpcmp::runtime::make_fixed_ym2151_sequence();

  RPCMP_CHECK(suite, first == second);
  RPCMP_CHECK(suite, first.version == rpcmp::contracts::kDeviceOpStreamVersion);
  RPCMP_CHECK(suite, first.tick_rate == rpcmp::runtime::kFixedYm2151TickRate);
  RPCMP_CHECK(suite, first.operations.size() == 17);
  RPCMP_CHECK(suite, first.operations.front().at_tick == 0);
  RPCMP_CHECK(suite, std::holds_alternative<rpcmp::contracts::ResetDevice>(
                         first.operations.front().operation));
  RPCMP_CHECK(suite, first.operations.back().at_tick == rpcmp::runtime::kFixedYm2151EndTick);
  RPCMP_CHECK(suite, std::holds_alternative<rpcmp::contracts::ResetDevice>(
                         first.operations.back().operation));
  RPCMP_CHECK(suite, encode_golden_fixture(first).size() == 254);
  RPCMP_CHECK(suite, to_hex(encode_golden_fixture(first)) == kExpectedGoldenHex);

  return suite.finish("fixed_ym2151_sequence");
}
