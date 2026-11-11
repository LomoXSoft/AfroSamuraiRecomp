// In-game options menu (F1, or hold both stick clicks): the launcher's main
// settings while playing, drawn over the paused game with ImGui.
#pragma once

#include <functional>

struct ImFontAtlas;
namespace rex::ui {
class ImGuiDrawer;
class ImmediateDrawer;
}

namespace afro {

// Fonts for the menu; call from OnConfigureFonts (after the drawer's own font).
void MenuAddFonts(ImFontAtlas* atlas);
// From OnCreateDialogs. immediate loads the button icons; post_ui runs a
// function on the UI thread.
void MenuInit(rex::ui::ImGuiDrawer* drawer, rex::ui::ImmediateDrawer* immediate,
              std::function<void(std::function<void()>)> post_ui);
// UI thread: open or close the menu.
void MenuToggle();
// Guest thread, once per frame (the present hook): opens the menu on the
// controller combination and keeps the game paused while it is open.
void MenuGuestFrame();
// App shutdown: release a paused game.
void MenuShutdown();

}  // namespace afro
