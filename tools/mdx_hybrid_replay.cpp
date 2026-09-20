// Offline research adapter; no UI, CPU MMIO, transport FIFO or target deadlines.
// Built against pinned JT51 with observation-only accumulator taps.
#include "Vjt51.h"
#include "verilated.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Event {
  std::uint64_t sample;
  std::uint32_t address;
  std::uint32_t value;
};

template <class T> bool read(std::istream& file, T& value) {
  return static_cast<bool>(file.read(reinterpret_cast<char*>(&value), sizeof(value)));
}

class Replay {
public:
  Vjt51 chip;
  std::uint64_t cycle = 0;
  std::vector<std::int16_t> samples;
  std::vector<std::int32_t> wide_samples;

  Replay() {
    chip.rst = 1;
    chip.cs_n = 0;
    chip.wr_n = 1;
    chip.a0 = 0;
    chip.din = 0;
    // Reset scans the complete operator bank. Native time always advances.
    for (int i = 0; i < 1024; ++i) {
      tick(false);
    }
    chip.rst = 0;
    cycle = 0;
  }

  void tick(bool capture = true) {
    chip.clk = 0;
    chip.cen = 1;
    chip.cen_p1 = (cycle & 1U) != 0U;
    chip.eval();
    // Observe the pre-edge sample strobe, as a synchronous consumer does.
    if (capture && chip.sample) {
      samples.push_back(static_cast<std::int16_t>(chip.left));
      samples.push_back(static_cast<std::int16_t>(chip.right));
      // Verilator exposes packed signed ports as unsigned host integers.
      const auto signed19 = [](std::uint32_t v) {
        return static_cast<std::int32_t>(v & 0x3ffffU) - static_cast<std::int32_t>(v & 0x40000U);
      };
      wide_samples.push_back(signed19(chip.wide_left));
      wide_samples.push_back(signed19(chip.wide_right));
    }
    chip.clk = 1;
    chip.eval();
    ++cycle;
  }

  void ready() {
    const auto limit = cycle + 128;
    while ((chip.dout & 128U) != 0U) {
      if (cycle == limit) {
        throw std::runtime_error("JT51 busy timeout");
      }
      tick();
    }
  }

  void write(const Event& event) {
    ready();
    chip.a0 = 0;
    chip.din = event.address;
    chip.wr_n = 0;
    tick();
    chip.wr_n = 1;
    tick();
    ready();
    chip.a0 = 1;
    chip.din = event.value;
    chip.wr_n = 0;
    // Keep data asserted through cen_p1: a cen-only write can miss busy.
    do {
      tick();
    } while (!chip.cen_p1);
    chip.wr_n = 1;
    tick();
    if ((chip.dout & 128U) == 0U) {
      throw std::runtime_error("JT51 did not latch busy");
    }
  }
};
} // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 5) {
      throw std::runtime_error("usage: replay events.bin frame-count output.s16 wide.s32");
    }
    const auto frames = std::stoull(argv[2]);
    if (frames == 0 || frames > 62500 * 21) {
      throw std::runtime_error("frame count outside probe bound");
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
      throw std::runtime_error("cannot open events");
    }
    std::vector<Event> events;
    Event event{};
    while (read(input, event.sample)) {
      if (!read(input, event.address) || !read(input, event.value) || event.address > 255 ||
          event.value > 255 || event.sample > frames || events.size() >= 131072 ||
          (!events.empty() && event.sample < events.back().sample)) {
        throw std::runtime_error("invalid event record");
      }
      events.push_back(event);
    }
    if (input.gcount() != 0 || !input.eof()) {
      throw std::runtime_error("truncated event timestamp");
    }
    Replay replay;
    std::uint64_t worst = 0;
    std::uint64_t runtime_worst = 0;
    std::uint64_t writes = 0;
    for (const auto& op : events) {
      // Events exactly at the prefix end belong to the following audio.
      if (op.sample == frames) {
        break;
      }
      const auto due = op.sample * 64;
      while (replay.cycle < due) {
        replay.tick();
      }
      replay.write(op);
      worst = std::max(worst, replay.cycle - due);
      if (op.sample != 0) {
        runtime_worst = std::max(runtime_worst, replay.cycle - due);
      }
      ++writes;
    }
    while (replay.samples.size() < frames * 2) {
      replay.tick();
    }
    std::ofstream output(argv[3], std::ios::binary);
    output.write(reinterpret_cast<const char*>(replay.samples.data()),
                 static_cast<std::streamsize>(frames * 4));
    if (!output) {
      throw std::runtime_error("audio write failed");
    }
    std::ofstream wide_output(argv[4], std::ios::binary);
    wide_output.write(reinterpret_cast<const char*>(replay.wide_samples.data()),
                      static_cast<std::streamsize>(frames * 8));
    if (!wide_output) {
      throw std::runtime_error("wide audio write failed");
    }
    std::printf("{\"writes\":%llu,\"frames\":%llu,\"max_bus_delay_cycles\":%llu,"
                "\"max_bus_delay_us\":%.2f,\"max_runtime_bus_delay_us\":%.2f}\n",
                static_cast<unsigned long long>(writes), static_cast<unsigned long long>(frames),
                static_cast<unsigned long long>(worst), static_cast<double>(worst) / 4.0,
                static_cast<double>(runtime_worst) / 4.0);
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
