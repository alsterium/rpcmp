#include "rpcmp/utility/mdx_ingest.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

int fail(const char* const message) {
  std::cerr << message << '\n';
  return 1;
}

constexpr auto kUsage = "usage: rpcmp_mdx_pack <input.mdx> <output.rpcmlib> [utf8-title]";

#ifdef _WIN32
rpcmp::utility::TextResult native_text(const std::wstring_view input) {
  if (input.size() > rpcmp::utility::kMetadataMaxBytes)
    return {rpcmp::utility::TextError::SizeLimit, {}};
  std::u16string utf16;
  utf16.reserve(input.size());
  for (const auto value : input)
    utf16.push_back(static_cast<char16_t>(value));
  return rpcmp::utility::utf16_to_utf8(utf16);
}
#endif

int pack(const std::filesystem::path& input_path, const std::filesystem::path& output_path,
         std::optional<std::string> title) {
  std::ifstream input(input_path, std::ios::binary | std::ios::ate);
  if (!input) {
    return fail("unable to open MDX input");
  }
  const auto size = input.tellg();
  if (size <= 0 || size > static_cast<std::streamoff>(rpcmp::runtime::mdx::kMdxMaxInputBytes))
    return fail("MDX input size outside 1..1048576 bytes");
  std::vector<std::uint8_t> source(static_cast<std::size_t>(size));
  input.seekg(0);
  if (!input.read(reinterpret_cast<char*>(source.data()),
                  static_cast<std::streamsize>(source.size())) ||
      input.peek() != std::char_traits<char>::eof() || input.bad())
    return fail("unable to read stable MDX input");
  input.close();

  rpcmp::utility::MdxSourceMetadata metadata;
  metadata.title_override = std::move(title);
  if (!metadata.title_override) {
#ifdef _WIN32
    auto stem = native_text(input_path.stem().native());
    if (!stem.ok())
      return fail("invalid filename metadata");
    metadata.filename_stem = std::move(stem.text);
#else
    metadata.filename_stem = input_path.stem().native();
#endif
  }

  static rpcmp::utility::MdxIngestWorkspace workspace{};
  const auto result =
      rpcmp::utility::ingest_mdx({source.data(), source.size()}, metadata, workspace);
  if (!result.ok()) {
    std::cerr << "MDX rejected: stage=" << static_cast<unsigned>(result.error)
              << " parse=" << static_cast<unsigned>(result.parse.error)
              << " admission=" << static_cast<unsigned>(result.admission.error)
              << " writer=" << static_cast<unsigned>(result.writer)
              << " metadata=" << static_cast<unsigned>(result.metadata_error) << '\n';
    return 2;
  }

  std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
  if (!output || !output.write(reinterpret_cast<const char*>(result.library_bytes.data()),
                               static_cast<std::streamsize>(result.library_bytes.size()))) {
    return fail("unable to write rpcmlib output");
  }
  output.close();
  if (!output)
    return fail("unable to close rpcmlib output");
  if (result.title_fallback != rpcmp::utility::TitleFallback::None)
    std::cerr << "title fallback=" << rpcmp::utility::title_fallback_name(result.title_fallback)
              << '\n';
  std::cout << "rpcmlib bytes=" << result.library_bytes.size() << '\n';
  return 0;
}

} // namespace

#ifdef _WIN32
int wmain(const int argc, wchar_t** const argv) {
  if (argc != 3 && argc != 4)
    return fail(kUsage);
  std::optional<std::string> title;
  if (argc == 4) {
    auto converted = native_text(argv[3]);
    if (!converted.ok())
      return fail("invalid title argument encoding or size");
    title = std::move(converted.text);
  }
  return pack(argv[1], argv[2], std::move(title));
}
#else
int main(const int argc, char** const argv) {
  if (argc != 3 && argc != 4)
    return fail(kUsage);
  std::optional<std::string> title;
  if (argc == 4)
    title = argv[3];
  return pack(argv[1], argv[2], std::move(title));
}
#endif
