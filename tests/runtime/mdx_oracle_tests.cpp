#include "rpcmp/runtime/mdx_engine.hpp"
#include "test_support.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct ObservedWrite {
  int driver_tick{};
  std::uint8_t address{};
  std::uint8_t value{};
};

std::vector<std::uint8_t> read_hex(const std::string& path) {
  std::ifstream input(path);
  std::vector<std::uint8_t> bytes;
  int high = -1;
  char character = 0;
  while (input.get(character)) {
    int nibble = -1;
    if (character >= '0' && character <= '9') {
      nibble = character - '0';
    } else if (character >= 'a' && character <= 'f') {
      nibble = character - 'a' + 10;
    }
    if (nibble < 0) {
      continue;
    }
    if (high < 0) {
      high = nibble;
    } else {
      bytes.push_back(static_cast<std::uint8_t>((high << 4) | nibble));
      high = -1;
    }
  }
  return high < 0 ? bytes : std::vector<std::uint8_t>{};
}

std::vector<ObservedWrite> read_oracle_trace(const std::string& path) {
  std::ifstream input(path);
  std::vector<ObservedWrite> writes;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') {
      continue;
    }
    std::istringstream fields(line);
    char kind = 0;
    int tick = 0;
    int sequence = 0;
    std::string first;
    std::string second;
    fields >> kind >> tick >> sequence >> first;
    if (kind == 'T') {
      writes.push_back({tick, 0x12, static_cast<std::uint8_t>(std::stoul(first, nullptr, 16))});
    } else if (kind == 'W') {
      fields >> second;
      writes.push_back({tick, static_cast<std::uint8_t>(std::stoul(first, nullptr, 16)),
                        static_cast<std::uint8_t>(std::stoul(second, nullptr, 16))});
    }
  }
  return writes;
}

std::vector<ObservedWrite> select(const std::vector<ObservedWrite>& source,
                                  const std::array<std::uint8_t, 4>& addresses) {
  std::vector<ObservedWrite> selected;
  for (const auto& write : source) {
    for (const auto address : addresses) {
      if (write.address == address) {
        selected.push_back(write);
      }
    }
  }
  return selected;
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  const std::string fixture_path =
      std::string(RPCMP_SOURCE_DIR) + "/tests/fixtures/mdx/oracle-fm.mdx.hex";
  const std::string trace_path =
      std::string(RPCMP_SOURCE_DIR) + "/tests/fixtures/mdx/oracle-fm.mdxtools.trace";
  const auto bytes = read_hex(fixture_path);
  rpcmp::runtime::mdx::MdxDocument document{};
  RPCMP_CHECK(suite, rpcmp::runtime::mdx::parse({bytes.data(), bytes.size()}, document).ok());
  static rpcmp::runtime::mdx::MdxEngineScratch scratch{};
  rpcmp::runtime::mdx::DocumentValidation validation{};
  RPCMP_CHECK(suite, rpcmp::runtime::mdx::prepare_mdx_playback(document, validation, scratch).ok());

  rpcmp::runtime::mdx::MdxEngineState state{};
  rpcmp::runtime::mdx::TimedYm2151Batch batch{};
  std::vector<ObservedWrite> actual;
  std::array<std::uint8_t, 256> actual_registers{};
  std::array<bool, 256> actual_seen{};
  for (int tick = 0; tick < 4; ++tick) {
    RPCMP_CHECK(
        suite, rpcmp::runtime::mdx::advance_mdx_tick(document, 48'000, state, batch, scratch).ok());
    for (std::size_t index = 0; index < batch.count; ++index) {
      const auto& write = batch.writes[index].write;
      actual.push_back({tick, write.address, write.value});
      actual_registers[write.address] = write.value;
      actual_seen[write.address] = true;
    }
  }

  const auto oracle = read_oracle_trace(trace_path);
  std::array<std::uint8_t, 256> oracle_registers{};
  std::array<bool, 256> oracle_seen{};
  for (const auto& write : oracle) {
    oracle_registers[write.address] = write.value;
    oracle_seen[write.address] = true;
  }
  for (std::size_t address = 0; address < actual_seen.size(); ++address) {
    if (actual_seen[address]) {
      RPCMP_CHECK(suite, oracle_seen[address]);
      RPCMP_CHECK(suite, actual_registers[address] == oracle_registers[address]);
    }
  }

  const std::array<std::uint8_t, 4> service_order_registers{0x12, 0x1b, 0x20, 0x08};
  const auto actual_order = select(actual, service_order_registers);
  const auto oracle_order = select(oracle, service_order_registers);
  RPCMP_CHECK(suite, actual_order.size() == oracle_order.size());
  for (std::size_t index = 0; index < actual_order.size() && index < oracle_order.size(); ++index) {
    RPCMP_CHECK(suite, actual_order[index].driver_tick == oracle_order[index].driver_tick);
    RPCMP_CHECK(suite, actual_order[index].address == oracle_order[index].address);
    RPCMP_CHECK(suite, actual_order[index].value == oracle_order[index].value);
  }
  RPCMP_CHECK(suite, state.timeline.scheduler_tick == 2'752);

  return suite.finish("MDX independent mdxtools oracle");
}
