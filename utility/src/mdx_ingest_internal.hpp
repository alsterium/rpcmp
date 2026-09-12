#ifndef RPCMP_UTILITY_MDX_INGEST_INTERNAL_HPP
#define RPCMP_UTILITY_MDX_INGEST_INTERNAL_HPP

#include "rpcmp/utility/mdx_ingest.hpp"

namespace rpcmp::utility::detail {
[[nodiscard]] bool is_mdx_filename(std::string_view filename);
// Shared admission/title preparation without serializing an intermediate file.
[[nodiscard]] MdxIngestResult prepare_mdx_metadata(runtime::mdx::ByteView source,
                                                   const MdxSourceMetadata& metadata,
                                                   MdxIngestWorkspace& workspace,
                                                   MdxIngestMetadata& output);
} // namespace rpcmp::utility::detail

#endif // RPCMP_UTILITY_MDX_INGEST_INTERNAL_HPP
