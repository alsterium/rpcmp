#ifndef RPCMP_SPIKE_MDX_HARDWARE_PROBE_HPP
#define RPCMP_SPIKE_MDX_HARDWARE_PROBE_HPP

#include "rpcmp/runtime/mdx_decoder.hpp"
#include "rpcmp/runtime/mdx_parser.hpp"

#include <cstddef>
#include <cstdint>

namespace rpcmp::spike {

inline constexpr std::size_t kMdxHardwareProbeFixtureBytes = 93;
inline constexpr std::uint32_t kMdxHardwareProbeDriverTicks = 4;
inline constexpr std::uint32_t kMdxHardwareProbeExpectedWrites = 33;
inline constexpr std::uint64_t kMdxHardwareProbeExpectedEndTick = 2'752;
inline constexpr std::uint64_t kMdxHardwareProbeExpectedDigest = 3'633'037'323'379'651'599ULL;

struct MdxHardwareProbeResult {
  runtime::mdx::MdxError parse_error{runtime::mdx::MdxError::None};
  runtime::mdx::DecodeError prepare_error{runtime::mdx::DecodeError::None};
  runtime::mdx::DecodeError playback_error{runtime::mdx::DecodeError::None};
  std::uint32_t driver_ticks{};
  std::uint32_t writes{};
  std::uint64_t write_digest{};
  std::uint64_t end_tick{};
  bool scheduler_ok{};

  [[nodiscard]] bool passed() const noexcept;
};

// Runs a self-authored FM-only MDX through parse, preparation, sequencing,
// rational timing, fixed-length bridge, scheduler, and a deterministic port.
// Large working storage is static so the Pocket stack is not part of the gate.
[[nodiscard]] MdxHardwareProbeResult run_mdx_hardware_probe() noexcept;

} // namespace rpcmp::spike

#endif // RPCMP_SPIKE_MDX_HARDWARE_PROBE_HPP
