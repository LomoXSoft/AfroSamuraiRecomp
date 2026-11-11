// In-game options menu. See afro_menu.h.
//
// Drawn with ReXGlue's ImGui drawer over the paused game: the guest's present
// hook (MenuGuestFrame) waits while the menu is open, so the game stops on its
// last frame. Settings are the launcher's (afro_language.cfg, same keys); each
// change is saved at once and, where the runtime reads it live, applied at once.
// Navigation uses the game's own pad state (rexruntime's afro_pad_buttons), so the
// keyboard works through its bindings (arrows, Space, E...) and every controller
// through its remap.

#include "afro_menu.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <imgui.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/immediate_drawer.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <wincodec.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")
#endif

namespace afro {
namespace {

using clk = std::chrono::steady_clock;

// ------------------------------------------------------------ runtime helpers
// Plain C exports of rexruntime (patches/rexglue/zzzzzzzzzz-afro-interlude.patch),
// looked up at run time: the setup links against the prebuilt SDK.
struct Rt {
  uint32_t (*pad_buttons)(uint32_t) = nullptr;
  void (*ui_input_block)(int) = nullptr;
};
const Rt& R() {
  static const Rt rt = [] {
    Rt r;
#ifdef _WIN32
    if (HMODULE m = GetModuleHandleW(L"rexruntime.dll")) {
      r.pad_buttons = reinterpret_cast<uint32_t (*)(uint32_t)>(GetProcAddress(m, "afro_pad_buttons"));
      r.ui_input_block = reinterpret_cast<void (*)(int)>(GetProcAddress(m, "afro_ui_input_block"));
    }
#endif
    if (!r.pad_buttons) REXLOG_WARN("afro menu: rexruntime.dll lacks afro_pad_buttons; keyboard only");
    return r;
  }();
  return rt;
}
uint32_t PadButtons() {
  if (!R().pad_buttons) return 0;
  uint32_t b = 0;
  for (uint32_t u = 0; u < 4; ++u) b |= R().pad_buttons(u);
  return b;
}
void BlockGuestInput(bool on) {
  if (R().ui_input_block) R().ui_input_block(on ? 1 : 0);
}

constexpr uint32_t kUp = 0x0001, kDown = 0x0002, kLeft = 0x0004, kRight = 0x0008,
                   kStart = 0x0010, kBack = 0x0020, kL3 = 0x0040, kR3 = 0x0080, kA = 0x1000,
                   kB = 0x2000;

// ---------------------------------------------------------------- config file
std::filesystem::path CfgPath() {
#ifdef _WIN32
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (n && n < MAX_PATH) return std::filesystem::path(buf).parent_path() / "afro_language.cfg";
#endif
  return "afro_language.cfg";
}
std::vector<std::string> ReadCfg() {
  std::vector<std::string> lines;
  std::ifstream in(CfgPath());
  std::string l;
  while (std::getline(in, l)) {
    while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
    lines.push_back(l);
  }
  return lines;
}
std::string CfgGet(const std::vector<std::string>& lines, const std::string& key) {
  for (const auto& l : lines)
    if (l.size() > key.size() && l.compare(0, key.size(), key) == 0 && l[key.size()] == '=')
      return l.substr(key.size() + 1);
  return {};
}
void CfgSet(const std::vector<std::pair<std::string, std::string>>& kv) {
  auto lines = ReadCfg();
  for (const auto& [k, v] : kv) {
    bool found = false;
    for (auto& l : lines)
      if (l.size() > k.size() && l.compare(0, k.size(), k) == 0 && l[k.size()] == '=') {
        l = k + "=" + v;
        found = true;
      }
    if (!found) lines.push_back(k + "=" + v);
  }
  std::ofstream out(CfgPath(), std::ios::trunc);
  for (const auto& l : lines) out << l << "\n";
}
void SetCvar(const char* name, const std::string& value) {
  if (!rex::cvar::SetFlagByName(name, value)) REXLOG_WARN("afro menu: cannot set {}={}", name, value);
}

// --------------------------------------------------------------------- texts
// Game language from user_language: 1 en, 5 es, 4 fr, 6 it.
int Lang() {
  switch (std::atoi(rex::cvar::GetFlagByName("user_language").c_str())) {
    case 5: return 1;
    case 4: return 2;
    case 6: return 3;
    default: return 0;
  }
}
using T4 = const char* const[4];
const char* Tr(const char* const* t) { return t[Lang()]; }

T4 kTitle = {"OPTIONS", "OPCIONES", "OPTIONS", "OPZIONI"};
T4 kGfx = {"GRAPHICS", "GR\xC3\x81" "FICOS", "GRAPHISMES", "GRAFICA"};
T4 kPlay = {"GAMEPLAY", "JUEGO", "JEU", "GIOCO"};
T4 kCtl = {"CONTROLS", "CONTROLES", "CONTR\xC3\x94LES", "CONTROLLI"};
T4 kExp = {"EXPERIMENTAL", "EXPERIMENTAL", "EXP\xC3\x89RIMENTAL", "SPERIMENTALE"};
T4 kRestart = {"after restart", "al reiniciar", "au red\xC3\xA9marrage", "al riavvio"};
T4 kResume = {"Back to the game", "Volver al juego", "Retour au jeu", "Torna al gioco"};
T4 kHintChange = {"Change", "Cambiar", "Changer", "Cambia"};
T4 kHintSelect = {"Select", "Seleccionar", "S\xC3\xA9lectionner", "Seleziona"};
T4 kHintBack = {"Back to the game", "Volver al juego", "Retour au jeu", "Torna al gioco"};
T4 kHintKeys = {"Key bindings: launcher (hold Shift at start)",
                "Asignar teclas: en el lanzador (Mayus al iniciar)",
                "Touches : dans le lanceur (Maj au d\xC3\xA9marrage)",
                "Assegna tasti: nel launcher (Maiusc all'avvio)"};

// ---------------------------------------------------------------------- items
struct Choice {
  const char* label[4];
  std::vector<std::pair<std::string, std::string>> cfg;  // keys this choice writes
};
struct Item {
  enum Kind { kHeader, kChoice, kSlider, kResume } kind;
  const char* const* label;  // T4
  std::vector<Choice> choices;
  std::vector<std::pair<std::string, std::string>> defaults;  // when the cfg lacks a key
  bool restart = false;
  // slider
  std::string key;
  int lo = 0, hi = 0, step = 1, def = 0, scale = 1;
  const char* unit = "";
  std::function<void(int)> apply;  // choice index or slider value
};

std::vector<Item> MakeItems() {
  std::vector<Item> v;
  auto header = [&](T4& l) { Item i{Item::kHeader, l}; v.push_back(i); };
  auto choice = [&](T4& l, std::vector<Choice> c, std::vector<std::pair<std::string, std::string>> d,
                    std::function<void(int)> apply, bool restart = false) {
    Item i{Item::kChoice, l};
    i.choices = std::move(c);
    i.defaults = std::move(d);
    i.apply = std::move(apply);
    i.restart = restart;
    v.push_back(std::move(i));
  };
  auto slider = [&](T4& l, const char* key, int lo, int hi, int step, int def, int scale,
                    const char* unit, const char* cvar) {
    Item i{Item::kSlider, l};
    i.key = key;
    i.lo = lo, i.hi = hi, i.step = step, i.def = def, i.scale = scale, i.unit = unit;
    i.apply = [cvar, scale](int val) {
      char b[32];
      if (scale == 100) std::snprintf(b, sizeof(b), "%d.%02d", val / 100, val % 100);
      else std::snprintf(b, sizeof(b), "%d", val);
      SetCvar(cvar, b);
    };
    v.push_back(std::move(i));
  };
  static T4 lDisplay = {"Display", "Pantalla", "Affichage", "Schermo"};
  static T4 lFps = {"Frame rate", "Fotogramas", "Images/s", "Fotogrammi"};
  static T4 lVsync = {"VSync", "VSync", "VSync", "VSync"};
  static T4 lScale = {"Internal resolution", "Resoluci\xC3\xB3n interna", "R\xC3\xA9solution interne",
                      "Risoluzione interna"};
  static T4 lDiff = {"Difficulty", "Dificultad", "Difficult\xC3\xA9", "Difficolt\xC3\xA0"};
  static T4 lLore = {"Loading screens", "Pantallas de carga", "\xC3\x89" "crans de chargement",
                     "Schermate di caricamento"};
  static T4 lLang = {"Language", "Idioma", "Langue", "Lingua"};
  static T4 lCtl = {"Control", "Control", "Contr\xC3\xB4le", "Controllo"};
  static T4 lSens = {"Mouse sensitivity", "Sensibilidad del rat\xC3\xB3n", "Sensibilit\xC3\xA9 souris",
                     "Sensibilit\xC3\xA0 mouse"};
  static T4 lDz = {"Mouse dead zone", "Zona muerta del rat\xC3\xB3n", "Zone morte souris",
                   "Zona morta mouse"};
  static T4 lSmooth = {"Mouse smoothing", "Suavizado del rat\xC3\xB3n", "Lissage souris",
                       "Fluidit\xC3\xA0 mouse"};
  static T4 lSepia = {"Sepia filter", "Filtro sepia", "Filtre s\xC3\xA9pia", "Filtro seppia"};
  static T4 lBlood = {"Blood effect", "Efecto de sangre", "Effet de sang", "Effetto sangue"};
  static T4 lDof = {"Depth of field", "Profundidad de campo", "Profondeur de champ",
                    "Profondit\xC3\xA0 di campo"};
  static T4 lBloom = {"Bloom", "Resplandor", "Halo lumineux", "Bagliore"};
  static T4 lFocus = {"Focus effect", "Efecto de concentraci\xC3\xB3n", "Effet de concentration",
                      "Effetto concentrazione"};
  const Choice on = {{"On", "S\xC3\xAD", "Oui", "S\xC3\xAC"}, {}};
  const Choice off = {{"Off", "No", "Non", "No"}, {}};
  auto onoff = [&](const char* key, const char* on_v, const char* off_v) {
    Choice a = on, b = off;
    a.cfg = {{key, on_v}};
    b.cfg = {{key, off_v}};
    return std::vector<Choice>{a, b};
  };
  auto fx = [](const char* cvar) {
    return [cvar](int i) { SetCvar(cvar, i == 0 ? "true" : "false"); };
  };

  header(kGfx);
  choice(lDisplay,
         {{{"Borderless fullscreen", "Pantalla completa sin bordes", "Plein \xC3\xA9" "cran sans bordure",
            "Schermo intero senza bordi"},
           {{"window", "0"}, {"exclusive", "0"}}},
          {{"Window", "Ventana", "Fen\xC3\xAAtre", "Finestra"}, {{"window", "1"}, {"exclusive", "0"}}},
          {{"Exclusive fullscreen", "Pantalla completa exclusiva", "Plein \xC3\xA9" "cran exclusif",
            "Schermo intero esclusivo"},
           {{"window", "0"}, {"exclusive", "1"}}}},
         {{"window", "0"}, {"exclusive", "0"}}, [](int i) {
           SetCvar("fullscreen_exclusive", i == 2 ? "true" : "false");
           SetCvar("fullscreen", i == 1 ? "false" : "true");
         });
  choice(lFps,
         {{{"30 (original)", "30 (original)", "30 (original)", "30 (originale)"}, {{"fps", "0"}}},
          {{"60", "60", "60", "60"}, {{"fps", "60"}, {"vsync", "1"}}}},
         {{"fps", "0"}}, [](int i) {
           SetCvar("afro_present_interval", i == 1 ? "1" : "2");
           SetCvar("afro_fps_cap", i == 1 ? "60" : "0");
           if (i == 1) SetCvar("vsync", "false");
         });
  choice(lVsync, onoff("vsync", "0", "1"), {{"vsync", "0"}},
         [](int i) { SetCvar("vsync", i == 0 ? "true" : "false"); });
  choice(lScale,
         {{{"1x (720p)", "1x (720p)", "1x (720p)", "1x (720p)"}, {{"scale", "1"}}},
          {{"2x (1440p)", "2x (1440p)", "2x (1440p)", "2x (1440p)"}, {{"scale", "2"}}},
          {{"3x (2160p)", "3x (2160p)", "3x (2160p)", "3x (2160p)"}, {{"scale", "3"}}}},
         {{"scale", "1"}}, nullptr, true);

  header(kPlay);
  choice(lDiff,
         {{{"Normal", "Normal", "Normal", "Normale"}, {{"difficulty", "0"}}},
          {{"Game Journalist", "Periodista de videojuegos", "Journaliste jeu vid\xC3\xA9o",
            "Giornalista di videogiochi"},
           {{"difficulty", "1"}}}},
         {{"difficulty", "0"}}, [](int i) { SetCvar("afro_player_invuln", i == 1 ? "true" : "false"); });
  choice(lLore,
         {{{"Full lore", "Historia completa", "Histoire compl\xC3\xA8te", "Storia completa"}, {{"lore", "0"}}},
          {{"Fast", "R\xC3\xA1pidas", "Rapides", "Veloci"}, {{"lore", "1"}}}},
         {{"lore", "0"}}, [](int i) { SetCvar("afro_interlude_full", i == 0 ? "true" : "false"); });
  choice(lLang,
         {{{"English", "English", "English", "English"}, {{"language", "1"}}},
          {{"Espa\xC3\xB1ol", "Espa\xC3\xB1ol", "Espa\xC3\xB1ol", "Espa\xC3\xB1ol"}, {{"language", "5"}}},
          {{"Fran\xC3\xA7" "ais", "Fran\xC3\xA7" "ais", "Fran\xC3\xA7" "ais", "Fran\xC3\xA7" "ais"}, {{"language", "4"}}},
          {{"Italiano", "Italiano", "Italiano", "Italiano"}, {{"language", "6"}}}},
         {{"language", "1"}}, nullptr, true);

  header(kCtl);
  choice(lCtl,
         {{{"Auto", "Auto", "Auto", "Auto"}, {{"control", "0"}}},
          {{"Controller", "Mando", "Manette", "Controller"}, {{"control", "1"}}},
          {{"Keyboard", "Teclado", "Clavier", "Tastiera"}, {{"control", "2"}}}},
         {{"control", "0"}}, nullptr, true);
  slider(lSens, "mouse_sensitivity", 10, 500, 5, 200, 100, "", "mnk_sensitivity");
  slider(lDz, "mouse_deadzone", 0, 80, 1, 55, 100, "", "afro_mouse_deadzone");
  slider(lSmooth, "mouse_smoothing_ms", 1, 50, 1, 5, 1, " ms", "afro_mouse_window_ms");

  header(kExp);
  choice(lSepia, onoff("sepia", "0", "1"), {{"sepia", "0"}}, fx("afro_fx_sepia"));
  choice(lBlood, onoff("fx_blood", "0", "1"), {{"fx_blood", "0"}}, fx("afro_fx_blood"));
  choice(lDof, onoff("fx_dof", "0", "1"), {{"fx_dof", "0"}}, fx("afro_fx_dof"));
  choice(lBloom, onoff("fx_bloom", "0", "1"), {{"fx_bloom", "0"}}, fx("afro_fx_bloom"));
  choice(lFocus, onoff("fx_focus", "0", "1"), {{"fx_focus", "0"}}, fx("afro_fx_focus"));

  Item resume{Item::kResume, kResume};
  v.push_back(resume);
  return v;
}

// -------------------------------------------------------------------- drawing
ImFont* g_font_title = nullptr;
ImFont* g_font_item = nullptr;
ImFont* g_font_small = nullptr;

const ImU32 kRed = IM_COL32(200, 22, 26, 255);
const ImU32 kRedDark = IM_COL32(110, 6, 10, 255);
const ImU32 kWhite = IM_COL32(242, 238, 232, 255);
const ImU32 kGrey = IM_COL32(150, 146, 140, 255);
const ImU32 kDim = IM_COL32(100, 96, 92, 255);

// An ink brush stroke from a to b (deterministic for a given seed): one smooth
// polygon whose edges wander slowly (interpolated noise), loaded at the start and
// thinning out to a dry, split tail, plus a few bristle streaks past its edges.
void Brush(ImDrawList* dl, ImVec2 a, ImVec2 b, float w, ImU32 col, unsigned seed) {
  auto rnd = [&seed]() {
    seed = seed * 1103515245u + 12345u;
    return float((seed >> 16) & 0x7fff) / 32767.0f;
  };
  const float dx = b.x - a.x, dy = b.y - a.y;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1) return;
  const float nx = -dy / len, ny = dx / len;
  // Low-frequency noise: 9 control values per edge, smoothly interpolated.
  float kl[9], kr[9];
  for (int k = 0; k < 9; ++k) kl[k] = rnd() - 0.5f, kr[k] = rnd() - 0.5f;
  auto noise = [](const float* kv, float t) {
    const float x = t * 8.0f;
    const int i = std::min(7, int(x));
    const float f = x - i, sm = f * f * (3 - 2 * f);
    return kv[i] + (kv[i + 1] - kv[i]) * sm;
  };
  const int n = 48;
  std::vector<ImVec2> pts;
  pts.reserve(2 * (n + 1));
  auto half_at = [&](float t) {
    // Blunt, loaded start; long tapering tail.
    const float start = std::min(1.0f, t / 0.06f);
    const float tail = std::pow(std::max(0.0f, 1.0f - std::max(0.0f, t - 0.55f) / 0.45f), 0.7f);
    return w * 0.5f * std::sqrt(start) * (0.25f + 0.75f * tail);
  };
  for (int i = 0; i <= n; ++i) {
    const float t = float(i) / n;
    const float h = half_at(t) * (1.0f + 0.22f * noise(kl, t));
    pts.push_back({a.x + dx * t + nx * h, a.y + dy * t + ny * h});
  }
  for (int i = n; i >= 0; --i) {
    const float t = float(i) / n;
    const float h = half_at(t) * (1.0f + 0.22f * noise(kr, t));
    pts.push_back({a.x + dx * t - nx * h, a.y + dy * t - ny * h});
  }
  dl->AddConcavePolyFilled(pts.data(), int(pts.size()), col);
  // Dry bristles: thin streaks running out of the tail and along the edges.
  for (int k = 0; k < 7; ++k) {
    const float off = (rnd() - 0.5f) * 0.9f;
    const float t0 = 0.35f + rnd() * 0.4f, t1 = std::min(1.04f, t0 + 0.25f + rnd() * 0.35f);
    const float h0 = half_at(t0) * 2 * off, h1 = half_at(std::min(1.0f, t1)) * 2 * off;
    dl->AddLine({a.x + dx * t0 + nx * h0, a.y + dy * t0 + ny * h0},
                {a.x + dx * t1 + nx * h1, a.y + dy * t1 + ny * h1}, col,
                std::max(1.0f, w * (0.03f + 0.04f * rnd())));
  }
}

// A key/button cap like the game's prompts: rounded box with a short label.
float KeyCap(ImDrawList* dl, ImFont* f, float size, ImVec2 p, const char* text) {
  const ImVec2 ts = f->CalcTextSizeA(size, 1e9f, 0, text);
  const float pad = size * 0.35f;
  const ImVec2 b{p.x + ts.x + pad * 2, p.y + size * 1.35f};
  dl->AddRectFilled(p, b, IM_COL32(20, 18, 18, 230), size * 0.25f);
  dl->AddRect(p, b, kWhite, size * 0.25f, 0, std::max(1.0f, size * 0.07f));
  dl->AddText(f, size, {p.x + pad, p.y + size * 0.17f}, kWhite, text);
  return b.x - p.x;
}

// Names of the confirm/back buttons for the device in use (afro_pad_family).
void ButtonNames(const char** ok, const char** back) {
  const int fam = std::atoi(rex::cvar::GetFlagByName("afro_pad_family").c_str());
  static std::string kb_ok, kb_back;
  switch (fam) {
    case 2: *ok = "X"; *back = "O"; return;      // PlayStation cross / circle
    case 3: *ok = "B"; *back = "A"; return;      // Nintendo, by position
    case 5: {                                    // keyboard: its bindings
      kb_ok = rex::cvar::GetFlagByName("keybind_a");
      kb_back = rex::cvar::GetFlagByName("keybind_b");
      kb_ok = kb_ok.substr(0, kb_ok.find(','));
      kb_back = kb_back.substr(0, kb_back.find(','));
      *ok = kb_ok.empty() ? "Space" : kb_ok.c_str();
      *back = kb_back.empty() ? "E" : kb_back.c_str();
      return;
    }
    default: *ok = "A"; *back = "B"; return;
  }
}

// ------------------------------------------------------------- button icons
// The hints use the same icons as the game's own prompts: glyphs\<family>\*.png
// next to the exe (zzzz-afro-button-glyphs.patch), named by Xbox position.
rex::ui::ImmediateDrawer* g_imm = nullptr;
std::unordered_map<std::string, std::unique_ptr<rex::ui::ImmediateTexture>> g_glyphs;

// Same choice as the GPU plugin's ResolvedFamily: afro_button_glyphs, else the
// device last used. 1 Xbox, 2 PlayStation, 3 Nintendo, 4 Steam, 5 keyboard.
int GlyphFamily() {
  const std::string s = rex::cvar::GetFlagByName("afro_button_glyphs");
  if (s == "xbox") return 1;
  if (s == "playstation" || s == "ps") return 2;
  if (s == "nintendo" || s == "switch") return 3;
  if (s == "steam") return 4;
  if (s == "keyboard") return 5;
  const int pad = std::atoi(rex::cvar::GetFlagByName("afro_pad_family").c_str());
  return (pad >= 1 && pad <= 5) ? pad : 1;
}

#ifdef _WIN32
// PNG to RGBA with WIC; empty on failure.
std::vector<uint8_t> DecodePng(const std::filesystem::path& path, uint32_t& w, uint32_t& h) {
  std::vector<uint8_t> out;
  const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  IWICImagingFactory* factory = nullptr;
  IWICBitmapDecoder* decoder = nullptr;
  IWICBitmapFrameDecode* frame = nullptr;
  IWICFormatConverter* conv = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                 IID_PPV_ARGS(&factory))) &&
      SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                   WICDecodeMetadataCacheOnDemand, &decoder)) &&
      SUCCEEDED(decoder->GetFrame(0, &frame)) &&
      SUCCEEDED(factory->CreateFormatConverter(&conv)) &&
      SUCCEEDED(conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                 nullptr, 0.0, WICBitmapPaletteTypeCustom)) &&
      SUCCEEDED(conv->GetSize(&w, &h)) && w > 0 && h > 0 && w <= 1024 && h <= 1024) {
    out.resize(size_t(w) * h * 4);
    if (FAILED(conv->CopyPixels(nullptr, w * 4, UINT(out.size()), out.data()))) out.clear();
  }
  if (conv) conv->Release();
  if (frame) frame->Release();
  if (decoder) decoder->Release();
  if (factory) factory->Release();
  if (SUCCEEDED(co)) CoUninitialize();
  return out;
}

std::filesystem::path GlyphDir() {
  static std::filesystem::path dir = [] {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::filesystem::path(std::wstring(buf, n)).parent_path() / "glyphs";
  }();
  return dir;
}
#endif

// UI thread. "playstation/A" -> its texture, or nullptr when there is no icon.
rex::ui::ImmediateTexture* Glyph(const std::string& name) {
  auto it = g_glyphs.find(name);
  if (it != g_glyphs.end()) return it->second.get();
  std::unique_ptr<rex::ui::ImmediateTexture> tex;
#ifdef _WIN32
  if (g_imm) {
    uint32_t w = 0, h = 0;
    std::vector<uint8_t> rgba = DecodePng(GlyphDir() / (name + ".png"), w, h);
    if (!rgba.empty()) {
      tex = g_imm->CreateTexture(w, h, rex::ui::ImmediateTextureFilter::kLinear, false,
                                 rgba.data());
    }
  }
#endif
  rex::ui::ImmediateTexture* raw = tex.get();
  g_glyphs.emplace(name, std::move(tex));
  return raw;
}

// The icon for a pad button (Xbox position: A, B, DPAD_LEFT...). On the
// keyboard, the key bound to it (`bind`, e.g. keybind_a) when one is set, like
// the game's keyboard prompts; `key` is the key cap to use for the D-pad.
rex::ui::ImmediateTexture* ButtonGlyph(const char* button, const char* bind, const char* key) {
  static const char* kDirs[] = {"xbox", "xbox", "playstation", "nintendo", "steam", "keyboard"};
  const int fam = GlyphFamily();
  const std::string dir = std::string(kDirs[fam]) + "/";
  if (fam == 5) {
    std::string k = key ? key : "";
    if (bind) {
      k = rex::cvar::GetFlagByName(bind);
      k = k.substr(0, k.find(','));
      const size_t plus = k.rfind('+');
      if (plus != std::string::npos && plus + 1 < k.size()) k = k.substr(plus + 1);
    }
    if (!k.empty()) {
      if (auto* t = Glyph(dir + "key_" + k)) return t;
    }
  }
  return Glyph(dir + button);
}

// Draws the icon `tex` with height `size` at p, or the text key cap when there
// is no icon. Returns the width used.
float Prompt(ImDrawList* dl, ImFont* f, float size, ImVec2 p, rex::ui::ImmediateTexture* tex,
             const char* text) {
  if (!tex) return KeyCap(dl, f, size * 0.62f, {p.x, p.y + size * 0.1f}, text);
  const float w = size * float(tex->width) / float(std::max<uint32_t>(1, tex->height));
  dl->AddImage(reinterpret_cast<ImTextureID>(tex), p, {p.x + w, p.y + size});
  return w;
}

// -------------------------------------------------------------------- dialog
class MenuDialog : public rex::ui::ImGuiDialog {
 public:
  explicit MenuDialog(rex::ui::ImGuiDrawer* d) : ImGuiDialog(d), items_(MakeItems()) {
    Load();
    sel_ = Next(-1, 1);
    opened_ = clk::now();
  }

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  void Load();
  int Next(int from, int dir) const {
    int i = from;
    for (int n = 0; n < int(items_.size()); ++n) {
      i = (i + dir + int(items_.size())) % int(items_.size());
      if (items_[i].kind != Item::kHeader) return i;
    }
    return 0;
  }
  void Change(int dir);
  bool Pressed(uint32_t now, uint32_t mask, bool repeat);

  std::vector<Item> items_;
  std::vector<int> value_;  // choice index or slider value per item
  int sel_ = 0;
  float scroll_ = 0;
  uint32_t prev_buttons_ = 0xFFFF;  // ignore what is held when the menu opens
  clk::time_point repeat_at_[16]{};
  clk::time_point opened_;
};

void MenuDialog::Load() {
  const auto cfg = ReadCfg();
  value_.assign(items_.size(), 0);
  for (size_t i = 0; i < items_.size(); ++i) {
    const Item& it = items_[i];
    if (it.kind == Item::kSlider) {
      const std::string s = CfgGet(cfg, it.key);
      value_[i] = std::clamp(s.empty() ? it.def : std::atoi(s.c_str()), it.lo, it.hi);
    } else if (it.kind == Item::kChoice) {
      auto get = [&](const std::string& k) {
        std::string s = CfgGet(cfg, k);
        if (s.empty())
          for (const auto& [dk, dv] : it.defaults)
            if (dk == k) s = dv;
        return s;
      };
      for (size_t c = 0; c < it.choices.size(); ++c) {
        bool match = true;
        for (const auto& [k, v] : it.choices[c].cfg)
          if (!(get(k) == v || (v == "0" && get(k).empty()))) match = false;
        if (match) {
          value_[i] = int(c);
          break;
        }
      }
    }
  }
}

void MenuDialog::Change(int dir) {
  Item& it = items_[sel_];
  if (it.kind == Item::kChoice) {
    const int n = int(it.choices.size());
    value_[sel_] = (value_[sel_] + dir + n) % n;
    CfgSet(it.choices[value_[sel_]].cfg);
    if (it.apply) it.apply(value_[sel_]);
    Load();  // 60 also writes vsync=1: show it
  } else if (it.kind == Item::kSlider) {
    value_[sel_] = std::clamp(value_[sel_] + dir * it.step, it.lo, it.hi);
    CfgSet({{it.key, std::to_string(value_[sel_])}});
    if (it.apply) it.apply(value_[sel_]);
  }
}

bool MenuDialog::Pressed(uint32_t now, uint32_t mask, bool repeat) {
  int bit = 0;
  while (bit < 16 && !(mask & (1u << bit))) ++bit;
  const auto t = clk::now();
  if ((now & mask) && !(prev_buttons_ & mask)) {
    repeat_at_[bit] = t + std::chrono::milliseconds(380);
    return true;
  }
  if (repeat && (now & mask) && (prev_buttons_ & mask) && t >= repeat_at_[bit]) {
    repeat_at_[bit] = t + std::chrono::milliseconds(70);
    return true;
  }
  return false;
}

void MenuDialog::OnDraw(ImGuiIO& io) {
  // ---- input: the game's pad state (keyboard included through its bindings)
  uint32_t b = PadButtons();
  // Arrows, Enter and Esc work too, whatever the bindings or control mode. They
  // are merged before edge detection, so a key that is also a pad button counts once.
  if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) b |= kUp;
  if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) b |= kDown;
  if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) b |= kLeft;
  if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) b |= kRight;
  if (ImGui::IsKeyDown(ImGuiKey_Enter)) b |= kA;
  if (ImGui::IsKeyDown(ImGuiKey_Escape)) b |= kB;
  if (prev_buttons_ == 0xFFFF) prev_buttons_ = b;  // first frame
  if (Pressed(b, kUp, true)) sel_ = Next(sel_, -1);
  if (Pressed(b, kDown, true)) sel_ = Next(sel_, 1);
  if (Pressed(b, kLeft, true)) Change(-1);
  if (Pressed(b, kRight, true)) Change(1);
  bool close = Pressed(b, kB, false) || Pressed(b, kStart, false) || Pressed(b, kBack, false);
  if (Pressed(b, kA, false)) {
    if (items_[sel_].kind == Item::kResume) close = true;
    else Change(1);
  }
  prev_buttons_ = b;
  if (close) {
    MenuToggle();  // destroys this dialog after the frame
    return;
  }

  // ---- drawing
  const float W = io.DisplaySize.x, H = io.DisplaySize.y;
  const float s = H / 1080.0f;
  ImFont* fT = g_font_title ? g_font_title : ImGui::GetFont();
  ImFont* fI = g_font_item ? g_font_item : ImGui::GetFont();
  ImFont* fS = g_font_small ? g_font_small : ImGui::GetFont();
  ImGui::SetNextWindowPos({0, 0});
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::Begin("##afro_menu", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float fade = std::min(1.0f, std::chrono::duration<float>(clk::now() - opened_).count() / 0.18f);
  const auto A = [fade](ImU32 c, float a = 1.0f) {
    return (c & 0x00FFFFFF) | (ImU32(((c >> 24) & 0xFF) * a * fade) << 24);
  };

  // Dim the paused game only lightly on the left: L3 + R3 also shows the game's
  // own stats there (top left), which stay readable. The red ink of the game's
  // menus sweeps up from the bottom-left corner, below them.
  dl->AddRectFilled({0, 0}, {W, H}, A(IM_COL32(8, 6, 6, 110)));
  Brush(dl, {-90 * s, H * 1.06f}, {W * 0.33f, H * 0.66f}, 230 * s, A(kRedDark, 0.75f), 7);
  Brush(dl, {-20 * s, H * 1.10f}, {W * 0.27f, H * 0.74f}, 110 * s, A(kRed, 0.55f), 11);
  Brush(dl, {W * 0.70f, H * 1.04f}, {W * 1.06f, H * 0.55f}, 150 * s, A(kRedDark, 0.45f), 23);
  // Black panel behind the list, with a soft left edge.
  const float px0 = W * 0.35f, px1 = std::min(W - 40 * s, px0 + 1100 * s);
  dl->AddRectFilledMultiColor({px0 - 60 * s, 0}, {px0, H}, A(IM_COL32(0, 0, 0, 0)),
                              A(IM_COL32(0, 0, 0, 215)), A(IM_COL32(0, 0, 0, 215)),
                              A(IM_COL32(0, 0, 0, 0)));
  dl->AddRectFilled({px0, 0}, {W, H}, A(IM_COL32(0, 0, 0, 215)));

  // Title with a brush underline.
  const float tx = px0 + 40 * s, ty = H * 0.07f;
  dl->AddText(fT, 84 * s, {tx + 4 * s, ty + 4 * s}, A(IM_COL32(0, 0, 0, 200)), Tr(kTitle));
  dl->AddText(fT, 84 * s, {tx, ty}, A(kWhite), Tr(kTitle));
  Brush(dl, {tx - 10 * s, ty + 100 * s}, {tx + 520 * s, ty + 92 * s}, 16 * s, A(kRed), 3);

  // List (scrolls to keep the selection in view).
  const float top = ty + 140 * s, bottom = H - 120 * s;
  const float rowH = 44 * s, headH = 58 * s;
  std::vector<float> ys(items_.size());
  float y = 0;
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i].kind == Item::kHeader) y += (i ? 14 * s : 0);
    if (items_[i].kind == Item::kResume) y += 18 * s;
    ys[i] = y;
    y += items_[i].kind == Item::kHeader ? headH : rowH;
  }
  const float view = bottom - top;
  const float want_lo = ys[sel_] - rowH * 2, want_hi = ys[sel_] + rowH * 3 - view;
  if (want_hi > scroll_) scroll_ = want_hi;
  if (want_lo < scroll_) scroll_ = std::max(0.0f, want_lo);
  dl->PushClipRect({px0, top - 6 * s}, {W, bottom + 6 * s}, true);
  const float lx = tx, vx = std::min(px1 - 40 * s, lx + 980 * s);
  for (size_t i = 0; i < items_.size(); ++i) {
    const Item& it = items_[i];
    const float yy = top + ys[i] - scroll_;
    if (yy < top - headH || yy > bottom) continue;
    if (it.kind == Item::kHeader) {
      dl->AddText(fS, 28 * s, {lx, yy + 16 * s}, A(kRed), Tr(it.label));
      const float hw = fS->CalcTextSizeA(28 * s, 1e9f, 0, Tr(it.label)).x;
      dl->AddLine({lx + hw + 16 * s, yy + 32 * s}, {vx, yy + 32 * s}, A(kRedDark), 2 * s);
      continue;
    }
    const bool on = int(i) == sel_;
    if (on) Brush(dl, {lx - 30 * s, yy + rowH * 0.52f}, {vx + 20 * s, yy + rowH * 0.48f}, rowH * 1.02f,
                  A(IM_COL32(120, 8, 12, 150)), 41 + unsigned(i));
    const ImU32 lc = on ? kWhite : kGrey;
    dl->AddText(fI, 34 * s, {lx, yy + 3 * s}, A(lc), Tr(it.label));
    if (it.kind == Item::kResume) continue;
    std::string val;
    if (it.kind == Item::kChoice) {
      val = it.choices[value_[i]].label[Lang()];
    } else {
      char buf[32];
      if (it.scale == 100) std::snprintf(buf, sizeof(buf), "%d.%02d", value_[i] / 100, value_[i] % 100);
      else std::snprintf(buf, sizeof(buf), "%d%s", value_[i], it.unit);
      val = buf;
    }
    const float vw = fI->CalcTextSizeA(32 * s, 1e9f, 0, val.c_str()).x;
    float vxr = vx;
    if (it.restart) {
      const std::string note = std::string("(") + Tr(kRestart) + ")";
      const float nw = fS->CalcTextSizeA(22 * s, 1e9f, 0, note.c_str()).x;
      dl->AddText(fS, 22 * s, {vx - nw, yy + 11 * s}, A(kDim), note.c_str());
      vxr = vx - nw - 18 * s;
    }
    const float vstart = vxr - vw - (on ? 30 * s : 0);
    if (it.kind == Item::kSlider) {
      // Bar under the number.
      const float bw = 260 * s, bx0 = vstart - bw - 26 * s, by = yy + rowH * 0.5f;
      const float f = float(value_[i] - it.lo) / float(it.hi - it.lo);
      dl->AddRectFilled({bx0, by - 3 * s}, {bx0 + bw, by + 3 * s}, A(IM_COL32(70, 66, 62, 255)));
      dl->AddRectFilled({bx0, by - 3 * s}, {bx0 + bw * f, by + 3 * s}, A(on ? kRed : kRedDark));
      dl->AddCircleFilled({bx0 + bw * f, by}, 8 * s, A(on ? kWhite : kGrey));
    }
    if (on) {
      // Arrows either side of the value, like the game's selectors.
      const float cy = yy + rowH * 0.52f, ah = 9 * s;
      const float lxa = vxr - vw - 34 * s, rxa = vxr + 4 * s;
      dl->AddTriangleFilled({lxa, cy}, {lxa + ah * 1.3f, cy - ah}, {lxa + ah * 1.3f, cy + ah}, A(kRed));
      dl->AddTriangleFilled({rxa + ah * 1.3f, cy}, {rxa, cy - ah}, {rxa, cy + ah}, A(kRed));
      dl->AddText(fI, 32 * s, {vxr - vw - 12 * s, yy + 4 * s}, A(kWhite), val.c_str());
    } else {
      dl->AddText(fI, 32 * s, {vxr - vw, yy + 4 * s}, A(kGrey), val.c_str());
    }
  }
  dl->PopClipRect();

  // Hints, bottom right like the game's: [ok] change  [back] back to the game.
  const char *ok, *back;
  ButtonNames(&ok, &back);
  const float hs = 24 * s;
  const float gs = 38 * s;  // icon height, centred on the hint text
  float hx = px0 + 40 * s;
  const float hy = H - 78 * s;
  const ImVec2 gp{0, hy + 2 * s + hs * 0.5f - gs * 0.5f};
  rex::ui::ImmediateTexture* left = ButtonGlyph("DPAD_LEFT", nullptr, "Left");
  rex::ui::ImmediateTexture* right = ButtonGlyph("DPAD_RIGHT", nullptr, "Right");
  if (left && right) {
    hx += Prompt(dl, fS, gs, {hx, gp.y}, left, "") + 2 * s;
    hx += Prompt(dl, fS, gs, {hx, gp.y}, right, "") + 10 * s;
  } else {
    dl->AddText(fS, hs, {hx, hy + 2 * s}, A(kWhite), "\xE2\x97\x84 \xE2\x96\xBA");  // ◄ ►
    hx += fS->CalcTextSizeA(hs, 1e9f, 0, "\xE2\x97\x84 \xE2\x96\xBA").x + 10 * s;
  }
  dl->AddText(fS, hs, {hx, hy + 2 * s}, A(kGrey), Tr(kHintChange));
  hx += fS->CalcTextSizeA(hs, 1e9f, 0, Tr(kHintChange)).x + 34 * s;
  hx += Prompt(dl, fS, gs, {hx, gp.y}, ButtonGlyph("A", "keybind_a", nullptr), ok) + 10 * s;
  dl->AddText(fS, hs, {hx, hy + 2 * s}, A(kGrey), Tr(kHintSelect));
  hx += fS->CalcTextSizeA(hs, 1e9f, 0, Tr(kHintSelect)).x + 34 * s;
  hx += Prompt(dl, fS, gs, {hx, gp.y}, ButtonGlyph("B", "keybind_b", nullptr), back) + 10 * s;
  dl->AddText(fS, hs, {hx, hy + 2 * s}, A(kGrey), Tr(kHintBack));
  dl->AddText(fS, 20 * s, {px0 + 40 * s, H - 40 * s}, A(kDim), Tr(kHintKeys));
  ImGui::End();
}

// ---------------------------------------------------------------------- state
rex::ui::ImGuiDrawer* g_drawer = nullptr;
std::function<void(std::function<void()>)> g_post_ui;
std::unique_ptr<MenuDialog> g_dialog;
std::atomic<bool> g_open{false};
std::atomic<bool> g_closing{false};

}  // namespace

void MenuAddFonts(ImFontAtlas* atlas) {
#ifdef _WIN32
  static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2010, 0x203A, 0x25A0, 0x25FF, 0};
  const char* bold = "C:\\Windows\\Fonts\\segoeuib.ttf";
  const char* semi = "C:\\Windows\\Fonts\\seguisb.ttf";
  std::error_code ec;
  if (std::filesystem::exists(bold, ec)) {
    ImFontConfig c;
    c.OversampleH = 2;
    g_font_title = atlas->AddFontFromFileTTF(bold, 84.0f, &c, ranges);
    g_font_item = atlas->AddFontFromFileTTF(bold, 34.0f, &c, ranges);
  }
  if (std::filesystem::exists(semi, ec)) {
    ImFontConfig c;
    c.OversampleH = 2;
    g_font_small = atlas->AddFontFromFileTTF(semi, 28.0f, &c, ranges);
  } else {
    g_font_small = g_font_item;
  }
#else
  (void)atlas;
#endif
}

void MenuInit(rex::ui::ImGuiDrawer* drawer, rex::ui::ImmediateDrawer* immediate,
              std::function<void(std::function<void()>)> post_ui) {
  g_drawer = drawer;
  g_imm = immediate;
  g_post_ui = std::move(post_ui);
}

void MenuToggle() {
  if (!g_drawer) return;
  if (g_dialog) {
    if (g_closing) return;
    // Closing from the dialog's own draw: destroy it after this frame.
    g_closing = true;
    if (g_post_ui) {
      g_post_ui([] {
        g_dialog.reset();
        g_closing = false;
        g_open = false;
        BlockGuestInput(false);
        REXLOG_INFO("afro menu: closed");
      });
    }
    return;
  }
  if (g_closing) return;
  BlockGuestInput(true);
  g_dialog = std::make_unique<MenuDialog>(g_drawer);
  g_open = true;
  REXLOG_INFO("afro menu: opened");
}

void MenuGuestFrame() {
  // Hold both stick clicks for half a second to open it.
  static clk::time_point held_since{};
  static bool latched = false;
  if (!g_open) {
    const uint32_t b = PadButtons();
    if ((b & (kL3 | kR3)) == (kL3 | kR3)) {
      if (held_since.time_since_epoch().count() == 0) held_since = clk::now();
      if (!latched && clk::now() - held_since > std::chrono::milliseconds(500)) {
        latched = true;
        if (g_post_ui) g_post_ui([] { if (!g_open) MenuToggle(); });
      }
    } else {
      held_since = {};
      latched = false;
    }
  }
  // Pause: the game's frame waits here while the menu is open.
  while (g_open) std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void MenuShutdown() {
  g_open = false;
  // Before the app destroys the immediate drawer the icons belong to.
  g_dialog.reset();
  g_glyphs.clear();
  g_imm = nullptr;
}

}  // namespace afro
