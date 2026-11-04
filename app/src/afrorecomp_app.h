// afrorecomp - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <array>

#include <rex/rex_app.h>

#include "language_picker.h"
#include "frame_pacing.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef AFRO_VERSION
#define AFRO_VERSION "dev"
#endif

class AfrorecompApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<AfrorecompApp>(new AfrorecompApp(ctx, "afrorecomp",
        PPCImageConfig));
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  void OnPreSetup(rex::RuntimeConfig& config) override {
    afro::ApplyStartupLanguage();
  }
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    (void)drawer;
    if (window()) window()->SetTitle("afrorecomp_v" AFRO_VERSION);
    SetGuestFrameStats(afro::GuestFrameStats);  // F3 overlay
#ifdef _WIN32
    // Title bar / taskbar icon from the exe's icon resource (id 1).
    HICON big = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
        GetSystemMetrics(SM_CYICON), 0));
    HICON small_ = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON), 0));
    if (big || small_) {
      EnumThreadWindows(GetCurrentThreadId(), [](HWND w, LPARAM p) -> BOOL {
        auto* icons = reinterpret_cast<HICON*>(p);
        if (icons[0]) SendMessageW(w, WM_SETICON, ICON_BIG, (LPARAM)icons[0]);
        if (icons[1]) SendMessageW(w, WM_SETICON, ICON_SMALL, (LPARAM)icons[1]);
        return TRUE;
      }, reinterpret_cast<LPARAM>(std::array<HICON, 2>{big, small_}.data()));
    }
#endif
  }
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}
};
