#ifndef RPCMP_CONTRACTS_CATALOG_HPP
#define RPCMP_CONTRACTS_CATALOG_HPP

#include <cstdint>

namespace rpcmp::contracts {

struct AlbumId {
  std::uint64_t value{};
};
constexpr bool operator==(AlbumId left, AlbumId right) noexcept {
  return left.value == right.value;
}
constexpr bool operator!=(AlbumId left, AlbumId right) noexcept { return !(left == right); }

} // namespace rpcmp::contracts

#endif // RPCMP_CONTRACTS_CATALOG_HPP
