#include "rpcmp/platform/pocket/sound_mmio_client.hpp"
#include "test_support.hpp"

#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace rpcmp::platform::pocket;
namespace player = rpcmp::player;
using C = SoundChannel;
struct Clock final : SoundClock {
  std::uint64_t now{};
  std::uint64_t now_us() noexcept override { return now; }
};
struct Write {
  std::uintptr_t offset{};
  std::uint32_t value{};
};
bool operator==(const Write& a, const Write& b) {
  return a.offset == b.offset && a.value == b.value;
}
struct Mmio final : IMmio32 {
  std::array<std::uint32_t, 256> words{};
  std::vector<Write> writes;
  std::vector<std::uintptr_t> reads;
  std::array<unsigned, 4> releases{};
  unsigned invalid{}, inhibits{};
  Clock* clock_on_submit{};
  std::uint64_t after_submit{};
  Mmio() {
    words[0] = 0x52534d31;
    words[1] = 0x00010001;
    words[2] = 15;
    words[3] = 0x880;
    words[6] = 12'288'000;
    words[7] = 48'000;
  }
  static unsigned shift(unsigned slot) { return slot == 3 ? 12 : slot * 2; }
  std::uint32_t read(std::uintptr_t address) noexcept override {
    if (address < 0x40000400 || address >= 0x40000800 || address % 4 != 0) {
      ++invalid;
      return 0;
    }
    const auto offset = address - 0x40000400;
    reads.push_back(offset);
    if (offset == 0 || offset == 4 || offset == 8 || offset == 12 || offset == 24 || offset == 28)
      return words[offset / 4];
    unsigned slot = 4;
    if (offset >= 0x180 && offset <= 0x1bc)
      slot = 0;
    if (offset >= 0x0b8 && offset <= 0x0c8)
      slot = 1;
    if (offset >= 0x200 && offset <= 0x280)
      slot = 2;
    if (offset >= 0x340 && offset <= 0x3b8)
      slot = 3;
    if (slot == 4 || (words[3] & (2U << shift(slot))) == 0) {
      ++invalid;
      return 0;
    }
    return words[offset / 4];
  }
  void write(std::uintptr_t address, std::uint32_t value) noexcept override {
    if (address < 0x40000400 || address >= 0x40000800 || address % 4 != 0) {
      ++invalid;
      return;
    }
    const auto offset = address - 0x40000400;
    writes.push_back({offset, value});
    if (offset == 0x010 && value == 1) {
      ++inhibits;
      words[3] |= 0x400;
      return;
    }
    constexpr std::array<std::uintptr_t, 4> submits{0x044, 0x0b0, 0x110, 0x31c};
    constexpr std::array<std::uintptr_t, 4> release{0x048, 0x0b4, 0x114, 0x320};
    for (unsigned i = 0; i < 4; ++i) {
      if (offset == submits[i]) {
        if (value != 1 || (words[3] & (3U << shift(i))) != 0)
          ++invalid;
        words[3] |= 1U << shift(i);
        if (clock_on_submit)
          clock_on_submit->now = after_submit;
        return;
      }
      if (offset == release[i]) {
        if (value != 1 || (words[3] & (2U << shift(i))) == 0)
          ++invalid;
        ++releases[i];
        words[3] &= ~(3U << shift(i));
        return;
      }
    }
    if ((offset >= 0x020 && offset <= 0x040) || (offset >= 0x080 && offset <= 0x0ac) ||
        (offset >= 0x100 && offset <= 0x10c) || (offset >= 0x300 && offset <= 0x318)) {
      words[offset / 4] = value;
      return;
    }
    ++invalid;
  }
  void response(unsigned slot, std::uintptr_t offset, std::initializer_list<std::uint32_t> data) {
    auto word = offset / 4;
    for (auto value : data)
      words[word++] = value;
    words[3] |= 3U << shift(slot);
  }
  void reset_response(std::uint32_t id = 1, std::uint32_t generation = 1, std::uint32_t epoch = 1) {
    response(0, 0x180, {id, 0, generation, 0, 0, 0, 0, 0, 0, 0, 0, 1, epoch, 0, generation, 0});
  }
  void empty_journal(std::uint32_t ticket = 1, std::uint32_t epoch = 1) {
    response(3, 0x340, {ticket, 0, epoch, 0, epoch, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0,
                        0,      0, 0,     0, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  }
  void capture_response(std::uint32_t ticket = 1, std::uint32_t epoch = 1) {
    response(2, 0x200, {ticket, 0, epoch,  0, epoch, 0, 1, 0, 1, 0, 20, 0, 1, 0, 0, 0, 2,
                        0x21,   0, 240000, 0, 1,     2, 0, 0, 0, 3, 0,  0, 0, 0, 0, 1});
  }
};
template <typename T> T required(const std::optional<T>& item) {
  if (!item)
    throw std::logic_error("missing client completion");
  return *item;
}
const player::AudioControlRequest reset{1, 1, player::AudioControlKind::Reset};
const player::MdxSourceOffer write_item{1, 1, 1, 0, 0, 0, 0x28, 0x3c, false, false};

void transcripts(rpcmp::test::Suite& suite) {
  Clock clock;
  Mmio mmio;
  SoundMmioClient client(mmio, clock);
  RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted && mmio.writes.empty());
  const player::AudioControlRequest request{
      0x1122334455667788ULL, 0x2132435465768798ULL, player::AudioControlKind::Resume,
      rpcmp::contracts::v2::RepeatApplication{0x99aabbccddeeff01ULL, 17}};
  auto offered_control = request;
  RPCMP_CHECK(suite, client.control(offered_control) == SoundSubmit::Accepted);
  offered_control = reset;
  const std::vector<Write> expected{{0x020, 0x55667788}, {0x024, 0x11223344}, {0x028, 0x65768798},
                                    {0x02c, 0x21324354}, {0x030, 3},          {0x034, 0xddeeff01},
                                    {0x038, 0x99aabbcc}, {0x03c, 1},          {0x040, 17},
                                    {0x044, 1}};
  RPCMP_CHECK(suite, mmio.writes == expected);
  RPCMP_CHECK(suite, client.control(reset) == SoundSubmit::Busy && mmio.writes == expected);
  mmio.response(0, 0x180,
                {0x55667788, 0x11223344, 0x65768798, 0x21324354, 0xddeeff01, 0x99aabbcc, 3, 1, 17,
                 0x31415926, 0x27182818, 1, 0x12345678, 0xabcdef01, 0x65768798, 0x21324354});
  clock.now = 1000;
  client.service();
  mmio.words[0x1a4 / 4] = 0; // The CPU result owns its copy after hardware release.
  RPCMP_CHECK(suite, client.busy(C::Control));
  const auto control = required(client.take_control());
  RPCMP_CHECK(
      suite, control.transfer == SoundTransfer::Complete && control.result == SoundReply::Success &&
                 control.request.operation_id == request.operation_id &&
                 control.request.repeat == request.repeat &&
                 control.frame == 0x2718281831415926ULL && control.epoch == 0xabcdef0112345678ULL &&
                 control.prepared_generation == request.play_generation &&
                 !client.busy(C::Control));
  client.service();
  RPCMP_CHECK(suite, !client.take_control() && mmio.releases[0] == 1);

  mmio.writes.clear();
  const player::MdxSourceOffer item{0x123456789abcdef0ULL,
                                    0x23456789abcdef01ULL,
                                    0x3456789abcdef012ULL,
                                    0x456789abcdef0123ULL,
                                    0x56789abcdef01234ULL,
                                    0x6789abcdef012345ULL,
                                    0,
                                    0,
                                    true,
                                    false};
  auto offered_item = item;
  RPCMP_CHECK(suite, client.feed(offered_item) == SoundSubmit::Accepted);
  offered_item = write_item;
  const std::vector<Write> feed{{0x080, 0x9abcdef0}, {0x084, 0x12345678}, {0x088, 0xabcdef01},
                                {0x08c, 0x23456789}, {0x090, 0xcdef0123}, {0x094, 0x456789ab},
                                {0x098, 0xdef01234}, {0x09c, 0x56789abc}, {0x0a0, 0xef012345},
                                {0x0a4, 0x6789abcd}, {0x0a8, 1},          {0x0ac, 0},
                                {0x0b0, 1}};
  RPCMP_CHECK(suite, mmio.writes == feed);
  mmio.response(1, 0x0b8, {2, 0x9abcdef0, 0x12345678, 0xabcdef01, 0x23456789});
  client.service();
  const auto reply = required(client.take_feed());
  RPCMP_CHECK(suite, reply.transfer == SoundTransfer::Complete &&
                         reply.item.status == player::MdxFeedStatus::Full &&
                         reply.item.token == item.token &&
                         reply.item.generation == item.generation &&
                         reply.item.epoch == item.epoch);
  const auto old_writes = mmio.writes.size();
  client.service();
  RPCMP_CHECK(suite, mmio.writes.size() == old_writes && !client.take_feed());
  RPCMP_CHECK(suite, client.feed(item) == SoundSubmit::Accepted);
  mmio.response(1, 0x0b8, {1, 0x9abcdef0, 0x12345678, 0xabcdef01, 0x23456789});
  client.service();
  RPCMP_CHECK(suite, required(client.take_feed()).item.status == player::MdxFeedStatus::Accepted);
  RPCMP_CHECK(suite, mmio.invalid == 0 && mmio.inhibits == 0);
}

void captures_and_journal(rpcmp::test::Suite& suite) {
  Clock clock;
  Mmio mmio;
  SoundMmioClient client(mmio, clock);
  RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted);
  RPCMP_CHECK(suite, client.capture({0x1122334455667788ULL, 0xabcdef0112345678ULL}) ==
                         SoundSubmit::Accepted);
  RPCMP_CHECK(suite, client.journal({0x5566778899aabbccULL, 0xabcdef0112345678ULL,
                                     0x300000061ULL}) == SoundSubmit::Accepted);
  const std::vector<Write> expected{{0x100, 0x55667788}, {0x104, 0x11223344}, {0x108, 0x12345678},
                                    {0x10c, 0xabcdef01}, {0x110, 1},          {0x300, 0x99aabbcc},
                                    {0x304, 0x55667788}, {0x308, 0x12345678}, {0x30c, 0xabcdef01},
                                    {0x310, 0x61},       {0x314, 3},          {0x318, 1},
                                    {0x31c, 1}};
  RPCMP_CHECK(suite, mmio.writes == expected);
  mmio.response(2, 0x200,
                {0x55667788, 0x11223344, 0x12345678, 0xabcdef01, 0x12345678, 0xabcdef01, 4,
                 5,          6,          7,          8,          9,          10,         11,
                 12,         13,         17,         0x23,       2,          239999,     959,
                 64,         14,         15,         16,         17,         3,          18,
                 19,         20,         21,         7,          1});
  mmio.response(3, 0x340,
                {0x99aabbcc, 0x55667788, 0x12345678, 0xabcdef01, 0x12345678, 0xabcdef01, 1,    2,
                 22,         23,         0x66,       3,          3,          0x61,       3,    24,
                 25,         26,         27,         28,         29,         0,          0x65, 3,
                 30,         31,         32,         33,         34,         35,         1});
  client.service();
  const auto capture = required(client.take_capture());
  const auto journal = required(client.take_journal());
  RPCMP_CHECK(suite,
              capture.transfer == SoundTransfer::Complete &&
                  capture.result == SoundReply::Success && capture.epoch == 0xabcdef0112345678ULL &&
                  capture.prepared_generation == 0x500000004ULL &&
                  capture.media.play_generation == 0x700000006ULL &&
                  capture.media.frame == 0x900000008ULL &&
                  capture.media.policy_revision == 0xb0000000aULL &&
                  capture.media.completed_loops == 0xd0000000cULL && capture.media.target == 17U &&
                  capture.media.paused && capture.media_enabled && !capture.quiescent &&
                  capture.media.phase == player::MediaPhase::RestoringGain &&
                  capture.media.gain == 239999 && capture.media.ramp_elapsed == 959 &&
                  capture.media.ramp_duration == 960 && capture.queued == 64 &&
                  capture.output.prefix == 0xf0000000eULL &&
                  capture.output.completed_loops == 0x1100000010ULL && capture.output.valid &&
                  capture.output.checkpoint && !capture.output.ended &&
                  capture.pending.prefix == 0x1300000012ULL &&
                  capture.pending.completed_loops == 0x1500000014ULL && capture.pending.ended);
  RPCMP_CHECK(
      suite,
      journal.transfer == SoundTransfer::Complete && journal.result == SoundJournalReply::Success &&
          journal.epoch == 0xabcdef0112345678ULL && journal.queued == 2 &&
          journal.lost == 0x1700000016ULL && journal.next_sequence == 0x300000066ULL &&
          journal.head.sequence == 0x300000061ULL && journal.head.generation == 0x1900000018ULL &&
          journal.head.frame == 0x1b0000001aULL && journal.head.prefix == 0x1d0000001cULL &&
          !journal.head.natural_end && journal.latest.sequence == 0x300000065ULL &&
          journal.latest.generation == 0x1f0000001eULL && journal.latest.frame == 0x2100000020ULL &&
          journal.latest.prefix == 0x2300000022ULL && journal.latest.natural_end);
  RPCMP_CHECK(suite, mmio.releases[2] == 1 && mmio.releases[3] == 1 && mmio.invalid == 0 &&
                         mmio.inhibits == 0);
}

void expiry_and_isolation(rpcmp::test::Suite& suite) {
  Clock clock;
  Mmio mmio;
  SoundMmioClient client(mmio, clock);
  RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted);
  RPCMP_CHECK(suite, client.journal({1, 1, 0}) == SoundSubmit::Accepted);
  clock.now = 999;
  client.service();
  RPCMP_CHECK(suite, !client.take_journal());
  clock.now = 1000;
  client.service();
  RPCMP_CHECK(suite, required(client.take_journal()).transfer == SoundTransfer::Timeout &&
                         client.busy(C::Journal) && !client.faulted() && mmio.inhibits == 0);
  RPCMP_CHECK(suite, client.journal({2, 1, 0}) == SoundSubmit::Busy);
  RPCMP_CHECK(suite, client.control(reset) == SoundSubmit::Accepted &&
                         client.feed(write_item) == SoundSubmit::Accepted);
  mmio.reset_response();
  mmio.response(1, 0x0b8, {1, 1, 0, 1, 0});
  client.service();
  RPCMP_CHECK(suite, required(client.take_control()).transfer == SoundTransfer::Complete &&
                         required(client.take_feed()).transfer == SoundTransfer::Complete);
  mmio.empty_journal();
  client.service();
  RPCMP_CHECK(suite, !client.take_journal() && !client.busy(C::Journal) && mmio.releases[3] == 1);
  RPCMP_CHECK(suite,
              client.control({2, 2, player::AudioControlKind::Reset}) == SoundSubmit::Accepted);
  clock.now = 2000;
  client.service();
  RPCMP_CHECK(suite, required(client.take_control()).transfer == SoundTransfer::Timeout &&
                         client.faulted() && mmio.inhibits == 1);
  mmio.reset_response(2, 2, 2);
  client.service();
  RPCMP_CHECK(suite, !client.take_control() && client.faulted());
  RPCMP_CHECK(suite, client.control({3, 3, player::AudioControlKind::Reset}) == SoundSubmit::Busy);
  mmio.words[3] &= ~0x400U;
  RPCMP_CHECK(suite,
              client.control({3, 3, player::AudioControlKind::Reset}) == SoundSubmit::Accepted);
  clock.now = 3000;
  mmio.reset_response(3, 3, 3);
  client.service();
  RPCMP_CHECK(suite, required(client.take_control()).transfer == SoundTransfer::Complete &&
                         !client.faulted());
  RPCMP_CHECK(suite, mmio.invalid == 0);
}
void later_inhibit(rpcmp::test::Suite& suite) {
  Clock clock;
  Mmio mmio;
  SoundMmioClient client(mmio, clock);
  RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted);
  RPCMP_CHECK(suite, client.control(reset) == SoundSubmit::Accepted);
  // Reset already completed in audio; a newer CPU inhibit is still crossing.
  mmio.reset_response();
  client.emergency_silence();
  client.service();
  RPCMP_CHECK(suite, required(client.take_control()).result == SoundReply::Success);
  RPCMP_CHECK(suite, client.faulted() && mmio.inhibits == 1);
}

SoundSubmit submit(SoundMmioClient& client, unsigned slot) {
  switch (slot) {
  case 0:
    return client.control(reset);
  case 1:
    return client.feed(write_item);
  case 2:
    return client.capture({1, 1});
  case 3:
    return client.journal({1, 1, 0});
  default:
    throw std::logic_error("bad test slot");
  }
}
void respond(Mmio& mmio, unsigned slot) {
  switch (slot) {
  case 0:
    mmio.reset_response();
    break;
  case 1:
    mmio.response(1, 0x0b8, {1, 1, 0, 1, 0});
    break;
  case 2:
    mmio.capture_response();
    break;
  case 3:
    mmio.empty_journal();
    break;
  default:
    throw std::logic_error("bad test slot");
  }
}
SoundTransfer take(SoundMmioClient& client, unsigned slot) {
  switch (slot) {
  case 0:
    return required(client.take_control()).transfer;
  case 1:
    return required(client.take_feed()).transfer;
  case 2:
    return required(client.take_capture()).transfer;
  case 3:
    return required(client.take_journal()).transfer;
  default:
    throw std::logic_error("bad test slot");
  }
}

void deadline_matrix(rpcmp::test::Suite& suite) {
  for (unsigned slot = 0; slot < 4; ++slot) {
    for (bool completed : {false, true}) {
      Clock clock;
      Mmio mmio;
      SoundMmioClient client(mmio, clock);
      RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted);
      // Time spent staging is outside the successfully submitted mailbox timer.
      mmio.clock_on_submit = &clock;
      mmio.after_submit = 500;
      RPCMP_CHECK(suite, submit(client, slot) == SoundSubmit::Accepted);
      clock.now = 1499;
      client.service();
      RPCMP_CHECK(suite, client.busy(static_cast<C>(slot)) && mmio.inhibits == 0);
      if (completed)
        respond(mmio, slot);
      clock.now = 1500;
      client.service();
      if (completed) {
        RPCMP_CHECK(suite, take(client, slot) == SoundTransfer::Complete && mmio.inhibits == 0);
      } else {
        const auto writes = mmio.writes.size();
        RPCMP_CHECK(suite,
                    submit(client, slot) == SoundSubmit::Busy && mmio.writes.size() == writes);
        // Leave the failed CPU result unread until the late hardware response drains.
        respond(mmio, slot);
        clock.now = 2000;
        client.service();
        RPCMP_CHECK(suite, take(client, slot) == SoundTransfer::Timeout &&
                               mmio.inhibits == (slot == 3 ? 0U : 1U));
      }
      RPCMP_CHECK(suite, !client.busy(static_cast<C>(slot)) && mmio.releases[slot] == 1 &&
                             mmio.invalid == 0);
    }
  }
  Clock clock;
  Mmio mmio;
  SoundMmioClient client(mmio, clock);
  clock.now = std::numeric_limits<std::uint64_t>::max() - 1000;
  RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted &&
                         client.feed(write_item) == SoundSubmit::Accepted);
  clock.now = std::numeric_limits<std::uint64_t>::max();
  client.service();
  RPCMP_CHECK(suite, required(client.take_feed()).transfer == SoundTransfer::Timeout);
  mmio.response(1, 0x0b8, {1, 1, 0, 1, 0});
  client.service();
  RPCMP_CHECK(suite, !client.take_feed() && mmio.releases[1] == 1);
}

void malformed_replies(rpcmp::test::Suite& suite) {
  // Each mutation targets a documented echo, enum, reserved bit or bound.
  const std::array<Write, 14> capture_errors{{{0x200, 2},
                                              {0x208, 2},
                                              {0x210, 2},
                                              {0x244, 0x100},
                                              {0x248, 3},
                                              {0x248, 20},
                                              {0x248, 160},
                                              {0x24c, 240001},
                                              {0x250, 1},
                                              {0x254, 65},
                                              {0x268, 8},
                                              {0x27c, 8},
                                              {0x280, 4},
                                              {0x240, 0}}};
  for (const auto error : capture_errors) {
    Clock clock;
    Mmio mmio;
    SoundMmioClient client(mmio, clock);
    RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted &&
                           client.capture({1, 1}) == SoundSubmit::Accepted);
    mmio.capture_response();
    mmio.words[error.offset / 4] = error.value;
    client.service();
    RPCMP_CHECK(suite, required(client.take_capture()).transfer == SoundTransfer::Protocol &&
                           client.faulted() && mmio.inhibits == 1 && mmio.releases[2] == 1 &&
                           mmio.invalid == 0);
  }
  const std::array<Write, 10> journal_errors{{{0x340, 2},
                                              {0x348, 2},
                                              {0x350, 2},
                                              {0x358, 5},
                                              {0x35c, 33},
                                              {0x368, 0},
                                              {0x370, 8},
                                              {0x374, 1},
                                              {0x394, 2},
                                              {0x3b8, 2}}};
  for (const auto error : journal_errors) {
    Clock clock;
    Mmio mmio;
    SoundMmioClient client(mmio, clock);
    RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted &&
                           client.journal({1, 1, 0}) == SoundSubmit::Accepted);
    mmio.empty_journal();
    mmio.words[error.offset / 4] = error.value;
    client.service();
    RPCMP_CHECK(suite, required(client.take_journal()).transfer == SoundTransfer::Protocol &&
                           !client.faulted() && mmio.inhibits == 0 && mmio.releases[3] == 1 &&
                           mmio.invalid == 0);
    RPCMP_CHECK(suite, client.control(reset) == SoundSubmit::Accepted &&
                           client.feed(write_item) == SoundSubmit::Accepted);
  }
  for (unsigned slot = 0; slot < 4; ++slot) {
    Clock clock;
    Mmio mmio;
    SoundMmioClient client(mmio, clock);
    RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted &&
                           submit(client, slot) == SoundSubmit::Accepted);
    respond(mmio, slot);
    mmio.words[3] &= ~(1U << Mmio::shift(slot));
    client.service();
    RPCMP_CHECK(suite, take(client, slot) == SoundTransfer::Protocol && mmio.releases[slot] == 1 &&
                           mmio.invalid == 0);
    RPCMP_CHECK(suite, mmio.inhibits == (slot == 3 ? 0U : 1U));
  }
}

void clock_and_old_transfers(rpcmp::test::Suite& suite) {
  for (unsigned slot = 0; slot < 4; ++slot) {
    Clock clock;
    clock.now = 100;
    Mmio mmio;
    SoundMmioClient client(mmio, clock);
    RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted &&
                           submit(client, slot) == SoundSubmit::Accepted);
    clock.now = 99;
    client.service();
    RPCMP_CHECK(suite, take(client, slot) == SoundTransfer::ClockFailure && client.faulted() &&
                           mmio.inhibits == 1);
    respond(mmio, slot);
    client.service();
    RPCMP_CHECK(suite, !client.busy(static_cast<C>(slot)) && mmio.releases[slot] == 1);
    RPCMP_CHECK(suite, client.control(reset) == SoundSubmit::ClockFailure &&
                           client.initialize() == SoundSubmit::ClockFailure);
  }
  Clock clock;
  Mmio mmio;
  SoundMmioClient client(mmio, clock);
  RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted &&
                         client.feed(write_item) == SoundSubmit::Accepted &&
                         client.capture({1, 1}) == SoundSubmit::Accepted &&
                         client.journal({1, 1, 0}) == SoundSubmit::Accepted);
  RPCMP_CHECK(suite,
              client.control({2, 2, player::AudioControlKind::Reset}) == SoundSubmit::Accepted);
  mmio.reset_response(2, 2, 2);
  client.service();
  RPCMP_CHECK(suite, required(client.take_control()).epoch == 2 && client.busy(C::Feed) &&
                         client.busy(C::Capture) && client.busy(C::Journal));
  mmio.response(1, 0x0b8, {5, 1, 0, 1, 0});
  mmio.capture_response();
  mmio.words[0x210 / 4] = 2;
  mmio.words[0x280 / 4] = 3;
  mmio.empty_journal();
  mmio.words[0x350 / 4] = 2;
  mmio.words[0x358 / 4] = 4;
  client.service();
  const auto feed = required(client.take_feed());
  const auto capture = required(client.take_capture());
  const auto journal = required(client.take_journal());
  RPCMP_CHECK(suite, feed.transfer == SoundTransfer::Complete &&
                         feed.item.status == player::MdxFeedStatus::Stale && feed.item.epoch == 1 &&
                         capture.transfer == SoundTransfer::Complete &&
                         capture.result == SoundReply::Stale && capture.request.epoch == 1 &&
                         capture.epoch == 2 && journal.transfer == SoundTransfer::Complete &&
                         journal.result == SoundJournalReply::Stale && journal.request.epoch == 1 &&
                         journal.epoch == 2 && mmio.inhibits == 0 && mmio.invalid == 0);
}

void admission(rpcmp::test::Suite& suite) {
  for (const auto offset : {0U, 4U, 8U, 24U, 28U}) {
    Clock clock;
    Mmio mmio;
    ++mmio.words[offset / 4];
    SoundMmioClient client(mmio, clock);
    RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Incompatible && mmio.writes.empty());
  }
  Clock clock;
  Mmio mmio;
  SoundMmioClient client(mmio, clock);
  RPCMP_CHECK(suite, client.control(reset) == SoundSubmit::NotInitialized);
  client.service();
  client.emergency_silence();
  RPCMP_CHECK(suite, mmio.reads.empty() && mmio.writes.empty());
  mmio.words[3] |= 1;
  RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Busy);
  mmio.words[3] &= ~1U;
  RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted);
  for (unsigned mutation = 0; mutation < 5; ++mutation) {
    auto input = reset;
    if (mutation == 0)
      input.operation_id = 0;
    if (mutation == 1)
      input.play_generation = 0;
    if (mutation == 2) {
      const std::uint8_t invalid_kind = 5;
      static_assert(sizeof(input.kind) == sizeof(invalid_kind));
      std::memcpy(&input.kind, &invalid_kind, sizeof(invalid_kind));
    }
    if (mutation == 3)
      input.kind = player::AudioControlKind::Start;
    if (mutation == 4)
      input.repeat = rpcmp::contracts::v2::RepeatApplication{1, 2};
    RPCMP_CHECK(suite, client.control(input) == SoundSubmit::Invalid);
  }
  auto invalid_item = write_item;
  invalid_item.ended = true;
  RPCMP_CHECK(suite, client.feed(invalid_item) == SoundSubmit::Invalid &&
                         client.capture({0, 1}) == SoundSubmit::Invalid &&
                         client.journal({0, 1, 0}) == SoundSubmit::Invalid &&
                         client.journal({1, 1, std::numeric_limits<std::uint64_t>::max()}) ==
                             SoundSubmit::Invalid &&
                         mmio.writes.empty());
  RPCMP_CHECK(suite, client.busy(static_cast<C>(255)));
}

void remote_results_and_exhaustion(rpcmp::test::Suite& suite) {
  for (std::uint32_t code = 2; code <= 4; ++code) {
    Clock clock;
    Mmio mmio;
    SoundMmioClient client(mmio, clock);
    RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted &&
                           client.control(reset) == SoundSubmit::Accepted);
    mmio.reset_response();
    mmio.words[0x1ac / 4] = code;
    client.service();
    const auto reply = required(client.take_control());
    RPCMP_CHECK(suite, reply.transfer == SoundTransfer::Complete &&
                           static_cast<std::uint32_t>(reply.result) == code && mmio.inhibits == 0);
  }
  for (std::uint32_t code = 3; code <= 4; ++code) {
    Clock clock;
    Mmio mmio;
    SoundMmioClient client(mmio, clock);
    RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted &&
                           client.feed(write_item) == SoundSubmit::Accepted);
    mmio.response(1, 0x0b8, {code, 1, 0, 1, 0});
    client.service();
    const auto reply = required(client.take_feed());
    RPCMP_CHECK(suite, reply.transfer == SoundTransfer::Complete &&
                           static_cast<std::uint32_t>(reply.item.status) == code);
  }
  Clock clock;
  Mmio mmio;
  SoundMmioClient client(mmio, clock);
  RPCMP_CHECK(suite, client.initialize() == SoundSubmit::Accepted &&
                         client.journal({1, 1, 0}) == SoundSubmit::Accepted);
  mmio.empty_journal();
  mmio.words[0x358 / 4] = 1;
  mmio.words[0x35c / 4] = 1;
  mmio.words[0x368 / 4] = mmio.words[0x36c / 4] = 0xffffffff;
  mmio.words[0x370 / 4] = 7;
  mmio.words[0x374 / 4] = 0xfffffffd;
  mmio.words[0x378 / 4] = 0xffffffff;
  mmio.words[0x37c / 4] = 1;
  mmio.words[0x384 / 4] = 5;
  mmio.words[0x38c / 4] = 3;
  mmio.words[0x3a0 / 4] = 1;
  mmio.words[0x3a8 / 4] = 6;
  mmio.words[0x3b0 / 4] = 4;
  client.service();
  const auto journal = required(client.take_journal());
  RPCMP_CHECK(suite, journal.transfer == SoundTransfer::Complete && journal.exhausted &&
                         journal.next_sequence == std::numeric_limits<std::uint64_t>::max() &&
                         journal.latest.sequence == 0 && journal.latest.frame == 6 &&
                         journal.head.epoch == 1 && journal.latest.epoch == 1 && !client.faulted());

  RPCMP_CHECK(suite, client.control(reset) == SoundSubmit::Accepted);
  mmio.words[3] &= ~1U; // An unconfirmed common reset cannot cancel CPU ownership.
  client.service();
  RPCMP_CHECK(suite, required(client.take_control()).transfer == SoundTransfer::Protocol);
  RPCMP_CHECK(suite, client.busy(C::Control) && client.control(reset) == SoundSubmit::Busy &&
                         mmio.releases[0] == 0);
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  transcripts(suite);
  captures_and_journal(suite);
  expiry_and_isolation(suite);
  later_inhibit(suite);
  deadline_matrix(suite);
  malformed_replies(suite);
  clock_and_old_transfers(suite);
  admission(suite);
  remote_results_and_exhaustion(suite);
  std::cout << "CPU client bytes=" << sizeof(SoundMmioClient) << '\n';
  return suite.finish("CPU sound MMIO client");
}
