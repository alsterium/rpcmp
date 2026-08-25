#ifndef RPCMP_UI_TEXT_UI_HPP
#define RPCMP_UI_TEXT_UI_HPP

#include "rpcmp/contracts/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rpcmp::ui {

enum class UiView : std::uint8_t { Overview, Channels };
enum class UiAction : std::uint8_t { SwitchView, SelectNext, SelectPrevious };

struct UiState {
  UiView view{UiView::Overview};
  std::size_t selected_channel{};
  std::size_t scroll_offset{};
};

void apply_action(UiState& state, UiAction action, std::size_t channel_count) noexcept;
std::vector<std::string> render(const contracts::PlayerSnapshot& snapshot, const UiState& state);

} // namespace rpcmp::ui

#endif // RPCMP_UI_TEXT_UI_HPP
