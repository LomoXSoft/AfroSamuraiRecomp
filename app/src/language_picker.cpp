// Startup language picker.
// Config: afro_language.cfg next to the executable ("language=N", "ask=0/1").
// The dialog is shown when there is no config, when ask=1, when Shift is held
// at launch, or with --choose_language. An explicit --user_language wins.
#include "language_picker.h"

#include <rex/cvar.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>
#include <initializer_list>

#ifndef AFRO_VERSION
#define AFRO_VERSION "dev"
#endif
#define AFRO_W2(x) L##x
#define AFRO_W(x) AFRO_W2(x)
#define AFRO_VERSION_W AFRO_W(AFRO_VERSION)

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

REXCVAR_DEFINE_BOOL(choose_language, false, "Afro",
                    "Show the language picker at startup");
REXCVAR_DEFINE_BOOL(advanced, false, "Afro",
                    "Show the advanced (diagnostic) options button in the launcher");

namespace afro {
namespace {

struct Lang { unsigned id; const wchar_t* label; };
// XLanguage values (rex/system/xcontent.h): 1 en, 4 fr, 5 es, 6 it.
const Lang kLangs[] = {{1, L"English"}, {5, L"Espa\u00f1ol"},
                       {4, L"Fran\u00e7ais"}, {6, L"Italiano"}};

std::filesystem::path ConfigPath() {
#ifdef _WIN32
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (n && n < MAX_PATH)
    return std::filesystem::path(buf).parent_path() / "afro_language.cfg";
#else
  std::error_code ec;
  auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
  if (!ec) return exe.parent_path() / "afro_language.cfg";
#endif
  return "afro_language.cfg";
}

bool Valid(unsigned id) {
  for (auto& l : kLangs) if (l.id == id) return true;
  return false;
}

// Launcher options (fps 0 = the game's own pacing).
// Advanced (diagnostic) options: each is a cvar; index 0 = leave the default.
struct AdvItem {
  const char* key;    // cfg key
  const char* cvar;
  const wchar_t* name;
  int n;              // number of choices
  const char* values[4];
  const wchar_t* labels[4];  // nullptr = use the localized Default/Off words
};
const AdvItem kAdv[] = {
    {"rt", "render_target_path_d3d12", L"Render target path", 3, {"", "rtv", "rov"},
     {L"Default (RTV)", L"D3D12 RTV", L"D3D12 ROV"}},
    {"readback", "readback_resolve", L"readback_resolve", 4, {"", "fast", "some", "full"},
     {L"none", L"fast", L"some", L"full"}},
    {"occlusion", "occlusion_query_enable", L"occlusion_query_enable", 2, {"", "false"}, {}},
    {"gamma16", "gamma_render_target_as_unorm16", L"gamma_render_target_as_unorm16", 2,
     {"", "false"}, {}},
    {"directresolve", "direct_host_resolve", L"direct_host_resolve", 2, {"", "false"}, {}},
    {"fx_dof", "afro_fx_dof", L"FX: depth of field", 2, {"", "false"}, {}},
    {"fx_bloom", "afro_fx_bloom", L"FX: bloom", 2, {"", "false"}, {}},
    {"fx_blood", "afro_fx_blood", L"FX: blood", 2, {"", "false"}, {}},
    {"fx_focus", "afro_fx_focus", L"FX: focus (slow-mo)", 2, {"", "false"}, {}},
    {"intfix", "afro_int_fetch_scale", L"GPU: integer fetch fix (d119505)", 2, {"", "false"}, {}},
    {"f24round", "depth_float24_round", L"depth_float24_round", 2, {"", "true"}, {L"Off", L"On"}},
    {"f24ps", "depth_float24_convert_in_pixel_shader", L"depth_float24_convert_in_pixel_shader", 2, {"", "true"}, {L"Off", L"On"}},
    {"msaa2x", "native_2x_msaa", L"native_2x_msaa", 2, {"", "false"}, {}},
    {"snorm16", "snorm16_render_target_full_range", L"snorm16_render_target_full_range", 2, {"", "false"}, {}},
    {"mrtclamp", "mrt_edram_used_range_clamp_to_min", L"mrt_edram_used_range_clamp_to_min", 2, {"", "false"}, {}},
    {"rtlog", "afro_rt_log", L"afro_rt_log (diagnostic, slow)", 2, {"", "true"}, {}},
    {"dumpshaders", "dump_shaders", L"Diagnostics: dump shaders", 2, {"", "shaderdump"}, {L"Off", L"On (shaderdump\\)"}},
};
constexpr int kAdvN = sizeof(kAdv) / sizeof(kAdv[0]);

struct Options { int fps = 0; int sepia = 0; /* 0 on, 1 off */ bool window = false; int scale = 1; int adv[kAdvN] = {}; };

// GPU-plugin cvars (rexgpu-xenos.dll) are registered only when the plugin is
// loaded, after the launcher runs, and SetFlagByName rejects unknown names.
// ReXGlue applies REX_<NAME> environment variables when such a late cvar
// registers, so fall back to that (command line still wins over it).
void SetCvar(const char* name, const std::string& value) {
  if (rex::cvar::SetFlagByName(name, value)) {
    std::fprintf(stderr, "[afro] cvar %s=%s (applied)\n", name, value.c_str());
    return;
  }
  std::string env = "REX_";
  for (const char* c = name; *c; ++c) env += static_cast<char>(std::toupper(static_cast<unsigned char>(*c)));
#ifdef _WIN32
  _putenv_s(env.c_str(), value.c_str());
#else
  setenv(env.c_str(), value.c_str(), 1);
#endif
  std::fprintf(stderr, "[afro] cvar %s=%s (deferred via %s)\n", name, value.c_str(), env.c_str());
}

bool Explicit(const char* name) {
  for (auto& f : rex::cvar::GetRegistry())
    if (f.name == name && f.source != rex::cvar::Source::kDefault) return true;
  return false;
}

#ifdef _WIN32
// Launcher window: options grouped so more can be added later (one row each).
struct UiText { const wchar_t *title, *lang, *remember, *hint, *play, *fps, *fps_orig, *disp, *full, *win, *scale, *adv, *def, *off, *ok, *on, *xbtn, *xwarn, *sepia, *sepia_on, *sepia_off; };
const UiText kText[] = {
    {L"afrorecomp_v" AFRO_VERSION_W L" - Launcher", L"Language:", L"Remember and don't ask again",
     L"Hold Shift at launch to show this window again.", L"Play",
     L"Frame rate:", L"Original (30 capped)", L"Display:", L"Fullscreen", L"Window", L"Internal resolution:", L"Advanced options...", L"Default", L"Off", L"OK", L"On",
     L"Advanced settings (experimental)...", L"Warning: these settings are experimental and not fully tested. The game runs at a fixed 30 FPS; the FPS cap may not behave as expected.",
     L"Sepia filter:", L"On (original)", L"Off"},
    {L"afrorecomp_v" AFRO_VERSION_W L" - Lanzador", L"Idioma:", L"Recordar y no volver a preguntar",
     L"Mant\u00e9n May\u00fas al iniciar para volver a ver esta ventana.", L"Jugar",
     L"Fotogramas:", L"Original (30 capados)", L"Pantalla:", L"Pantalla completa", L"Ventana", L"Resoluci\u00f3n interna:", L"Opciones avanzadas...", L"Por defecto", L"Desactivado", L"Aceptar", L"Activado",
     L"Ajustes avanzados (experimental)...", L"Atenci\u00f3n: estos ajustes son experimentales y no est\u00e1n probados del todo. El juego funciona a 30 FPS fijos; el l\u00edmite de FPS puede no comportarse como se espera.",
     L"Filtro sepia:", L"Activado (original)", L"Desactivado"},
    {L"afrorecomp_v" AFRO_VERSION_W L" - Lanceur", L"Langue :", L"M\u00e9moriser et ne plus demander",
     L"Maintenez Maj au d\u00e9marrage pour revoir cette fen\u00eatre.", L"Jouer",
     L"Images/s :", L"Original (limit\u00e9 \u00e0 30)", L"Affichage :", L"Plein \u00e9cran", L"Fen\u00eatre", L"R\u00e9solution interne :", L"Options avanc\u00e9es...", L"Par d\u00e9faut", L"D\u00e9sactiv\u00e9", L"OK", L"Activ\u00e9",
     L"R\u00e9glages avanc\u00e9s (exp\u00e9rimental)...", L"Attention : ces r\u00e9glages sont exp\u00e9rimentaux et pas enti\u00e8rement test\u00e9s. Le jeu tourne \u00e0 30 FPS fixes ; la limite de FPS peut ne pas se comporter comme pr\u00e9vu.",
     L"Filtre s\u00e9pia :", L"Activ\u00e9 (original)", L"D\u00e9sactiv\u00e9"},
    {L"afrorecomp_v" AFRO_VERSION_W L" - Avvio", L"Lingua:", L"Ricorda e non chiedere pi\u00f9",
     L"Tieni premuto Maiusc all'avvio per rivedere questa finestra.", L"Gioca",
     L"Fotogrammi:", L"Originale (limitato a 30)", L"Schermo:", L"Schermo intero", L"Finestra", L"Risoluzione interna:", L"Opzioni avanzate...", L"Predefinito", L"Disattivato", L"OK", L"Attivato",
     L"Impostazioni avanzate (sperimentale)...", L"Attenzione: queste impostazioni sono sperimentali e non del tutto testate. Il gioco gira a 30 FPS fissi; il limite di FPS potrebbe non comportarsi come previsto.",
     L"Filtro seppia:", L"Attivato (originale)", L"Disattivato"},
};
constexpr int kIdCombo = 101, kIdCheck = 102, kIdPlay = IDOK, kIdLangLbl = 103,
              kIdHint = 104, kIdFpsLbl = 105, kIdFps = 106, kIdDispLbl = 107,
              kIdDisp = 108, kIdScaleLbl = 109, kIdScale = 110,
              kIdAdvBtn = 111, kIdXBtn = 112, kIdXWarn = 113, kIdXSepiaLbl = 114,
              kIdXSepia = 115, kIdAdvBase = 200;
constexpr int kFpsValues[] = {0, 30, 60};
struct PickState { int sel = 0; bool remember = false; bool done = false; Options opt; };

void FillCombo(HWND h, int id, std::initializer_list<const wchar_t*> items) {
  HWND c = GetDlgItem(h, id);
  int cur = static_cast<int>(SendMessageW(c, CB_GETCURSEL, 0, 0));
  SendMessageW(c, CB_RESETCONTENT, 0, 0);
  for (auto* i : items) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)i);
  SendMessageW(c, CB_SETCURSEL, cur < 0 ? 0 : cur, 0);
}

void ApplyTexts(HWND h, int sel) {
  const UiText& t = kText[sel];
  SetWindowTextW(h, t.title);
  SetDlgItemTextW(h, kIdLangLbl, t.lang);
  SetDlgItemTextW(h, kIdCheck, t.remember);
  SetDlgItemTextW(h, kIdHint, t.hint);
  SetDlgItemTextW(h, kIdPlay, t.play);
  SetDlgItemTextW(h, kIdDispLbl, t.disp);
  SetDlgItemTextW(h, kIdScaleLbl, t.scale);
  SetDlgItemTextW(h, kIdAdvBtn, t.adv);
  SetDlgItemTextW(h, kIdXBtn, t.xbtn);
  FillCombo(h, kIdDisp, {t.full, t.win});
}

HFONT UiFont() {
  static HFONT font = nullptr;
  if (!font) {
    NONCLIENTMETRICSW ncm{sizeof(ncm)};
    font = SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)
               ? CreateFontIndirectW(&ncm.lfMessageFont)
               : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  }
  return font;
}

struct AdvState { bool done = false; Options* opt; };

LRESULT CALLBACK AdvProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto* st = reinterpret_cast<AdvState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if ((m == WM_COMMAND && (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL)) || m == WM_CLOSE) {
    if (LOWORD(w) == IDOK && m == WM_COMMAND)
      for (int i = 0; i < kAdvN; ++i)
        st->opt->adv[i] = std::clamp(static_cast<int>(SendDlgItemMessageW(
                                         h, kIdAdvBase + i, CB_GETCURSEL, 0, 0)),
                                     0, kAdv[i].n - 1);
    st->done = true;
    DestroyWindow(h);
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

// Modal "advanced options" window (diagnostics; values are cvars).
void ShowAdvanced(HWND owner, int sel, Options* o) {
  const UiText& t = kText[sel];
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSW wc{};
  wc.lpfnWndProc = AdvProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  wc.lpszClassName = L"AfroAdvanced";
  RegisterClassW(&wc);
  const double sc = GetDpiForSystem() / 96.0;
  auto S = [&](int v) { return static_cast<int>(v * sc + 0.5); };
  const DWORD style = WS_CAPTION | WS_SYSMENU;
  const int rowH = 32, top = 16;
  RECT rc{0, 0, S(460), S(top + kAdvN * rowH + 60)};
  AdjustWindowRectEx(&rc, style, FALSE, WS_EX_DLGMODALFRAME);
  RECT orc;
  GetWindowRect(owner, &orc);
  std::wstring title = t.adv;
  while (!title.empty() && title.back() == L'.') title.pop_back();
  HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, wc.lpszClassName, title.c_str(),
                           style | WS_VISIBLE, orc.left + S(20), orc.top + S(20),
                           rc.right - rc.left, rc.bottom - rc.top, owner, nullptr, inst, nullptr);
  if (!h) return;
  AdvState st;
  st.opt = o;
  SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
  auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD s_, int x, int y, int cw, int ch,
                int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | s_, S(x), S(y), S(cw), S(ch),
                             h, reinterpret_cast<HMENU>(INT_PTR(id)), inst, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), TRUE);
    return c;
  };
  for (int i = 0; i < kAdvN; ++i) {
    const AdvItem& a = kAdv[i];
    int y = top + i * rowH;
    mk(L"STATIC", a.name, SS_CENTERIMAGE, 16, y, 240, 26, 0);
    HWND c = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 264, y, 180, 200,
                kIdAdvBase + i);
    for (int k = 0; k < a.n; ++k) {
      const wchar_t* lb = a.labels[0] ? a.labels[k] : (k == 0 ? t.def : std::string(a.values[k]) == "true" ? t.on : t.off);
      std::wstring txt = lb;
      if (k == 0 && a.labels[0]) txt += std::wstring(L" (") + t.def + L")";
      SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)txt.c_str());
    }
    SendMessageW(c, CB_SETCURSEL, std::clamp(o->adv[i], 0, a.n - 1), 0);
  }
  mk(L"BUTTON", t.ok, WS_TABSTOP | BS_DEFPUSHBUTTON, 170, top + kAdvN * rowH + 12, 120, 32, IDOK);
  EnableWindow(owner, FALSE);
  MSG msg;
  while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(h, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  EnableWindow(owner, TRUE);
  SetForegroundWindow(owner);
  UnregisterClassW(wc.lpszClassName, inst);
}

LRESULT CALLBACK XProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto* st = reinterpret_cast<AdvState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if ((m == WM_COMMAND && (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL)) || m == WM_CLOSE) {
    if (LOWORD(w) == IDOK && m == WM_COMMAND) {
      int fi = static_cast<int>(SendDlgItemMessageW(h, kIdFps, CB_GETCURSEL, 0, 0));
      st->opt->fps = kFpsValues[std::clamp(fi, 0, 2)];
      st->opt->sepia = SendDlgItemMessageW(h, kIdXSepia, CB_GETCURSEL, 0, 0) == 1 ? 1 : 0;
    }
    st->done = true;
    DestroyWindow(h);
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

// Modal "Advanced settings (experimental)" window: user-facing, always available.
// Only FPS cap and sepia filter; saved only when "Remember" is ticked.
void ShowExperimental(HWND owner, int sel, Options* o) {
  const UiText& t = kText[sel];
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSW wc{};
  wc.lpfnWndProc = XProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  wc.lpszClassName = L"AfroExperimental";
  RegisterClassW(&wc);
  const double sc = GetDpiForSystem() / 96.0;
  auto S = [&](int v) { return static_cast<int>(v * sc + 0.5); };
  const DWORD style = WS_CAPTION | WS_SYSMENU;
  RECT rc{0, 0, S(420), S(244)};
  AdjustWindowRectEx(&rc, style, FALSE, WS_EX_DLGMODALFRAME);
  RECT orc;
  GetWindowRect(owner, &orc);
  std::wstring title = t.xbtn;
  while (!title.empty() && title.back() == L'.') title.pop_back();
  HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, wc.lpszClassName, title.c_str(),
                           style | WS_VISIBLE, orc.left + S(20), orc.top + S(20),
                           rc.right - rc.left, rc.bottom - rc.top, owner, nullptr, inst, nullptr);
  if (!h) return;
  AdvState st;
  st.opt = o;
  SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
  auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD s_, int x, int y, int cw, int ch,
                int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | s_, S(x), S(y), S(cw), S(ch),
                             h, reinterpret_cast<HMENU>(INT_PTR(id)), inst, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), TRUE);
    return c;
  };
  const DWORD cbs = WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL;
  mk(L"STATIC", t.xwarn, SS_SUNKEN, 16, 14, 388, 68, kIdXWarn);
  mk(L"STATIC", t.fps, SS_CENTERIMAGE, 16, 98, 180, 26, 0);
  HWND fps = mk(L"COMBOBOX", L"", cbs, 204, 98, 200, 200, kIdFps);
  for (auto* i : {t.fps_orig, L"30", L"60"}) SendMessageW(fps, CB_ADDSTRING, 0, (LPARAM)i);
  SendMessageW(fps, CB_SETCURSEL, o->fps == 30 ? 1 : o->fps == 60 ? 2 : 0, 0);
  mk(L"STATIC", t.sepia, SS_CENTERIMAGE, 16, 136, 180, 26, kIdXSepiaLbl);
  HWND sep = mk(L"COMBOBOX", L"", cbs, 204, 136, 200, 200, kIdXSepia);
  SendMessageW(sep, CB_ADDSTRING, 0, (LPARAM)t.sepia_on);
  SendMessageW(sep, CB_ADDSTRING, 0, (LPARAM)t.sepia_off);
  SendMessageW(sep, CB_SETCURSEL, o->sepia ? 1 : 0, 0);
  mk(L"BUTTON", t.ok, WS_TABSTOP | BS_DEFPUSHBUTTON, 150, 190, 120, 32, IDOK);
  EnableWindow(owner, FALSE);
  MSG msg;
  while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(h, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  EnableWindow(owner, TRUE);
  SetForegroundWindow(owner);
  UnregisterClassW(wc.lpszClassName, inst);
}

LRESULT CALLBACK PickProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto* st = reinterpret_cast<PickState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
  switch (m) {
    case WM_COMMAND:
      if (LOWORD(w) == kIdAdvBtn) {
        ShowAdvanced(h, st->sel, &st->opt);
      } else if (LOWORD(w) == kIdXBtn) {
        ShowExperimental(h, st->sel, &st->opt);
      } else if (LOWORD(w) == kIdCombo && HIWORD(w) == CBN_SELCHANGE) {
        st->sel = static_cast<int>(SendDlgItemMessageW(h, kIdCombo, CB_GETCURSEL, 0, 0));
        ApplyTexts(h, st->sel);
      } else if (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL) {
        st->opt.window = SendDlgItemMessageW(h, kIdDisp, CB_GETCURSEL, 0, 0) == 1;
        int si = static_cast<int>(SendDlgItemMessageW(h, kIdScale, CB_GETCURSEL, 0, 0));
        st->opt.scale = 1 + std::clamp(si, 0, 2);
        st->remember = IsDlgButtonChecked(h, kIdCheck) == BST_CHECKED;
        st->done = true;
        DestroyWindow(h);
      }
      return 0;
    case WM_CLOSE:
      st->remember = IsDlgButtonChecked(h, kIdCheck) == BST_CHECKED;
      st->done = true;
      DestroyWindow(h);
      return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

unsigned ShowPicker(unsigned initial, bool* remember, Options* o) {
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSW wc{};
  wc.lpfnWndProc = PickProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  wc.lpszClassName = L"AfroLauncher";
  RegisterClassW(&wc);
  const double sc = GetDpiForSystem() / 96.0;
  auto S = [&](int v) { return static_cast<int>(v * sc + 0.5); };
  const DWORD style = WS_CAPTION | WS_SYSMENU;
  RECT rc{0, 0, S(400), S(336)};
  AdjustWindowRectEx(&rc, style, FALSE, WS_EX_DLGMODALFRAME);
  const int W = rc.right - rc.left, H = rc.bottom - rc.top;
  int x = (GetSystemMetrics(SM_CXSCREEN) - W) / 2;
  int y = (GetSystemMetrics(SM_CYSCREEN) - H) / 2;
  HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, wc.lpszClassName,
                           L"Afro Samurai", style | WS_VISIBLE, x, y, W, H,
                           nullptr, nullptr, inst, nullptr);
  if (!h) return initial;
  PickState st;
  st.opt = *o;
  st.remember = *remember;
  for (int i = 0; i < 4; ++i) if (kLangs[i].id == initial) st.sel = i;
  SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
  HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  NONCLIENTMETRICSW ncm{sizeof(ncm)};
  if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
    ncm.lfMessageFont.lfHeight = static_cast<LONG>(ncm.lfMessageFont.lfHeight);
    font = CreateFontIndirectW(&ncm.lfMessageFont);
  }
  auto mk = [&](const wchar_t* cls, DWORD st_, int cx, int cy, int cw, int ch,
                int id) {
    HWND c = CreateWindowExW(0, cls, L"", WS_CHILD | WS_VISIBLE | st_, S(cx),
                             S(cy), S(cw), S(ch), h,
                             reinterpret_cast<HMENU>(INT_PTR(id)), inst, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return c;
  };
  // Row: language. Add future options (resolution, vsync...) as more rows.
  const DWORD cbs = WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL;
  mk(L"STATIC", SS_CENTERIMAGE, 20, 20, 150, 26, kIdLangLbl);
  HWND combo = mk(L"COMBOBOX", cbs, 180, 20, 200, 200, kIdCombo);
  for (auto& l : kLangs) SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)l.label);
  SendMessageW(combo, CB_SETCURSEL, st.sel, 0);
  mk(L"STATIC", SS_CENTERIMAGE, 20, 56, 150, 26, kIdDispLbl);
  HWND disp = mk(L"COMBOBOX", cbs, 180, 56, 200, 200, kIdDisp);
  mk(L"STATIC", SS_CENTERIMAGE, 20, 92, 150, 26, kIdScaleLbl);
  HWND scale = mk(L"COMBOBOX", cbs, 180, 92, 200, 200, kIdScale);
  for (auto* i : {L"1x (720p)", L"2x (1440p)", L"3x (2160p)"})
    SendMessageW(scale, CB_ADDSTRING, 0, (LPARAM)i);
  SendMessageW(disp, CB_ADDSTRING, 0, (LPARAM)L"-");
  SendMessageW(disp, CB_SETCURSEL, o->window ? 1 : 0, 0);
  SendMessageW(scale, CB_SETCURSEL, (o->scale >= 1 && o->scale <= 3) ? o->scale - 1 : 0, 0);
  mk(L"BUTTON", WS_TABSTOP, 20, 132, 360, 28, kIdXBtn);  // always shown
  if (REXCVAR_GET(advanced)) mk(L"BUTTON", WS_TABSTOP, 20, 166, 360, 28, kIdAdvBtn);
  mk(L"BUTTON", WS_TABSTOP | BS_AUTOCHECKBOX, 20, 206, 360, 22, kIdCheck);
  mk(L"STATIC", 0, 20, 232, 360, 36, kIdHint);
  HWND play = mk(L"BUTTON", WS_TABSTOP | BS_DEFPUSHBUTTON, 140, 282, 120, 34,
                 kIdPlay);
  if (st.remember) CheckDlgButton(h, kIdCheck, BST_CHECKED);
  ApplyTexts(h, st.sel);
  SetForegroundWindow(h);
  SetFocus(play);
  MSG msg;
  while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(h, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  UnregisterClassW(wc.lpszClassName, inst);
  *remember = st.remember;
  *o = st.opt;
  return kLangs[st.sel].id;
}
#endif

}  // namespace

unsigned ApplyStartupLanguage() {
  // Explicit --user_language (command line / env / toml) wins.
  for (auto& f : rex::cvar::GetRegistry()) {
    if (f.name == "user_language" && f.source != rex::cvar::Source::kDefault &&
        !REXCVAR_GET(choose_language))
      return std::strtoul(rex::cvar::GetFlagByName("user_language").c_str(), nullptr, 10);
  }
  auto path = ConfigPath();
  Options opt;
  unsigned lang = 0;
  bool ask = true;
  std::string seen_version;
  {
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
      if (line.rfind("language=", 0) == 0) lang = std::strtoul(line.c_str() + 9, nullptr, 10);
      if (line.rfind("ask=", 0) == 0) ask = line[4] != '0';
      if (line.rfind("version=", 0) == 0) { seen_version = line.substr(8); while (!seen_version.empty() && (seen_version.back() == '\r' || seen_version.back() == ' ')) seen_version.pop_back(); }
      if (line.rfind("fps=", 0) == 0) opt.fps = std::atoi(line.c_str() + 4);
      if (line.rfind("sepia=", 0) == 0) opt.sepia = line[6] == '1' ? 1 : 0;
      if (line.rfind("window=", 0) == 0) opt.window = line[7] == '1';
      for (int i = 0; i < kAdvN; ++i) {
        std::string k = std::string(kAdv[i].key) + "=";
        if (line.rfind(k, 0) == 0)
          opt.adv[i] = std::clamp(std::atoi(line.c_str() + k.size()), 0, kAdv[i].n - 1);
      }
      if (line.rfind("scale=", 0) == 0) opt.scale = std::clamp(std::atoi(line.c_str() + 6), 1, 3);
    }
  }
  if (!Valid(lang)) { lang = 0; ask = true; }
  // Internal resolution defaults to native (1x); a saved scale is only kept
  // when "Remember" was ticked (ask=0).
  const int saved_scale = opt.scale;
  if (ask) opt.scale = 1;
  // Same for the frame rate: default "Original (30 capped)" = no host cap.
  const int saved_fps = opt.fps;
  if (ask) opt.fps = 0;
  // Without --advanced, saved diagnostic options are ignored (safe defaults).
  const Options saved_adv = opt;
  if (!REXCVAR_GET(advanced))
    for (int i = 0; i < kAdvN; ++i) opt.adv[i] = 0;
  // First run of a new version: always show the launcher (new options may
  // exist), even if "remember" was ticked before; the tick is kept preset.
  const bool new_version = seen_version != AFRO_VERSION;
  bool show = ask || new_version || REXCVAR_GET(choose_language);
#ifdef _WIN32
  if (GetAsyncKeyState(VK_SHIFT) & 0x8000) show = true;
  if (show) {
    bool remember = !ask;
    unsigned initial = lang;
    if (!initial) {  // First run: preselect the Windows UI language.
      switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
        case LANG_SPANISH: initial = 5; break;
        case LANG_FRENCH: initial = 4; break;
        case LANG_ITALIAN: initial = 6; break;
        default: initial = 1;
      }
    }
    const Options stored = opt;  // advanced options persist only with "remember"
    lang = ShowPicker(initial, &remember, &opt);
    std::ofstream out(path, std::ios::trunc);
    out << "language=" << lang << "\nask=" << (remember ? 0 : 1) << "\nfps=" << (remember ? opt.fps : saved_fps)
        << "\nsepia=" << (remember ? opt.sepia : stored.sepia)
        << "\nwindow=" << (opt.window ? 1 : 0) << "\nscale=" << (remember ? opt.scale : saved_scale)
        << "\nversion=" << AFRO_VERSION << "\n";
    for (int i = 0; i < kAdvN; ++i)
      out << kAdv[i].key << "="
          << (!REXCVAR_GET(advanced) ? saved_adv.adv[i] : remember ? opt.adv[i] : stored.adv[i])
          << "\n";
  }
#else
  (void)show;  // No native picker on Linux: use config or default English.
#endif
  if (!lang) lang = 1;
  // Command-line / toml values win over the launcher.
  // Original (0) passes no override: the game keeps its own 30 FPS pacing.
  if (opt.fps > 0 && !Explicit("afro_fps_cap"))
    SetCvar("afro_fps_cap", std::to_string(opt.fps));
  if (opt.sepia && !Explicit("afro_fx_sepia"))
    SetCvar("afro_fx_sepia", "false");
  if (!Explicit("fullscreen"))
    SetCvar("fullscreen", opt.window ? "false" : "true");
  for (int i = 0; i < kAdvN; ++i)
    if (opt.adv[i] > 0 && !Explicit(kAdv[i].cvar))
      SetCvar(kAdv[i].cvar, kAdv[i].values[opt.adv[i]]);
  // Default render path: RTV (host render targets; black frames fixed by the
  // afro_edram_no_wrap GPU plugin patch). ROV stays selectable as a fallback.
  if (opt.adv[0] == 0 && !Explicit("render_target_path_d3d12"))
    SetCvar("render_target_path_d3d12", "rtv");
  if (!Explicit("resolution_scale") && opt.scale > 1)
    SetCvar("resolution_scale", std::to_string(opt.scale));
  rex::cvar::SetFlagByName("user_language", std::to_string(lang));
  std::fprintf(stderr, "[afro] user_language=%u (config: %s)\n", lang,
               path.string().c_str());
  return lang;
}

}  // namespace afro
