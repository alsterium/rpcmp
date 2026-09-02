#ifndef RPCMP_UTILITY_MDX_INGEST_HPP
#define RPCMP_UTILITY_MDX_INGEST_HPP

#include "rpcmp/library/formats.hpp"
#include "rpcmp/runtime/mdx_engine.hpp"
#include "rpcmp/utility/writer.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace rpcmp::utility {

struct MdxIngestMetadata {
  std::string title;
  std::string artist;
};

enum class MdxIngestError : std::uint8_t { None = 0, Parse, Admission, Write };

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

  [[nodiscard]] bool ok() const noexcept { return error == MdxIngestError::None; }
};

// The caller owns the large fixed workspace so its placement is explicit.
// Output bytes are populated only after complete structural and playback
// admission succeeds.
[[nodiscard]] MdxIngestResult ingest_single_mdx(runtime::mdx::ByteView source,
                                                const MdxIngestMetadata& metadata,
                                                MdxIngestWorkspace& workspace);

} // namespace rpcmp::utility

#endif // RPCMP_UTILITY_MDX_INGEST_HPP
