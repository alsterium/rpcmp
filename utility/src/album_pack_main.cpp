#include "rpcmp/utility/album_native.hpp"

#include <iostream>
#include <string_view>

namespace {
using namespace rpcmp::utility;

std::string quoted_path(const std::string_view path) {
  constexpr char hex[] = "0123456789abcdef";
  const bool valid = normalize_utf8(path).ok();
  std::string output = "\"";
  for (std::size_t i = 0; i < path.size(); ++i) {
    const auto c = static_cast<unsigned char>(path[i]);
    if (valid && c == 0xc2 && i + 1 < path.size() &&
        static_cast<unsigned char>(path[i + 1]) >= 0x80 &&
        static_cast<unsigned char>(path[i + 1]) <= 0x9f) {
      const auto control = static_cast<unsigned char>(path[++i]);
      output += "\\u00";
      output += hex[control >> 4U];
      output += hex[control & 15U];
    } else if (c < 0x20 || c == 0x7f || (!valid && c >= 0x80)) {
      output += "\\x";
      output += hex[c >> 4U];
      output += hex[c & 15U];
    } else {
      if (c == '\\' || c == '"')
        output += '\\';
      output += static_cast<char>(c);
    }
  }
  return output + '"';
}
const char* error_name(const AlbumIngestError error) {
  switch (error) {
  case AlbumIngestError::None:
    return "none";
  case AlbumIngestError::InvalidSource:
    return "invalid-source";
  case AlbumIngestError::Capacity:
    return "capacity";
  case AlbumIngestError::NameCollision:
    return "name-collision";
  case AlbumIngestError::Read:
    return "read-failed";
  case AlbumIngestError::NoTracks:
    return "no-accepted-tracks";
  case AlbumIngestError::DuplicateTrack:
    return "duplicate-track";
  case AlbumIngestError::Writer:
    return "writer-failed";
  }
  return "unknown";
}
void report(const AlbumDiagnostic& diagnostic) {
  switch (diagnostic.kind) {
  case AlbumDiagnosticKind::Accepted:
    if (diagnostic.fallback == TitleFallback::None)
      return;
    std::cout << "title-fallback=" << title_fallback_name(diagnostic.fallback);
    break;
  case AlbumDiagnosticKind::NotMdx:
    std::cout << "skip=not-mdx";
    break;
  case AlbumDiagnosticKind::Link:
    std::cout << "skip=link";
    break;
  case AlbumDiagnosticKind::Other:
    std::cout << "skip=non-regular";
    break;
  case AlbumDiagnosticKind::Error:
  case AlbumDiagnosticKind::Excluded:
    std::cout << (diagnostic.kind == AlbumDiagnosticKind::Error ? "error=" : "exclude=");
    if (diagnostic.stage == MdxIngestError::Parse)
      std::cout << "malformed-mdx parse=" << static_cast<unsigned>(diagnostic.parse.error)
                << " offset=" << diagnostic.parse.byte_offset;
    else if (diagnostic.stage == MdxIngestError::Admission)
      std::cout << (diagnostic.admission.error == rpcmp::runtime::mdx::DecodeError::UnsupportedPcm
                        ? "pcm-not-supported"
                        : "playback-not-admitted")
                << " admission=" << static_cast<unsigned>(diagnostic.admission.error)
                << " offset=" << diagnostic.admission.byte_offset;
    else
      std::cout << "invalid-title metadata=" << static_cast<unsigned>(diagnostic.metadata);
    break;
  }
  std::cout << " path=" << quoted_path(diagnostic.path) << '\n';
}
int pack(const std::filesystem::path& root, const std::filesystem::path& destination) {
  NativeAlbumInput input;
  const auto scanned = input.scan(root);
  if (!scanned.ok()) {
    std::cerr << "scan=" << error_name(scanned.error) << " path=" << quoted_path(scanned.path)
              << '\n';
    return 1;
  }
  if (input.output_conflicts(destination)) {
    std::cerr << "output conflicts with an input MDX or cannot be checked\n";
    return 1;
  }
  static MdxIngestWorkspace workspace;
  const auto result = ingest_album_sources(input.manifest(), input, workspace);
  for (const auto& diagnostic : result.diagnostics)
    report(diagnostic);
  std::cout << "accepted=" << result.accepted << " excluded=" << result.excluded
            << " skipped=" << result.skipped << '\n';
  if (!result.ok()) {
    std::cerr << "error=" << error_name(result.error) << " path=" << quoted_path(result.error_path)
              << " conflicting=" << quoted_path(result.conflicting_path)
              << " writer=" << static_cast<unsigned>(result.writer) << '\n';
    return 1;
  }
  NativeAlbumOutput output(destination);
  const auto published = publish_album({result.bytes.data(), result.bytes.size()}, output);
  if (!published.ok()) {
    std::cerr << "output-error=" << static_cast<unsigned>(published.error)
              << " cleanup-failed=" << published.cleanup_failed << '\n';
    return 1;
  }
  std::cout << "rpcmlib bytes=" << result.bytes.size() << '\n';
  return result.status == AlbumIngestStatus::WithExclusions ? 2 : 0;
}
} // namespace

#ifdef _WIN32
int wmain(const int argc, wchar_t** const argv) {
#else
int main(const int argc, char** const argv) {
#endif
  if (argc != 3) {
    std::cerr << "usage: rpcmp_album_pack <input-folder> <output.rpcmlib>\n";
    return 1;
  }
  return pack(argv[1], argv[2]);
}
