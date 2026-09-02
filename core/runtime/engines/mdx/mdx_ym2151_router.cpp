#include "rpcmp/runtime/mdx_ym2151_router.hpp"

#include <algorithm>
#include <climits>

namespace rpcmp::runtime::mdx {
namespace {

constexpr std::array<std::uint8_t, 8> kCarrierMasks{0x08, 0x08, 0x08, 0x08, 0x0c, 0x0e, 0x0e, 0x0f};
constexpr std::array<std::uint8_t, 16> kVolumeAttenuation{
    0x2a, 0x28, 0x25, 0x22, 0x20, 0x1d, 0x1a, 0x18, 0x15, 0x12, 0x10, 0x0d, 0x0a, 0x08, 0x05, 0x02};
constexpr std::array<std::uint8_t, 96> kKeyCode{
    0x00, 0x01, 0x02, 0x04, 0x05, 0x06, 0x08, 0x09, 0x0a, 0x0c, 0x0d, 0x0e, 0x10, 0x11, 0x12, 0x14,
    0x15, 0x16, 0x18, 0x19, 0x1a, 0x1c, 0x1d, 0x1e, 0x20, 0x21, 0x22, 0x24, 0x25, 0x26, 0x28, 0x29,
    0x2a, 0x2c, 0x2d, 0x2e, 0x30, 0x31, 0x32, 0x34, 0x35, 0x36, 0x38, 0x39, 0x3a, 0x3c, 0x3d, 0x3e,
    0x40, 0x41, 0x42, 0x44, 0x45, 0x46, 0x48, 0x49, 0x4a, 0x4c, 0x4d, 0x4e, 0x50, 0x51, 0x52, 0x54,
    0x55, 0x56, 0x58, 0x59, 0x5a, 0x5c, 0x5d, 0x5e, 0x60, 0x61, 0x62, 0x64, 0x65, 0x66, 0x68, 0x69,
    0x6a, 0x6c, 0x6d, 0x6e, 0x70, 0x71, 0x72, 0x74, 0x75, 0x76, 0x78, 0x79, 0x7a, 0x7c, 0x7d, 0x7e};

DecodeResult failure(const DecodeError error, const SemanticAction& action) noexcept {
  return {error, action.instruction.byte_offset, action.logical_channel};
}

bool append(Ym2151WriteBatch& batch, const std::uint8_t channel, const std::uint8_t address,
            const std::uint8_t value) noexcept {
  if (batch.count == batch.writes.size()) {
    return false;
  }
  batch.writes[batch.count++] = {address, value, channel};
  return true;
}

std::uint8_t attenuation(const std::uint8_t volume) noexcept {
  return (volume & 0x80U) != 0 ? static_cast<std::uint8_t>(volume & 0x7fU)
                               : kVolumeAttenuation[volume];
}

bool write_voice(const Voice& voice, const std::uint8_t channel, Ym2151WriteBatch& batch) noexcept {
  const std::uint8_t carriers = kCarrierMasks[voice.feedback_connection & 0x07U];
  for (std::size_t group = 0; group < voice.operators.size(); ++group) {
    for (std::size_t op = 0; op < voice.operators[group].size(); ++op) {
      std::uint8_t value = voice.operators[group][op];
      if (group == 1 && ((carriers >> op) & 1U) != 0) {
        value = 0x7f;
      }
      const auto address = static_cast<std::uint8_t>(0x40U + group * 0x20U + op * 8U + channel);
      if (!append(batch, channel, address, value)) {
        return false;
      }
    }
  }
  return true;
}

std::uint16_t current_pitch(const Ym2151ChannelState& state) noexcept {
  const auto offset = state.portamento_accumulator >> 8;
  const auto pitch =
      std::max<std::int32_t>(0, std::min<std::int32_t>(0x17ff, state.base_pitch + offset));
  return static_cast<std::uint16_t>(pitch);
}

bool write_pitch(Ym2151ChannelState& state, const std::uint8_t channel,
                 Ym2151WriteBatch& batch) noexcept {
  const std::uint16_t pitch = current_pitch(state);
  if (pitch == state.last_pitch) {
    return true;
  }
  state.last_pitch = pitch;
  const auto scaled = static_cast<std::uint32_t>(pitch) * 4U;
  return append(batch, channel, static_cast<std::uint8_t>(0x30U + channel),
                static_cast<std::uint8_t>(scaled)) &&
         append(batch, channel, static_cast<std::uint8_t>(0x28U + channel),
                kKeyCode[static_cast<std::size_t>(pitch) >> 6U]);
}

bool write_note(const Voice& voice, Ym2151ChannelState& state, const std::uint8_t channel,
                Ym2151WriteBatch& batch) noexcept {
  if (state.applied_voice != state.selected_voice) {
    if (!write_voice(voice, channel, batch)) {
      return false;
    }
    state.applied_voice = state.selected_voice;
  }
  const auto control = static_cast<std::uint8_t>((state.pan << 6U) | voice.feedback_connection);
  if (!append(batch, channel, static_cast<std::uint8_t>(0x20U + channel), control) ||
      !write_pitch(state, channel, batch)) {
    return false;
  }

  const std::uint8_t carriers = kCarrierMasks[voice.feedback_connection & 0x07U];
  const std::uint8_t add = attenuation(state.volume);
  for (std::size_t op = 0; op < 4; ++op) {
    if (((carriers >> op) & 1U) == 0) {
      continue;
    }
    const auto level = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(voice.operators[1][op]) + static_cast<std::uint16_t>(add));
    if (!append(batch, channel, static_cast<std::uint8_t>(0x60U + op * 8U + channel),
                static_cast<std::uint8_t>(
                    std::min<std::uint16_t>(static_cast<std::uint16_t>(0x7f), level)))) {
      return false;
    }
  }
  if (!state.key_on) {
    if (!append(batch, channel, 0x08,
                static_cast<std::uint8_t>((voice.slot_mask << 3U) | channel))) {
      return false;
    }
    state.key_on = true;
  }
  return true;
}

std::uint16_t compute_gate_ticks(const Ym2151ChannelState& state,
                                 const std::uint16_t duration_ticks) noexcept {
  const auto raw = static_cast<std::uint8_t>(duration_ticks - 1U);
  const auto signed_gate = static_cast<std::int8_t>(state.gate_parameter);
  if (signed_gate >= 0) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(state.gate_parameter) * raw) / 8U + 1U);
  }
  const auto sum = static_cast<std::uint32_t>(state.gate_parameter) + raw;
  return sum >= 0x100U ? static_cast<std::uint16_t>((sum & 0xffU) + 1U) : 1U;
}

bool begin_tick(Ym2151ChannelState& state, const std::uint8_t channel,
                Ym2151WriteBatch& batch) noexcept {
  if (state.portamento_active && state.delay_ticks == 0) {
    const auto next = static_cast<std::int64_t>(state.portamento_accumulator) +
                      static_cast<std::int64_t>(state.portamento_delta);
    if (next < INT32_MIN || next > INT32_MAX) {
      return false;
    }
    state.portamento_accumulator = static_cast<std::int32_t>(next);
  }
  if (!state.suppress_key_off && state.gate_ticks != 0) {
    --state.gate_ticks;
    if (state.gate_ticks == 0 && state.key_on) {
      if (!append(batch, channel, 0x08, channel)) {
        return false;
      }
      state.key_on = false;
    }
  }
  return true;
}

} // namespace

DecodeResult route_ym2151_batch(const MdxDocument& document, const DocumentTickBatch& actions,
                                Ym2151RouterState& state, Ym2151WriteBatch& writes,
                                Ym2151RouterScratch& scratch) noexcept {
  if (actions.count > actions.actions.size()) {
    return {DecodeError::RangeOutsideInput, 0, 0xff};
  }
  scratch.candidate_state = state;
  scratch.pending_batch.count = 0;
  std::size_t action_index = 0;

  for (std::uint8_t logical_channel = 0; logical_channel < kMdxFmTrackCount; ++logical_channel) {
    Ym2151ChannelState& channel = scratch.candidate_state.channels[logical_channel];
    const SemanticAction synthetic{logical_channel, {}};
    if (!begin_tick(channel, logical_channel, scratch.pending_batch)) {
      return failure(DecodeError::BudgetExhausted, synthetic);
    }
    const bool has_actions = action_index < actions.count &&
                             actions.actions[action_index].logical_channel == logical_channel;
    if (has_actions) {
      channel.suppress_key_off = false;
    }

    while (action_index < actions.count &&
           actions.actions[action_index].logical_channel == logical_channel) {
      const SemanticAction& action = actions.actions[action_index++];
      switch (action.instruction.kind) {
      case InstructionKind::TimerB:
        if (!append(scratch.pending_batch, logical_channel, 0x12, action.instruction.value)) {
          return failure(DecodeError::BudgetExhausted, action);
        }
        break;
      case InstructionKind::DirectWrite:
        if (!append(scratch.pending_batch, logical_channel, action.instruction.value,
                    action.instruction.second_value)) {
          return failure(DecodeError::BudgetExhausted, action);
        }
        break;
      case InstructionKind::SelectVoice:
        channel.selected_voice = action.instruction.value;
        break;
      case InstructionKind::Pan:
        channel.pan = static_cast<std::uint8_t>(action.instruction.value & 0x03U);
        break;
      case InstructionKind::Volume:
        if ((action.instruction.value & 0x80U) == 0 && action.instruction.value > 15) {
          return failure(DecodeError::RangeOutsideInput, action);
        }
        channel.volume = action.instruction.value;
        break;
      case InstructionKind::Gate:
        channel.gate_parameter = action.instruction.value;
        break;
      case InstructionKind::SuppressKeyOff:
        channel.suppress_key_off = true;
        break;
      case InstructionKind::Detune:
        channel.detune = action.instruction.signed_value;
        break;
      case InstructionKind::Portamento:
        channel.portamento_delta = static_cast<std::int32_t>(action.instruction.signed_value) * 256;
        channel.portamento_active = true;
        break;
      case InstructionKind::KeyOnDelay:
        channel.key_on_delay = action.instruction.value;
        break;
      case InstructionKind::Note:
        if (channel.selected_voice >= document.voices.size() ||
            !document.voices[channel.selected_voice].present) {
          return failure(DecodeError::MissingVoice, action);
        }
        channel.base_pitch =
            static_cast<std::int32_t>(action.instruction.value & 0x7fU) * 64 + 5 + channel.detune;
        channel.portamento_accumulator = 0;
        channel.last_pitch = 0xffff;
        channel.gate_ticks = compute_gate_ticks(channel, action.instruction.duration_ticks);
        channel.delay_ticks = channel.key_on_delay;
        channel.pending_note = true;
        break;
      case InstructionKind::Rest:
      case InstructionKind::RepeatStart:
      case InstructionKind::RepeatEnd:
      case InstructionKind::RepeatEscape:
      case InstructionKind::TrackEnd:
      case InstructionKind::TrackLoop:
      case InstructionKind::ReleaseChannel:
      case InstructionKind::WaitChannel:
        break;
      }
    }

    if (channel.pending_note) {
      if (channel.delay_ticks != 0) {
        --channel.delay_ticks;
      } else {
        if (!write_note(document.voices[channel.selected_voice], channel, logical_channel,
                        scratch.pending_batch)) {
          return failure(DecodeError::BudgetExhausted, synthetic);
        }
        channel.pending_note = false;
      }
    } else if (channel.portamento_active &&
               !write_pitch(channel, logical_channel, scratch.pending_batch)) {
      return failure(DecodeError::BudgetExhausted, synthetic);
    }
  }

  if (action_index != actions.count) {
    return failure(DecodeError::RangeOutsideInput, actions.actions[action_index]);
  }

  state = scratch.candidate_state;
  writes = scratch.pending_batch;
  return {};
}

} // namespace rpcmp::runtime::mdx
