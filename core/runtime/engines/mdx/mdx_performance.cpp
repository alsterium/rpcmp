#include "rpcmp/runtime/mdx_performance.hpp"

namespace rpcmp::runtime::mdx {

void Ym2151ObservationBuffer::begin_tick(const Ym2151ObservationState& state) noexcept {
  first_ = 0;
  batch_.count = 0;
  batch_.lost_before = 0;
  batch_.capture_lost = state.csm;
  for (const auto& channel : state.channels)
    batch_.capture_lost |= !channel.gate_known;
}

void Ym2151ObservationBuffer::append(const Ym2151ObservationState& state,
                                     const std::uint8_t channel, const Ym2151PerformanceKind kind,
                                     const std::uint16_t after_write) noexcept {
  if (!state.channels[channel].gate_known) {
    batch_.capture_lost = true;
    return;
  }
  const auto index = (first_ + batch_.count) % kMdxPerformanceCapacity;
  batch_.events[index] = {after_write, channel, kind, state.channels[channel]};
  if (batch_.count == kMdxPerformanceCapacity) {
    first_ = static_cast<std::uint16_t>((first_ + 1) % kMdxPerformanceCapacity);
    ++batch_.lost_before;
  } else {
    ++batch_.count;
  }
}

void Ym2151ObservationBuffer::pitch(Ym2151ObservationState& state, const std::uint8_t channel,
                                    const std::uint16_t after_write) noexcept {
  const auto kc_address = 0x28U + channel;
  const auto kf_address = 0x30U + channel;
  const auto kc = state.registers[kc_address] & 0x7fU;
  const auto kf = state.registers[kf_address] >> 2U;
  std::optional<std::uint16_t> value;
  if (state.known[kc_address] && state.known[kf_address] && (kc & 3U) != 3U &&
      (channel != 7 || (state.registers[0x0f] & 0x80U) == 0)) {
    const auto semitone = (kc >> 4U) * 12U + (kc & 0x0fU) - ((kc & 0x0fU) >> 2U);
    value = static_cast<std::uint16_t>(semitone * 64U + kf);
  }
  if (value != state.channels[channel].pitch_64) {
    state.channels[channel].pitch_64 = value;
    append(state, channel, Ym2151PerformanceKind::PitchChanged, after_write);
  }
}

void Ym2151ObservationBuffer::voice(Ym2151ObservationState& state, const std::uint8_t channel,
                                    const std::uint8_t voice_number,
                                    const std::uint16_t after_write) noexcept {
  if (state.channels[channel].voice != voice_number) {
    state.channels[channel].voice = voice_number;
    append(state, channel, Ym2151PerformanceKind::InstrumentChanged, after_write);
  }
}

void Ym2151ObservationBuffer::write(Ym2151ObservationState& state, const std::uint8_t address,
                                    const std::uint8_t value, const std::uint16_t after_write,
                                    const bool direct) noexcept {
  const auto previous = state.registers[address];
  const bool known = state.known[address];
  state.registers[address] = value;
  state.known[address] = true;
  const auto channel = static_cast<std::uint8_t>(address & 7U);
  if (address == 0x08) {
    const auto target = static_cast<std::uint8_t>(value & 7U);
    const auto mask = static_cast<std::uint8_t>((value >> 3U) & 0x0fU);
    const auto old_mask = state.key_masks[target];
    state.key_masks[target] = mask;
    auto& observed = state.channels[target];
    if (state.csm || (!observed.gate_known && mask != 0))
      return;
    const bool was_known = observed.gate_known;
    observed.gate_known = true;
    observed.key_on = mask != 0;
    if (mask == 0 && (old_mask != 0 || !was_known))
      append(state, target, Ym2151PerformanceKind::KeyOff, after_write);
    else if ((mask & static_cast<std::uint8_t>(~old_mask)) != 0)
      append(state, target, Ym2151PerformanceKind::KeyOn, after_write);
  } else if (address == 0x14) {
    state.csm = (value & 0x80U) != 0;
    if (state.csm) {
      batch_.capture_lost = true;
      for (auto& observed : state.channels)
        observed.gate_known = false;
    }
  } else if (address == 0x0f) {
    pitch(state, 7, after_write);
  } else if (direct && address >= 0x28 && address <= 0x37) {
    pitch(state, channel, after_write);
  } else if (direct && known && state.channels[channel].voice &&
             ((address >= 0x40 && previous != value) ||
              (address >= 0x20 && address <= 0x27 && (previous & 0x3fU) != (value & 0x3fU)))) {
    state.channels[channel].voice.reset();
    append(state, channel, Ym2151PerformanceKind::InstrumentChanged, after_write);
  }
}

void Ym2151ObservationBuffer::copy_to(const Ym2151ObservationState& state,
                                      const std::uint16_t write_count,
                                      Ym2151PerformanceBatch& output) const noexcept {
  output.channels = state.channels;
  output.write_count = write_count;
  output.count = batch_.count;
  output.lost_before = batch_.lost_before;
  output.capture_lost = batch_.capture_lost;
  for (std::uint16_t i = 0; i < batch_.count; ++i)
    output.events[i] = batch_.events[(first_ + i) % kMdxPerformanceCapacity];
}

} // namespace rpcmp::runtime::mdx
