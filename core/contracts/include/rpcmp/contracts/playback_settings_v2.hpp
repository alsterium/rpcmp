#ifndef RPCMP_CONTRACTS_PLAYBACK_SETTINGS_V2_HPP
#define RPCMP_CONTRACTS_PLAYBACK_SETTINGS_V2_HPP

#include <cstdint>
#include <optional>

namespace rpcmp::contracts::v2 {
inline constexpr std::uint64_t kPlaybackSettings = 1ULL << 7U;
enum class SettingsRestore : std::uint8_t {
  Reading,
  Restored,
  Missing,
  Recovered,
  Invalid,
  Conflict,
  Unsupported,
  IoError,
  TimedOut
};
enum class SettingsSave : std::uint8_t { NotSaved, Pending, Writing, Saved, Failed, Unavailable };
enum class SettingsError : std::uint8_t {
  Io = 1,
  Unsupported,
  InvalidRecord,
  Conflict,
  Timeout,
  Protocol,
  SequenceExhausted,
  RequestExhausted,
  Clock,
  InvalidConfiguration,
  NotDurable,
  ReadbackMismatch,
  UnsupportedBackend,
  NotQuiescent
};
struct PlaybackSettingsObservation {
  std::uint64_t policy_revision{1};
  std::optional<std::uint64_t> persisted_revision;
  SettingsRestore restore{SettingsRestore::Reading};
  SettingsSave save{SettingsSave::NotSaved};
  std::optional<SettingsError> error;
};
[[nodiscard]] constexpr bool
valid_settings_observation(const PlaybackSettingsObservation& s) noexcept {
  return s.policy_revision != 0 &&
         static_cast<std::uint8_t>(s.restore) <=
             static_cast<std::uint8_t>(SettingsRestore::TimedOut) &&
         static_cast<std::uint8_t>(s.save) <=
             static_cast<std::uint8_t>(SettingsSave::Unavailable) &&
         (!s.persisted_revision ||
          (*s.persisted_revision != 0 && *s.persisted_revision <= s.policy_revision)) &&
         (s.save != SettingsSave::Saved || s.persisted_revision == s.policy_revision) &&
         (s.save == SettingsSave::Failed || s.save == SettingsSave::Unavailable) ==
             s.error.has_value() &&
         (!s.error || (static_cast<std::uint8_t>(*s.error) >= 1 &&
                       static_cast<std::uint8_t>(*s.error) <=
                           static_cast<std::uint8_t>(SettingsError::NotQuiescent))) &&
         (s.restore != SettingsRestore::Reading ||
          (!s.persisted_revision && s.save == SettingsSave::NotSaved));
}
} // namespace rpcmp::contracts::v2

#endif // RPCMP_CONTRACTS_PLAYBACK_SETTINGS_V2_HPP
