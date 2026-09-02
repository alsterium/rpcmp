#include "rpcmp/spike/mdx_hardware_probe.hpp"
#include "test_support.hpp"

#include <array>
#include <fstream>
#include <string>

int main() {
  rpcmp::test::Suite suite;
  std::array<rpcmp::spike::MdxHardwareWrite, rpcmp::spike::kMdxHardwareProbeExpectedWrites>
      actual{};
  const auto result = rpcmp::spike::run_mdx_hardware_probe(actual.data(), actual.size());
  RPCMP_CHECK(suite, result.passed());

  std::ifstream input(RPCMP_SOURCE_DIR "/specs/fixtures/mdx-fm-probe-trace-v1.csv");
  RPCMP_CHECK(suite, input.good());
  std::string line;
  std::size_t index{};
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#' || line == "at_tick,address,value") {
      continue;
    }
    const auto first_comma = line.find(',');
    const auto second_comma = line.find(',', first_comma + 1);
    RPCMP_CHECK(suite, first_comma != std::string::npos && second_comma != std::string::npos);
    const auto tick = std::stoull(line.substr(0, first_comma), nullptr, 0);
    const auto address =
        std::stoul(line.substr(first_comma + 1, second_comma - first_comma - 1), nullptr, 0);
    const auto value = std::stoul(line.substr(second_comma + 1), nullptr, 0);
    RPCMP_CHECK(suite, index < actual.size());
    if (index < actual.size()) {
      RPCMP_CHECK(suite, actual[index].at_tick == tick);
      RPCMP_CHECK(suite, actual[index].address == address);
      RPCMP_CHECK(suite, actual[index].value == value);
      ++index;
    }
  }
  RPCMP_CHECK(suite, index == actual.size());
  return suite.finish("MDX hardware trace fixture");
}
