#include "rpcmp/runtime/mdx_ym2151_router.hpp"
#include "test_support.hpp"

#include <cstddef>
#include <cstdint>

namespace {

using rpcmp::runtime::mdx::DecodeError;
using rpcmp::runtime::mdx::DocumentTickBatch;
using rpcmp::runtime::mdx::InstructionKind;
using rpcmp::runtime::mdx::MdxDocument;
using rpcmp::runtime::mdx::SemanticAction;
using rpcmp::runtime::mdx::SemanticInstruction;
using rpcmp::runtime::mdx::Ym2151RouterScratch;
using rpcmp::runtime::mdx::Ym2151RouterState;
using rpcmp::runtime::mdx::Ym2151WriteBatch;

SemanticAction action(const std::uint8_t channel, const InstructionKind kind,
                      const std::uint8_t value = 0, const std::uint8_t second = 0) {
  return {channel, {kind, 100, 0, 0, value, second}};
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  MdxDocument document{};
  auto& voice = document.voices[3];
  voice.present = true;
  voice.feedback_connection = 0;
  voice.slot_mask = 0x0f;
  for (std::size_t group = 0; group < voice.operators.size(); ++group) {
    for (std::size_t op = 0; op < voice.operators[group].size(); ++op) {
      voice.operators[group][op] = static_cast<std::uint8_t>(group * 4 + op + 1);
    }
  }

  DocumentTickBatch actions{};
  actions.actions[actions.count++] = action(0, InstructionKind::TimerB, 0xc8);
  actions.actions[actions.count++] = action(0, InstructionKind::DirectWrite, 0x1b, 0x02);
  actions.actions[actions.count++] = action(0, InstructionKind::SelectVoice, 3);
  actions.actions[actions.count++] = action(0, InstructionKind::Note, 0x80);

  Ym2151RouterState state{};
  Ym2151WriteBatch writes{};
  static Ym2151RouterScratch scratch{};
  RPCMP_CHECK(
      suite,
      rpcmp::runtime::mdx::route_ym2151_batch(document, actions, state, writes, scratch).ok());
  RPCMP_CHECK(suite, writes.count == 31);
  RPCMP_CHECK(suite, writes.writes[0].address == 0x12 && writes.writes[0].value == 0xc8);
  RPCMP_CHECK(suite, writes.writes[1].address == 0x1b && writes.writes[1].value == 0x02);
  RPCMP_CHECK(suite, writes.writes[2].address == 0x40 && writes.writes[2].value == 1);
  RPCMP_CHECK(suite, writes.writes[5].address == 0x58 && writes.writes[5].value == 4);
  RPCMP_CHECK(suite, writes.writes[6].address == 0x60 && writes.writes[6].value == 5);
  RPCMP_CHECK(suite, writes.writes[9].address == 0x78 && writes.writes[9].value == 0x7f);
  RPCMP_CHECK(suite, writes.writes[26].address == 0x20 && writes.writes[26].value == 0xc0);
  RPCMP_CHECK(suite, writes.writes[27].address == 0x30 && writes.writes[27].value == 20);
  RPCMP_CHECK(suite, writes.writes[28].address == 0x28 && writes.writes[28].value == 0);
  RPCMP_CHECK(suite, writes.writes[29].address == 0x78 && writes.writes[29].value == 29);
  RPCMP_CHECK(suite, writes.writes[30].address == 0x08 && writes.writes[30].value == 0x78);

  actions = {};
  actions.actions[actions.count++] = action(0, InstructionKind::Pan, 1);
  actions.actions[actions.count++] = action(0, InstructionKind::Volume, 0x82);
  auto detune = action(0, InstructionKind::Detune);
  detune.instruction.signed_value = -5;
  actions.actions[actions.count++] = detune;
  actions.actions[actions.count++] = action(0, InstructionKind::Note, 0x80);
  RPCMP_CHECK(
      suite,
      rpcmp::runtime::mdx::route_ym2151_batch(document, actions, state, writes, scratch).ok());
  RPCMP_CHECK(suite, writes.count == 5);
  RPCMP_CHECK(suite, writes.writes[0].address == 0x20 && writes.writes[0].value == 0x40);
  RPCMP_CHECK(suite, writes.writes[1].address == 0x30 && writes.writes[1].value == 0);
  RPCMP_CHECK(suite, writes.writes[2].address == 0x28 && writes.writes[2].value == 0);
  RPCMP_CHECK(suite, writes.writes[3].address == 0x78 && writes.writes[3].value == 10);
  RPCMP_CHECK(suite, writes.writes[4].address == 0x08 && writes.writes[4].value == 0x78);

  actions = {};
  actions.actions[actions.count++] = action(1, InstructionKind::DirectWrite, 0x20, 0xff);
  actions.actions[actions.count++] = action(1, InstructionKind::SelectVoice, 4);
  actions.actions[actions.count++] = action(1, InstructionKind::Note, 0x81);
  writes.count = 1;
  writes.writes[0] = {0xaa, 0x55, 7};
  const auto missing =
      rpcmp::runtime::mdx::route_ym2151_batch(document, actions, state, writes, scratch);
  RPCMP_CHECK(suite, missing.error == DecodeError::MissingVoice);
  RPCMP_CHECK(suite, state.channels[1].selected_voice == 256);
  RPCMP_CHECK(suite, writes.count == 1);
  RPCMP_CHECK(suite, writes.writes[0].address == 0xaa);

  return suite.finish("mdx ym2151 note-start router");
}
