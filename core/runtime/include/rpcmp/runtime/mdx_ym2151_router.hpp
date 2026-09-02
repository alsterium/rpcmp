#ifndef RPCMP_RUNTIME_MDX_YM2151_ROUTER_HPP
#define RPCMP_RUNTIME_MDX_YM2151_ROUTER_HPP

#include "rpcmp/runtime/mdx_document_machine.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rpcmp::runtime::mdx {

inline constexpr std::size_t kMdxMaxYm2151WritesPerBatch = 8'192;

struct Ym2151Write {
  std::uint8_t address{};
  std::uint8_t value{};
  std::uint8_t logical_channel{};
};

struct Ym2151ChannelState {
  std::uint16_t selected_voice{256};
  std::uint16_t applied_voice{256};
  std::int16_t detune{};
  std::int32_t base_pitch{};
  std::int32_t portamento_accumulator{};
  std::int32_t portamento_delta{};
  std::uint16_t last_pitch{0xffff};
  std::uint16_t gate_ticks{};
  std::uint8_t pan{3};
  std::uint8_t volume{8};
  std::uint8_t gate_parameter{8};
  std::uint8_t key_on_delay{};
  std::uint8_t delay_ticks{};
  bool portamento_active{};
  bool suppress_key_off{};
  bool pending_note{};
  bool key_on{};
};

struct Ym2151RouterState {
  std::array<Ym2151ChannelState, kMdxFmTrackCount> channels{};
};

struct Ym2151WriteBatch {
  std::array<Ym2151Write, kMdxMaxYm2151WritesPerBatch> writes{};
  std::size_t count{};
};

struct Ym2151RouterScratch {
  Ym2151RouterState candidate_state{};
  Ym2151WriteBatch pending_batch{};
};

// Converts one ordered semantic tick to the MXDRV-derived FM write order,
// including gate, delayed key-on, tie, and per-tick portamento lifecycle.
[[nodiscard]] DecodeResult route_ym2151_batch(const MdxDocument& document,
                                              const DocumentTickBatch& actions,
                                              Ym2151RouterState& state, Ym2151WriteBatch& writes,
                                              Ym2151RouterScratch& scratch) noexcept;

} // namespace rpcmp::runtime::mdx

#endif // RPCMP_RUNTIME_MDX_YM2151_ROUTER_HPP
