#include "rpcmp/utility/stable_id.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rpcmp::utility {
namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants{
    0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U, 0x3956C25BU, 0x59F111F1U, 0x923F82A4U,
    0xAB1C5ED5U, 0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U, 0x72BE5D74U, 0x80DEB1FEU,
    0x9BDC06A7U, 0xC19BF174U, 0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU, 0x2DE92C6FU,
    0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU, 0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
    0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U, 0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU,
    0x53380D13U, 0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U, 0xA2BFE8A1U, 0xA81A664BU,
    0xC24B8B70U, 0xC76C51A3U, 0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U, 0x19A4C116U,
    0x1E376C08U, 0x2748774CU, 0x34B0BCB5U, 0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
    0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U, 0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U,
    0xC67178F2U};

std::uint32_t rotate_right(const std::uint32_t value, const std::uint32_t count) {
  return (value >> count) | (value << (32U - count));
}

void append_u32(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
  for (std::uint32_t shift = 0; shift < 32; shift += 8) {
    bytes.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

void append_u64(std::vector<std::uint8_t>& bytes, const std::uint64_t value) {
  for (std::uint32_t shift = 0; shift < 64; shift += 8) {
    bytes.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

void append_view(std::vector<std::uint8_t>& output, const library::ByteView input) {
  if (input.size != 0) {
    output.insert(output.end(), input.data, input.data + input.size);
  }
}

std::uint64_t digest_id(const std::vector<std::uint8_t>& bytes) {
  const auto digest = sha256({bytes.data(), bytes.size()});
  std::uint64_t id{};
  for (std::size_t index = 0; index < 8; ++index) {
    id |= static_cast<std::uint64_t>(digest[index]) << (index * 8U);
  }
  return id;
}

void append_counter(std::vector<std::uint8_t>& bytes, const std::uint32_t collision_counter) {
  if (collision_counter != 0) {
    append_u32(bytes, collision_counter);
  }
}

} // namespace

std::array<std::uint8_t, 32> sha256(const library::ByteView bytes) {
  std::vector<std::uint8_t> padded;
  padded.reserve(bytes.size + 72);
  append_view(padded, bytes);
  padded.push_back(0x80U);
  while ((padded.size() % 64) != 56) {
    padded.push_back(0);
  }
  const std::uint64_t bit_length = static_cast<std::uint64_t>(bytes.size) * 8U;
  for (std::int32_t shift = 56; shift >= 0; shift -= 8) {
    padded.push_back(static_cast<std::uint8_t>(bit_length >> shift));
  }

  std::array<std::uint32_t, 8> state{0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
                                     0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U};
  for (std::size_t block = 0; block < padded.size(); block += 64) {
    std::array<std::uint32_t, 64> words{};
    for (std::size_t index = 0; index < 16; ++index) {
      const std::size_t offset = block + index * 4;
      words[index] = (static_cast<std::uint32_t>(padded[offset]) << 24U) |
                     (static_cast<std::uint32_t>(padded[offset + 1]) << 16U) |
                     (static_cast<std::uint32_t>(padded[offset + 2]) << 8U) |
                     static_cast<std::uint32_t>(padded[offset + 3]);
    }
    for (std::size_t index = 16; index < words.size(); ++index) {
      const std::uint32_t sigma0 = rotate_right(words[index - 15], 7) ^
                                   rotate_right(words[index - 15], 18) ^ (words[index - 15] >> 3U);
      const std::uint32_t sigma1 = rotate_right(words[index - 2], 17) ^
                                   rotate_right(words[index - 2], 19) ^ (words[index - 2] >> 10U);
      words[index] = words[index - 16] + sigma0 + words[index - 7] + sigma1;
    }

    auto working = state;
    for (std::size_t round = 0; round < words.size(); ++round) {
      const std::uint32_t sum1 =
          rotate_right(working[4], 6) ^ rotate_right(working[4], 11) ^ rotate_right(working[4], 25);
      const std::uint32_t choice = (working[4] & working[5]) ^ (~working[4] & working[6]);
      const std::uint32_t temporary1 =
          working[7] + sum1 + choice + kRoundConstants[round] + words[round];
      const std::uint32_t sum0 =
          rotate_right(working[0], 2) ^ rotate_right(working[0], 13) ^ rotate_right(working[0], 22);
      const std::uint32_t majority =
          (working[0] & working[1]) ^ (working[0] & working[2]) ^ (working[1] & working[2]);
      const std::uint32_t temporary2 = sum0 + majority;
      for (std::size_t index = 7; index > 0; --index) {
        working[index] = working[index - 1];
      }
      working[4] += temporary1;
      working[0] = temporary1 + temporary2;
    }
    for (std::size_t index = 0; index < state.size(); ++index) {
      state[index] += working[index];
    }
  }

  std::array<std::uint8_t, 32> digest{};
  for (std::size_t index = 0; index < state.size(); ++index) {
    digest[index * 4] = static_cast<std::uint8_t>(state[index] >> 24U);
    digest[index * 4 + 1] = static_cast<std::uint8_t>(state[index] >> 16U);
    digest[index * 4 + 2] = static_cast<std::uint8_t>(state[index] >> 8U);
    digest[index * 4 + 3] = static_cast<std::uint8_t>(state[index]);
  }
  return digest;
}

contracts::BlobId stable_blob_id(const std::uint32_t kind, const library::ByteView bytes,
                                 const std::uint32_t collision_counter) {
  std::vector<std::uint8_t> identity{'b', 'l', 'o', 'b', 0};
  append_u32(identity, kind);
  append_view(identity, bytes);
  append_counter(identity, collision_counter);
  return {digest_id(identity)};
}

contracts::StringId stable_string_id(const library::ByteView normalized_utf8,
                                     const std::uint32_t collision_counter) {
  std::vector<std::uint8_t> identity{'s', 't', 'r', 'i', 'n', 'g', 0};
  append_view(identity, normalized_utf8);
  append_counter(identity, collision_counter);
  return {digest_id(identity)};
}

contracts::TrackId stable_track_id(const TrackIdentity& track,
                                   const std::uint32_t collision_counter) {
  std::vector<std::uint8_t> identity{'t', 'r', 'a', 'c', 'k', 0};
  append_u32(identity, track.format);
  append_u64(identity, track.primary_blob_id.value);
  for (std::size_t index = 0; index < track.dependency_count; ++index) {
    append_u32(identity, track.dependencies[index].role);
    append_u64(identity, track.dependencies[index].blob_id.value);
  }
  for (const contracts::StringId id :
       {track.title_id, track.artist_id, track.album_id, track.composer_id, track.system_id}) {
    append_u64(identity, id.value);
  }
  append_counter(identity, collision_counter);
  return {digest_id(identity)};
}

} // namespace rpcmp::utility
