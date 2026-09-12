#include "rpcmp/utility/album_ingest.hpp"

#include "mdx_ingest_internal.hpp"
#include "rpcmp/library/album_catalog.hpp"

#include <algorithm>
#include <map>
#include <string_view>
#include <utility>

namespace rpcmp::utility {
namespace {

int bytes_compare(const std::string_view a, const std::string_view b) {
  const auto length = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < length; ++i) {
    const auto x = static_cast<unsigned char>(a[i]);
    const auto y = static_cast<unsigned char>(b[i]);
    if (x != y)
      return x < y ? -1 : 1;
  }
  return a.size() == b.size() ? 0 : a.size() < b.size() ? -1 : 1;
}
bool digit(const char value) { return value >= '0' && value <= '9'; }
int natural_compare(const std::string_view a, const std::string_view b) {
  std::size_t i = 0;
  std::size_t j = 0;
  while (i < a.size() && j < b.size()) {
    const bool numeric = digit(a[i]);
    if (numeric != digit(b[j]))
      return static_cast<unsigned char>(a[i]) < static_cast<unsigned char>(b[j]) ? -1 : 1;
    auto end_a = i + 1;
    auto end_b = j + 1;
    while (end_a < a.size() && digit(a[end_a]) == numeric)
      ++end_a;
    while (end_b < b.size() && digit(b[end_b]) == numeric)
      ++end_b;
    auto start_a = i;
    auto start_b = j;
    if (numeric) {
      while (start_a + 1 < end_a && a[start_a] == '0')
        ++start_a;
      while (start_b + 1 < end_b && b[start_b] == '0')
        ++start_b;
      if (end_a - start_a != end_b - start_b)
        return end_a - start_a < end_b - start_b ? -1 : 1;
    }
    const auto order =
        bytes_compare(a.substr(start_a, end_a - start_a), b.substr(start_b, end_b - start_b));
    if (order != 0)
      return order;
    i = end_a;
    j = end_b;
  }
  if (i != a.size() || j != b.size())
    return i == a.size() ? -1 : 1;
  return bytes_compare(a, b);
}
struct Entry {
  std::size_t source_index{};
  std::string path;
  std::string folder;
  std::string filename;
  std::string stem;
};
} // namespace

bool detail::is_mdx_filename(const std::string_view filename) {
  const auto dot = filename.find_last_of('.');
  if (dot == std::string_view::npos || dot == 0 || filename.size() - dot != 4)
    return false;
  const auto lower = [](const char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
  return lower(filename[dot + 1]) == 'm' && lower(filename[dot + 2]) == 'd' &&
         lower(filename[dot + 3]) == 'x';
}
namespace {
bool capacity_error(const MdxIngestResult& result) {
  using runtime::mdx::MdxError;
  return result.metadata_error == TextError::SizeLimit ||
         result.parse.error == MdxError::InputTooLarge ||
         result.parse.error == MdxError::TitleTooLong ||
         result.parse.error == MdxError::PdxReferenceTooLong ||
         result.admission.error == runtime::mdx::DecodeError::BudgetExhausted;
}

} // namespace

AlbumIngestResult ingest_album_sources(const AlbumSourceManifest& source, AlbumReadPort& reader,
                                       MdxIngestWorkspace& workspace) {
  AlbumIngestResult result;
  const auto fail = [&](const AlbumIngestError error, const std::string& path) {
    result.error = error;
    // Reject oversized names without copying them again for diagnostics.
    if (path.size() <= kMetadataMaxBytes)
      result.error_path = path;
    std::sort(result.diagnostics.begin(), result.diagnostics.end(),
              [](const AlbumDiagnostic& a, const AlbumDiagnostic& b) { return a.path < b.path; });
    return std::move(result);
  };
  if (source.entries.size() > kAlbumMaxSourceEntries || source.root_name.size() > kMetadataMaxBytes)
    return fail(AlbumIngestError::Capacity, {});
  auto root = normalize_utf8(source.root_name);
  if (!root.ok() || root.text.empty())
    return fail(root.error == TextError::SizeLimit ? AlbumIngestError::Capacity
                                                   : AlbumIngestError::InvalidSource,
                {});
  std::size_t raw_bytes = source.root_name.size();
  std::size_t normalized_bytes = root.text.size();
  std::vector<Entry> entries;
  entries.reserve(source.entries.size());
  std::map<std::string, AlbumEntryKind> paths;
  for (std::size_t i = 0; i < source.entries.size(); ++i) {
    const auto& input = source.entries[i];
    if (input.kind > AlbumEntryKind::Other)
      return fail(AlbumIngestError::InvalidSource, input.relative_path);
    if (input.relative_path.size() > kMetadataMaxBytes ||
        input.relative_path.size() > kAlbumMaxSourceText - raw_bytes)
      return fail(AlbumIngestError::Capacity, input.relative_path);
    raw_bytes += input.relative_path.size();
    auto path = normalize_utf8(input.relative_path);
    if (!path.ok())
      return fail(path.error == TextError::SizeLimit ? AlbumIngestError::Capacity
                                                     : AlbumIngestError::InvalidSource,
                  input.relative_path);
    if (path.text.size() > kAlbumMaxSourceText - normalized_bytes)
      return fail(AlbumIngestError::Capacity, input.relative_path);
    normalized_bytes += path.text.size();
    if (path.text == "." || !library::valid_album_key({path.text.data(), path.text.size()}))
      return fail(AlbumIngestError::InvalidSource, input.relative_path);
    if (static_cast<std::size_t>(std::count(path.text.begin(), path.text.end(), '/')) >=
        kAlbumMaxSourceDepth)
      return fail(AlbumIngestError::Capacity, input.relative_path);
    if (!paths.emplace(path.text, input.kind).second)
      return fail(AlbumIngestError::NameCollision, path.text);
    const auto slash = path.text.find_last_of('/');
    Entry entry;
    entry.source_index = i;
    entry.folder = slash == std::string::npos ? "." : path.text.substr(0, slash);
    entry.filename = slash == std::string::npos ? path.text : path.text.substr(slash + 1);
    entry.stem = entry.filename.substr(0, entry.filename.find_last_of('.'));
    entry.path = std::move(path.text);
    entries.push_back(std::move(entry));
  }
  for (const auto& entry : entries) {
    const auto parent = paths.find(entry.folder);
    if (entry.folder != "." &&
        (parent == paths.end() || parent->second != AlbumEntryKind::Directory))
      return fail(AlbumIngestError::InvalidSource, entry.path);
  }
  std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
    const auto folder = natural_compare(a.folder, b.folder);
    if (folder != 0)
      return folder < 0;
    const auto stem = natural_compare(a.stem, b.stem);
    return stem != 0 ? stem < 0 : bytes_compare(a.filename, b.filename) < 0;
  });
  NormalizedLibrary normalized;
  std::vector<NormalizedAlbum> albums;
  std::vector<std::string> accepted_paths;
  std::uint64_t blob_bytes = 0;
  for (const auto& entry : entries) {
    const auto& input = source.entries[entry.source_index];
    if (input.kind == AlbumEntryKind::Directory)
      continue;
    AlbumDiagnostic diagnostic;
    diagnostic.path = entry.path;
    if (input.kind != AlbumEntryKind::File || !detail::is_mdx_filename(entry.filename)) {
      diagnostic.kind = input.kind == AlbumEntryKind::Link    ? AlbumDiagnosticKind::Link
                        : input.kind == AlbumEntryKind::Other ? AlbumDiagnosticKind::Other
                                                              : AlbumDiagnosticKind::NotMdx;
      ++result.skipped;
      result.diagnostics.push_back(std::move(diagnostic));
      continue;
    }
    if (input.size > runtime::mdx::kMdxMaxInputBytes)
      return fail(AlbumIngestError::Capacity, entry.path);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(input.size));
    if (!reader.read_file(entry.source_index, bytes.data(), bytes.size()))
      return fail(AlbumIngestError::Read, entry.path);
    MdxIngestMetadata metadata;
    const auto prepared = detail::prepare_mdx_metadata(
        {bytes.data(), bytes.size()}, {std::nullopt, entry.stem, {}}, workspace, metadata);
    diagnostic.stage = prepared.error;
    diagnostic.parse = prepared.parse;
    diagnostic.admission = prepared.admission;
    diagnostic.metadata = prepared.metadata_error;
    diagnostic.fallback = prepared.title_fallback;
    if (!prepared.ok()) {
      diagnostic.kind =
          capacity_error(prepared) ? AlbumDiagnosticKind::Error : AlbumDiagnosticKind::Excluded;
      result.diagnostics.push_back(std::move(diagnostic));
      if (capacity_error(prepared))
        return fail(AlbumIngestError::Capacity, entry.path);
      ++result.excluded;
      continue;
    }
    if (normalized.tracks.size() == library::kAlbumMaxTracks)
      return fail(AlbumIngestError::Capacity, entry.path);
    const auto slash = entry.folder.find_last_of('/');
    const auto album_name = entry.folder == "."          ? root.text
                            : slash == std::string::npos ? entry.folder
                                                         : entry.folder.substr(slash + 1);
    auto blob =
        std::find_if(normalized.blobs.begin(), normalized.blobs.end(),
                     [&](const NormalizedBlob& existing) { return existing.bytes == bytes; });
    const auto blob_index = static_cast<std::size_t>(blob - normalized.blobs.begin());
    if (blob == normalized.blobs.end()) {
      if (bytes.size() > library::kAlbumMaxFileBytes - blob_bytes)
        return fail(AlbumIngestError::Capacity, entry.path);
      blob_bytes += bytes.size();
      normalized.blobs.push_back({library::kMdxFourcc, std::move(bytes)});
    }
    for (std::size_t i = 0; i < normalized.tracks.size(); ++i) {
      const auto& previous = normalized.tracks[i];
      if (previous.primary_blob_index == blob_index && previous.title == metadata.title &&
          previous.album == album_name) {
        result.conflicting_path = accepted_paths[i];
        return fail(AlbumIngestError::DuplicateTrack, entry.path);
      }
    }
    if (albums.empty() || albums.back().folder_key != entry.folder)
      albums.push_back({entry.folder, album_name, {}});
    NormalizedTrack track;
    track.format = library::kMdxFourcc;
    track.primary_blob_index = blob_index;
    track.title = std::move(metadata.title);
    track.album = album_name;
    albums.back().track_indices.push_back(normalized.tracks.size());
    normalized.tracks.push_back(std::move(track));
    accepted_paths.push_back(entry.path);
    result.diagnostics.push_back(std::move(diagnostic));
    ++result.accepted;
  }
  if (normalized.tracks.empty())
    return fail(AlbumIngestError::NoTracks, {});
  auto written = write_album_rpcmlib(normalized, albums);
  if (!written.ok()) {
    result.writer = written.error;
    return fail(AlbumIngestError::Writer, {});
  }
  result.bytes = std::move(written.bytes);
  result.status =
      result.excluded == 0 ? AlbumIngestStatus::Complete : AlbumIngestStatus::WithExclusions;
  std::sort(result.diagnostics.begin(), result.diagnostics.end(),
            [](const AlbumDiagnostic& a, const AlbumDiagnostic& b) { return a.path < b.path; });
  return result;
}

AlbumOutputResult publish_album(const library::ByteView bytes, AlbumOutputPort& output) {
  if (bytes.data == nullptr || bytes.size == 0 || bytes.size > library::kAlbumMaxFileBytes)
    return {AlbumOutputError::InvalidSize};
  AlbumOutputError error = AlbumOutputError::None;
  if (!output.begin())
    error = AlbumOutputError::Begin;
  else if (!output.write(bytes))
    error = AlbumOutputError::Write;
  else if (!output.finish())
    error = AlbumOutputError::Finish;
  else if (!output.publish())
    error = AlbumOutputError::Publish;
  if (error != AlbumOutputError::None)
    return {error, !output.discard()};
  return {};
}

} // namespace rpcmp::utility
