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

struct Options { int fps = 0; /* 0=Original(30), 60=Experimental(60) */ int sepia = 0; /* 0 on, 1 off */ int vsync = 0; /* 0 on, 1 off (guest vblank timer) */ int invuln = 0; /* 0 off, 1 on — session test only, not persisted */ int tick = 0; /* legacy only: migrate 60+non-orig tick → Experimental */ bool window = false; int scale = 1; int adv[kAdvN] = {}; int control = 0; /* 0 auto (keyboard, a pad takes over when used), 1 controller only, 2 keyboard icons */ int mouse[3] = {kMouse[0].def, kMouse[1].def, kMouse[2].def}; };

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
struct UiText { const wchar_t *title, *lang, *remember, *hint, *play, *fps, *fps_orig, *fps_exp, *disp, *full, *win, *scale, *adv, *def, *off, *ok, *on, *xbtn, *xwarn, *sepia, *sepia_on, *sepia_off, *gfx, *gfx_rtv, *gfx_rov, *cheats, *cheat_title, *cheat_inv, *cancel, *ctrl, *ctrl_auto, *ctrl_pad, *ctrl_kb, *mouse_sens, *mouse_dz, *mouse_win, *vsync, *vsync_on, *vsync_off; };
const UiText kText[] = {
    {L"afrorecomp_v" AFRO_VERSION_W L" - Launcher", L"Language:", L"Remember and don't ask again",
     L"Hold Shift at launch to show this window again.", L"Play",
     L"Frame rate:", L"30 (default)", L"60 (experimental)", L"Display:", L"Fullscreen", L"Window", L"Internal resolution:", L"Advanced options...", L"Default", L"Off", L"OK", L"On",
     L"Advanced settings...", L"Warning: these settings are experimental and not fully tested. 60 (experimental) unlocks the present interval; results vary.",
     L"Sepia filter:", L"On (default)", L"Off",
     L"Render path:", L"RTV (default)", L"ROV (experimental)",
     L"Experimental\u2026", L"Experimental", L"Invulnerability", L"Cancel",
     L"Controls:", L"Auto (keyboard + controller)", L"Controller", L"Keyboard",
     L"Mouse sensitivity:", L"Mouse dead zone:", L"Mouse smoothing:",
     L"VSync (60 Hz):", L"On (default)", L"Off (smoother below 60, may tear)"},
    {L"afrorecomp_v" AFRO_VERSION_W L" - Lanzador", L"Idioma:", L"Recordar y no volver a preguntar",
     L"Mantén Mayús al iniciar para volver a ver esta ventana.", L"Jugar",
     L"Fotogramas:", L"30 (por defecto)", L"60 (experimental)", L"Pantalla:", L"Pantalla completa", L"Ventana", L"Resolución interna:", L"Opciones avanzadas...", L"Por defecto", L"Desactivado", L"Aceptar", L"Activado",
     L"Ajustes avanzados...", L"Atención: estos ajustes son experimentales y no están probados del todo. 60 (experimental) desbloquea el intervalo de presentación; los resultados varían.",
     L"Filtro sepia:", L"Activado (por defecto)", L"Desactivado",
     L"Ruta de render:", L"RTV (por defecto)", L"ROV (experimental)",
     L"Experimental\u2026", L"Experimental", L"Invulnerabilidad", L"Cancelar",
     L"Control:", L"Autom\u00e1tico (teclado + mando)", L"Mando", L"Teclado",
     L"Sensibilidad del rat\u00f3n:", L"Zona muerta del rat\u00f3n:", L"Suavizado del rat\u00f3n:",
     L"VSync (60 Hz):", L"Activado (por defecto)", L"Desactivado (m\u00e1s fluido bajo 60, puede haber tearing)"},
    {L"afrorecomp_v" AFRO_VERSION_W L" - Lanceur", L"Langue :", L"Mémoriser et ne plus demander",
     L"Maintenez Maj au démarrage pour revoir cette fenêtre.", L"Jouer",
     L"Images/s :", L"30 (par défaut)", L"60 (expérimental)", L"Affichage :", L"Plein écran", L"Fenêtre", L"Résolution interne :", L"Options avancées...", L"Par défaut", L"Désactivé", L"OK", L"Activé",
     L"Réglages avancés...", L"Attention : ces réglages sont expérimentaux et pas entièrement testés. 60 (expérimental) débloque l'intervalle de présentation ; les résultats varient.",
     L"Filtre sépia :", L"Activé (par défaut)", L"Désactivé",
     L"Chemin de rendu :", L"RTV (par d\u00e9faut)", L"ROV (exp\u00e9rimental)",
     L"Exp\u00e9rimental\u2026", L"Exp\u00e9rimental", L"Invuln\u00e9rabilit\u00e9",
     L"Annuler",
     L"Contr\u00f4les :", L"Auto (clavier + manette)", L"Manette", L"Clavier",
     L"Sensibilit\u00e9 souris :", L"Zone morte souris :", L"Lissage souris :",
     L"VSync (60 Hz) :", L"Activ\u00e9 (par d\u00e9faut)", L"D\u00e9sactiv\u00e9 (plus fluide sous 60, risque de d\u00e9chirement)"},
    {L"afrorecomp_v" AFRO_VERSION_W L" - Avvio", L"Lingua:", L"Ricorda e non chiedere più",
     L"Tieni premuto Maiusc all'avvio per rivedere questa finestra.", L"Gioca",
     L"Fotogrammi:", L"30 (predefinito)", L"60 (sperimentale)", L"Schermo:", L"Schermo intero", L"Finestra", L"Risoluzione interna:", L"Opzioni avanzate...", L"Predefinito", L"Disattivato", L"OK", L"Attivato",
     L"Impostazioni avanzate...", L"Attenzione: queste impostazioni sono sperimentali e non del tutto testate. 60 (sperimentale) sblocca l'intervallo di presentazioni; i risultati variano.",
     L"Filtro seppia:", L"Attivato (predefinito)", L"Disattivato",
     L"Percorso di rendering:", L"RTV (predefinito)", L"ROV (sperimentale)",
     L"Sperimentale\u2026", L"Sperimentale", L"Invulnerabilit\u00e0", L"Annulla",
     L"Controlli:", L"Automatico (tastiera + controller)", L"Controller", L"Tastiera",
     L"Sensibilità mouse:", L"Zona morta mouse:", L"Smussatura mouse:",
     L"VSync (60 Hz):", L"Attivato (predefinito)", L"Disattivato (pi\u00f9 fluido sotto 60, possibile tearing)"},
};
constexpr int kIdCombo = 101, kIdCheck = 102, kIdPlay = IDOK, kIdLangLbl = 103,
              kIdHint = 104, kIdFpsLbl = 105, kIdFps = 106, kIdDispLbl = 107,
              kIdDisp = 108, kIdScaleLbl = 109, kIdScale = 110,
              kIdAdvBtn = 111, kIdXBtn = 112, kIdXWarn = 113, kIdXSepiaLbl = 114,
              kIdXSepia = 115, kIdXGfxLbl = 117, kIdXGfx = 118, kIdXCheats = 121,
              kIdCheatInv = 122, kIdCheatOk = 123, kIdCheatCancel = 124,
              kIdSavesBtn = 116, kIdAdvBase = 200, kIdCtrlLbl = 125, kIdCtrl = 126,
              kIdXMouse = 130,     // 130..132 sliders
              kIdXMouseVal = 133,  // 133..135 their values
              kIdXVsyncLbl = 136, kIdXVsync = 137;

constexpr int kFpsValues[] = {0, 60};  // Original (30), Experimental (60)
struct PickState { int sel = 0; bool remember = false; bool done = false; bool play = false; Options opt; };

void FillCombo(HWND h, int id, std::initializer_list<const wchar_t*> items) {
  HWND c = GetDlgItem(h, id);
  int cur = static_cast<int>(SendMessageW(c, CB_GETCURSEL, 0, 0));
  SendMessageW(c, CB_RESETCONTENT, 0, 0);
  for (auto* i : items) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)i);
  SendMessageW(c, CB_SETCURSEL, cur < 0 ? 0 : cur, 0);
}

// Save management (same language order as kText).
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
  SetDlgItemTextW(h, kIdSavesBtn, kSaveText[sel].manage);
  FillCombo(h, kIdDisp, {t.full, t.win});
  SetDlgItemTextW(h, kIdCtrlLbl, t.ctrl);
  FillCombo(h, kIdCtrl, {t.ctrl_auto, t.ctrl_pad, t.ctrl_kb});
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

// Advanced settings keep the Experimental popup's choices as drafts until OK.
struct AdvState { bool done = false; Options* opt; int sel = 0; int fps_draft = 0; int sepia_draft = 0; int vsync_draft = 0; };

struct ExpState { bool done = false; bool accept = false; int fps = 0; int sepia = 0; int vsync = 0; };

// Experimental popup (not fully tested options): warning, frame rate, sepia, vsync.
// OK updates the drafts. Cancel, X and Esc leave them unchanged.
void ShowExperimentalPopup(HWND owner, int sel, int& fps, int& sepia, int& vsync) {
  const UiText& t = kText[sel];
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSW wc{};
  wc.lpfnWndProc = [](HWND h, UINT m, WPARAM w, LPARAM l) -> LRESULT {
    auto* st = reinterpret_cast<ExpState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (!st) return DefWindowProcW(h, m, w, l);
    const auto discard = [&] {
      st->accept = false;
      st->done = true;
      DestroyWindow(h);
    };
    const auto accept = [&] {
      int fi = static_cast<int>(SendDlgItemMessageW(h, kIdFps, CB_GETCURSEL, 0, 0));
      st->fps = kFpsValues[std::clamp(fi, 0, 1)];
      st->sepia = SendDlgItemMessageW(h, kIdXSepia, CB_GETCURSEL, 0, 0) == 1 ? 1 : 0;
      st->vsync = SendDlgItemMessageW(h, kIdXVsync, CB_GETCURSEL, 0, 0) == 1 ? 1 : 0;
      st->accept = true;
      st->done = true;
      DestroyWindow(h);
    };
    if (m == WM_KEYDOWN && w == VK_ESCAPE) {
      discard();
      return 0;
    }
    if (m == WM_CLOSE) {
      discard();
      return 0;
    }
    if (m == WM_COMMAND) {
      const int id = LOWORD(w);
      if (id == kIdCheatOk || id == IDOK) {
        accept();
        return 0;
      }
      if (id == kIdCheatCancel || id == IDCANCEL) {
        discard();
        return 0;
      }
    }
    return DefWindowProcW(h, m, w, l);
  };
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  wc.lpszClassName = L"AfroExperimentalPopup";
  RegisterClassW(&wc);
  const double sc = GetDpiForSystem() / 96.0;
  auto S = [&](int v) { return static_cast<int>(v * sc + 0.5); };
  const DWORD style = WS_CAPTION | WS_SYSMENU;
  RECT rc{0, 0, S(460), S(268)};
  AdjustWindowRectEx(&rc, style, FALSE, WS_EX_DLGMODALFRAME);
  RECT orc{};
  GetWindowRect(owner, &orc);
  HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, wc.lpszClassName, t.cheat_title,
                           style | WS_VISIBLE, orc.left + S(30), orc.top + S(30),
                           rc.right - rc.left, rc.bottom - rc.top, owner, nullptr, inst, nullptr);
  if (!h) return;
  ExpState st;
  st.fps = fps;
  st.sepia = sepia;
  st.vsync = vsync;
  SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
  auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD s_, int x, int y, int cw, int ch,
                int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | s_, S(x), S(y), S(cw), S(ch),
                             h, reinterpret_cast<HMENU>(INT_PTR(id)), inst, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), TRUE);
    return c;
  };
  const DWORD cbs = WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL;
  mk(L"STATIC", t.xwarn, SS_SUNKEN, 16, 14, 428, 68, kIdXWarn);
  mk(L"STATIC", t.fps, SS_CENTERIMAGE, 16, 98, 180, 26, 0);
  HWND fc = mk(L"COMBOBOX", L"", cbs, 204, 98, 240, 200, kIdFps);
  SendMessageW(fc, CB_ADDSTRING, 0, (LPARAM)t.fps_orig);
  SendMessageW(fc, CB_ADDSTRING, 0, (LPARAM)t.fps_exp);
  SendMessageW(fc, CB_SETCURSEL, fps == 60 ? 1 : 0, 0);
  mk(L"STATIC", t.sepia, SS_CENTERIMAGE, 16, 136, 180, 26, kIdXSepiaLbl);
  HWND sp = mk(L"COMBOBOX", L"", cbs, 204, 136, 240, 200, kIdXSepia);
  SendMessageW(sp, CB_ADDSTRING, 0, (LPARAM)t.sepia_on);
  SendMessageW(sp, CB_ADDSTRING, 0, (LPARAM)t.sepia_off);
  SendMessageW(sp, CB_SETCURSEL, sepia ? 1 : 0, 0);
  // VSync off: the emulated 60 Hz vblank stops rounding slow frames down to 30.
  mk(L"STATIC", t.vsync, SS_CENTERIMAGE, 16, 174, 180, 26, kIdXVsyncLbl);
  HWND vs = mk(L"COMBOBOX", L"", cbs, 204, 174, 240, 200, kIdXVsync);
  SendMessageW(vs, CB_ADDSTRING, 0, (LPARAM)t.vsync_on);
  SendMessageW(vs, CB_ADDSTRING, 0, (LPARAM)t.vsync_off);
  SendMessageW(vs, CB_SETCURSEL, vsync ? 1 : 0, 0);
  mk(L"BUTTON", t.ok, WS_TABSTOP | BS_DEFPUSHBUTTON, 70, 222, 150, 32, kIdCheatOk);
  mk(L"BUTTON", t.cancel, WS_TABSTOP, 236, 222, 150, 32, kIdCheatCancel);
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
  if (st.accept) {
    fps = st.fps;
    sepia = st.sepia;
    vsync = st.vsync;
  }
}

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

LRESULT CALLBACK XProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto* st = reinterpret_cast<AdvState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (st && m == WM_KEYDOWN && w == VK_ESCAPE) {
    st->done = true;
    DestroyWindow(h);
    return 0;
  }
  // Mouse sliders: keep the value next to each one current while dragging.
  if (st && m == WM_HSCROLL && l) {
    const int r = GetDlgCtrlID(reinterpret_cast<HWND>(l)) - kIdXMouse;
    if (r >= 0 && r < 3) {
      const int v = static_cast<int>(SendMessageW(reinterpret_cast<HWND>(l), TBM_GETPOS, 0, 0));
      SetDlgItemTextW(h, kIdXMouseVal + r, MouseText(r, v).c_str());
    }
    return 0;
  }
  if (st && m == WM_COMMAND && LOWORD(w) == kIdXCheats) {
    ShowExperimentalPopup(h, st->sel, st->fps_draft, st->sepia_draft, st->vsync_draft);
    return 0;
  }
  if ((m == WM_COMMAND && (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL)) || m == WM_CLOSE) {
    if (st && LOWORD(w) == IDOK && m == WM_COMMAND) {
      // Experimental popup OK already updated the drafts; Cancel did not.
      st->opt->fps = st->fps_draft;
      st->opt->sepia = st->sepia_draft;
      st->opt->vsync = st->vsync_draft;
      // Reuse cfg key rt / adv[0]: 0 = RTV default, 2 = ROV (same as Advanced options).
      st->opt->adv[0] = SendDlgItemMessageW(h, kIdXGfx, CB_GETCURSEL, 0, 0) == 1 ? 2 : 0;
      for (int r = 0; r < 3; ++r)
        st->opt->mouse[r] = std::clamp(static_cast<int>(SendDlgItemMessageW(
                                           h, kIdXMouse + r, TBM_GETPOS, 0, 0)),
                                       kMouse[r].lo, kMouse[r].hi);
      st->opt->invuln = IsDlgButtonChecked(h, kIdCheatInv) == BST_CHECKED ? 1 : 0;
      st->opt->tick = 0;  // tick UI removed; Experimental encodes interval unlock
    }
    if (st) st->done = true;
    DestroyWindow(h);
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

// Modal advanced settings (tested options): render path (RTV/ROV), mouse
// sliders, invulnerability, and the Experimental button (frame rate, sepia).
// Saved when "Remember" is ticked, except invulnerability (session only).
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
  RECT rc{0, 0, S(460), S(300)};
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
  st.sel = sel;
  st.fps_draft = o->fps;
  st.sepia_draft = o->sepia;
  st.vsync_draft = o->vsync;
  SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
  auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD s_, int x, int y, int cw, int ch,
                int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | s_, S(x), S(y), S(cw), S(ch),
                             h, reinterpret_cast<HMENU>(INT_PTR(id)), inst, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), TRUE);
    return c;
  };
  const DWORD cbs = WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL;
  mk(L"STATIC", t.gfx, SS_CENTERIMAGE, 16, 16, 180, 26, kIdXGfxLbl);
  HWND gfx = mk(L"COMBOBOX", L"", cbs, 204, 16, 240, 200, kIdXGfx);
  SendMessageW(gfx, CB_ADDSTRING, 0, (LPARAM)t.gfx_rtv);
  SendMessageW(gfx, CB_ADDSTRING, 0, (LPARAM)t.gfx_rov);
  // adv[0]: 0/1 = RTV, 2 = ROV (same cfg key as Advanced options "rt").
  SendMessageW(gfx, CB_SETCURSEL, o->adv[0] == 2 ? 1 : 0, 0);
  // Mouse camera (keyboard play): sensitivity, dead zone, smoothing.
  const wchar_t* mouse_lbl[3] = {t.mouse_sens, t.mouse_dz, t.mouse_win};
  INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_BAR_CLASSES};
  InitCommonControlsEx(&icc);
  for (int r = 0; r < 3; ++r) {
    const MouseSlider& ms = kMouse[r];
    const int y = 54 + r * 38;
    const int v = std::clamp(o->mouse[r], ms.lo, ms.hi);
    mk(L"STATIC", mouse_lbl[r], SS_CENTERIMAGE, 16, y, 180, 26, 0);
    HWND tb = mk(TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 198, y, 192, 26,
                 kIdXMouse + r);
    SendMessageW(tb, TBM_SETRANGEMIN, FALSE, ms.lo);
    SendMessageW(tb, TBM_SETRANGEMAX, FALSE, ms.hi);
    SendMessageW(tb, TBM_SETLINESIZE, 0, ms.step);
    SendMessageW(tb, TBM_SETPAGESIZE, 0, ms.step * 5);
    SendMessageW(tb, TBM_SETPOS, TRUE, v);
    mk(L"STATIC", MouseText(r, v).c_str(), SS_CENTERIMAGE | SS_RIGHT, 392, y, 52, 26,
       kIdXMouseVal + r);
  }
  HWND inv = mk(L"BUTTON", t.cheat_inv, WS_TABSTOP | BS_AUTOCHECKBOX, 16, 168, 428, 26, kIdCheatInv);
  SendMessageW(inv, BM_SETCHECK, o->invuln ? BST_CHECKED : BST_UNCHECKED, 0);
  mk(L"BUTTON", t.cheats, WS_TABSTOP, 16, 206, 428, 28, kIdXCheats);
  mk(L"BUTTON", t.ok, WS_TABSTOP | BS_DEFPUSHBUTTON, 170, 250, 120, 32, IDOK);
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
      } else if (LOWORD(w) == kIdAdvBtn) {
        ShowAdvanced(h, st->sel, &st->opt);
      } else if (LOWORD(w) == kIdXBtn) {
        ShowExperimental(h, st->sel, &st->opt);
      } else if (LOWORD(w) == kIdCombo && HIWORD(w) == CBN_SELCHANGE) {
        st->sel = static_cast<int>(SendDlgItemMessageW(h, kIdCombo, CB_GETCURSEL, 0, 0));
        ApplyTexts(h, st->sel);
      } else if (LOWORD(w) == IDCANCEL) {  // Esc: same as closing the window
        PostMessageW(h, WM_CLOSE, 0, 0);
      } else if (LOWORD(w) == IDOK) {
        st->play = true;
        st->opt.window = SendDlgItemMessageW(h, kIdDisp, CB_GETCURSEL, 0, 0) == 1;
        st->opt.control = std::clamp(
            static_cast<int>(SendDlgItemMessageW(h, kIdCtrl, CB_GETCURSEL, 0, 0)), 0, 2);
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
  // Vertical layout (unscaled px); rows stack with no gaps when the
  // advanced button is hidden.
  const bool adv = REXCVAR_GET(advanced);
  const int yAdv = 202, ySaves = adv ? 236 : 202, yCheck = ySaves + 40,
            yHint = yCheck + 26, yPlay = yHint + 42, yEnd = yPlay + 34 + 20;
  RECT rc{0, 0, S(400), S(yEnd)};
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
  mk(L"STATIC", SS_CENTERIMAGE, 20, 128, 150, 26, kIdCtrlLbl);
  HWND ctrl = mk(L"COMBOBOX", cbs, 180, 128, 200, 200, kIdCtrl);
  SendMessageW(ctrl, CB_ADDSTRING, 0, (LPARAM)L"-");
  SendMessageW(ctrl, CB_SETCURSEL, std::clamp(o->control, 0, 2), 0);
  mk(L"BUTTON", WS_TABSTOP, 20, 168, 360, 28, kIdXBtn);  // always shown
  if (adv) mk(L"BUTTON", WS_TABSTOP, 20, yAdv, 360, 28, kIdAdvBtn);
  mk(L"BUTTON", WS_TABSTOP, 20, ySaves, 360, 28, kIdSavesBtn);
  mk(L"BUTTON", WS_TABSTOP | BS_AUTOCHECKBOX, 20, yCheck, 360, 22, kIdCheck);
  mk(L"STATIC", 0, 20, yHint, 360, 36, kIdHint);
  HWND play = mk(L"BUTTON", WS_TABSTOP | BS_DEFPUSHBUTTON, 140, yPlay, 120, 34,
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
      if (line.rfind("tick=", 0) == 0) opt.tick = std::clamp(std::atoi(line.c_str() + 5), 0, 3);
      if (line.rfind("window=", 0) == 0) opt.window = line[7] == '1';
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
    }
  }
  if (!Valid(lang)) { lang = 0; ask = true; }
  // Migrate pre-single-FPS saves: Original unless fps was 60 AND tick was non-original.
  if (opt.fps == 60 && opt.tick > 0)
    opt.fps = 60;  // Experimental (60)
  else
    opt.fps = 0;   // Original (30) — covers old 0/30/60+original-tick
  opt.tick = 0;
  // Internal resolution defaults to native (1x); a saved scale is only kept
  // when "Remember" was ticked (ask=0).
  const int saved_scale = opt.scale;
  if (ask) opt.scale = 1;
  // Same for the frame rate: default Original (30) = interval 2, no host cap.
  const int saved_fps = opt.fps;
  if (ask) opt.fps = 0;
  // Without --advanced, most diagnostic options are ignored (safe defaults).
  // Render path (adv[0] / cfg "rt") stays user-facing for RTV/ROV A/B tests.
  // Quad depth is not a launcher option. A saved qdepth= line is ignored so an
  // old Off/Full choice cannot override the runtime default. A command-line or
  // REX_AFRO_QUAD_DEPTH_OFFSET value still wins because this loop never sets it.
  const Options saved_adv = opt;
  if (!REXCVAR_GET(advanced)) {
    const int rt = opt.adv[0];
    for (int i = 0; i < kAdvN; ++i) opt.adv[i] = 0;
    opt.adv[0] = (rt == 2) ? 2 : 0;
  }
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
    if (!lang) {  // Launcher closed without Play: quit, keep the old config.
      std::fprintf(stderr, "[afro] launcher closed, not starting the game\n");
      std::fflush(nullptr);
      ExitProcess(0);
    }
    std::ofstream out(path, std::ios::trunc);
    out << "language=" << lang << "\nask=" << (remember ? 0 : 1) << "\nfps=" << (remember ? opt.fps : saved_fps)
        << "\nsepia=" << (remember ? opt.sepia : stored.sepia)
        << "\nvsync=" << (remember ? opt.vsync : stored.vsync)
        << "\ntick=0"
        << "\nwindow=" << (opt.window ? 1 : 0) << "\ncontrol=" << opt.control << "\nscale=" << (remember ? opt.scale : saved_scale)
        << "\nversion=" << AFRO_VERSION << "\n";
    for (int r = 0; r < 3; ++r) out << kMouse[r].key << "=" << opt.mouse[r] << "\n";
    for (int i = 0; i < kAdvN; ++i) {
      int v;
      if (i == 0)
        v = remember ? opt.adv[i] : saved_adv.adv[i];
      else if (!REXCVAR_GET(advanced))
        v = saved_adv.adv[i];
      else
        v = remember ? opt.adv[i] : stored.adv[i];
      out << kAdv[i].key << "=" << v << "\n";
    }
  }
#else
  (void)show;  // No native picker on Linux: use config or default English.
#endif
  if (!lang) lang = 1;
  // Command-line / toml values win over the launcher.
  // Original (0): present interval 2 (default), no host cap.
  // Experimental (60): present interval 1 + host cap 60.
  if (opt.fps == 60) {
    if (!Explicit("afro_present_interval"))
      SetCvar("afro_present_interval", "1");
    if (!Explicit("afro_fps_cap"))
      SetCvar("afro_fps_cap", "60");
  }
  if (opt.sepia && !Explicit("afro_fx_sepia"))
    SetCvar("afro_fx_sepia", "false");
  // VSync off (Experimental): GPU plugin cvar, so SetCvar defers it via REX_VSYNC.
  // frame_pacing.cpp then holds 30 FPS scenes (retail, cutscenes) at 30 itself.
  if (opt.vsync && !Explicit("vsync"))
    SetCvar("vsync", "false");
  // The SDK marked every CPU-uploaded page stale again at each frame end, so all
  // meshes were sent to the GPU every frame (10-30 MB). CPU writes are still
  // caught by the page write watches; this only drops the extra re-upload.
  if (!Explicit("clear_memory_page_state"))
    SetCvar("clear_memory_page_state", "false");
  // Invulnerability: session-only. Never read/written in afro.cfg; default OFF each launch
  // unless Cheats OK then Advanced OK set it for this run (or the cvar is passed).
  if (opt.invuln && !Explicit("afro_player_invuln"))
    SetCvar("afro_player_invuln", "true");
  if (opt.invuln)
    std::fprintf(stderr,
                 "[afro] invuln: requested ON for this session (player HP only; not saved)\n");
  if (!Explicit("fullscreen"))
    SetCvar("fullscreen", opt.window ? "false" : "true");
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
