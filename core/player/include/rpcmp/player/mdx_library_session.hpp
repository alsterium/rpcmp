#ifndef RPCMP_PLAYER_MDX_LIBRARY_SESSION_HPP
#define RPCMP_PLAYER_MDX_LIBRARY_SESSION_HPP

#include "rpcmp/library/formats.hpp"
#include "rpcmp/library/logical_library.hpp"
#include "rpcmp/runtime/mdx_engine.hpp"

#include <cstdint>

namespace rpcmp::player {

enum class MdxSessionError : std::uint8_t {
  None = 0,
  TrackNotFound,
  UnsupportedFormat,
  BlobNotFound,
  UnexpectedBlobKind,
  Parse,
  Admission,
};

struct MdxLibrarySession {
  contracts::TrackId track_id{};
  contracts::BlobId primary_blob_id{};
  runtime::mdx::MdxDocument document{};
  runtime::mdx::DocumentValidation validation{};
  runtime::mdx::MdxEngineState state{};
};

struct MdxLibrarySessionWorkspace {
  runtime::mdx::MdxDocument document{};
  runtime::mdx::DocumentValidation validation{};
  runtime::mdx::MdxEngineScratch engine{};
};

struct MdxSessionResult {
  MdxSessionError error{MdxSessionError::None};
  runtime::mdx::ParseResult parse{};
  runtime::mdx::DecodeResult admission{};

  [[nodiscard]] bool ok() const noexcept { return error == MdxSessionError::None; }
};

// The library bytes must remain alive and immutable for the session lifetime.
// The destination commits only after track/blob resolution, parse, and complete
// playback admission all succeed.
[[nodiscard]] MdxSessionResult
prepare_mdx_library_session(const library::LogicalLibrary& library, contracts::TrackId track_id,
                            MdxLibrarySession& output,
                            MdxLibrarySessionWorkspace& workspace) noexcept;

} // namespace rpcmp::player

#endif // RPCMP_PLAYER_MDX_LIBRARY_SESSION_HPP
