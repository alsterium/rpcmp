#include "rpcmp/spike/mdx_hardware_probe.hpp"

#include <cstdio>

int main() {
  std::array<rpcmp::spike::MdxHardwareWrite, rpcmp::spike::kMdxHardwareProbeExpectedWrites> trace{};
  const auto result = rpcmp::spike::run_mdx_hardware_probe(trace.data(), trace.size());
  if (!result.passed()) {
    return 1;
  }
  std::printf("# RPCMP self-authored MDX FM probe trace v1\n");
  std::printf("# tick_rate=48000 end_tick=%llu writes=%lu digest=%016llx\n",
              static_cast<unsigned long long>(result.end_tick),
              static_cast<unsigned long>(result.writes),
              static_cast<unsigned long long>(result.write_digest));
  std::printf("at_tick,address,value\n");
  for (const auto& write : trace) {
    std::printf("%llu,0x%02x,0x%02x\n", static_cast<unsigned long long>(write.at_tick),
                static_cast<unsigned>(write.address), static_cast<unsigned>(write.value));
  }
  return 0;
}
