#include "rpcmp/utility/mdx_ingest.hpp"

#include <utility>

namespace rpcmp::utility {

MdxIngestResult ingest_single_mdx(const runtime::mdx::ByteView source,
                                  const MdxIngestMetadata& metadata,
                                  MdxIngestWorkspace& workspace) {
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
    return result;
  }

  NormalizedLibrary normalized;
  normalized.blobs.push_back(
      {kMdxFourcc, std::vector<std::uint8_t>(source.data, source.data + source.size)});
  NormalizedTrack track;
  track.format = kMdxFourcc;
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

} // namespace rpcmp::utility
