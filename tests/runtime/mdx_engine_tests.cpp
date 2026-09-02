#include "rpcmp/runtime/mdx_engine.hpp"
#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using rpcmp::runtime::mdx::ByteView;
using rpcmp::runtime::mdx::MdxDocument;
using rpcmp::runtime::mdx::TrackTarget;

MdxDocument document_with(const ByteView first, const ByteView pcm) {
  static constexpr std::array<std::uint8_t, 2> kEnd{0xf1, 0x00};
  MdxDocument document{};
  for (std::size_t index = 0; index < document.tracks.size(); ++index) {
    document.tracks[index] = {
        static_cast<std::uint8_t>(index),
        index < 8 ? TrackTarget::Ym2151 : TrackTarget::LegacyAdpcm,
        index * 100,
        {kEnd.data(), kEnd.size()},
    };
  }
  document.tracks[0].source = first;
  document.tracks[8].source = pcm;
  return document;
}

void install_voice(MdxDocument& document) {
  auto& voice = document.voices[3];
  voice.present = true;
  voice.feedback_connection = 0;
  voice.slot_mask = 0x0f;
  for (std::size_t group = 0; group < voice.operators.size(); ++group) {
    for (std::size_t op = 0; op < voice.operators[group].size(); ++op) {
      voice.operators[group][op] = static_cast<std::uint8_t>(group * 4 + op + 1);
    }
  }
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  static constexpr std::array<std::uint8_t, 2> kEnd{0xf1, 0x00};
  static constexpr std::array<std::uint8_t, 11> kFmTick{0xff, 0xc8, 0xfe, 0x1b, 0x02, 0xfd,
                                                        0x03, 0x80, 0x00, 0xf1, 0x00};
  auto document = document_with({kFmTick.data(), kFmTick.size()}, {kEnd.data(), kEnd.size()});
  install_voice(document);
  static rpcmp::runtime::mdx::MdxEngineScratch scratch{};
  rpcmp::runtime::mdx::DocumentValidation validation{};
  RPCMP_CHECK(suite, rpcmp::runtime::mdx::prepare_mdx_playback(document, validation, scratch).ok());

  rpcmp::runtime::mdx::MdxEngineState state{};
  rpcmp::runtime::mdx::TimedYm2151Batch batch{};
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::advance_mdx_tick(document, 48'000, state, batch, scratch).ok());
  RPCMP_CHECK(suite, batch.count == 31);
  RPCMP_CHECK(suite, batch.writes[0].at_tick == 0);
  RPCMP_CHECK(suite, batch.writes[0].write.address == 0x12);
  RPCMP_CHECK(suite, batch.writes[1].write.address == 0x1b);
  RPCMP_CHECK(suite, batch.writes[30].write.address == 0x08);
  RPCMP_CHECK(suite, state.document.tracks[0].cursor == 9);
  RPCMP_CHECK(suite, state.ym2151.channels[0].key_on);
  RPCMP_CHECK(suite, state.timeline.scheduler_tick == 688);

  static constexpr std::array<std::uint8_t, 6> kMissingVoice{0xfd, 0x04, 0x80, 0x00, 0xf1, 0x00};
  const auto failing =
      document_with({kMissingVoice.data(), kMissingVoice.size()}, {kEnd.data(), kEnd.size()});
  state = {};
  batch.count = 1;
  batch.writes[0].at_tick = 99;
  const auto before = state;
  const auto missing =
      rpcmp::runtime::mdx::advance_mdx_tick(failing, 48'000, state, batch, scratch);
  RPCMP_CHECK(suite, missing.error == rpcmp::runtime::mdx::DecodeError::MissingVoice);
  RPCMP_CHECK(suite, state.document.tracks[0].cursor == before.document.tracks[0].cursor);
  RPCMP_CHECK(suite, state.timeline.scheduler_tick == before.timeline.scheduler_tick);
  RPCMP_CHECK(suite, !state.ym2151.channels[0].key_on);
  RPCMP_CHECK(suite, batch.count == 1);
  RPCMP_CHECK(suite, batch.writes[0].at_tick == 99);

  static constexpr std::array<std::uint8_t, 3> kActivePcm{0x80, 0x00, 0xf1};
  const auto active_pcm =
      document_with({kEnd.data(), kEnd.size()}, {kActivePcm.data(), kActivePcm.size()});
  validation.tracks[0].instruction_count = 77;
  const auto rejected = rpcmp::runtime::mdx::prepare_mdx_playback(active_pcm, validation, scratch);
  RPCMP_CHECK(suite, rejected.error == rpcmp::runtime::mdx::DecodeError::UnsupportedPcm);
  RPCMP_CHECK(suite, validation.tracks[0].instruction_count == 77);

  return suite.finish("MDX transactional engine");
}
