#include "rpcmp/utility/mdx_ingest.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

int fail(const char* const message) {
  std::cerr << message << '\n';
  return 1;
}

} // namespace

int main(const int argc, char** const argv) {
  if (argc != 4) {
    return fail("usage: rpcmp_mdx_pack <input.mdx> <output.rpcmlib> <utf8-title>");
  }

  std::ifstream input(argv[1], std::ios::binary);
  if (!input) {
    return fail("unable to open MDX input");
  }
  const std::vector<std::uint8_t> source{std::istreambuf_iterator<char>(input), {}};
  if (input.bad() || source.empty()) {
    return fail("unable to read MDX input");
  }

  static rpcmp::utility::MdxIngestWorkspace workspace{};
  const auto result =
      rpcmp::utility::ingest_single_mdx({source.data(), source.size()}, {argv[3], ""}, workspace);
  if (!result.ok()) {
    std::cerr << "MDX rejected: stage=" << static_cast<unsigned>(result.error)
              << " parse=" << static_cast<unsigned>(result.parse.error)
              << " admission=" << static_cast<unsigned>(result.admission.error)
              << " writer=" << static_cast<unsigned>(result.writer) << '\n';
    return 2;
  }

  std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
  if (!output || !output.write(reinterpret_cast<const char*>(result.library_bytes.data()),
                               static_cast<std::streamsize>(result.library_bytes.size()))) {
    return fail("unable to write rpcmlib output");
  }
  output.close();
  if (!output)
    return fail("unable to close rpcmlib output");
  std::cout << "rpcmlib bytes=" << result.library_bytes.size() << '\n';
  return 0;
}
