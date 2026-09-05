#include "rpcmp/runtime/mdx_engine.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

// Local research only: stdout contains aggregate categories, not identities.
// An explicitly requested candidate copy is a local, non-redistributable artifact.
int main(const int argc, char** const argv) {
  if (argc != 2 && argc != 3) {
    std::cerr << "usage: rpcmp_mdx_admission <corpus-directory> [new-local-candidate.mdx]\n";
    return 1;
  }
  std::error_code error;
  if (argc == 3 && (std::filesystem::exists(argv[2], error) || error)) {
    std::cerr << "candidate output already exists or cannot be inspected\n";
    return 1;
  }
  std::filesystem::recursive_directory_iterator entry(argv[1], error), end;
  if (error) {
    std::cerr << "unable to open corpus directory\n";
    return 1;
  }
  std::vector<std::filesystem::path> paths;
  for (; entry != end; entry.increment(error)) {
    if (error) {
      std::cerr << "unable to enumerate corpus\n";
      return 1;
    }
    auto extension = entry->path().extension().native();
    using PathChar = std::filesystem::path::value_type;
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const PathChar value) {
      return value >= 'A' && value <= 'Z' ? static_cast<PathChar>(value - 'A' + 'a') : value;
    });
    if (extension == std::filesystem::path(".mdx").native() && entry->is_regular_file(error)) {
      paths.push_back(entry->path());
    }
    if (error) {
      std::cerr << "unable to inspect corpus entry\n";
      return 1;
    }
  }
  if (error) {
    std::cerr << "unable to finish corpus enumeration\n";
    return 1;
  }
  std::sort(paths.begin(), paths.end());
  std::map<std::tuple<unsigned, unsigned, unsigned>, std::uint64_t> counts;
  static rpcmp::runtime::mdx::MdxDocument document;
  static rpcmp::runtime::mdx::DocumentValidation validation;
  static rpcmp::runtime::mdx::MdxEngineScratch scratch;
  std::uint64_t accepted{};
  for (const auto& path : paths) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const auto size = input.tellg();
    if (!input || size < 0 ||
        size > static_cast<std::streamoff>(rpcmp::runtime::mdx::kMdxMaxInputBytes)) {
      std::cerr << "unable to read bounded corpus file\n";
      return 1;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    if (!input || !input.read(reinterpret_cast<char*>(bytes.data()),
                              static_cast<std::streamsize>(bytes.size()))) {
      std::cerr << "unable to read corpus file\n";
      return 1;
    }
    const auto parsed = rpcmp::runtime::mdx::parse({bytes.data(), bytes.size()}, document);
    if (!parsed.ok()) {
      ++counts[{static_cast<unsigned>(parsed.error), 0, 256}];
      continue;
    }
    const auto admitted = rpcmp::runtime::mdx::prepare_mdx_playback(document, validation, scratch);
    if (!admitted.ok()) {
      const unsigned opcode =
          admitted.byte_offset < bytes.size() ? bytes[admitted.byte_offset] : 256;
      ++counts[{0, static_cast<unsigned>(admitted.error), opcode}];
      continue;
    }
    if (accepted == 0 && argc == 3) {
      std::ofstream output(argv[2], std::ios::binary);
      if (!output || !output.write(reinterpret_cast<const char*>(bytes.data()),
                                   static_cast<std::streamsize>(bytes.size()))) {
        std::cerr << "unable to write local candidate\n";
        return 1;
      }
      output.close();
      if (!output) {
        std::cerr << "unable to close local candidate\n";
        return 1;
      }
    }
    ++accepted;
  }
  std::cout << "files=" << paths.size() << " accepted=" << accepted << '\n';
  std::cout << "parse,admission,byte_or_256,count\n";
  for (const auto& item : counts) {
    std::cout << std::get<0>(item.first) << ',' << std::get<1>(item.first) << ','
              << std::get<2>(item.first) << ',' << item.second << '\n';
  }
  return 0;
}
