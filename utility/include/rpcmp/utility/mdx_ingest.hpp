#ifndef RPCMP_UTILITY_MDX_INGEST_HPP
#define RPCMP_UTILITY_MDX_INGEST_HPP

#include "rpcmp/library/formats.hpp"
#include "rpcmp/runtime/mdx_engine.hpp"
#include "rpcmp/utility/metadata.hpp"
#include "rpcmp/utility/writer.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rpcmp::utility {

struct MdxIngestMetadata {
  std::string title;
  std::string artist;
};

struct MdxSourceMetadata {
  std::optional<std::string> title_override;
  std::string filename_stem;
  std::string artist;
};

enum class MdxIngestError : std::uint8_t { None = 0, Parse, Admission, Write, Metadata };

struct MdxIngestWorkspace {
  runtime::mdx::MdxDocument document{};
  runtime::mdx::MdxEngineScratch engine{};
  runtime::mdx::DocumentValidation validation{};
};

struct MdxIngestResult {
  MdxIngestError error{MdxIngestError::None};
  runtime::mdx::ParseResult parse{};
  runtime::mdx::DecodeResult admission{};
  WriterError writer{WriterError::None};
  std::vector<std::uint8_t> library_bytes;
  TextError metadata_error{TextError::None};
  TitleFallback title_fallback{TitleFallback::None};

  [[nodiscard]] bool ok() const noexcept { return error == MdxIngestError::None; }
};

// The caller owns the large fixed workspace so its placement is explicit.
// Output bytes are populated only after complete structural and playback
// admission succeeds.
[[nodiscard]] MdxIngestResult ingest_single_mdx(runtime::mdx::ByteView source,
                                                const MdxIngestMetadata& metadata,
                                                MdxIngestWorkspace& workspace);

// Raw UTF-8 metadata is normalized under host-metadata-v1 after admission.
// Without an override, choose the embedded CP932 title or filename fallback.
[[nodiscard]] MdxIngestResult ingest_mdx(runtime::mdx::ByteView source,
                                         const MdxSourceMetadata& metadata,
                                         MdxIngestWorkspace& workspace);

} // namespace rpcmp::utility

#endif // RPCMP_UTILITY_MDX_INGEST_HPP
