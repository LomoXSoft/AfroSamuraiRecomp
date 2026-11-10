// Startup language picker.
// Config: afro_language.cfg next to the executable ("language=N", "ask=0/1").
// The dialog is shown when there is no config, when ask=1, when Shift is held
// at launch, or with --choose_language. An explicit --user_language wins.
#include "language_picker.h"

#include <rex/cvar.h>

#include <cctype>
#include <cstring>
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
#include <shellapi.h>
#include <shlobj.h>
#include <ctime>
#include <tlhelp32.h>
#include <vector>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
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
    {"rt", "render_target_path_d3d12", L"Render path (GPU)", 3, {"", "rtv", "rov"},
     {L"RTV (default)", L"RTV", L"ROV (experimental)"}},
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

// Mouse camera sliders (Advanced settings). Stored as integers: sensitivity
// and dead zone in hundredths, smoothing in ms. The defaults match the
// runtime's (mnk driver) cvar defaults.
struct MouseSlider {
  const char* key;   // cfg key
  const char* cvar;
  int lo, hi, def, step;
  int scale;         // 100 = shown and sent as value/100, 1 = as is
  const wchar_t* unit;
};
const MouseSlider kMouse[3] = {
    {"mouse_sensitivity", "mnk_sensitivity", 10, 500, 200, 5, 100, L""},
    {"mouse_deadzone", "afro_mouse_deadzone", 0, 80, 55, 1, 100, L""},
    {"mouse_smoothing_ms", "afro_mouse_window_ms", 1, 50, 5, 1, 1, L" ms"},
};
std::wstring MouseText(int r, int v) {
  wchar_t buf[32];
  if (kMouse[r].scale == 100)
    swprintf(buf, 32, L"%d.%02d%ls", v / 100, v % 100, kMouse[r].unit);
  else
    swprintf(buf, 32, L"%d%ls", v, kMouse[r].unit);
  return buf;
}
std::string MouseCvarValue(int r, int v) {
  char buf[32];
  if (kMouse[r].scale == 100)
    snprintf(buf, 32, "%d.%02d", v / 100, v % 100);
  else
    snprintf(buf, 32, "%d", v);
  return buf;
}

struct Options { int fps = 0; /* 0=Original(30), 60=Experimental(60) */ int sepia = 0; /* 0 on, 1 off */ int vsync = 0; /* 0 on, 1 off (guest vblank timer) */ int invuln = 0; /* Difficulty: 0 Normal, 1 Game Journalist (invulnerable); saved */ int tick = 0; /* legacy only: migrate 60+non-orig tick → Experimental */ int lore = 0; /* Loading screens: 0 full lore (default), 1 fast */ bool window = false; bool exclusive = false; /* Display: borderless (default), window, exclusive fullscreen */ int scale = 1; int adv[kAdvN] = {}; int control = 0; /* 0 auto (keyboard, a pad takes over when used), 1 controller only, 2 keyboard icons */ int mouse[3] = {kMouse[0].def, kMouse[1].def, kMouse[2].def}; std::string keys[24]; /* key bindings, empty = default (see kKeyActions) */ std::string padmap[3]; /* controller remap per pad family (kPadFamilies), empty = as the pad is */ };

struct KeyAction { const char* cvar; const char* def; };
constexpr int kKeyN = 24;
static_assert(kKeyN == sizeof(Options::keys) / sizeof(Options::keys[0]));
const KeyAction kKeyActions[kKeyN] = {
    {"keybind_lstick_up", "W"},           {"keybind_lstick_down", "S"},
    {"keybind_lstick_left", "A"},         {"keybind_lstick_right", "D"},
    {"keybind_x", "LMB"},                 {"keybind_y", "RMB"},
    {"keybind_a", "Space"},               {"keybind_b", "E"},
    {"keybind_right_trigger", "Q"},       {"keybind_left_trigger", "R"},
    {"keybind_left_shoulder", "1"},       {"keybind_right_shoulder", "3"},
    {"keybind_rstick_up", "Shift+Up"},    {"keybind_rstick_down", "Shift+Down"},
    {"keybind_rstick_left", "Shift+Left"}, {"keybind_rstick_right", "Shift+Right"},
    {"keybind_dpad_up", "Up"},            {"keybind_dpad_down", "Down"},
    {"keybind_dpad_left", "Left"},        {"keybind_dpad_right", "Right"},
    {"keybind_start", "X,Return"},        {"keybind_back", "Escape"},
    {"keybind_lstick_press", "F"},        {"keybind_rstick_press", "K"},
};
// Config key for an action: bind_<cvar without "keybind_">.
std::string KeyCfgName(int i) { return std::string("bind_") + (kKeyActions[i].cvar + 8); }

// Controller remap (Key bindings -> Controller). The runtime's afro_padmap_<family>
// cvar lists, for each button the game reads (kPadOut order), the physical button
// that drives it, both named by Xbox position. Xbox also covers generic pads and
// the Steam Deck; Nintendo positions are physical (the bottom button is Xbox A).
constexpr int kPadOutN = 16;
const char* const kPadOut[kPadOutN] = {"A",    "B",     "X",  "Y",  "LB", "RB",   "LT",   "RT",
                                       "Back", "Start", "LS", "RS", "Up", "Down", "Left", "Right"};
const char* const kPadFamilies[3] = {"xbox", "playstation", "nintendo"};
// Rows of the window: the manual's actions, each the game button it presses.
const int kPadRows[kPadOutN] = {2, 3, 1, 0, 10, 4, 6, 7, 5, 11, 13, 12, 14, 15, 9, 8};

void PadIdentity(uint8_t m[kPadOutN]) {
  for (int i = 0; i < kPadOutN; ++i) m[i] = uint8_t(i);
}
// "A,B,..." -> map; anything that is not 16 known names leaves the pad as it is.
void PadParse(const std::string& s, uint8_t m[kPadOutN]) {
  PadIdentity(m);
  if (s.empty()) return;
  uint8_t t[kPadOutN];
  size_t pos = 0;
  for (int o = 0; o < kPadOutN; ++o) {
    size_t end = s.find(',', pos);
    if ((end == std::string::npos) != (o == kPadOutN - 1)) return;
    std::string tok = s.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    int found = -1;
    for (int k = 0; k < kPadOutN; ++k)
      if (tok == kPadOut[k]) found = k;
    if (found < 0) return;
    t[o] = uint8_t(found);
    pos = end + 1;
  }
  std::copy(t, t + kPadOutN, m);
}
std::string PadString(const uint8_t m[kPadOutN]) {
  bool identity = true;
  for (int i = 0; i < kPadOutN; ++i) identity = identity && m[i] == i;
  if (identity) return {};
  std::string s;
  for (int i = 0; i < kPadOutN; ++i) s += std::string(i ? "," : "") + kPadOut[m[i]];
  return s;
}


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
// Launcher: Play, Settings and saved games. Settings has three tabs: Graphics,
// Gameplay and Experimental (options that are not fully tested).
struct UiText {
  const wchar_t *title, *play, *settings, *remember, *hint, *tab_gfx, *tab_play, *tab_exp,
      *lang, *disp, *full, *win, *scale, *fps, *fps30, *fps60, *vsync, *vsync_on, *vsync_off,
      *ctrl, *ctrl_auto, *ctrl_pad, *ctrl_kb, *mouse_sens, *mouse_dz, *mouse_win, *diff, *diff_normal, *diff_journo, *diff_note,
      *xwarn, *sepia, *blood, *dof, *bloom, *focus, *fx_on, *fx_off, *gfx, *gfx_rtv, *gfx_rov,
      *adv, *def, *off, *on, *ok, *cancel;
};
const UiText kText[] = {
    {L"afrorecomp_v" AFRO_VERSION_W L" - Launcher", L"Play", L"Settings...",
     L"Skip this window at launch", L"Hold Shift at launch to show it again.",
     L"Graphics", L"Gameplay", L"Experimental",
     L"Language:", L"Display:", L"Borderless fullscreen", L"Window", L"Internal resolution:",
     L"Frame rate:", L"30 (original, default)", L"60",
     L"VSync:", L"On", L"Off (recommended at 60)",
     L"Controls:", L"Auto (keyboard + controller)", L"Controller", L"Keyboard",
     L"Mouse sensitivity:", L"Mouse dead zone:", L"Mouse smoothing:",
     L"Difficulty:", L"Normal (default)", L"Game Journalist",
     L"Game Journalist: you can't die.",
     L"These options are not fully tested. They may cause visual problems.",
     L"Sepia filter:", L"Blood effect:", L"Depth of field:", L"Bloom:", L"Focus effect:",
     L"On (default)", L"Off",
     L"Render path:", L"RTV (default)", L"ROV",
     L"Advanced options...", L"Default", L"Off", L"On", L"OK", L"Cancel"},
    {L"afrorecomp_v" AFRO_VERSION_W L" - Lanzador", L"Jugar", L"Ajustes...",
     L"Saltar esta ventana al iniciar", L"Mant\u00e9n May\u00fas al iniciar para volver a verla.",
     L"Gr\u00e1ficos", L"Jugabilidad", L"Experimental",
     L"Idioma:", L"Pantalla:", L"Pantalla completa sin bordes", L"Ventana", L"Resoluci\u00f3n interna:",
     L"Fotogramas:", L"30 (original, por defecto)", L"60",
     L"VSync:", L"Activado", L"Desactivado (recomendado a 60)",
     L"Control:", L"Autom\u00e1tico (teclado + mando)", L"Mando", L"Teclado",
     L"Sensibilidad del rat\u00f3n:", L"Zona muerta del rat\u00f3n:", L"Suavizado del rat\u00f3n:",
     L"Dificultad:", L"Normal (por defecto)", L"Periodista de videojuegos",
     L"Periodista de videojuegos: no puedes morir.",
     L"Estas opciones no est\u00e1n probadas del todo. Pueden causar fallos visuales.",
     L"Filtro sepia:", L"Efecto de sangre:", L"Profundidad de campo:", L"Resplandor (bloom):",
     L"Efecto de concentraci\u00f3n:",
     L"Activado (por defecto)", L"Desactivado",
     L"Ruta de render:", L"RTV (por defecto)", L"ROV",
     L"Opciones avanzadas...", L"Por defecto", L"Desactivado", L"Activado", L"Aceptar", L"Cancelar"},
    {L"afrorecomp_v" AFRO_VERSION_W L" - Lanceur", L"Jouer", L"R\u00e9glages...",
     L"Ne plus afficher cette fen\u00eatre au d\u00e9marrage",
     L"Maintenez Maj au d\u00e9marrage pour la revoir.",
     L"Graphismes", L"Jouabilit\u00e9", L"Exp\u00e9rimental",
     L"Langue :", L"Affichage :", L"Plein \u00e9cran sans bordure", L"Fen\u00eatre", L"R\u00e9solution interne :",
     L"Images/s :", L"30 (original, par d\u00e9faut)", L"60",
     L"VSync :", L"Activ\u00e9e", L"D\u00e9sactiv\u00e9e (conseill\u00e9 \u00e0 60)",
     L"Contr\u00f4les :", L"Auto (clavier + manette)", L"Manette", L"Clavier",
     L"Sensibilit\u00e9 souris :", L"Zone morte souris :", L"Lissage souris :",
     L"Difficult\u00e9 :", L"Normal (par d\u00e9faut)", L"Journaliste jeu vid\u00e9o",
     L"Journaliste jeu vid\u00e9o : tu ne peux pas mourir.",
     L"Ces options ne sont pas enti\u00e8rement test\u00e9es. Elles peuvent causer des d\u00e9fauts visuels.",
     L"Filtre s\u00e9pia :", L"Effet de sang :", L"Profondeur de champ :", L"Bloom :",
     L"Effet de concentration :",
     L"Activ\u00e9 (par d\u00e9faut)", L"D\u00e9sactiv\u00e9",
     L"Chemin de rendu :", L"RTV (par d\u00e9faut)", L"ROV",
     L"Options avanc\u00e9es...", L"Par d\u00e9faut", L"D\u00e9sactiv\u00e9", L"Activ\u00e9", L"OK", L"Annuler"},
    {L"afrorecomp_v" AFRO_VERSION_W L" - Avvio", L"Gioca", L"Impostazioni...",
     L"Salta questa finestra all'avvio", L"Tieni premuto Maiusc all'avvio per rivederla.",
     L"Grafica", L"Gameplay", L"Sperimentale",
     L"Lingua:", L"Schermo:", L"Schermo intero senza bordi", L"Finestra", L"Risoluzione interna:",
     L"Fotogrammi:", L"30 (originale, predefinito)", L"60",
     L"VSync:", L"Attivato", L"Disattivato (consigliato a 60)",
     L"Controlli:", L"Automatico (tastiera + controller)", L"Controller", L"Tastiera",
     L"Sensibilit\u00e0 mouse:", L"Zona morta mouse:", L"Smussatura mouse:",
     L"Difficolt\u00e0:", L"Normale (predefinito)", L"Giornalista di videogiochi",
     L"Giornalista di videogiochi: non puoi morire.",
     L"Queste opzioni non sono del tutto testate. Possono causare difetti visivi.",
     L"Filtro seppia:", L"Effetto sangue:", L"Profondit\u00e0 di campo:", L"Bloom:",
     L"Effetto concentrazione:",
     L"Attivato (predefinito)", L"Disattivato",
     L"Percorso di rendering:", L"RTV (predefinito)", L"ROV",
     L"Opzioni avanzate...", L"Predefinito", L"Disattivato", L"Attivato", L"OK", L"Annulla"},
};
// Main window.
constexpr int kIdPlay = IDOK, kIdSettingsBtn = 111, kIdSavesBtn = 116;
// Settings window.
constexpr int kIdTabBase = 170,  // 170..173 tab buttons
              kIdCombo = 101, kIdLangLbl = 103, kIdCheck = 102, kIdHint = 104,
              kIdFpsLbl = 105, kIdFps = 106, kIdDispLbl = 107, kIdDisp = 108,
              kIdScaleLbl = 109, kIdScale = 110, kIdAdvBtn = 112, kIdXWarn = 113,
              kIdXGfxLbl = 117, kIdXGfx = 118, kIdCheatInv = 122 /* Difficulty combo */, kIdSetOk = 123,
              kIdSetCancel = 124, kIdCtrlLbl = 125, kIdCtrl = 126,
              kIdXMouse = 130,     // 130..132 sliders
              kIdXMouseVal = 133,  // 133..135 their values
              kIdXVsyncLbl = 136, kIdXVsync = 137,
              kIdXMouseLbl = 143,  // 143..145 slider labels
              kIdFxLbl = 150,      // 150..154 effect labels
              kIdFx = 160,         // 160..164 effect combos
              kIdKeysBtn = 146,    // Controls tab: Key bindings...
              kIdDiffLbl = 147, kIdDiffNote = 148,
              kIdAdvBase = 200;

constexpr int kFpsValues[] = {0, 60};  // 30 (original), 60
// Effects on the Experimental tab: sepia has its own option, the others are
// Advanced-list cvars (index 0 = on, 1 = "false").
const char* const kFxAdvKey[5] = {nullptr, "fx_blood", "fx_dof", "fx_bloom", "fx_focus"};
int AdvIndex(const char* key) {
  for (int i = 0; i < kAdvN; ++i)
    if (std::strcmp(kAdv[i].key, key) == 0) return i;
  return -1;
}
// Advanced-list entries the Settings window shows (kept without --advanced).
bool UserFacingAdv(int i) {
  if (std::strcmp(kAdv[i].key, "rt") == 0) return true;
  for (const char* k : kFxAdvKey)
    if (k && std::strcmp(kAdv[i].key, k) == 0) return true;
  return false;
}

struct PickState { int sel = 0; bool remember = false; bool done = false; bool play = false; Options opt; };

void FillCombo(HWND h, int id, std::initializer_list<const wchar_t*> items) {
  HWND c = GetDlgItem(h, id);
  int cur = static_cast<int>(SendMessageW(c, CB_GETCURSEL, 0, 0));
  SendMessageW(c, CB_RESETCONTENT, 0, 0);
  for (auto* i : items) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)i);
  SendMessageW(c, CB_SETCURSEL, cur < 0 ? 0 : cur, 0);
}

// Save management (same language order as kText).
// Third Display choice (the first two are kText full / win).
// Gameplay -> Loading screens (label, full, fast, note).
struct LoreText { const wchar_t *lbl, *full, *fast, *note; };
const LoreText kLoreText[] = {
    {L"Loading screens:", L"Full lore (default)", L"Fast",
     L"Full lore: the loading story plays one full pass, like on Xbox 360. Once the "
     L"level is loaded, any button continues."},
    {L"Pantallas de carga:", L"Historia completa (predeterminado)", L"R\u00e1pidas",
     L"Historia completa: la historia de la carga se ve entera, como en Xbox 360. "
     L"Cualquier bot\u00f3n la salta cuando el nivel ya ha cargado."},
    {L"\u00c9crans de chargement :", L"Histoire compl\u00e8te (par d\u00e9faut)", L"Rapides",
     L"Histoire compl\u00e8te : l'histoire du chargement est jou\u00e9e en entier, comme "
     L"sur Xbox 360. N'importe quel bouton la passe une fois le niveau charg\u00e9."},
    {L"Schermate di caricamento:", L"Storia completa (predefinito)", L"Veloci",
     L"Storia completa: la storia del caricamento viene mostrata per intero, come su "
     L"Xbox 360. Qualsiasi tasto la salta quando il livello \u00e8 caricato."},
};
constexpr int kIdLoreLbl = 175, kIdLore = 176, kIdLoreNote = 177;

const wchar_t* const kExclText[] = {L"Exclusive fullscreen", L"Pantalla completa exclusiva",
                                    L"Plein écran exclusif", L"Schermo intero esclusivo"};

struct SaveText {
  const wchar_t *manage, *folder, *open, *backup, *list, *restore, *del, *close, *done, *empty,
      *fail, *pick, *restore_q, *restore_ok, *del_q, *del_ok, *running, *none, *openbk;
};
const SaveText kSaveText[] = {
    {L"Manage saved games...", L"Saves folder:", L"Open saves folder", L"Back up saves",
     L"Backups (newest first):", L"Restore selected backup", L"Delete save data...", L"Close",
     L"Saves backed up to:\n%s\n\nOpen the backup folder?", L"There are no saves yet in:\n%s",
     L"The operation failed:\n%s", L"Select a backup in the list first.",
     L"Restore this backup?\n%s\n\nYour current saves are replaced by it. A safety backup of the current saves is made first.",
     L"Backup restored.\nA copy of the previous saves was kept in:\n%s",
     L"DELETE all save data?\n\nThis removes your saved games (the shader cache and the backups are kept). A safety backup is made first.\n\nThis cannot be undone from the game.",
     L"Save data deleted.\nA safety backup was kept in:\n%s",
     L"Afro Samurai is running. Close the game first.", L"(no backups yet)", L"Open backups folder"},
    {L"Gestionar partidas guardadas...", L"Carpeta de partidas:", L"Abrir carpeta de partidas", L"Copia de seguridad",
     L"Copias (la m\u00e1s reciente primero):", L"Restaurar la copia seleccionada", L"Borrar datos guardados...", L"Cerrar",
     L"Partidas copiadas en:\n%s\n\n\u00bfAbrir la carpeta de copias?", L"Todav\u00eda no hay partidas en:\n%s",
     L"La operaci\u00f3n ha fallado:\n%s", L"Selecciona antes una copia de la lista.",
     L"\u00bfRestaurar esta copia?\n%s\n\nTus partidas actuales se sustituyen por ella. Antes se hace una copia de seguridad de las actuales.",
     L"Copia restaurada.\nLas partidas anteriores se han guardado en:\n%s",
     L"\u00bfBORRAR todos los datos guardados?\n\nSe eliminan tus partidas (la cach\u00e9 de shaders y las copias se conservan). Antes se hace una copia de seguridad.\n\nNo se puede deshacer desde el juego.",
     L"Datos guardados borrados.\nSe ha guardado una copia de seguridad en:\n%s",
     L"Afro Samurai se est\u00e1 ejecutando. Cierra antes el juego.", L"(todav\u00eda no hay copias)", L"Abrir carpeta de copias de seguridad"},
    {L"G\u00e9rer les sauvegardes...", L"Dossier des sauvegardes :", L"Ouvrir les sauvegardes", L"Copier les sauvegardes",
     L"Copies (la plus r\u00e9cente d'abord) :", L"Restaurer la copie s\u00e9lectionn\u00e9e", L"Supprimer les sauvegardes...", L"Fermer",
     L"Sauvegardes copi\u00e9es dans :\n%s\n\nOuvrir le dossier des copies ?", L"Aucune sauvegarde pour l'instant dans :\n%s",
     L"L'op\u00e9ration a \u00e9chou\u00e9 :\n%s", L"S\u00e9lectionne d'abord une copie dans la liste.",
     L"Restaurer cette copie ?\n%s\n\nTes sauvegardes actuelles seront remplac\u00e9es. Une copie de s\u00e9curit\u00e9 des actuelles est faite avant.",
     L"Copie restaur\u00e9e.\nLes sauvegardes pr\u00e9c\u00e9dentes ont \u00e9t\u00e9 gard\u00e9es dans :\n%s",
     L"SUPPRIMER toutes les sauvegardes ?\n\nTes parties sauvegard\u00e9es sont effac\u00e9es (le cache des shaders et les copies sont conserv\u00e9s). Une copie de s\u00e9curit\u00e9 est faite avant.\n\nImpossible d'annuler depuis le jeu.",
     L"Sauvegardes supprim\u00e9es.\nUne copie de s\u00e9curit\u00e9 a \u00e9t\u00e9 gard\u00e9e dans :\n%s",
     L"Afro Samurai est en cours d'ex\u00e9cution. Ferme d'abord le jeu.", L"(aucune copie pour l'instant)", L"Ouvrir le dossier des copies"},
    {L"Gestisci salvataggi...", L"Cartella dei salvataggi:", L"Apri cartella salvataggi", L"Backup salvataggi",
     L"Backup (il pi\u00f9 recente per primo):", L"Ripristina il backup selezionato", L"Elimina dati salvati...", L"Chiudi",
     L"Salvataggi copiati in:\n%s\n\nAprire la cartella dei backup?", L"Ancora nessun salvataggio in:\n%s",
     L"Operazione non riuscita:\n%s", L"Seleziona prima un backup nell'elenco.",
     L"Ripristinare questo backup?\n%s\n\nI salvataggi attuali verranno sostituiti. Prima viene fatto un backup di sicurezza di quelli attuali.",
     L"Backup ripristinato.\nI salvataggi precedenti sono stati conservati in:\n%s",
     L"ELIMINARE tutti i dati salvati?\n\nI salvataggi vengono cancellati (la cache degli shader e i backup restano). Prima viene fatto un backup di sicurezza.\n\nNon si pu\u00f2 annullare dal gioco.",
     L"Dati salvati eliminati.\nUn backup di sicurezza \u00e8 stato conservato in:\n%s",
     L"Afro Samurai \u00e8 in esecuzione. Chiudi prima il gioco.", L"(ancora nessun backup)", L"Apri cartella dei backup"},
};
constexpr int kIdSvFolder = 300, kIdSvOpen = 301, kIdSvBackup = 302, kIdSvList = 303,
              kIdSvRestore = 304, kIdSvDelete = 305, kIdSvOpenBk = 306;

void ApplyMainTexts(HWND h, int sel) {
  const UiText& t = kText[sel];
  SetWindowTextW(h, t.title);
  SetDlgItemTextW(h, kIdLangLbl, t.lang);
  SetDlgItemTextW(h, kIdPlay, t.play);
  SetDlgItemTextW(h, kIdSettingsBtn, t.settings);
  SetDlgItemTextW(h, kIdSavesBtn, kSaveText[sel].manage);
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
      if (k == 0 && a.labels[0] && txt.find(L'(') == std::wstring::npos)
        txt += std::wstring(L" (") + t.def + L")";
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

// Key bindings (Settings -> Controls -> Key bindings...). Each action is one of
// the runtime's keybind_* cvars; the value is a key name the runtime parses
// (src/ui/keybinds.cpp), optionally with Shift+/Ctrl+/Alt+, alternatives
// separated by commas. Empty in the config = the runtime default.
struct KeyText {
  const wchar_t *tab, *button, *title, *hint, *press, *reset;
  const wchar_t* action[kKeyN];
};
const KeyText kKeyText[] = {
    {L"Controls", L"Key bindings...", L"Key bindings",
     L"Click an action, then press the new key or mouse button. Hold Shift, Ctrl or Alt for a "
     L"combination. A key already in use moves to the action you changed.",
     L"Press a key...", L"Restore defaults",
     {L"Move forward", L"Move back", L"Move left", L"Move right", L"Light attack", L"Heavy attack",
      L"Jump", L"Kick / menu back", L"Guard", L"Focus", L"Overfocus", L"RB button", L"Camera up",
      L"Camera down", L"Camera left", L"Camera right", L"D-pad up", L"D-pad down (compass)",
      L"D-pad left", L"D-pad right", L"Pause (Start)", L"Skill menu (Back)", L"Dash",
      L"Center camera"}},
    {L"Controles", L"Asignar teclas...", L"Asignar teclas",
     L"Haz clic en una acción y pulsa la nueva tecla o botón del ratón. Mantén "
     L"Mayús, Ctrl o Alt para una combinación. Una tecla ya usada pasa a la acción "
     L"que has cambiado.",
     L"Pulsa una tecla...", L"Restablecer",
     {L"Avanzar", L"Retroceder", L"Izquierda", L"Derecha", L"Ataque ligero", L"Ataque fuerte",
      L"Saltar", L"Patada / atrás en menús", L"Bloquear", L"Concentración",
      L"Sobreconcentración", L"Botón RB", L"Cámara arriba", L"Cámara abajo",
      L"Cámara izquierda", L"Cámara derecha", L"Cruceta arriba", L"Cruceta abajo (brújula)",
      L"Cruceta izquierda", L"Cruceta derecha", L"Pausa (Start)", L"Técnicas (Back)",
      L"Esprintar", L"Centrar cámara"}},
    {L"Contrôles", L"Touches...", L"Touches",
     L"Clique sur une action, puis appuie sur la nouvelle touche ou le bouton de souris. Maintiens "
     L"Maj, Ctrl ou Alt pour une combinaison. Une touche déjà utilisée passe à "
     L"l'action modifiée.",
     L"Appuie sur une touche...", L"Par défaut",
     {L"Avancer", L"Reculer", L"Gauche", L"Droite", L"Attaque légère", L"Attaque lourde", L"Sauter",
      L"Coup de pied / retour menu", L"Parer", L"Concentration", L"Surconcentration", L"Bouton RB",
      L"Caméra haut", L"Caméra bas", L"Caméra gauche", L"Caméra droite",
      L"Croix haut", L"Croix bas (boussole)", L"Croix gauche", L"Croix droite", L"Pause (Start)",
      L"Techniques (Back)", L"Sprint", L"Centrer la caméra"}},
    {L"Controlli", L"Assegna tasti...", L"Assegna tasti",
     L"Fai clic su un'azione, poi premi il nuovo tasto o pulsante del mouse. Tieni premuto Maiusc, "
     L"Ctrl o Alt per una combinazione. Un tasto già usato passa all'azione modificata.",
     L"Premi un tasto...", L"Predefiniti",
     {L"Avanti", L"Indietro", L"Sinistra", L"Destra", L"Attacco leggero", L"Attacco pesante", L"Salto",
      L"Calcio / indietro nei menu", L"Parata", L"Concentrazione", L"Iperconcentrazione", L"Tasto RB",
      L"Camera su", L"Camera giù", L"Camera sinistra", L"Camera destra", L"Croce su",
      L"Croce giù (bussola)", L"Croce sinistra", L"Croce destra", L"Pausa (Start)",
      L"Abilità (Back)", L"Scatto", L"Centra la camera"}},
};

// Controller page of the Key bindings window. action[] follows kPadRows.
struct PadText {
  const wchar_t *dev_lbl, *dev[4], *hint, *action[kPadOutN], *dpad[4], *ls, *rs, *face_ps[4],
      *back_xb, *start_xb, *back_ps;
};
const PadText kPadText[] = {
    {L"Controller:",
     {L"Keyboard and mouse", L"Xbox, generic pads and Steam Deck", L"PlayStation (DualSense, DualShock)",
      L"Nintendo Switch"},
     L"Choose the button for each action. A button already in use swaps with the action you "
     L"changed. Buttons are named as printed on that controller.",
     {L"Light attack", L"Heavy attack", L"Kick / menu back", L"Jump", L"Dash", L"Overfocus", L"Focus",
      L"Guard", L"RB button", L"Center camera", L"Ninja Ninja compass", L"D-pad up", L"D-pad left",
      L"D-pad right", L"Pause", L"Skill menu"},
     {L"D-pad up", L"D-pad down", L"D-pad left", L"D-pad right"},
     L"Left stick click", L"Right stick click", {L"Cross", L"Circle", L"Square", L"Triangle"},
     L"Back / View", L"Start / Menu", L"Create / Share"},
    {L"Mando:",
     {L"Teclado y ratón", L"Xbox, mandos genéricos y Steam Deck", L"PlayStation (DualSense, DualShock)",
      L"Nintendo Switch"},
     L"Elige el botón de cada acción. Un botón ya usado se intercambia con la acción que has "
     L"cambiado. Los botones se llaman como aparecen en ese mando.",
     {L"Ataque ligero", L"Ataque fuerte", L"Patada / atrás en menús", L"Saltar", L"Esprintar",
      L"Sobreconcentración", L"Concentración", L"Bloquear", L"Botón RB", L"Centrar cámara",
      L"Brújula de Ninja Ninja", L"Cruceta arriba", L"Cruceta izquierda", L"Cruceta derecha",
      L"Pausa", L"Menú de técnicas"},
     {L"Cruceta arriba", L"Cruceta abajo", L"Cruceta izquierda", L"Cruceta derecha"},
     L"Clic stick izquierdo", L"Clic stick derecho", {L"Equis", L"Círculo", L"Cuadrado", L"Triángulo"},
     L"Back / Vista", L"Start / Menú", L"Crear / Share"},
    {L"Manette :",
     {L"Clavier et souris", L"Xbox, manettes génériques et Steam Deck",
      L"PlayStation (DualSense, DualShock)", L"Nintendo Switch"},
     L"Choisis le bouton de chaque action. Un bouton déjà utilisé est échangé avec l'action "
     L"modifiée. Les boutons portent le nom inscrit sur cette manette.",
     {L"Attaque légère", L"Attaque lourde", L"Coup de pied / retour menu", L"Sauter", L"Sprint",
      L"Surconcentration", L"Concentration", L"Parer", L"Bouton RB", L"Centrer la caméra",
      L"Boussole de Ninja Ninja", L"Croix haut", L"Croix gauche", L"Croix droite", L"Pause",
      L"Menu des techniques"},
     {L"Croix haut", L"Croix bas", L"Croix gauche", L"Croix droite"},
     L"Clic stick gauche", L"Clic stick droit", {L"Croix", L"Rond", L"Carré", L"Triangle"},
     L"Back / Affichage", L"Start / Menu", L"Créer / Share"},
    {L"Controller:",
     {L"Tastiera e mouse", L"Xbox, controller generici e Steam Deck",
      L"PlayStation (DualSense, DualShock)", L"Nintendo Switch"},
     L"Scegli il tasto per ogni azione. Un tasto già usato viene scambiato con l'azione "
     L"modificata. I tasti hanno il nome stampato su quel controller.",
     {L"Attacco leggero", L"Attacco pesante", L"Calcio / indietro nei menu", L"Salto", L"Scatto",
      L"Iperconcentrazione", L"Concentrazione", L"Parata", L"Tasto RB", L"Centra la camera",
      L"Bussola di Ninja Ninja", L"Croce su", L"Croce sinistra", L"Croce destra", L"Pausa",
      L"Menu abilità"},
     {L"Croce su", L"Croce giù", L"Croce sinistra", L"Croce destra"},
     L"Clic stick sinistro", L"Clic stick destro", {L"Croce", L"Cerchio", L"Quadrato", L"Triangolo"},
     L"Back / Visualizza", L"Start / Menu", L"Crea / Share"},
};

// Name of physical button s (Xbox position) on pad family fam, as printed on it.
std::wstring PadButtonName(int sel, int fam, int s) {
  const PadText& p = kPadText[sel];
  if (s >= 12) return p.dpad[s - 12];
  static const wchar_t* const kXb[8] = {L"A", L"B", L"X", L"Y", L"LB", L"RB", L"LT", L"RT"};
  static const wchar_t* const kPs[8] = {nullptr, nullptr, nullptr, nullptr, L"L1", L"R1", L"L2", L"R2"};
  static const wchar_t* const kNx[8] = {L"B", L"A", L"Y", L"X", L"L", L"R", L"ZL", L"ZR"};
  if (s < 8) {
    if (fam == 1) return s < 4 ? p.face_ps[s] : kPs[s];
    return fam == 2 ? kNx[s] : kXb[s];
  }
  switch (s) {
    case 8: return fam == 1 ? p.back_ps : fam == 2 ? L"− (-)" : p.back_xb;
    case 9: return fam == 1 ? L"Options" : fam == 2 ? L"+" : p.start_xb;
    case 10: return fam == 1 ? L"L3" : p.ls;
    default: return fam == 1 ? L"R3" : p.rs;
  }
}

// Windows virtual key -> the runtime's key name.
const char* VkName(UINT vk) {
  static const struct { UINT vk; const char* name; } kNames[] = {
      {VK_SPACE, "Space"},     {VK_ESCAPE, "Escape"},     {VK_RETURN, "Return"},
      {VK_TAB, "Tab"},         {VK_BACK, "Backspace"},    {VK_DELETE, "Delete"},
      {VK_INSERT, "Insert"},   {VK_HOME, "Home"},         {VK_END, "End"},
      {VK_PRIOR, "PageUp"},    {VK_NEXT, "PageDown"},     {VK_LEFT, "Left"},
      {VK_RIGHT, "Right"},     {VK_UP, "Up"},             {VK_DOWN, "Down"},
      {VK_OEM_3, "Backtick"},  {VK_OEM_MINUS, "Minus"},   {VK_OEM_PLUS, "Plus"},
      {VK_OEM_COMMA, "Comma"}, {VK_OEM_PERIOD, "Period"}, {VK_OEM_1, "Semicolon"},
      {VK_OEM_2, "Slash"},     {VK_OEM_5, "Backslash"},   {VK_OEM_4, "LBracket"},
      {VK_OEM_6, "RBracket"},  {VK_OEM_7, "Quote"},       {VK_ADD, "NumpadPlus"},
      {VK_SUBTRACT, "NumpadMinus"}, {VK_MULTIPLY, "NumpadStar"}, {VK_DIVIDE, "NumpadSlash"},
      {VK_PAUSE, "Pause"},     {VK_CAPITAL, "CapsLock"},  {VK_NUMLOCK, "NumLock"},
      {VK_SCROLL, "ScrollLock"}, {VK_LBUTTON, "LMB"},     {VK_RBUTTON, "RMB"},
      {VK_MBUTTON, "MMB"},
  };
  static char buf[16];
  if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
    buf[0] = char(vk);
    buf[1] = 0;
    return buf;
  }
  if (vk >= VK_F1 && vk <= VK_F24) {
    std::snprintf(buf, sizeof(buf), "F%u", vk - VK_F1 + 1);
    return buf;
  }
  if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
    std::snprintf(buf, sizeof(buf), "Numpad%u", vk - VK_NUMPAD0);
    return buf;
  }
  for (auto& n : kNames)
    if (n.vk == vk) return n.name;
  return nullptr;
}

std::wstring KeyLabel(const std::string& v) {
  std::wstring w;
  for (char c : v) {
    if (c == ',') w += L", ";
    else w += wchar_t(c);
  }
  return w;
}

constexpr int kIdKeyBtn = 400;  // 400..423 one button per action
constexpr int kIdKeyLbl = 430;  // 430..453 their labels
constexpr int kIdKeyHint = 460, kIdKeyReset = 461, kIdKeyOk = 462, kIdKeyCancel = 463;
constexpr int kIdKeyDev = 464, kIdKeyDevLbl = 465;
constexpr int kIdPadLbl = 470;    // 470..485 controller action labels
constexpr int kIdPadCombo = 490;  // 490..505 their button lists

struct KeysState {
  bool done = false, accept = false;
  int sel = 0, capturing = -1;
  int dev = 0;  // 0 keyboard, 1.. pad family + 1 (kPadFamilies)
  std::string keys[kKeyN];  // full values (defaults filled in)
  uint8_t pad[3][kPadOutN];  // per family: game button -> physical button
};

void KeysRefresh(HWND h, const KeysState& st) {
  for (int i = 0; i < kKeyN; ++i)
    SetDlgItemTextW(h, kIdKeyBtn + i,
                    st.capturing == i ? kKeyText[st.sel].press : KeyLabel(st.keys[i]).c_str());
  if (st.dev > 0)
    for (int r = 0; r < kPadOutN; ++r)
      SendDlgItemMessageW(h, kIdPadCombo + r, CB_SETCURSEL, st.pad[st.dev - 1][kPadRows[r]], 0);
}

// Keyboard page or the page of one controller family.
void KeysShowDevice(HWND h, KeysState& st) {
  st.capturing = -1;
  const bool kb = st.dev == 0;
  for (int i = 0; i < kKeyN; ++i) {
    ShowWindow(GetDlgItem(h, kIdKeyBtn + i), kb ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, kIdKeyLbl + i), kb ? SW_SHOW : SW_HIDE);
  }
  for (int r = 0; r < kPadOutN; ++r) {
    HWND c = GetDlgItem(h, kIdPadCombo + r);
    ShowWindow(c, kb ? SW_HIDE : SW_SHOW);
    ShowWindow(GetDlgItem(h, kIdPadLbl + r), kb ? SW_HIDE : SW_SHOW);
    if (kb) continue;
    SendMessageW(c, CB_RESETCONTENT, 0, 0);
    for (int s = 0; s < kPadOutN; ++s)
      SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)PadButtonName(st.sel, st.dev - 1, s).c_str());
  }
  SetDlgItemTextW(h, kIdKeyHint, kb ? kKeyText[st.sel].hint : kPadText[st.sel].hint);
  KeysRefresh(h, st);
}

// A key (or mouse button) was pressed while an action waits for one.
void KeysAssign(HWND h, KeysState* st, UINT vk) {
  if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LSHIFT ||
      vk == VK_RSHIFT || vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_LMENU ||
      vk == VK_RMENU || vk == VK_LWIN || vk == VK_RWIN)
    return;  // a bare modifier never fires in the runtime: wait for the key
  const char* name = VkName(vk);
  if (!name) return;
  std::string v;
  if (GetKeyState(VK_CONTROL) & 0x8000) v += "Ctrl+";
  if (GetKeyState(VK_MENU) & 0x8000) v += "Alt+";
  if (GetKeyState(VK_SHIFT) & 0x8000) v += "Shift+";
  v += name;
  const int i = st->capturing;
  // The key moves here: the action that had it gets this action's old key.
  for (int j = 0; j < kKeyN; ++j)
    if (j != i && st->keys[j] == v) st->keys[j] = st->keys[i];
  st->keys[i] = v;
  st->capturing = -1;
  KeysRefresh(h, *st);
}

LRESULT CALLBACK KeysProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto* st = reinterpret_cast<KeysState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (!st) return DefWindowProcW(h, m, w, l);
  if (m == WM_CLOSE) {
    st->done = true;
    DestroyWindow(h);
    return 0;
  }
  if (m == WM_COMMAND && HIWORD(w) == CBN_SELCHANGE) {
    const int id = LOWORD(w);
    const int cur = static_cast<int>(SendMessageW(reinterpret_cast<HWND>(l), CB_GETCURSEL, 0, 0));
    if (id == kIdKeyDev && cur >= 0 && cur <= 3) {
      st->dev = cur;
      KeysShowDevice(h, *st);
    } else if (id >= kIdPadCombo && id < kIdPadCombo + kPadOutN && st->dev > 0 && cur >= 0 &&
               cur < kPadOutN) {
      // The button moves here: the action that had it gets this action's old button.
      uint8_t* m_ = st->pad[st->dev - 1];
      const int o = kPadRows[id - kIdPadCombo];
      for (int j = 0; j < kPadOutN; ++j)
        if (j != o && m_[j] == cur) m_[j] = m_[o];
      m_[o] = uint8_t(cur);
      KeysRefresh(h, *st);
    }
    return 0;
  }
  if (m == WM_COMMAND && HIWORD(w) == BN_CLICKED) {
    const int id = LOWORD(w);
    if (id >= kIdKeyBtn && id < kIdKeyBtn + kKeyN) {
      st->capturing = id - kIdKeyBtn;
      KeysRefresh(h, *st);
    } else if (id == kIdKeyReset) {
      // Only the page shown (keyboard or that controller).
      if (st->dev == 0)
        for (int i = 0; i < kKeyN; ++i) st->keys[i] = kKeyActions[i].def;
      else
        PadIdentity(st->pad[st->dev - 1]);
      st->capturing = -1;
      KeysRefresh(h, *st);
    } else if (id == kIdKeyOk) {
      st->accept = true;
      st->done = true;
      DestroyWindow(h);
    } else if (id == kIdKeyCancel) {
      st->done = true;
      DestroyWindow(h);
    }
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

// Returns true on OK; then the options hold the new bindings (empty = default).
bool ShowKeyBindings(HWND owner, int sel, Options& opt) {
  const KeyText& t = kKeyText[sel];
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSW wc{};
  wc.lpfnWndProc = KeysProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  wc.lpszClassName = L"AfroKeys";
  RegisterClassW(&wc);
  const double sc = GetDpiForSystem() / 96.0;
  auto S = [&](int v) { return static_cast<int>(v * sc + 0.5); };
  const DWORD style = WS_CAPTION | WS_SYSMENU;
  const int rows = kKeyN / 2, top = 108, rowH = 32, yBtn = top + rows * rowH + 16;
  RECT rc{0, 0, S(640), S(yBtn + 32 + 14)};
  AdjustWindowRectEx(&rc, style, FALSE, WS_EX_DLGMODALFRAME);
  RECT orc{};
  GetWindowRect(owner, &orc);
  const int W = rc.right - rc.left, H = rc.bottom - rc.top;
  const int x = std::max(0, static_cast<int>((orc.left + orc.right - W) / 2));
  const int y = std::max(0, static_cast<int>((orc.top + orc.bottom - H) / 2));
  HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, wc.lpszClassName, t.title, style,
                           x, y, W, H, owner, nullptr, inst, nullptr);
  if (!h) return false;
  KeysState st;
  st.sel = sel;
  for (int i = 0; i < kKeyN; ++i) st.keys[i] = opt.keys[i].empty() ? kKeyActions[i].def : opt.keys[i];
  for (int f = 0; f < 3; ++f) PadParse(opt.padmap[f], st.pad[f]);
  SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
  auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD s_, int cx, int cy, int cw, int ch,
                int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | s_, S(cx), S(cy), S(cw), S(ch), h,
                             reinterpret_cast<HMENU>(INT_PTR(id)), inst, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), TRUE);
    return c;
  };
  const PadText& pt = kPadText[sel];
  mk(L"STATIC", pt.dev_lbl, SS_CENTERIMAGE, 16, 12, 120, 26, kIdKeyDevLbl);
  HWND dev = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 140, 12, 320, 200,
                kIdKeyDev);
  for (const wchar_t* d : pt.dev) SendMessageW(dev, CB_ADDSTRING, 0, (LPARAM)d);
  SendMessageW(dev, CB_SETCURSEL, 0, 0);
  mk(L"STATIC", t.hint, 0, 16, 50, 608, 48, kIdKeyHint);
  for (int i = 0; i < kKeyN; ++i) {
    const int col = i / rows, r = i % rows;
    const int cx = 16 + col * 312, cy = top + r * rowH;
    mk(L"STATIC", t.action[i], SS_CENTERIMAGE, cx, cy, 160, 26, kIdKeyLbl + i);
    mk(L"BUTTON", L"", WS_TABSTOP, cx + 164, cy, 132, 26, kIdKeyBtn + i);
  }
  // Controller pages: one list per action, 8 rows per column.
  for (int r = 0; r < kPadOutN; ++r) {
    const int col = r / 8, rr = r % 8;
    const int cx = 16 + col * 312, cy = top + rr * rowH;
    mk(L"STATIC", pt.action[r], SS_CENTERIMAGE, cx, cy, 160, 26, kIdPadLbl + r);
    mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, cx + 164, cy, 132, 320,
       kIdPadCombo + r);
  }
  KeysShowDevice(h, st);
  mk(L"BUTTON", t.reset, WS_TABSTOP, 16, yBtn, 170, 32, kIdKeyReset);
  mk(L"BUTTON", kText[sel].ok, WS_TABSTOP | BS_DEFPUSHBUTTON, 354, yBtn, 130, 32, kIdKeyOk);
  mk(L"BUTTON", kText[sel].cancel, WS_TABSTOP, 494, yBtn, 130, 32, kIdKeyCancel);
  KeysRefresh(h, st);
  ShowWindow(h, SW_SHOW);
  EnableWindow(owner, FALSE);
  MSG msg;
  while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    // While an action waits for a key, the next key or mouse button press in
    // this window is the new binding (Esc and Enter included).
    if (st.capturing >= 0 && (msg.hwnd == h || IsChild(h, msg.hwnd))) {
      UINT vk = 0;
      switch (msg.message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: vk = static_cast<UINT>(msg.wParam); break;
        case WM_LBUTTONDOWN:
          // A left click on another button stops waiting and works as a click;
          // on the waiting button itself (or the window) it binds the left button.
          if (msg.hwnd != h && msg.hwnd != GetDlgItem(h, kIdKeyBtn + st.capturing)) {
            st.capturing = -1;
            KeysRefresh(h, st);
            break;
          }
          vk = VK_LBUTTON;
          break;
        case WM_RBUTTONDOWN: vk = VK_RBUTTON; break;
        case WM_MBUTTONDOWN: vk = VK_MBUTTON; break;
        case WM_KEYUP: case WM_SYSKEYUP: case WM_CHAR: case WM_SYSCHAR:
        case WM_RBUTTONUP: case WM_MBUTTONUP:
          continue;  // swallowed while waiting
      }
      if (vk) {
        KeysAssign(h, &st, vk);
        continue;
      }
    }
    if (!IsDialogMessageW(h, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  EnableWindow(owner, TRUE);
  SetForegroundWindow(owner);
  UnregisterClassW(wc.lpszClassName, inst);
  if (!st.accept) return false;
  for (int i = 0; i < kKeyN; ++i)
    opt.keys[i] = st.keys[i] == kKeyActions[i].def ? std::string() : st.keys[i];
  for (int f = 0; f < 3; ++f) opt.padmap[f] = PadString(st.pad[f]);
  return true;
}

// Settings window. Every change is a draft until OK; Cancel, X and Esc drop it.
struct SettingsState {
  bool done = false, accept = false;
  int sel = 0, tab = 0;
  bool remember = false;
  Options opt;
  std::vector<HWND> tabs[4];
};

void ApplySettingsTexts(HWND h, const SettingsState& st) {
  const UiText& t = kText[st.sel];
  std::wstring title = t.settings;
  while (!title.empty() && title.back() == L'.') title.pop_back();
  SetWindowTextW(h, title.c_str());
  SetDlgItemTextW(h, kIdTabBase + 0, t.tab_gfx);
  SetDlgItemTextW(h, kIdTabBase + 1, t.tab_play);
  SetDlgItemTextW(h, kIdTabBase + 2, kKeyText[st.sel].tab);
  SetDlgItemTextW(h, kIdTabBase + 3, t.tab_exp);
  SetDlgItemTextW(h, kIdKeysBtn, kKeyText[st.sel].button);
  SetDlgItemTextW(h, kIdDispLbl, t.disp);
  SetDlgItemTextW(h, kIdScaleLbl, t.scale);
  SetDlgItemTextW(h, kIdFpsLbl, t.fps);
  SetDlgItemTextW(h, kIdXVsyncLbl, t.vsync);
  SetDlgItemTextW(h, kIdCtrlLbl, t.ctrl);
  const wchar_t* mouse_lbl[3] = {t.mouse_sens, t.mouse_dz, t.mouse_win};
  for (int r = 0; r < 3; ++r) SetDlgItemTextW(h, kIdXMouseLbl + r, mouse_lbl[r]);
  SetDlgItemTextW(h, kIdDiffLbl, t.diff);
  FillCombo(h, kIdCheatInv, {t.diff_normal, t.diff_journo});
  SetDlgItemTextW(h, kIdDiffNote, t.diff_note);
  SetDlgItemTextW(h, kIdLoreLbl, kLoreText[st.sel].lbl);
  FillCombo(h, kIdLore, {kLoreText[st.sel].full, kLoreText[st.sel].fast});
  SetDlgItemTextW(h, kIdLoreNote, kLoreText[st.sel].note);
  SetDlgItemTextW(h, kIdXWarn, t.xwarn);
  const wchar_t* fx_lbl[5] = {t.sepia, t.blood, t.dof, t.bloom, t.focus};
  for (int i = 0; i < 5; ++i) {
    SetDlgItemTextW(h, kIdFxLbl + i, fx_lbl[i]);
    FillCombo(h, kIdFx + i, {t.fx_on, t.fx_off});
  }
  SetDlgItemTextW(h, kIdXGfxLbl, t.gfx);
  SetDlgItemTextW(h, kIdAdvBtn, t.adv);
  SetDlgItemTextW(h, kIdCheck, t.remember);
  SetDlgItemTextW(h, kIdHint, t.hint);
  SetDlgItemTextW(h, kIdSetOk, t.ok);
  SetDlgItemTextW(h, kIdSetCancel, t.cancel);
  FillCombo(h, kIdDisp, {t.full, t.win, kExclText[st.sel]});
  FillCombo(h, kIdFps, {t.fps30, t.fps60});
  FillCombo(h, kIdXVsync, {t.vsync_on, t.vsync_off});
  FillCombo(h, kIdCtrl, {t.ctrl_auto, t.ctrl_pad, t.ctrl_kb});
  FillCombo(h, kIdXGfx, {t.gfx_rtv, t.gfx_rov});
}

int Cur(HWND h, int id) { return static_cast<int>(SendDlgItemMessageW(h, id, CB_GETCURSEL, 0, 0)); }
void SetCur(HWND h, int id, int v) { SendDlgItemMessageW(h, id, CB_SETCURSEL, v, 0); }

// Experimental tab <-> options (also around the --advanced list, which edits the same values).
void ExpToOptions(HWND h, Options& o) {
  o.sepia = Cur(h, kIdFx) == 1 ? 1 : 0;
  for (int i = 1; i < 5; ++i) {
    const int a = AdvIndex(kFxAdvKey[i]);
    if (a >= 0) o.adv[a] = Cur(h, kIdFx + i) == 1 ? 1 : 0;
  }
  // adv[0] ("rt"): 0 = RTV default, 2 = ROV.
  o.adv[0] = Cur(h, kIdXGfx) == 1 ? 2 : 0;
}
void OptionsToExp(HWND h, const Options& o) {
  SetCur(h, kIdFx, o.sepia ? 1 : 0);
  for (int i = 1; i < 5; ++i) {
    const int a = AdvIndex(kFxAdvKey[i]);
    SetCur(h, kIdFx + i, a >= 0 && o.adv[a] ? 1 : 0);
  }
  SetCur(h, kIdXGfx, o.adv[0] == 2 ? 1 : 0);
}

void ShowTab(HWND h, SettingsState* st, int tab) {
  st->tab = tab;
  for (int i = 0; i < 4; ++i) {
    for (HWND c : st->tabs[i]) ShowWindow(c, i == tab ? SW_SHOW : SW_HIDE);
    CheckDlgButton(h, kIdTabBase + i, i == tab ? BST_CHECKED : BST_UNCHECKED);
  }
}

LRESULT CALLBACK SettingsProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto* st = reinterpret_cast<SettingsState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (!st) return DefWindowProcW(h, m, w, l);
  const auto finish = [&](bool accept) {
    st->accept = accept;
    st->done = true;
    DestroyWindow(h);
  };
  if (m == WM_CLOSE) {
    finish(false);
    return 0;
  }
  // Mouse sliders: keep the value next to each one current while dragging.
  if (m == WM_HSCROLL && l) {
    const int r = GetDlgCtrlID(reinterpret_cast<HWND>(l)) - kIdXMouse;
    if (r >= 0 && r < 3) {
      const int v = static_cast<int>(SendMessageW(reinterpret_cast<HWND>(l), TBM_GETPOS, 0, 0));
      SetDlgItemTextW(h, kIdXMouseVal + r, MouseText(r, v).c_str());
    }
    return 0;
  }
  if (m == WM_COMMAND) {
    const int id = LOWORD(w);
    if (id >= kIdTabBase && id < kIdTabBase + 4) {
      ShowTab(h, st, id - kIdTabBase);
    } else if (id == kIdFps && HIWORD(w) == CBN_SELCHANGE) {
      // VSync on rounds every frame that misses 60 down to 30: off suits 60.
      SetCur(h, kIdXVsync, Cur(h, kIdFps) == 1 ? 1 : 0);
    } else if (id == kIdKeysBtn) {
      ShowKeyBindings(h, st->sel, st->opt);
    } else if (id == kIdAdvBtn) {
      ExpToOptions(h, st->opt);
      ShowAdvanced(h, st->sel, &st->opt);
      OptionsToExp(h, st->opt);
    } else if (id == kIdSetOk || id == IDOK) {
      Options& o = st->opt;
      o.window = Cur(h, kIdDisp) == 1;
      o.exclusive = Cur(h, kIdDisp) == 2;
      o.scale = 1 + std::clamp(Cur(h, kIdScale), 0, 2);
      o.fps = kFpsValues[std::clamp(Cur(h, kIdFps), 0, 1)];
      o.vsync = Cur(h, kIdXVsync) == 1 ? 1 : 0;
      o.control = std::clamp(Cur(h, kIdCtrl), 0, 2);
      for (int r = 0; r < 3; ++r)
        o.mouse[r] = std::clamp(static_cast<int>(SendDlgItemMessageW(h, kIdXMouse + r, TBM_GETPOS, 0, 0)),
                                kMouse[r].lo, kMouse[r].hi);
      o.invuln = Cur(h, kIdCheatInv) == 1 ? 1 : 0;  // Game Journalist
      o.lore = Cur(h, kIdLore) == 1 ? 1 : 0;
      ExpToOptions(h, o);
      st->remember = IsDlgButtonChecked(h, kIdCheck) == BST_CHECKED;
      finish(true);
    } else if (id == kIdSetCancel || id == IDCANCEL) {
      finish(false);
    }
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

// Returns true on OK; then sel, remember and the options hold the new values.
bool ShowSettings(HWND owner, int& sel, bool& remember, Options& opt) {
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSW wc{};
  wc.lpfnWndProc = SettingsProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  wc.lpszClassName = L"AfroSettings";
  RegisterClassW(&wc);
  INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_BAR_CLASSES};
  InitCommonControlsEx(&icc);
  const double sc = GetDpiForSystem() / 96.0;
  auto S = [&](int v) { return static_cast<int>(v * sc + 0.5); };
  const bool adv = REXCVAR_GET(advanced);
  const DWORD style = WS_CAPTION | WS_SYSMENU;
  const int yLine2 = 386, yCheck = 396, yHint = 420, yBtn = 450, yEnd = yBtn + 32 + 14;
  RECT rc{0, 0, S(480), S(yEnd)};
  AdjustWindowRectEx(&rc, style, FALSE, WS_EX_DLGMODALFRAME);
  RECT orc{};
  GetWindowRect(owner, &orc);
  const int W = rc.right - rc.left, H = rc.bottom - rc.top;
  const int x = std::max(0, static_cast<int>((orc.left + orc.right - W) / 2));
  const int y = std::max(0, static_cast<int>((orc.top + orc.bottom - H) / 2));
  HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, wc.lpszClassName, L"", style,
                           x, y, W, H, owner, nullptr, inst, nullptr);
  if (!h) return false;
  SettingsState st;
  st.sel = sel;
  st.remember = remember;
  st.opt = opt;
  SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
  // tab: 0..2 = shown on that tab only, -1 = always.
  auto mk = [&](int tab, const wchar_t* cls, DWORD s_, int cx, int cy, int cw, int ch, int id) {
    HWND c = CreateWindowExW(0, cls, L"", WS_CHILD | (tab < 0 ? WS_VISIBLE : 0) | s_, S(cx), S(cy),
                             S(cw), S(ch), h, reinterpret_cast<HMENU>(INT_PTR(id)), inst, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), TRUE);
    if (tab >= 0) st.tabs[tab].push_back(c);
    return c;
  };
  const DWORD cbs = WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL;
  const int lx = 24, lw = 180, cx = 212, cw = 244;
  auto row = [&](int tab, int yy, int lbl_id, int ctl_id) {
    mk(tab, L"STATIC", SS_CENTERIMAGE, lx, yy, lw, 26, lbl_id);
    return mk(tab, L"COMBOBOX", cbs, cx, yy, cw, 220, ctl_id);
  };
  for (int i = 0; i < 4; ++i)
    mk(-1, L"BUTTON", WS_TABSTOP | BS_AUTORADIOBUTTON | BS_PUSHLIKE | (i == 0 ? WS_GROUP : 0),
       12 + i * 115, 12, 111, 30, kIdTabBase + i);
  mk(-1, L"STATIC", SS_ETCHEDHORZ, 12, 52, 456, 2, 0);
  // Graphics.
  row(0, 66, kIdDispLbl, kIdDisp);
  HWND scale = row(0, 104, kIdScaleLbl, kIdScale);
  for (auto* i : {L"1x (720p)", L"2x (1440p)", L"3x (2160p)"})
    SendMessageW(scale, CB_ADDSTRING, 0, (LPARAM)i);
  row(0, 142, kIdFpsLbl, kIdFps);
  row(0, 180, kIdXVsyncLbl, kIdXVsync);
  // Gameplay.
  // Difficulty: Game Journalist turns on the player invulnerability. Loading
  // screens: Fast turns off the full lore interlude (afro_interlude_full).
  row(1, 66, kIdDiffLbl, kIdCheatInv);
  mk(1, L"STATIC", 0, lx, 104, 432, 40, kIdDiffNote);
  row(1, 152, kIdLoreLbl, kIdLore);
  mk(1, L"STATIC", 0, lx, 190, 432, 56, kIdLoreNote);
  // Controls.
  row(2, 66, kIdCtrlLbl, kIdCtrl);
  for (int r = 0; r < 3; ++r) {
    const MouseSlider& ms = kMouse[r];
    const int yy = 104 + r * 38;
    const int v = std::clamp(opt.mouse[r], ms.lo, ms.hi);
    mk(2, L"STATIC", SS_CENTERIMAGE, lx, yy, lw, 26, kIdXMouseLbl + r);
    HWND tb = mk(2, TRACKBAR_CLASSW, WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 206, yy, 196, 26, kIdXMouse + r);
    SendMessageW(tb, TBM_SETRANGEMIN, FALSE, ms.lo);
    SendMessageW(tb, TBM_SETRANGEMAX, FALSE, ms.hi);
    SendMessageW(tb, TBM_SETLINESIZE, 0, ms.step);
    SendMessageW(tb, TBM_SETPAGESIZE, 0, ms.step * 5);
    SendMessageW(tb, TBM_SETPOS, TRUE, v);
    HWND val = mk(2, L"STATIC", SS_CENTERIMAGE | SS_RIGHT, 404, yy, 52, 26, kIdXMouseVal + r);
    SetWindowTextW(val, MouseText(r, v).c_str());
  }
  mk(2, L"BUTTON", WS_TABSTOP, lx, 224, 432, 30, kIdKeysBtn);
  // Experimental.
  mk(3, L"STATIC", SS_SUNKEN, lx, 64, 432, 40, kIdXWarn);
  for (int i = 0; i < 5; ++i) row(3, 114 + i * 38, kIdFxLbl + i, kIdFx + i);
  row(3, 304, kIdXGfxLbl, kIdXGfx);
  if (adv) mk(3, L"BUTTON", WS_TABSTOP, lx, 344, 432, 28, kIdAdvBtn);
  // Always shown.
  mk(-1, L"STATIC", SS_ETCHEDHORZ, 12, yLine2, 456, 2, 0);
  mk(-1, L"BUTTON", WS_TABSTOP | BS_AUTOCHECKBOX, lx, yCheck, 432, 22, kIdCheck);
  mk(-1, L"STATIC", 0, lx + 18, yHint, 414, 22, kIdHint);
  mk(-1, L"BUTTON", WS_TABSTOP | BS_DEFPUSHBUTTON, 104, yBtn, 130, 32, kIdSetOk);
  mk(-1, L"BUTTON", WS_TABSTOP, 246, yBtn, 130, 32, kIdSetCancel);
  ApplySettingsTexts(h, st);
  SetCur(h, kIdDisp, opt.window ? 1 : opt.exclusive ? 2 : 0);
  SetCur(h, kIdScale, (opt.scale >= 1 && opt.scale <= 3) ? opt.scale - 1 : 0);
  SetCur(h, kIdFps, opt.fps == 60 ? 1 : 0);
  SetCur(h, kIdXVsync, opt.vsync ? 1 : 0);
  SetCur(h, kIdCtrl, std::clamp(opt.control, 0, 2));
  SetCur(h, kIdCheatInv, opt.invuln ? 1 : 0);
  SetCur(h, kIdLore, opt.lore ? 1 : 0);
  OptionsToExp(h, opt);
  CheckDlgButton(h, kIdCheck, remember ? BST_CHECKED : BST_UNCHECKED);
  ShowTab(h, &st, 0);
  ShowWindow(h, SW_SHOW);
  SetFocus(GetDlgItem(h, kIdSetOk));
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
  if (!st.accept) return false;
  sel = st.sel;
  remember = st.remember;
  opt = st.opt;
  return true;
}

// Same rule as ReXApp::SetupEnvironment: --user_data_root, else
// Documents (FOLDERID_Documents) \ app name ("afrorecomp").
std::filesystem::path SaveRoot() {
  std::string cv = rex::cvar::GetFlagByName("user_data_root");
  if (!cv.empty()) return std::filesystem::path(cv);
  std::filesystem::path docs;
  PWSTR p = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &p))) docs = p;
  if (p) CoTaskMemFree(p);
  return docs / L"afrorecomp";
}
std::filesystem::path BackupBase() {
  auto root = SaveRoot();
  return root.parent_path() / (root.filename().wstring() + L"-backups");
}
bool IsCache(const std::filesystem::path& p) { return _wcsicmp(p.filename().c_str(), L"cache") == 0; }

void Explore(const std::filesystem::path& dir) {
  ShellExecuteW(nullptr, L"explore", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::wstring Fmt(const wchar_t* f, const std::wstring& a) {
  std::wstring out(f);
  auto i = out.find(L"%s");
  if (i != std::wstring::npos) out.replace(i, 2, a);
  return out;
}
std::wstring Widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

// Another afrorecomp.exe (not this launcher process) running?
bool GameRunning() {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W pe{sizeof(pe)};
  bool found = false;
  for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
    if (_wcsicmp(pe.szExeFile, L"afrorecomp.exe") == 0 && pe.th32ProcessID != GetCurrentProcessId()) found = true;
  CloseHandle(snap);
  return found;
}

bool HasSaves(const std::filesystem::path& root) {
  std::error_code ec;
  for (auto& e : std::filesystem::directory_iterator(root, ec))
    if (!IsCache(e.path())) return true;
  return false;
}

// Copy the save root (except the shader cache) to a new folder
// afrorecomp-backups\saves-YYYYMMDD-HHMMSS[suffix]. Never overwrites.
bool MakeBackup(const wchar_t* suffix, std::filesystem::path* dst_out, std::error_code& ec) {
  auto root = SaveRoot();
  wchar_t stamp[32];
  std::time_t now = std::time(nullptr);
  std::tm tm{};
  localtime_s(&tm, &now);
  wcsftime(stamp, 32, L"saves-%Y%m%d-%H%M%S", &tm);
  auto base = BackupBase();
  std::wstring name = std::wstring(stamp) + suffix;
  auto dst = base / name;
  for (int n = 2; std::filesystem::exists(dst, ec); ++n) dst = base / (name + L"-" + std::to_wstring(n));
  ec.clear();
  std::filesystem::create_directories(dst, ec);
  if (!ec)
    for (auto& e : std::filesystem::directory_iterator(root, ec)) {
      if (IsCache(e.path())) continue;
      std::filesystem::copy(e.path(), dst / e.path().filename(),
                            std::filesystem::copy_options::recursive, ec);
      if (ec) break;
    }
  *dst_out = dst;
  return !ec;
}

// Remove the save data in the root, keeping the shader cache (and never
// touching the backups folder, which is a sibling of the root).
bool ClearSaves(std::error_code& ec) {
  auto root = SaveRoot();
  auto base = BackupBase();
  std::vector<std::filesystem::path> items;
  for (auto& e : std::filesystem::directory_iterator(root, ec))
    if (!IsCache(e.path()) && e.path() != base) items.push_back(e.path());
  if (ec) return false;
  for (auto& p : items) { std::filesystem::remove_all(p, ec); if (ec) return false; }
  return true;
}

struct SavesState { int sel = 0; bool done = false; std::vector<std::filesystem::path> backups; };

void FillBackups(HWND h, SavesState* st) {
  st->backups.clear();
  std::error_code ec;
  for (auto& e : std::filesystem::directory_iterator(BackupBase(), ec))
    if (e.is_directory(ec)) st->backups.push_back(e.path());
  std::sort(st->backups.begin(), st->backups.end(),
            [](const auto& a, const auto& b) { return a.filename().wstring() > b.filename().wstring(); });
  HWND lb = GetDlgItem(h, kIdSvList);
  SendMessageW(lb, LB_RESETCONTENT, 0, 0);
  for (auto& b : st->backups) SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)b.filename().c_str());
  if (st->backups.empty()) SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)kSaveText[st->sel].none);
  EnableWindow(GetDlgItem(h, kIdSvRestore), !st->backups.empty());
}

void Msg(HWND h, const std::wstring& m, UINT f) { MessageBoxW(h, m.c_str(), L"Afro Samurai", f); }

void DoBackup(HWND h, SavesState* st) {
  const SaveText& t = kSaveText[st->sel];
  auto root = SaveRoot();
  if (!HasSaves(root)) { Msg(h, Fmt(t.empty, root.wstring()), MB_OK | MB_ICONINFORMATION); return; }
  std::error_code ec;
  std::filesystem::path dst;
  if (!MakeBackup(L"", &dst, ec)) { Msg(h, Fmt(t.fail, Widen(ec.message()) + L"\n" + dst.wstring()), MB_OK | MB_ICONERROR); return; }
  FillBackups(h, st);
  if (MessageBoxW(h, Fmt(t.done, dst.wstring()).c_str(), L"Afro Samurai", MB_YESNO | MB_ICONINFORMATION) == IDYES)
    Explore(BackupBase());
}

// Safety backup of the current saves before restore/delete. Returns false
// (and reports) on failure; *dst is empty when there was nothing to back up.
bool SafetyBackup(HWND h, const SaveText& t, const wchar_t* suffix, std::filesystem::path* dst) {
  dst->clear();
  if (!HasSaves(SaveRoot())) return true;
  std::error_code ec;
  if (MakeBackup(suffix, dst, ec)) return true;
  Msg(h, Fmt(t.fail, Widen(ec.message()) + L"\n" + dst->wstring()), MB_OK | MB_ICONERROR);
  return false;
}

void DoRestore(HWND h, SavesState* st) {
  const SaveText& t = kSaveText[st->sel];
  int i = static_cast<int>(SendDlgItemMessageW(h, kIdSvList, LB_GETCURSEL, 0, 0));
  if (i < 0 || i >= static_cast<int>(st->backups.size())) { Msg(h, t.pick, MB_OK | MB_ICONINFORMATION); return; }
  auto src = st->backups[i];
  if (MessageBoxW(h, Fmt(t.restore_q, src.wstring()).c_str(), L"Afro Samurai",
                  MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
  if (GameRunning()) { Msg(h, t.running, MB_OK | MB_ICONWARNING); return; }
  std::filesystem::path safety;
  if (!SafetyBackup(h, t, L"-before-restore", &safety)) return;
  std::error_code ec;
  auto root = SaveRoot();
  std::filesystem::create_directories(root, ec);
  if (!ec && ClearSaves(ec))
    for (auto& e : std::filesystem::directory_iterator(src, ec)) {
      if (IsCache(e.path())) continue;
      std::filesystem::copy(e.path(), root / e.path().filename(), std::filesystem::copy_options::recursive, ec);
      if (ec) break;
    }
  if (ec) { Msg(h, Fmt(t.fail, Widen(ec.message()) + L"\n" + safety.wstring()), MB_OK | MB_ICONERROR); }
  else Msg(h, Fmt(t.restore_ok, safety.empty() ? L"-" : safety.wstring()), MB_OK | MB_ICONINFORMATION);
  FillBackups(h, st);
}

void DoDelete(HWND h, SavesState* st) {
  const SaveText& t = kSaveText[st->sel];
  auto root = SaveRoot();
  if (!HasSaves(root)) { Msg(h, Fmt(t.empty, root.wstring()), MB_OK | MB_ICONINFORMATION); return; }
  if (MessageBoxW(h, t.del_q, L"Afro Samurai", MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2) != IDYES) return;
  if (GameRunning()) { Msg(h, t.running, MB_OK | MB_ICONWARNING); return; }
  std::filesystem::path safety;
  if (!SafetyBackup(h, t, L"-before-delete", &safety)) return;
  std::error_code ec;
  if (!ClearSaves(ec)) Msg(h, Fmt(t.fail, Widen(ec.message()) + L"\n" + safety.wstring()), MB_OK | MB_ICONERROR);
  else Msg(h, Fmt(t.del_ok, safety.wstring()), MB_OK | MB_ICONINFORMATION);
  FillBackups(h, st);
}

LRESULT CALLBACK SavesProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto* st = reinterpret_cast<SavesState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (m == WM_CLOSE || (m == WM_COMMAND && (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL))) {
    st->done = true;
    DestroyWindow(h);
    return 0;
  }
  if (m == WM_COMMAND && st) {
    switch (LOWORD(w)) {
      case kIdSvOpen: { std::error_code ec; std::filesystem::create_directories(SaveRoot(), ec); Explore(SaveRoot()); break; }
      case kIdSvBackup: DoBackup(h, st); break;
      case kIdSvOpenBk: { std::error_code ec; std::filesystem::create_directories(BackupBase(), ec); Explore(BackupBase()); break; }
      case kIdSvRestore: DoRestore(h, st); break;
      case kIdSvDelete: DoDelete(h, st); break;
    }
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

// Modal "Manage saved games" dialog (same style as the Advanced settings one).
void ShowSaves(HWND owner, int sel) {
  const SaveText& t = kSaveText[sel];
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSW wc{};
  wc.lpfnWndProc = SavesProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  wc.lpszClassName = L"AfroSaves";
  RegisterClassW(&wc);
  const double sc = GetDpiForSystem() / 96.0;
  auto S = [&](int v) { return static_cast<int>(v * sc + 0.5); };
  const DWORD style = WS_CAPTION | WS_SYSMENU;
  RECT rc{0, 0, S(420), S(446)};
  AdjustWindowRectEx(&rc, style, FALSE, WS_EX_DLGMODALFRAME);
  RECT orc;
  GetWindowRect(owner, &orc);
  std::wstring title = t.manage;
  while (!title.empty() && title.back() == L'.') title.pop_back();
  HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, wc.lpszClassName, title.c_str(),
                           style | WS_VISIBLE, orc.left + S(20), orc.top + S(20),
                           rc.right - rc.left, rc.bottom - rc.top, owner, nullptr, inst, nullptr);
  if (!h) return;
  SavesState st;
  st.sel = sel;
  SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
  auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD s_, int x, int y, int cw, int ch,
                int id, DWORD ex = 0) {
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | s_, S(x), S(y), S(cw), S(ch),
                             h, reinterpret_cast<HMENU>(INT_PTR(id)), inst, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), TRUE);
    return c;
  };
  std::wstring folder = std::wstring(t.folder) + L"\n" + SaveRoot().wstring();
  mk(L"STATIC", folder.c_str(), SS_SUNKEN | SS_PATHELLIPSIS, 16, 14, 388, 40, kIdSvFolder);
  mk(L"BUTTON", t.open, WS_TABSTOP, 16, 64, 388, 28, kIdSvOpen);
  mk(L"BUTTON", t.backup, WS_TABSTOP, 16, 98, 388, 28, kIdSvBackup);
  mk(L"STATIC", t.list, 0, 16, 138, 388, 20, 0);
  mk(L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 16, 160, 388, 110,
     kIdSvList, WS_EX_CLIENTEDGE);
  mk(L"BUTTON", t.restore, WS_TABSTOP, 16, 276, 388, 28, kIdSvRestore);
  mk(L"BUTTON", t.openbk, WS_TABSTOP, 16, 310, 388, 28, kIdSvOpenBk);
  mk(L"BUTTON", t.del, WS_TABSTOP, 16, 352, 388, 28, kIdSvDelete);
  HWND close = mk(L"BUTTON", t.close, WS_TABSTOP | BS_DEFPUSHBUTTON, 150, 396, 120, 32, IDCANCEL);
  FillBackups(h, &st);
  SetFocus(close);
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
      if (LOWORD(w) == kIdSavesBtn) {
        ShowSaves(h, st->sel);
      } else if (LOWORD(w) == kIdSettingsBtn) {
        if (ShowSettings(h, st->sel, st->remember, st->opt)) ApplyMainTexts(h, st->sel);
      } else if (LOWORD(w) == kIdCombo && HIWORD(w) == CBN_SELCHANGE) {
        st->sel = std::clamp(static_cast<int>(SendDlgItemMessageW(h, kIdCombo, CB_GETCURSEL, 0, 0)), 0, 3);
        ApplyMainTexts(h, st->sel);
      } else if (LOWORD(w) == IDCANCEL) {  // Esc: same as closing the window
        PostMessageW(h, WM_CLOSE, 0, 0);
      } else if (LOWORD(w) == IDOK) {
        st->play = true;
        st->done = true;
        DestroyWindow(h);
      }
      return 0;
    case WM_CLOSE:
      st->done = true;
      DestroyWindow(h);
      return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

// Main window: language, Play, Settings, saved games. The language stays here so
// a player who does not read English can find it.
// Returns 0 when the window was closed (X, Alt+F4, Esc) instead of Play.
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
  RECT rc{0, 0, S(360), S(204)};
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
  auto mkc = [&](const wchar_t* cls, DWORD st_, int cx, int cy, int cw, int ch, int id) {
    HWND c = CreateWindowExW(0, cls, L"", WS_CHILD | WS_VISIBLE | st_, S(cx), S(cy), S(cw), S(ch),
                             h, reinterpret_cast<HMENU>(INT_PTR(id)), inst, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), TRUE);
    return c;
  };
  auto mk = [&](DWORD st_, int cx, int cy, int cw, int ch, int id) {
    return mkc(L"BUTTON", WS_TABSTOP | st_, cx, cy, cw, ch, id);
  };
  mkc(L"STATIC", SS_CENTERIMAGE, 20, 20, 120, 26, kIdLangLbl);
  HWND lang = mkc(L"COMBOBOX", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 150, 20, 190, 200, kIdCombo);
  for (auto& lg : kLangs) SendMessageW(lang, CB_ADDSTRING, 0, (LPARAM)lg.label);
  SendMessageW(lang, CB_SETCURSEL, st.sel, 0);
  HWND play = mk(BS_DEFPUSHBUTTON, 20, 60, 320, 46, kIdPlay);
  mk(0, 20, 118, 320, 30, kIdSettingsBtn);
  mk(0, 20, 156, 320, 30, kIdSavesBtn);
  ApplyMainTexts(h, st.sel);
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
  if (!st.play) return 0;
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
      if (line.rfind("vsync=", 0) == 0) opt.vsync = line[6] == '1' ? 1 : 0;
      if (line.rfind("difficulty=", 0) == 0) opt.invuln = line[11] == '1' ? 1 : 0;
      if (line.rfind("tick=", 0) == 0) opt.tick = std::clamp(std::atoi(line.c_str() + 5), 0, 3);
      if (line.rfind("window=", 0) == 0) opt.window = line[7] == '1';
      if (line.rfind("exclusive=", 0) == 0) opt.exclusive = line[10] == '1';
      if (line.rfind("lore=", 0) == 0) opt.lore = line[5] == '1' ? 1 : 0;
      if (line.rfind("control=", 0) == 0) opt.control = std::clamp(std::atoi(line.c_str() + 8), 0, 2);
      for (int r = 0; r < 3; ++r) {
        std::string k = std::string(kMouse[r].key) + "=";
        if (line.rfind(k, 0) == 0)
          opt.mouse[r] = std::clamp(std::atoi(line.c_str() + k.size()), kMouse[r].lo, kMouse[r].hi);
      }
      for (int i = 0; i < kAdvN; ++i) {
        std::string k = std::string(kAdv[i].key) + "=";
        if (line.rfind(k, 0) == 0)
          opt.adv[i] = std::clamp(std::atoi(line.c_str() + k.size()), 0, kAdv[i].n - 1);
      }
      if (line.rfind("scale=", 0) == 0) opt.scale = std::clamp(std::atoi(line.c_str() + 6), 1, 3);
      for (int i = 0; i < kKeyN; ++i) {
        std::string k = KeyCfgName(i) + "=";
        if (line.rfind(k, 0) == 0) {
          std::string v = line.substr(k.size());
          while (!v.empty() && (v.back() == '\r' || v.back() == ' ')) v.pop_back();
          opt.keys[i] = v;
        }
      }
      for (int f = 0; f < 3; ++f) {
        std::string k = std::string("padmap_") + kPadFamilies[f] + "=";
        if (line.rfind(k, 0) == 0) {
          std::string v = line.substr(k.size());
          while (!v.empty() && (v.back() == '\r' || v.back() == ' ')) v.pop_back();
          uint8_t m[kPadOutN];
          PadParse(v, m);
          opt.padmap[f] = PadString(m);  // anything unreadable = the pad as it is
        }
      }
    }
  }
  if (!Valid(lang)) { lang = 0; ask = true; }
  // Settings are saved on every Play; "ask" only says whether the launcher shows.
  // Configs older than 0.2.0:
  // - Without a version line (before 0.0.16) fps=60 meant 60 only with a
  //   non-original tick mode.
  // - 60 was experimental with VSync always on; 60 now defaults to VSync off.
  // - The effect switches were diagnostic (ignored without --advanced): reset them.
  const bool pre020 = seen_version.empty() || seen_version.rfind("0.0", 0) == 0 ||
                      seen_version.rfind("0.1", 0) == 0;
  if (seen_version.empty() && opt.tick == 0) opt.fps = 0;
  opt.fps = opt.fps == 60 ? 60 : 0;
  opt.tick = 0;
  if (pre020 && opt.fps == 60) opt.vsync = 1;
  if (pre020)
    for (const char* k : kFxAdvKey)
      if (k && AdvIndex(k) >= 0) opt.adv[AdvIndex(k)] = 0;
  // Without --advanced, the diagnostic options are ignored (safe defaults); the
  // ones the Settings window shows (render path, effects) stay.
  // Quad depth is not a launcher option. A saved qdepth= line is ignored so an
  // old Off/Full choice cannot override the runtime default. A command-line or
  // REX_AFRO_QUAD_DEPTH_OFFSET value still wins because this loop never sets it.
  if (opt.adv[0] != 2) opt.adv[0] = 0;
  const Options saved_adv = opt;
  if (!REXCVAR_GET(advanced))
    for (int i = 0; i < kAdvN; ++i)
      if (!UserFacingAdv(i)) opt.adv[i] = 0;
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
    lang = ShowPicker(initial, &remember, &opt);
    if (!lang) {  // Launcher closed without Play: quit, keep the old config.
      std::fprintf(stderr, "[afro] launcher closed, not starting the game\n");
      std::fflush(nullptr);
      ExitProcess(0);
    }
    std::ofstream out(path, std::ios::trunc);
    out << "language=" << lang << "\nask=" << (remember ? 0 : 1) << "\nfps=" << opt.fps
        << "\nsepia=" << opt.sepia << "\nvsync=" << opt.vsync << "\ntick=0"
        << "\nwindow=" << (opt.window ? 1 : 0) << "\nexclusive=" << (opt.exclusive ? 1 : 0)
        << "\nlore=" << opt.lore << "\ncontrol=" << opt.control
        << "\nscale=" << opt.scale << "\ndifficulty=" << opt.invuln
        << "\nversion=" << AFRO_VERSION << "\n";
    for (int r = 0; r < 3; ++r) out << kMouse[r].key << "=" << opt.mouse[r] << "\n";
    for (int i = 0; i < kKeyN; ++i) out << KeyCfgName(i) << "=" << opt.keys[i] << "\n";
    for (int f = 0; f < 3; ++f) out << "padmap_" << kPadFamilies[f] << "=" << opt.padmap[f] << "\n";
    // Diagnostic options keep their stored value unless --advanced showed them.
    for (int i = 0; i < kAdvN; ++i)
      out << kAdv[i].key << "="
          << ((UserFacingAdv(i) || REXCVAR_GET(advanced)) ? opt.adv[i] : saved_adv.adv[i]) << "\n";
  }
#else
  (void)show;  // No native picker on Linux: use config or default English.
#endif
  if (!lang) lang = 1;
  // Command-line / toml values win over the launcher.
  // 30 (0): present interval 2 (default), no host cap.
  // 60: present interval 1 + host cap 60.
  if (opt.fps == 60) {
    if (!Explicit("afro_present_interval"))
      SetCvar("afro_present_interval", "1");
    if (!Explicit("afro_fps_cap"))
      SetCvar("afro_fps_cap", "60");
  }
  if (opt.sepia && !Explicit("afro_fx_sepia"))
    SetCvar("afro_fx_sepia", "false");
  // VSync off: GPU plugin cvar, so SetCvar defers it via REX_VSYNC.
  // frame_pacing.cpp then holds 30 FPS scenes (retail, cutscenes) at 30 itself.
  if (opt.vsync && !Explicit("vsync"))
    SetCvar("vsync", "false");
  // The SDK marked every CPU-uploaded page stale again at each frame end, so all
  // meshes were sent to the GPU every frame (10-30 MB). CPU writes are still
  // caught by the page write watches; this only drops the extra re-upload.
  if (!Explicit("clear_memory_page_state"))
    SetCvar("clear_memory_page_state", "false");
  // Difficulty: Game Journalist turns on the player invulnerability. Saved as
  // difficulty= like the other settings (0.2.1; before that it was session-only).
  if (opt.invuln && !Explicit("afro_player_invuln"))
    SetCvar("afro_player_invuln", "true");
  if (opt.invuln)
    std::fprintf(stderr,
                 "[afro] invuln: Game Journalist difficulty, ON (player HP only)\n");
  if (!Explicit("fullscreen"))
    SetCvar("fullscreen", opt.window ? "false" : "true");
  // Display: the default fullscreen is a borderless window over the whole screen
  // (no mode change, instant Alt+Tab); Exclusive fullscreen takes the display.
  if (opt.lore && !Explicit("afro_interlude_full"))
    SetCvar("afro_interlude_full", "false");
  if (!opt.window && opt.exclusive && !Explicit("fullscreen_exclusive"))
    SetCvar("fullscreen_exclusive", "true");
  // Controls. Auto: the keyboard plays as a pad and its key caps are shown until
  // a controller connects or is used (then its own icons). Controller: no
  // keyboard pad. Keyboard: key cap icons whatever is connected.
  if (!Explicit("mnk_mode"))
    SetCvar("mnk_mode", opt.control == 1 ? "false" : "true");
  // The mouse moves the camera whenever the keyboard can play.
  if (opt.control != 1 && !Explicit("mnk_mouse"))
    SetCvar("mnk_mouse", "true");
  for (int r = 0; r < 3; ++r)
    if (!Explicit(kMouse[r].cvar))
      SetCvar(kMouse[r].cvar, MouseCvarValue(r, opt.mouse[r]).c_str());
  // Key bindings (Settings -> Controls); the command line still wins.
  for (int i = 0; i < kKeyN; ++i)
    if (!opt.keys[i].empty() && !Explicit(kKeyActions[i].cvar))
      SetCvar(kKeyActions[i].cvar, opt.keys[i]);
  // Controller remap (Key bindings -> Controller), one per pad family.
  for (int f = 0; f < 3; ++f) {
    const std::string cvar = std::string("afro_padmap_") + kPadFamilies[f];
    if (!opt.padmap[f].empty() && !Explicit(cvar.c_str())) SetCvar(cvar.c_str(), opt.padmap[f]);
  }
  if (opt.control == 2 && !Explicit("afro_button_glyphs"))
    SetCvar("afro_button_glyphs", "keyboard");
  for (int i = 0; i < kAdvN; ++i)
    if (opt.adv[i] > 0 && !Explicit(kAdv[i].cvar))
      SetCvar(kAdv[i].cvar, kAdv[i].values[opt.adv[i]]);
  // Default render path: RTV (host render targets; black frames fixed by the
  // afro_edram_no_wrap GPU plugin patch). ROV selectable for A/B (experimental).
  // GPU plugin registers this cvar late (kInitOnly): SetCvar falls back to
  // REX_RENDER_TARGET_PATH_D3D12; RegisterFlag applies env when the plugin loads.
  const char* rt_req = (opt.adv[0] == 2) ? "rov" : "rtv";
  const bool rt_explicit = Explicit("render_target_path_d3d12");
  if (!rt_explicit)
    SetCvar("render_target_path_d3d12", rt_req);
  // Path is kInitOnly in the GPU plugin; deferred via REX_RENDER_TARGET_PATH_D3D12.
  std::fprintf(stderr,
               "[afro] render_target_path_d3d12 requested=%s (launcher rt=%d%s)\n",
               rt_req, opt.adv[0],
               rt_explicit ? "; cmdline/config already set" : "");
  if (!Explicit("resolution_scale") && opt.scale > 1)
    SetCvar("resolution_scale", std::to_string(opt.scale));
  rex::cvar::SetFlagByName("user_language", std::to_string(lang));
  std::fprintf(stderr, "[afro] user_language=%u (config: %s)\n", lang,
               path.string().c_str());
  return lang;
}

}  // namespace afro
