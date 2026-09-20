#include "rpcmp/player/mdx_library_session.hpp"

namespace rpcmp::player {

MdxSessionResult prepare_mdx_library_session(const library::LogicalLibrary& library,
                                             const contracts::TrackId track_id,
                                             MdxLibrarySession& output,
                                             MdxLibrarySessionWorkspace& workspace) noexcept {
  return prepare_mdx_library_session(library, track_id, output, workspace.document,
                                     workspace.validation, workspace.engine);
}

MdxSessionResult prepare_mdx_library_session(const library::LogicalLibrary& library,
                                             const contracts::TrackId track_id,
                                             MdxLibrarySession& output,
                                             runtime::mdx::MdxDocument& document,
                                             runtime::mdx::DocumentValidation& validation,
                                             runtime::mdx::MdxEngineScratch& engine) noexcept {
  MdxSessionResult result;
  library::TrackView track;
  if (!library.find_track(track_id, track)) {
    result.error = MdxSessionError::TrackNotFound;
    return result;
  }
  if (track.format != library::kMdxFourcc) {
    result.error = MdxSessionError::UnsupportedFormat;
    return result;
  }

  library::BlobView blob;
  if (!library.find_blob(track.primary_blob_id, blob)) {
    result.error = MdxSessionError::BlobNotFound;
    return result;
  }
  if (blob.kind != library::kMdxFourcc) {
    result.error = MdxSessionError::UnexpectedBlobKind;
    return result;
  }

  document = {};
  result.parse = runtime::mdx::parse({blob.bytes.data, blob.bytes.size}, document);
  if (!result.parse.ok()) {
    result.error = MdxSessionError::Parse;
    return result;
  }
  result.admission = runtime::mdx::prepare_mdx_playback(document, validation, engine);
  if (!result.admission.ok()) {
    result.error = MdxSessionError::Admission;
    return result;
  }

  output = {track.track_id, track.primary_blob_id, document, validation, {}};
  return result;
}

} // namespace rpcmp::player
