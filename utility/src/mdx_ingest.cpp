#include "rpcmp/utility/mdx_ingest.hpp"

#include <utility>

namespace rpcmp::utility {
namespace {

MdxIngestResult admit(const runtime::mdx::ByteView source, MdxIngestWorkspace& workspace) {
  MdxIngestResult result;
  workspace.document = {};
  result.parse = runtime::mdx::parse(source, workspace.document);
  if (!result.parse.ok()) {
    result.error = MdxIngestError::Parse;
    return result;
  }

  result.admission = runtime::mdx::prepare_mdx_playback(workspace.document, workspace.validation,
                                                        workspace.engine);
  if (!result.admission.ok()) {
    result.error = MdxIngestError::Admission;
  }
  return result;
}

MdxIngestResult write(const runtime::mdx::ByteView source, const MdxIngestMetadata& metadata,
                      MdxIngestResult result) {
  NormalizedLibrary normalized;
  normalized.blobs.push_back(
      {library::kMdxFourcc, std::vector<std::uint8_t>(source.data, source.data + source.size)});
  NormalizedTrack track;
  track.format = library::kMdxFourcc;
  track.primary_blob_index = 0;
  track.title = metadata.title;
  track.artist = metadata.artist;
  normalized.tracks.push_back(std::move(track));

  auto written = write_rpcmlib(normalized);
  result.writer = written.error;
  if (!written.ok()) {
    result.error = MdxIngestError::Write;
    return result;
  }
  result.library_bytes = std::move(written.bytes);
  return result;
}

} // namespace

MdxIngestResult ingest_single_mdx(const runtime::mdx::ByteView source,
                                  const MdxIngestMetadata& metadata,
                                  MdxIngestWorkspace& workspace) {
  auto result = admit(source, workspace);
  if (!result.ok())
    return result;
  return write(source, metadata, std::move(result));
}

MdxIngestResult ingest_mdx(const runtime::mdx::ByteView source, const MdxSourceMetadata& metadata,
                           MdxIngestWorkspace& workspace) {
  auto result = admit(source, workspace);
  if (!result.ok())
    return result;
  MdxTitleResult selected;
  if (metadata.title_override) {
    selected.title = normalize_utf8(*metadata.title_override);
  } else {
    const auto raw = workspace.document.original_title;
    const std::string_view title =
        raw.size == 0 ? std::string_view{}
                      : std::string_view{reinterpret_cast<const char*>(raw.data), raw.size};
    selected = select_mdx_title(title, metadata.filename_stem);
  }
  result.title_fallback = selected.fallback;
  if (!selected.title.ok()) {
    result.error = MdxIngestError::Metadata;
    result.metadata_error = selected.title.error;
    return result;
  }
  const auto artist = normalize_utf8(metadata.artist);
  if (!artist.ok()) {
    result.error = MdxIngestError::Metadata;
    result.metadata_error = artist.error;
    return result;
  }
  return write(source, {selected.title.text, artist.text}, std::move(result));
}

} // namespace rpcmp::utility
