#include "rpcmp/utility/album_native.hpp"

#include "mdx_ingest_internal.hpp"

#include <cerrno>
#include <fstream>
#include <system_error>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace rpcmp::utility {
namespace {
namespace fs = std::filesystem;
#ifdef _WIN32
using FileHandle = HANDLE;
constexpr FileHandle kNoFile = nullptr;
#else
using FileHandle = int;
constexpr FileHandle kNoFile = -1;
#endif

TextResult native_text(const fs::path& path) {
#ifdef _WIN32
  const auto native = path.generic_wstring();
  if (native.size() > kMetadataMaxBytes)
    return {TextError::SizeLimit, {}};
  std::u16string text;
  text.reserve(native.size());
  for (const auto value : native)
    text.push_back(static_cast<char16_t>(value));
  return utf16_to_utf8(text);
#else
  const auto native = path.generic_string();
  // Keep raw bytes in the manifest; NFC normalization belongs to the builder.
  if (native.size() > kMetadataMaxBytes)
    return {TextError::SizeLimit, {}};
  return {TextError::None, native};
#endif
}

bool entry_kind(const fs::path& path, AlbumEntryKind& kind, std::uint64_t* size = nullptr) {
#ifdef _WIN32
  WIN32_FILE_ATTRIBUTE_DATA attributes{};
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes))
    return false;
  kind = (attributes.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ? AlbumEntryKind::Link
         : (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? AlbumEntryKind::Directory
         : (attributes.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) != 0    ? AlbumEntryKind::Other
                                                                         : AlbumEntryKind::File;
  if (size != nullptr && kind == AlbumEntryKind::File)
    *size = (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32U) | attributes.nFileSizeLow;
#else
  std::error_code error;
  const auto status = fs::symlink_status(path, error);
  if (error)
    return false;
  kind = fs::is_symlink(status)        ? AlbumEntryKind::Link
         : fs::is_directory(status)    ? AlbumEntryKind::Directory
         : fs::is_regular_file(status) ? AlbumEntryKind::File
                                       : AlbumEntryKind::Other;
  if (size != nullptr && kind == AlbumEntryKind::File) {
    *size = fs::file_size(path, error);
    if (error)
      return false;
  }
#endif
  return true;
}
bool valid_destination(const fs::path& path) {
  std::error_code error;
  const auto status = fs::symlink_status(path, error);
  if (status.type() == fs::file_type::not_found)
    return true;
  if (error)
    return false;
  AlbumEntryKind kind{};
  return entry_kind(path, kind) && kind == AlbumEntryKind::File;
}
bool close_file(FileHandle& handle) {
  if (handle == kNoFile)
    return true;
#ifdef _WIN32
  if (!CloseHandle(handle))
    return false;
  handle = kNoFile;
  return true;
#else
  const auto descriptor = handle;
  handle = kNoFile; // POSIX close errors are not safely retryable on the same fd.
  return ::close(descriptor) == 0;
#endif
}

} // namespace

AlbumScanResult NativeAlbumInput::scan(const std::filesystem::path& root) {
  NativeAlbumInput candidate;
  std::error_code error;
  candidate.root_ = fs::absolute(root, error).lexically_normal();
  if (error)
    return {AlbumIngestError::InvalidSource, {}};
  while (candidate.root_.has_relative_path() && candidate.root_.filename().empty())
    candidate.root_ = candidate.root_.parent_path();
  AlbumEntryKind root_kind{};
  if (!entry_kind(candidate.root_, root_kind) || root_kind != AlbumEntryKind::Directory)
    return {AlbumIngestError::InvalidSource, {}};
  auto name = native_text(candidate.root_.filename());
  if (!name.ok() || name.text.empty())
    return {name.error == TextError::SizeLimit ? AlbumIngestError::Capacity
                                               : AlbumIngestError::InvalidSource,
            {}};
  candidate.manifest_.root_name = std::move(name.text);
  std::size_t text_bytes = candidate.manifest_.root_name.size();
  fs::recursive_directory_iterator current(candidate.root_, fs::directory_options::none, error);
  const fs::recursive_directory_iterator end;
  while (!error && current != end) {
    if (candidate.manifest_.entries.size() == kAlbumMaxSourceEntries ||
        current.depth() >= static_cast<int>(kAlbumMaxSourceDepth))
      return {AlbumIngestError::Capacity, {}};
    const auto path = current->path();
    auto relative = native_text(path.lexically_relative(candidate.root_));
    if (!relative.ok() || relative.text.size() > kAlbumMaxSourceText - text_bytes)
      return {relative.error != TextError::None && relative.error != TextError::SizeLimit
                  ? AlbumIngestError::InvalidSource
                  : AlbumIngestError::Capacity,
              {}};
    text_bytes += relative.text.size();
    AlbumEntryKind kind{};
    std::uint64_t size = 0;
    if (!entry_kind(path, kind, &size))
      return {AlbumIngestError::Read, relative.text};
    if (kind != AlbumEntryKind::Directory)
      current.disable_recursion_pending();
    candidate.manifest_.entries.push_back({std::move(relative.text), size, kind});
    candidate.paths_.push_back(path);
    current.increment(error);
  }
  if (error)
    return {AlbumIngestError::Read, {}};
  root_ = std::move(candidate.root_);
  manifest_ = std::move(candidate.manifest_);
  paths_ = std::move(candidate.paths_);
  return {};
}

bool NativeAlbumInput::read_file(const std::size_t entry, std::uint8_t* const bytes,
                                 const std::size_t size) {
  if (entry >= paths_.size() || manifest_.entries[entry].kind != AlbumEntryKind::File ||
      size != manifest_.entries[entry].size || size > runtime::mdx::kMdxMaxInputBytes ||
      (size != 0 && bytes == nullptr))
    return false;
  AlbumEntryKind kind{};
  if (!entry_kind(root_, kind) || kind != AlbumEntryKind::Directory)
    return false;
  auto checked = root_;
  const auto relative = paths_[entry].lexically_relative(root_);
  for (const auto& component : relative) {
    checked /= component;
    if (!entry_kind(checked, kind) ||
        kind != (checked == paths_[entry] ? AlbumEntryKind::File : AlbumEntryKind::Directory))
      return false;
  }
  std::ifstream input(paths_[entry], std::ios::binary | std::ios::ate);
  if (!input || input.tellg() != static_cast<std::streamoff>(size))
    return false;
  input.seekg(0);
  if (size != 0 && !input.read(reinterpret_cast<char*>(bytes), static_cast<std::streamsize>(size)))
    return false;
  return input.peek() == std::char_traits<char>::eof() && !input.bad();
}

bool NativeAlbumInput::output_conflicts(const std::filesystem::path& destination) const {
  std::error_code error;
  const bool exists = fs::exists(destination, error);
  if (error)
    return true;
  if (!exists)
    return false;
  for (std::size_t i = 0; i < manifest_.entries.size(); ++i) {
    const auto& entry = manifest_.entries[i];
    const auto slash = entry.relative_path.find_last_of('/');
    const auto name = entry.relative_path.substr(slash == std::string::npos ? 0 : slash + 1);
    if (entry.kind == AlbumEntryKind::File && detail::is_mdx_filename(name)) {
      const bool same = fs::equivalent(destination, paths_[i], error);
      if (error || same)
        return true;
    }
  }
  return false;
}

NativeAlbumOutput::NativeAlbumOutput(std::filesystem::path destination)
    : destination_(std::move(destination)) {}
NativeAlbumOutput::~NativeAlbumOutput() {
  // Last resort for an interrupted caller; publish_album reports cleanup failure.
  static_cast<void>(discard());
}

bool NativeAlbumOutput::begin() {
  if (owned_ || handle_ != kNoFile || !valid_destination(destination_))
    return false;
#ifdef _WIN32
  const auto pid = GetCurrentProcessId();
#else
  const auto pid = ::getpid();
#endif
  for (std::uint32_t attempt = 0; attempt < 64; ++attempt) {
    auto name = destination_.filename();
    name += ".rpcmp-tmp-" + std::to_string(pid) + "-" + std::to_string(attempt);
    temporary_ = destination_.parent_path() / name;
#ifdef _WIN32
    const auto handle = CreateFileW(temporary_.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
      if (GetLastError() == ERROR_FILE_EXISTS || GetLastError() == ERROR_ALREADY_EXISTS)
        continue;
      return false;
    }
    handle_ = handle;
#else
    handle_ = ::open(temporary_.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (handle_ == kNoFile) {
      if (errno == EEXIST)
        continue;
      return false;
    }
#endif
    owned_ = true;
    return true;
  }
  return false;
}

bool NativeAlbumOutput::write(const library::ByteView bytes) {
  if (handle_ == kNoFile)
    return false;
  std::size_t offset = 0;
  while (offset < bytes.size) {
#ifdef _WIN32
    DWORD written = 0;
    if (!WriteFile(handle_, bytes.data + offset, static_cast<DWORD>(bytes.size - offset), &written,
                   nullptr) ||
        written == 0)
      return false;
#else
    const auto written = ::write(handle_, bytes.data + offset, bytes.size - offset);
    if (written < 0 && errno == EINTR)
      continue;
    if (written <= 0)
      return false;
#endif
    offset += static_cast<std::size_t>(written);
  }
  return true;
}

bool NativeAlbumOutput::finish() {
  if (handle_ == kNoFile)
    return false;
#ifdef _WIN32
  const bool flushed = FlushFileBuffers(handle_) != 0;
#else
  const bool flushed = ::fsync(handle_) == 0;
#endif
  const bool closed = close_file(handle_);
  return flushed && closed;
}

bool NativeAlbumOutput::publish() {
  if (!owned_ || handle_ != kNoFile || !valid_destination(destination_))
    return false;
#ifdef _WIN32
  if (!MoveFileExW(temporary_.c_str(), destination_.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    return false;
#else
  std::error_code error;
  fs::rename(temporary_, destination_, error);
  if (error)
    return false;
#endif
  owned_ = false;
  return true;
}

bool NativeAlbumOutput::discard() {
  if (!close_file(handle_))
    return false;
  if (!owned_)
    return true;
  std::error_code error;
  fs::remove(temporary_, error);
  if (error)
    return false;
  owned_ = false;
  return true;
}

} // namespace rpcmp::utility
