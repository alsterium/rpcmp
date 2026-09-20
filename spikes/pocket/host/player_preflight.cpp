#include "rpcmp/library/album_catalog.hpp"
#include "rpcmp/player/mdx_library_session.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {
int check(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  const auto length = input.tellg();
  if (!input || length <= 0 ||
      length > static_cast<std::streamoff>(rpcmp::library::kAlbumMaxFileBytes))
    return 1;
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
  input.seekg(0, std::ios::beg);
  if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length)))
    return 1;
  rpcmp::library::AlbumCatalog catalog;
  if (!rpcmp::library::AlbumCatalog::open({bytes.data(), bytes.size()}, catalog).ok() ||
      catalog.album_count() == 0 || catalog.track_count() == 0)
    return 1;
  static rpcmp::player::MdxLibrarySession session;
  static rpcmp::player::MdxLibrarySessionWorkspace workspace;
  for (std::uint32_t a = 0; a < catalog.album_count(); ++a) {
    rpcmp::library::AlbumView album;
    if (!catalog.album_at(a, album))
      return 1;
    for (std::uint32_t t = 0; t < album.track_count; ++t) {
      rpcmp::library::TrackView track;
      if (!catalog.track_at(album.album_id, t, track) ||
          !rpcmp::player::prepare_mdx_library_session(catalog.library(), track.track_id, session,
                                                      workspace)
               .ok())
        return 1;
    }
  }
  // Aggregate evidence only: never leak local music identities into build logs.
  std::cout << "albums=" << catalog.album_count() << " tracks=" << catalog.track_count()
            << "\nresult=PASS\n";
  return 0;
}
} // namespace

#ifdef _WIN32
int wmain(const int argc, wchar_t** const argv) {
#else
int main(const int argc, char** const argv) {
#endif
  if (argc != 2) {
    std::cerr << "usage: rpcmp_player_preflight <music.rpcmlib>\n";
    return 1;
  }
  const auto result = check(argv[1]);
  if (result != 0)
    std::cerr << "M6 catalog/MDX preflight failed\n";
  return result;
}
