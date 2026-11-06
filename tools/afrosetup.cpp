// Graphical installer for AfroSamuraiRecomp 0.1.0.
// Embedded source only. Closing the window never terminates a running build.

#include "afrosetup_cmd.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

namespace {

enum Page { kLanguage, kMode, kFolder, kGame, kLicense, kOptions, kSummary, kProgress, kResult };
enum Lang { kEn, kEs, kFr, kIt };

struct Text {
  const wchar_t *title, *back, *next, *cancel, *browse, *install, *finish, *play;
  const wchar_t *lang_title, *name_en, *name_es, *name_fr, *name_it;
  const wchar_t *folder_title;
  const wchar_t *game_title, *game_iso, *game_dir, *game_europe;
  const wchar_t *lic_before, *lic_word, *lic_after;
  const wchar_t *opt_title, *opt_short, *opt_keep, *opt_clean, *space_known, *space_unknown;
  const wchar_t *sum_title, *sum_lang, *sum_folder, *sum_game, *sum_lic, *sum_short, *sum_keep, *sum_yes, *sum_no;
  const wchar_t *run_title, *run_minutes, *run_details, *run_hide, *run_close;
  const wchar_t *done_title, *fail_title;
  const wchar_t *mode_title, *mode_install, *mode_update, *update_btn;
  const wchar_t *upd_folder, *upd_bad, *upd_running, *upd_same, *upd_tools_ok, *upd_tools_part, *upd_tools_missing;
  const wchar_t *upd_xex_ok, *upd_xex_need, *upd_ver_unknown, *upd_lic_kept;
};

const Text kText[] = {
    {L"AfroSamuraiRecomp", L"Back", L"Next", L"Cancel", L"Browse...", L"Install", L"Finish", L"Play",
     L"Select a language", L"English", L"Espa\u00f1ol", L"Fran\u00e7ais", L"Italiano",
     L"Select the installation folder",
     L"Game files", L"Game ISO", L"Extracted game folder", L"Europe disc only.",
     L"I accept the ", L"license", L" for Visual Studio Build Tools",
     L"After installation", L"Create desktop and Start menu shortcuts", L"Keep the downloaded tools",
     L"Delete the downloaded tools when installation finishes",
     L"Approximate space those tools would free: ",
     L"The recoverable space is calculated when the downloaded tools exist, not before.",
     L"Summary", L"Language", L"Folder", L"Game", L"License", L"Shortcuts", L"Tools afterwards", L"Yes", L"No",
     L"Installing", L"This will take several minutes", L"Show details", L"Hide details",
     L"The installation keeps running. Closing this window does not stop it.",
     L"Installation finished", L"Installation did not finish",
     L"Install or update", L"Install", L"Update", L"Update",
     L"Select the existing installation folder", L"This folder is not an AfroSamuraiRecomp installation.",
     L"Afrorecomp is running. Close it. This setup will not stop it.",
     L"Same version. Program files are rebuilt from this setup and replaced only after the build succeeds.",
     L"Build tools already here will be reused.", L"Some build tools are missing and only those will be downloaded again.",
     L"Build tools are missing and will be downloaded again. The extracted game is kept.",
     L"Extracted game found. No ISO is required.", L"Extracted game not found. The Europe ISO is required.",
     L"not recorded", L"Build-tools license already recorded with the installed compiler."},
    {L"AfroSamuraiRecomp", L"Atr\u00e1s", L"Siguiente", L"Cancelar", L"Examinar...", L"Instalar", L"Finalizar", L"Jugar",
     L"Seleccione idioma", L"English", L"Espa\u00f1ol", L"Fran\u00e7ais", L"Italiano",
     L"Seleccione la carpeta de instalaci\u00f3n",
     L"Archivos del juego", L"ISO del juego", L"Carpeta del juego extra\u00eddo", L"Solo el disco de Europa.",
     L"Acepto la ", L"licencia", L" de Visual Studio Build Tools",
     L"Despu\u00e9s de la instalaci\u00f3n", L"Crear accesos en el escritorio y el men\u00fa Inicio",
     L"Conservar las herramientas descargadas", L"Borrar las herramientas descargadas al terminar",
     L"Espacio aproximado que liberar\u00edan esas herramientas: ",
     L"El espacio recuperable se calcular\u00e1 cuando existan las herramientas descargadas.",
     L"Resumen", L"Idioma", L"Carpeta", L"Juego", L"Licencia", L"Accesos", L"Herramientas despu\u00e9s", L"S\u00ed", L"No",
     L"Instalando", L"Este proceso tardar\u00e1 varios minutos", L"Ver detalles", L"Ocultar detalles",
     L"La instalaci\u00f3n sigue. Cerrar esta ventana no la detiene.",
     L"Instalaci\u00f3n finalizada", L"La instalaci\u00f3n no ha terminado",
     L"Instalar o actualizar", L"Instalar", L"Actualizar", L"Actualizar",
     L"Seleccione la instalaci\u00f3n existente", L"Esta carpeta no es una instalaci\u00f3n de AfroSamuraiRecomp.",
     L"Afrorecomp est\u00e1 en ejecuci\u00f3n. Ci\u00e9rrelo. Este programa no lo detendr\u00e1.",
     L"Misma versi\u00f3n. Se recompilan los archivos del programa y solo se sustituyen si la compilaci\u00f3n termina bien.",
     L"Las herramientas de compilaci\u00f3n que ya est\u00e1n se reutilizan.",
     L"Faltan algunas herramientas. Solo esas se volver\u00e1n a descargar.",
     L"Faltan las herramientas de compilaci\u00f3n. Se descargar\u00e1n otra vez. El juego extra\u00eddo no se borra.",
     L"El juego extra\u00eddo ya est\u00e1. No hace falta la ISO.", L"No est\u00e1 el juego extra\u00eddo. Hace falta la ISO.",
     L"no registrada", L"La licencia de las herramientas ya aceptada sigue registrada."},
    {L"AfroSamuraiRecomp", L"Retour", L"Suivant", L"Annuler", L"Parcourir...", L"Installer", L"Terminer", L"Jouer",
     L"Choisissez la langue", L"English", L"Espa\u00f1ol", L"Fran\u00e7ais", L"Italiano",
     L"S\u00e9lectionnez le dossier d'installation",
     L"Fichiers du jeu", L"ISO du jeu", L"Dossier du jeu extrait", L"Disque Europe uniquement.",
     L"J'accepte la ", L"licence", L" Visual Studio Build Tools",
     L"Apr\u00e8s l'installation", L"Cr\u00e9er des raccourcis sur le bureau et dans le menu D\u00e9marrer",
     L"Garder les outils t\u00e9l\u00e9charg\u00e9s", L"Supprimer les outils t\u00e9l\u00e9charg\u00e9s \u00e0 la fin",
     L"Espace approximatif que ces outils lib\u00e9reraient : ",
     L"L'espace r\u00e9cup\u00e9rable sera calcul\u00e9 lorsque les outils t\u00e9l\u00e9charg\u00e9s existeront.",
     L"R\u00e9sum\u00e9", L"Langue", L"Dossier", L"Jeu", L"Licence", L"Raccourcis", L"Outils ensuite", L"Oui", L"Non",
     L"Installation", L"Cette op\u00e9ration prendra plusieurs minutes", L"Voir les d\u00e9tails", L"Masquer les d\u00e9tails",
     L"L'installation continue. Fermer cette fen\u00eatre ne l'arr\u00eate pas.",
     L"Installation termin\u00e9e", L"L'installation ne s'est pas termin\u00e9e",
     L"Installer ou mettre \u00e0 jour", L"Installer", L"Mettre \u00e0 jour", L"Mettre \u00e0 jour",
     L"S\u00e9lectionnez l'installation existante", L"Ce dossier n'est pas une installation d'AfroSamuraiRecomp.",
     L"Afrorecomp est en cours d'ex\u00e9cution. Fermez-le. Ce programme ne l'arr\u00eatera pas.",
     L"M\u00eame version. Les fichiers du programme sont recompil\u00e9s et remplac\u00e9s seulement si la compilation r\u00e9ussit.",
     L"Les outils de compilation d\u00e9j\u00e0 pr\u00e9sents sont r\u00e9utilis\u00e9s.",
     L"Certains outils manquent. Seuls ceux-l\u00e0 seront t\u00e9l\u00e9charg\u00e9s de nouveau.",
     L"Les outils de compilation manquent et seront t\u00e9l\u00e9charg\u00e9s de nouveau. Le jeu extrait est conserv\u00e9.",
     L"Jeu extrait trouv\u00e9. L'ISO n'est pas n\u00e9cessaire.", L"Jeu extrait introuvable. L'ISO Europe est n\u00e9cessaire.",
     L"non enregistr\u00e9e", L"La licence des outils d\u00e9j\u00e0 accept\u00e9e reste enregistr\u00e9e."},
    {L"AfroSamuraiRecomp", L"Indietro", L"Avanti", L"Annulla", L"Sfoglia...", L"Installa", L"Fine", L"Gioca",
     L"Seleziona la lingua", L"English", L"Espa\u00f1ol", L"Fran\u00e7ais", L"Italiano",
     L"Seleziona la cartella di installazione",
     L"File del gioco", L"ISO del gioco", L"Cartella del gioco estratto", L"Solo il disco europeo.",
     L"Accetto la ", L"licenza", L" di Visual Studio Build Tools",
     L"Dopo l'installazione", L"Crea collegamenti sul desktop e nel menu Start",
     L"Tieni gli strumenti scaricati", L"Elimina gli strumenti scaricati alla fine",
     L"Spazio approssimativo che quegli strumenti libererebbero: ",
     L"Lo spazio recuperabile sar\u00e0 calcolato quando gli strumenti scaricati esisteranno.",
     L"Riepilogo", L"Lingua", L"Cartella", L"Gioco", L"Licenza", L"Collegamenti", L"Strumenti dopo", L"S\u00ec", L"No",
     L"Installazione", L"Questa operazione richieder\u00e0 alcuni minuti", L"Mostra dettagli", L"Nascondi dettagli",
     L"L'installazione continua. Chiudere questa finestra non la ferma.",
     L"Installazione terminata", L"L'installazione non \u00e8 terminata",
     L"Installa o aggiorna", L"Installa", L"Aggiorna", L"Aggiorna",
     L"Seleziona l'installazione esistente", L"Questa cartella non \u00e8 un'installazione di AfroSamuraiRecomp.",
     L"Afrorecomp \u00e8 in esecuzione. Chiudilo. Questo programma non lo terminer\u00e0.",
     L"Stessa versione. I file del programma vengono ricompilati e sostituiti solo se la compilazione riesce.",
     L"Gli strumenti di compilazione gi\u00e0 presenti vengono riutilizzati.",
     L"Mancano alcuni strumenti. Verranno scaricati di nuovo solo quelli.",
     L"Mancano gli strumenti di compilazione e verranno scaricati di nuovo. Il gioco estratto resta.",
     L"Gioco estratto trovato. L'ISO non serve.", L"Gioco estratto assente. Serve l'ISO europea.",
     L"non registrata", L"La licenza degli strumenti gi\u00e0 accettata resta registrata."},
};

const wchar_t kLicenseUrl[] = L"https://go.microsoft.com/fwlink/?LinkId=2327714";
const COLORREF kCharcoal = RGB(28, 28, 30);
const COLORREF kInk = RGB(245, 245, 245);
const COLORREF kRed = RGB(176, 36, 40);
const COLORREF kField = RGB(255, 255, 255);
const COLORREF kFieldText = RGB(20, 20, 20);
const int kBack = 1001, kNext = 1002, kCancel = 1003, kBrowse = 1004, kBrowse2 = 1005;
const int kEdit = 1100, kEdit2 = 1101, kLog = 1102, kCheck = 1200, kRadio0 = 1300;
const int kDetails = 1400, kPlay = 1500, kBar = 1600, kStatus = 1601, kLink = 1700;

HINSTANCE g_inst;
HWND g_wnd;
Page g_page = kLanguage;
int g_lang = kEn;
UINT g_dpi = 96;
std::wstring g_install, g_iso, g_xexdir, g_cli_tree;
bool g_use_iso = true;
bool g_update = false;
bool g_license = false;
bool g_shortcut = true;
bool g_keep = true;
std::wstring g_log, g_status;
int g_pct = 0;
bool g_show_details = false;
bool g_child_running = false;
bool g_spawned = false;
bool g_success = false;
DWORD g_exit_code = 1;
bool g_payload_ok = false;
std::wstring g_dump_dir;
std::vector<HWND> g_dyn;
HFONT g_font = nullptr;
HFONT g_link_font = nullptr;
HBRUSH g_charcoal = nullptr;
HBRUSH g_white = nullptr;
WNDPROC g_edit_proc = nullptr;

int Dpi(int v) { return MulDiv(v, static_cast<int>(g_dpi), 96); }
const Text& T() { return kText[g_lang]; }

void SetFont(HWND h) {
  if (g_font) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
}

LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  const LRESULT r = CallWindowProcW(g_edit_proc, h, m, w, l);
  if (m == WM_NCPAINT || m == WM_NCACTIVATE) {
    HDC dc = GetWindowDC(h);
    RECT rc{};
    GetWindowRect(h, &rc);
    OffsetRect(&rc, -rc.left, -rc.top);
    HBRUSH brush = CreateSolidBrush(kRed);
    FrameRect(dc, &rc, brush);
    InflateRect(&rc, -1, -1);
    FrameRect(dc, &rc, brush);
    DeleteObject(brush);
    ReleaseDC(h, dc);
  }
  return r;
}

HWND Mk(const wchar_t* cls, const wchar_t* text, DWORD style, DWORD ex, int x, int y, int w, int h, int id) {
  HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, Dpi(x), Dpi(y), Dpi(w), Dpi(h), g_wnd,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_inst, nullptr);
  SetFont(c);
  if (wcscmp(cls, L"Edit") == 0) {
    if (!g_edit_proc) g_edit_proc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(c, GWLP_WNDPROC));
    SetWindowLongPtrW(c, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc));
  }
  g_dyn.push_back(c);
  return c;
}

void ClearDyn() {
  for (HWND h : g_dyn) DestroyWindow(h);
  g_dyn.clear();
}

std::wstring GetEdit(int id) {
  HWND h = GetDlgItem(g_wnd, id);
  if (!h) return {};
  const int n = GetWindowTextLengthW(h);
  std::wstring s(static_cast<size_t>(n), L'\0');
  GetWindowTextW(h, s.data(), n + 1);
  return s;
}

std::wstring Trim(std::wstring s) {
  while (!s.empty() && (s.back() == L'\r' || s.back() == L'\n' || s.back() == L' ' || s.back() == L'\t')) s.pop_back();
  size_t i = 0;
  while (i < s.size() && (s[i] == L' ' || s[i] == L'\t' || s[i] == 0xFEFF)) ++i;
  return s.substr(i);
}

std::wstring ReadVersionFile(const std::wstring& tree) {
  FILE* f = nullptr;
  if (_wfopen_s(&f, (tree + L"\\app\\VERSION").c_str(), L"rb") != 0 || !f) return {};
  char buf[256];
  const size_t n = fread(buf, 1, sizeof buf, f);
  fclose(f);
  return Trim(std::wstring(buf, buf + n));
}

bool FileExists(const std::wstring& path) {
  const DWORD a = GetFileAttributesW(path.c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool PathExists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

bool AfrorecompRunning() {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof pe;
  bool found = false;
  if (Process32FirstW(snap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, L"afrorecomp.exe") == 0) found = true;
    } while (!found && Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return found;
}

std::wstring InstalledVersion(const std::wstring& root) {
  const std::wstring direct = ReadVersionFile(root + L"\\afrorecomp");
  if (!direct.empty()) return direct;
  FILE* f = nullptr;
  if (_wfopen_s(&f, (root + L"\\.install\\build.rev").c_str(), L"rb") != 0 || !f) return {};
  char buf[256];
  const size_t n = fread(buf, 1, sizeof buf, f);
  fclose(f);
  std::wstring rev;
  rev.reserve(n);
  for (size_t i = 0; i < n; ++i) rev.push_back(static_cast<wchar_t>(static_cast<unsigned char>(buf[i])));
  return afrosetup::VersionFromBuildRev(Trim(rev));
}

bool ExtractedGame() { return FileExists(g_install + L"\\gamedata\\default.xex"); }

bool MsvcRecorded() {
  return afrosetup::LicenseAlreadyRecorded(
      g_update, FileExists(g_install + L"\\tools\\msvc\\setup_x64.bat"),
      FileExists(g_install + L"\\.install\\msvc.done"));
}

afrosetup::ToolPlan ToolState(int* present_out, int* total_out) {
  int present = 0, total = 0;
  for (const auto& stamp : afrosetup::kReuseStamps) {
    if (std::string(stamp.stamp) == "gamedata") continue;
    ++total;
    std::wstring rel(stamp.relative_file, stamp.relative_file + std::strlen(stamp.relative_file));
    for (wchar_t& c : rel)
      if (c == L'/') c = L'\\';
    if (PathExists(g_install + L"\\" + rel)) ++present;
  }
  if (present_out) *present_out = present;
  if (total_out) *total_out = total;
  return afrosetup::PlanTools(present, total);
}

bool ExistingInstall() {
  return FileExists(g_install + L"\\game\\Play.bat") || FileExists(g_install + L"\\game\\afrorecomp.exe");
}

Page Forward(Page p) {
  if (p == kLanguage) return kMode;
  if (p == kMode) return kFolder;
  if (p == kFolder) return (!g_update || !ExtractedGame()) ? kGame : (MsvcRecorded() ? kOptions : kLicense);
  if (p == kGame) return MsvcRecorded() ? kOptions : kLicense;
  if (p == kLicense) return kOptions;
  if (p == kOptions) return kSummary;
  return p;
}

Page Backward(Page p) {
  if (p == kSummary) return kOptions;
  if (p == kOptions) return MsvcRecorded() ? ( !g_update || !ExtractedGame() ? kGame : kFolder) : kLicense;
  if (p == kLicense) return (!g_update || !ExtractedGame()) ? kGame : kFolder;
  if (p == kGame) return kFolder;
  if (p == kFolder) return kMode;
  if (p == kMode) return kLanguage;
  return p;
}

std::wstring BrowseFolder(const wchar_t* title) {
  BROWSEINFOW bi{};
  bi.hwndOwner = g_wnd;
  bi.lpszTitle = title;
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
  PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
  if (!pidl) return {};
  wchar_t path[MAX_PATH];
  std::wstring out;
  if (SHGetPathFromIDListW(pidl, path)) out = path;
  CoTaskMemFree(pidl);
  return out;
}

std::wstring BrowseIso() {
  wchar_t file[MAX_PATH] = {};
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof ofn;
  ofn.hwndOwner = g_wnd;
  ofn.lpstrFilter = L"ISO (*.iso)\0*.iso\0All files\0*.*\0";
  ofn.lpstrFile = file;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
  if (!GetOpenFileNameW(&ofn)) return {};
  return file;
}

void ReadPage() {
  if (g_page == kFolder) g_install = Trim(GetEdit(kEdit));
  if (g_page == kGame) {
    g_use_iso = IsDlgButtonChecked(g_wnd, kRadio0) == BST_CHECKED;
    g_iso = Trim(GetEdit(kEdit));
    g_xexdir = Trim(GetEdit(kEdit2));
  }
  if (g_page == kLicense) g_license = IsDlgButtonChecked(g_wnd, kCheck) == BST_CHECKED;
  if (g_page == kOptions) {
    g_shortcut = IsDlgButtonChecked(g_wnd, kCheck) == BST_CHECKED;
    g_keep = IsDlgButtonChecked(g_wnd, kRadio0) == BST_CHECKED;
  }
  if (g_page == kMode) g_update = IsDlgButtonChecked(g_wnd, kRadio0 + 1) == BST_CHECKED;
  if (g_page == kLanguage) {
    for (int i = 0; i < 4; ++i)
      if (IsDlgButtonChecked(g_wnd, kRadio0 + i) == BST_CHECKED) g_lang = i;
  }
}

std::wstring SpaceSentence() {
  const std::filesystem::path iso = g_use_iso ? std::filesystem::path(g_iso) : std::filesystem::path();
  const afrosetup::SpaceEstimate est =
      afrosetup::RecoverableBuildBytes(std::filesystem::path(g_install), iso);
  if (!est.known) return T().space_unknown;
  return std::wstring(T().space_known) + afrosetup::FormatByteAmount(est.bytes);
}

void SyncGameFields() {
  EnableWindow(GetDlgItem(g_wnd, kEdit), g_use_iso);
  EnableWindow(GetDlgItem(g_wnd, kBrowse), g_use_iso);
  EnableWindow(GetDlgItem(g_wnd, kEdit2), !g_use_iso);
  EnableWindow(GetDlgItem(g_wnd, kBrowse2), !g_use_iso);
}

void Layout();

void Go(Page p) {
  ReadPage();
  g_page = p;
  Layout();
}

bool LoadOwnPayload(std::vector<afrosetup::PayloadFile>& files, std::string& error) {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"rb") != 0 || !f) {
    error = "cannot read setup exe";
    return false;
  }
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> buf(static_cast<size_t>(n > 0 ? n : 0));
  if (!buf.empty()) fread(buf.data(), 1, buf.size(), f);
  fclose(f);
  return afrosetup::ParsePayload(buf.data(), buf.size(), files, error);
}

std::wstring TempRoot() {
  wchar_t tmp[MAX_PATH];
  GetTempPathW(MAX_PATH, tmp);
  return std::wstring(tmp) + L"afrosetup-" + std::to_wstring(GetCurrentProcessId());
}

bool ExtractPayload(const std::wstring& dest, std::wstring& error) {
  std::vector<afrosetup::PayloadFile> files;
  std::string err;
  if (!LoadOwnPayload(files, err)) {
    error.assign(err.begin(), err.end());
    return false;
  }
  for (const auto& file : files) {
    std::wstring rel(file.path.begin(), file.path.end());
    for (wchar_t& c : rel)
      if (c == L'/') c = L'\\';
    const std::wstring full = dest + L"\\" + rel;
    const size_t slash = full.find_last_of(L'\\');
    if (slash != std::wstring::npos) {
      for (size_t i = 3; i < slash; ++i)
        if (full[i] == L'\\') CreateDirectoryW(full.substr(0, i).c_str(), nullptr);
      CreateDirectoryW(full.substr(0, slash).c_str(), nullptr);
    }
    FILE* out = nullptr;
    if (_wfopen_s(&out, full.c_str(), L"wb") != 0 || !out) {
      error = L"cannot write " + full;
      return false;
    }
    if (!file.bytes.empty()) fwrite(file.bytes.data(), 1, file.bytes.size(), out);
    fclose(out);
  }
  return true;
}

void ApplyProgress(const std::wstring& chunk) {
  size_t start = 0;
  while (start < chunk.size()) {
    const size_t end = chunk.find(L"\r\n", start);
    const std::wstring line = chunk.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
    afrosetup::ProgressEvent ev;
    if (afrosetup::ParseProgressLine(line, ev)) {
      g_pct = ev.percent;
      g_status = afrosetup::PrimaryProgress(ev.percent, ev.status, g_lang);
      if (HWND bar = GetDlgItem(g_wnd, kBar)) SendMessageW(bar, PBM_SETPOS, g_pct, 0);
      if (HWND st = GetDlgItem(g_wnd, kStatus)) SetWindowTextW(st, g_status.c_str());
    }
    if (end == std::wstring::npos) break;
    start = end + 2;
  }
}

void AppendLog(const std::wstring& more) {
  g_log += more;
  ApplyProgress(more);
  HWND edit = GetDlgItem(g_wnd, kLog);
  if (!edit) return;
  SetWindowTextW(edit, g_log.c_str());
  SendMessageW(edit, EM_SETSEL, g_log.size(), g_log.size());
  SendMessageW(edit, EM_SCROLLCARET, 0, 0);
}

void Ingest(const std::string& bytes) { AppendLog(afrosetup::NormalizeConsole(bytes)); }

afrosetup::Request MakeRequest(const std::wstring& script, const std::wstring& tree) {
  afrosetup::Request r;
  wchar_t windir[MAX_PATH];
  GetWindowsDirectoryW(windir, MAX_PATH);
  r.powershell_exe = std::wstring(windir) + L"\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
  r.script_path = script;
  r.install_dir = g_install;
  if (g_use_iso) r.iso = g_iso;
  else r.gamedata_dir = g_xexdir;
  r.language = g_lang == kEs ? L"es" : g_lang == kFr ? L"fr" : g_lang == kIt ? L"it" : L"en";
  r.accept_license = g_license;
  r.license_recorded = MsvcRecorded();
  r.shortcut = g_shortcut;
  r.keep_build_files = g_keep;
  r.update = g_update;
  if (g_update && ExtractedGame()) {
    r.iso.clear();
    r.gamedata_dir.clear();
  }
  if (MsvcRecorded()) r.accept_license = false;
  r.local_tree = tree;
  return r;
}

void ShowError(const std::wstring& msg) { MessageBoxW(g_wnd, msg.c_str(), T().title, MB_ICONWARNING); }

bool ScriptIsLocalTree(const std::wstring& script) {
  FILE* sf = nullptr;
  if (_wfopen_s(&sf, script.c_str(), L"rb") != 0 || !sf) return false;
  std::string text;
  char buf[4096];
  size_t n = 0;
  while ((n = fread(buf, 1, sizeof buf, sf)) > 0) text.append(buf, n);
  fclose(sf);
  return text.find("LocalTree") != std::string::npos && text.find("Refusing -Ref main") != std::string::npos;
}

void StartInstall() {
  if (!afrosetup::MaySpawn(true, g_child_running)) return;
  ReadPage();
  if (g_update && AfrorecompRunning()) {
    ShowError(T().upd_running);
    return;
  }
  if (!MsvcRecorded() && !g_license) return;
  const std::wstring temp = TempRoot();
  const bool bundled = g_cli_tree.empty();
  const std::wstring tree = bundled ? temp + L"\\source" : g_cli_tree;
  if (bundled) {
    CreateDirectoryW(temp.c_str(), nullptr);
    CreateDirectoryW(tree.c_str(), nullptr);
    std::wstring error;
    if (!ExtractPayload(tree, error)) {
      ShowError(error);
      return;
    }
  }
  if (ReadVersionFile(tree) != afrosetup::kExpectedVersion) {
    ShowError(L"app\\VERSION is not 0.1.0.");
    return;
  }
  const std::wstring script = tree + L"\\tools\\install.ps1";
  if (!ScriptIsLocalTree(script)) {
    ShowError(L"tools\\install.ps1 would not stay on the embedded source.");
    return;
  }
  CreateDirectoryW(g_install.c_str(), nullptr);
  const afrosetup::Request req = MakeRequest(script, tree);
  const auto problems = afrosetup::Validate(req);
  if (!problems.empty()) {
    ShowError(problems.front());
    return;
  }
  const auto argv = afrosetup::BuildArgv(req);
  if (afrosetup::ArgvUsesPublicMain(argv)) {
    ShowError(L"Refusing to launch a command that selects public main.");
    return;
  }
  const std::wstring cmd = afrosetup::JoinCommandLine(argv);
  FILE* cf = nullptr;
  if (_wfopen_s(&cf, (g_install + L"\\afrosetup-command.txt").c_str(), L"wb") == 0 && cf) {
    const unsigned char bom[] = {0xFF, 0xFE};
    fwrite(bom, 1, 2, cf);
    fwrite(cmd.data(), sizeof(wchar_t), cmd.size(), cf);
    fclose(cf);
  }
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof sa;
  sa.bInheritHandle = TRUE;
  HANDLE read_pipe = nullptr, write_pipe = nullptr;
  if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) return;
  SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOW si{};
  si.cb = sizeof si;
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  si.hStdOutput = write_pipe;
  si.hStdError = write_pipe;
  PROCESS_INFORMATION pi{};
  std::vector<wchar_t> mutable_cmd(cmd.begin(), cmd.end());
  mutable_cmd.push_back(L'\0');
  if (!CreateProcessW(nullptr, mutable_cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                      (tree + L"\\tools").c_str(), &si, &pi)) {
    CloseHandle(read_pipe);
    CloseHandle(write_pipe);
    ShowError(L"PowerShell was not started.");
    return;
  }
  CloseHandle(write_pipe);
  CloseHandle(pi.hThread);
  g_spawned = true;
  g_child_running = true;
  g_success = false;
  g_pct = 0;
  g_status.clear();
  g_log.clear();
  g_show_details = false;
  Go(kProgress);
  std::string pending;
  auto drain = [&] {
    DWORD avail = 0;
    while (PeekNamedPipe(read_pipe, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
      char buf[4096];
      DWORD got = 0;
      if (!ReadFile(read_pipe, buf, sizeof buf, &got, nullptr) || got == 0) break;
      pending.append(buf, buf + got);
    }
    if (!pending.empty()) {
      Ingest(pending);
      pending.clear();
    }
  };
  while (WaitForSingleObject(pi.hProcess, 50) == WAIT_TIMEOUT) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) continue;
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    drain();
  }
  drain();
  GetExitCodeProcess(pi.hProcess, &g_exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(read_pipe);
  g_child_running = false;
  const std::wstring play = g_install + L"\\game\\Play.bat";
  g_success = g_exit_code == 0 && FileExists(play);
  Ingest("\r\n");
  Go(kResult);
}

void LaunchGame() {
  if (!g_success || afrosetup::CloseShouldKillChild(false)) return;
  const std::wstring play = g_install + L"\\game\\Play.bat";
  const std::wstring dir = g_install + L"\\game";
  if (!FileExists(play)) return;
  ShellExecuteW(g_wnd, L"open", play.c_str(), nullptr, dir.c_str(), SW_SHOWNORMAL);
}

void Layout() {
  ClearDyn();
  const Text& t = T();
  SetWindowTextW(g_wnd, t.title);
  SetWindowTextW(GetDlgItem(g_wnd, kBack), t.back);
  SetWindowTextW(GetDlgItem(g_wnd, kCancel), t.cancel);
  const wchar_t* next = t.next;
  if (g_page == kSummary) next = g_update ? t.update_btn : t.install;
  if (g_page == kResult) next = t.finish;
  SetWindowTextW(GetDlgItem(g_wnd, kNext), next);
  EnableWindow(GetDlgItem(g_wnd, kBack), !g_child_running && g_page != kLanguage && g_page != kProgress);
  EnableWindow(GetDlgItem(g_wnd, kNext), g_page != kProgress);
  ShowWindow(GetDlgItem(g_wnd, kNext), g_page == kProgress ? SW_HIDE : SW_SHOW);

  auto heading = [&](const wchar_t* s) {
    Mk(L"STATIC", s, SS_LEFT, 0, 24, 28, 600, 28, 0);
  };
  if (g_page == kLanguage) {
    heading(t.lang_title);
    const wchar_t* names[] = {t.name_en, t.name_es, t.name_fr, t.name_it};
    for (int i = 0; i < 4; ++i)
      Mk(L"BUTTON", names[i], WS_TABSTOP | BS_AUTORADIOBUTTON | (i == 0 ? WS_GROUP : 0), 0, 36, 88 + i * 36, 280,
         28, kRadio0 + i);
    CheckRadioButton(g_wnd, kRadio0, kRadio0 + 3, kRadio0 + g_lang);
  } else if (g_page == kMode) {
    heading(t.mode_title);
    Mk(L"BUTTON", t.mode_install, WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP, 0, 36, 100, 400, 32, kRadio0);
    Mk(L"BUTTON", t.mode_update, WS_TABSTOP | BS_AUTORADIOBUTTON, 0, 36, 144, 400, 32, kRadio0 + 1);
    CheckRadioButton(g_wnd, kRadio0, kRadio0 + 1, g_update ? kRadio0 + 1 : kRadio0);
  } else if (g_page == kFolder) {
    heading(g_update ? t.upd_folder : t.folder_title);
    Mk(L"EDIT", g_install.c_str(), WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, 24, 88, 460, 28, kEdit);
    Mk(L"BUTTON", t.browse, WS_TABSTOP | BS_OWNERDRAW, 0, 496, 86, 120, 28, kBrowse);
    if (g_update && !g_install.empty()) {
      std::wstring info;
      if (AfrorecompRunning()) info += std::wstring(t.upd_running) + L"\r\n";
      if (!ExistingInstall()) info += std::wstring(t.upd_bad) + L"\r\n";
      else {
        const std::wstring installed = InstalledVersion(g_install);
        info += (installed.empty() ? t.upd_ver_unknown : installed) + std::wstring(L"  \u2192  0.1.0\r\n");
        if (afrosetup::SameVersionRefresh(installed, L"0.1.0")) info += std::wstring(t.upd_same) + L"\r\n";
        const auto plan = ToolState(nullptr, nullptr);
        info += std::wstring(plan == afrosetup::ToolPlan::kReuseAll
                                 ? t.upd_tools_ok
                                 : plan == afrosetup::ToolPlan::kRecoverMissing ? t.upd_tools_part
                                                                                : t.upd_tools_missing) +
                L"\r\n";
        info += ExtractedGame() ? t.upd_xex_ok : t.upd_xex_need;
        if (MsvcRecorded()) info += std::wstring(L"\r\n") + t.upd_lic_kept;
      }
      Mk(L"STATIC", info.c_str(), SS_LEFT, 0, 24, 140, 590, 160, 0);
    }
  } else if (g_page == kGame) {
    heading(t.game_title);
    Mk(L"BUTTON", t.game_iso, WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP, 0, 24, 76, 560, 26, kRadio0);
    Mk(L"EDIT", g_iso.c_str(), WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, 44, 108, 440, 28, kEdit);
    Mk(L"BUTTON", t.browse, WS_TABSTOP | BS_OWNERDRAW, 0, 496, 106, 120, 28, kBrowse);
    Mk(L"BUTTON", t.game_dir, WS_TABSTOP | BS_AUTORADIOBUTTON, 0, 24, 156, 560, 26, kRadio0 + 1);
    Mk(L"EDIT", g_xexdir.c_str(), WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, 44, 188, 440, 28, kEdit2);
    Mk(L"BUTTON", t.browse, WS_TABSTOP | BS_OWNERDRAW, 0, 496, 186, 120, 28, kBrowse2);
    CheckRadioButton(g_wnd, kRadio0, kRadio0 + 1, g_use_iso ? kRadio0 : kRadio0 + 1);
    Mk(L"STATIC", t.game_europe, SS_LEFT, 0, 24, 236, 580, 28, 0);
    SyncGameFields();
  } else if (g_page == kLicense) {
    heading(t.sum_lic);
    Mk(L"BUTTON", L"", WS_TABSTOP | BS_AUTOCHECKBOX, 0, 24, 88, 22, 22, kCheck);
    CheckDlgButton(g_wnd, kCheck, g_license ? BST_CHECKED : BST_UNCHECKED);
    Mk(L"STATIC", t.lic_before, SS_LEFT, 0, 52, 90, 140, 24, 0);
    HWND link = Mk(L"STATIC", t.lic_word, SS_NOTIFY | SS_LEFT, 0, 192, 90, 110, 24, kLink);
    if (g_link_font) SendMessageW(link, WM_SETFONT, reinterpret_cast<WPARAM>(g_link_font), TRUE);
    Mk(L"STATIC", t.lic_after, SS_LEFT, 0, 300, 90, 280, 24, 0);
    EnableWindow(GetDlgItem(g_wnd, kNext), g_license);
  } else if (g_page == kOptions) {
    heading(t.opt_title);
    Mk(L"BUTTON", t.opt_short, WS_TABSTOP | BS_AUTOCHECKBOX, 0, 24, 80, 580, 28, kCheck);
    CheckDlgButton(g_wnd, kCheck, g_shortcut ? BST_CHECKED : BST_UNCHECKED);
    Mk(L"BUTTON", t.opt_keep, WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP, 0, 24, 124, 580, 26, kRadio0);
    Mk(L"BUTTON", t.opt_clean, WS_TABSTOP | BS_AUTORADIOBUTTON, 0, 24, 156, 580, 26, kRadio0 + 1);
    CheckRadioButton(g_wnd, kRadio0, kRadio0 + 1, g_keep ? kRadio0 : kRadio0 + 1);
    Mk(L"STATIC", SpaceSentence().c_str(), SS_LEFT, 0, 24, 210, 590, 70, 0);
  } else if (g_page == kSummary) {
    heading(t.sum_title);
    const wchar_t* lang = g_lang == kEs ? t.name_es : g_lang == kFr ? t.name_fr : g_lang == kIt ? t.name_it : t.name_en;
    std::wstring game = g_update && ExtractedGame() ? std::wstring(t.upd_xex_ok) : (g_use_iso ? g_iso : g_xexdir);
    std::wstring body = std::wstring(g_update ? t.mode_update : t.mode_install) + L"\r\n" + t.sum_lang + L": " + lang +
                        L"\r\n" + t.sum_folder + L": " + g_install + L"\r\n" + t.sum_game + L": " + game + L"\r\n";
    if (g_update) {
      const std::wstring installed = InstalledVersion(g_install);
      body += (installed.empty() ? t.upd_ver_unknown : installed) + std::wstring(L"  \u2192  0.1.0\r\n");
      if (afrosetup::SameVersionRefresh(installed, L"0.1.0")) body += std::wstring(t.upd_same) + L"\r\n";
    }
    body += std::wstring(t.sum_lic) + L": " + (MsvcRecorded() ? t.upd_lic_kept : (g_license ? t.sum_yes : t.sum_no)) +
            L"\r\n" + t.sum_short + L": " + (g_shortcut ? t.sum_yes : t.sum_no) + L"\r\n" + t.sum_keep + L": " +
            (g_keep ? t.opt_keep : t.opt_clean);
    Mk(L"EDIT", body.c_str(), ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE, 24, 76,
       590, 220, kLog);
    const bool media_ok = g_update && ExtractedGame() ? true : (g_use_iso ? !g_iso.empty() : FileExists(g_xexdir + L"\\default.xex"));
    const bool license_ok = MsvcRecorded() || g_license;
    const bool update_ok = !g_update || (ExistingInstall() && !AfrorecompRunning());
    EnableWindow(GetDlgItem(g_wnd, kNext), g_payload_ok && media_ok && !g_install.empty() && license_ok && update_ok);
  } else if (g_page == kProgress) {
    heading(t.run_title);
    Mk(L"STATIC", t.run_minutes, SS_LEFT, 0, 24, 64, 590, 24, 0);
    HWND bar = Mk(PROGRESS_CLASSW, L"", 0, 0, 24, 100, 590, 18, kBar);
    SendMessageW(bar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(bar, PBM_SETPOS, g_pct, 0);
    SendMessageW(bar, PBM_SETBARCOLOR, 0, kRed);
    const std::wstring status = g_status.empty() ? std::to_wstring(g_pct) + L" %" : g_status;
    Mk(L"STATIC", status.c_str(), SS_LEFT, 0, 24, 128, 590, 48, kStatus);
    Mk(L"BUTTON", g_show_details ? t.run_hide : t.run_details, WS_TABSTOP | BS_OWNERDRAW, 0, 24, 184, 180, 28,
       kDetails);
    if (g_show_details)
      Mk(L"EDIT", g_log.c_str(), ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE, 24, 222,
         590, 160, kLog);
  } else if (g_page == kResult) {
    heading(g_success ? t.done_title : t.fail_title);
    if (g_success)
      Mk(L"BUTTON", t.play, WS_TABSTOP | BS_OWNERDRAW, 0, 180, 150, 280, 64, kPlay);
    else
      Mk(L"STATIC", g_status.c_str(), SS_LEFT, 0, 24, 120, 590, 48, 0);
    Mk(L"BUTTON", g_show_details ? t.run_hide : t.run_details, WS_TABSTOP | BS_OWNERDRAW, 0, 24, 240, 180, 28,
       kDetails);
    if (g_show_details)
      Mk(L"EDIT", g_log.c_str(), ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE, 24, 276,
         590, 110, kLog);
  }
}

void DrawChromeButton(const DRAWITEMSTRUCT* d, bool primary) {
  COLORREF fill = primary ? kRed : RGB(42, 42, 46);
  if (d->itemState & ODS_DISABLED) fill = RGB(72, 72, 76);
  HBRUSH brush = CreateSolidBrush(fill);
  FillRect(d->hDC, &d->rcItem, brush);
  DeleteObject(brush);
  HPEN pen = CreatePen(PS_SOLID, 1, primary ? RGB(230, 90, 90) : RGB(190, 190, 190));
  HGDIOBJ old_pen = SelectObject(d->hDC, pen);
  HGDIOBJ old_brush = SelectObject(d->hDC, GetStockObject(NULL_BRUSH));
  Rectangle(d->hDC, d->rcItem.left, d->rcItem.top, d->rcItem.right, d->rcItem.bottom);
  SelectObject(d->hDC, old_pen);
  SelectObject(d->hDC, old_brush);
  DeleteObject(pen);
  wchar_t text[160];
  GetWindowTextW(d->hwndItem, text, 160);
  SetBkMode(d->hDC, TRANSPARENT);
  SetTextColor(d->hDC, kInk);
  RECT rc = d->rcItem;
  DrawTextW(d->hDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void SaveClientBmp(const std::wstring& path) {
  RECT rc{};
  GetClientRect(g_wnd, &rc);
  const int w = rc.right, h = rc.bottom;
  HDC window_dc = GetDC(g_wnd);
  HDC mem = CreateCompatibleDC(window_dc);
  HBITMAP bmp = CreateCompatibleBitmap(window_dc, w, h);
  HGDIOBJ old = SelectObject(mem, bmp);
  BitBlt(mem, 0, 0, w, h, window_dc, 0, 0, SRCCOPY);
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = w;
  info.bmiHeader.biHeight = -h;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 24;
  const int stride = ((w * 3 + 3) & ~3);
  std::vector<uint8_t> pixels(static_cast<size_t>(stride * h));
  GetDIBits(mem, bmp, 0, h, pixels.data(), &info, DIB_RGB_COLORS);
  FILE* f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"wb") == 0 && f) {
    const uint32_t header = 14 + 40;
    const uint32_t bytes = header + static_cast<uint32_t>(pixels.size());
    const unsigned char file_hdr[14] = {'B', 'M', static_cast<unsigned char>(bytes), static_cast<unsigned char>(bytes >> 8),
                                        static_cast<unsigned char>(bytes >> 16), static_cast<unsigned char>(bytes >> 24), 0, 0, 0, 0, 54, 0, 0, 0};
    info.bmiHeader.biHeight = h;
    info.bmiHeader.biSizeImage = static_cast<DWORD>(pixels.size());
    fwrite(file_hdr, 1, 14, f);
    fwrite(&info.bmiHeader, 1, 40, f);
    fwrite(pixels.data(), 1, pixels.size(), f);
    fclose(f);
  }
  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(g_wnd, window_dc);
}

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
    case WM_ERASEBKGND: {
      RECT rc{};
      GetClientRect(h, &rc);
      FillRect(reinterpret_cast<HDC>(w), &rc, g_charcoal);
      return 1;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps{};
      HDC dc = BeginPaint(h, &ps);
      RECT bar{0, 0, 4000, Dpi(5)};
      RECT rc{};
      GetClientRect(h, &rc);
      bar.right = rc.right;
      HBRUSH red = CreateSolidBrush(kRed);
      FillRect(dc, &bar, red);
      DeleteObject(red);
      HPEN pen = CreatePen(PS_SOLID, Dpi(2), kInk);
      HGDIOBJ old = SelectObject(dc, pen);
      MoveToEx(dc, Dpi(28), Dpi(22), nullptr);
      LineTo(dc, Dpi(78), Dpi(8));
      SelectObject(dc, old);
      DeleteObject(pen);
      EndPaint(h, &ps);
      return 0;
    }
    case WM_CTLCOLOREDIT: {
      HDC dc = reinterpret_cast<HDC>(w);
      SetBkColor(dc, kField);
      SetTextColor(dc, kFieldText);
      return reinterpret_cast<LRESULT>(g_white);
    }
    case WM_CTLCOLORSTATIC: {
      HDC dc = reinterpret_cast<HDC>(w);
      wchar_t cls[32];
      GetClassNameW(reinterpret_cast<HWND>(l), cls, 32);
      SetBkMode(dc, OPAQUE);
      if (wcscmp(cls, L"Button") == 0) {
        SetBkColor(dc, kField);
        SetTextColor(dc, kFieldText);
        return reinterpret_cast<LRESULT>(g_white);
      }
      if (reinterpret_cast<HWND>(l) == GetDlgItem(h, kLink)) {
        SetBkColor(dc, kCharcoal);
        SetTextColor(dc, RGB(90, 150, 255));
        return reinterpret_cast<LRESULT>(g_charcoal);
      }
      SetBkColor(dc, kCharcoal);
      SetTextColor(dc, kInk);
      return reinterpret_cast<LRESULT>(g_charcoal);
    }
    case WM_DRAWITEM:
      if (const auto* d = reinterpret_cast<DRAWITEMSTRUCT*>(l)) {
        const bool primary = d->CtlID == kNext || d->CtlID == kPlay;
        DrawChromeButton(d, primary);
      }
      return TRUE;
    case WM_SETCURSOR:
      if (reinterpret_cast<HWND>(w) == GetDlgItem(h, kLink)) {
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return TRUE;
      }
      break;
    case WM_COMMAND: {
      const int id = LOWORD(w);
      if (id == kLink) {
        ShellExecuteW(h, L"open", kLicenseUrl, nullptr, nullptr, SW_SHOWNORMAL);
        return 0;
      }
      if (id == kDetails) {
        g_show_details = !g_show_details;
        Layout();
        return 0;
      }
      if (id == kPlay) {
        LaunchGame();
        return 0;
      }
      if (id == kRadio0 || id == kRadio0 + 1) {
        if (g_page == kMode) g_update = id == kRadio0 + 1;
        if (g_page == kGame) {
          g_use_iso = id == kRadio0;
          SyncGameFields();
        }
        if (g_page == kOptions) g_keep = id == kRadio0;
        return 0;
      }
      if (id == kCancel || id == IDCANCEL) {
        if (g_child_running) {
          MessageBoxW(h, T().run_close, T().title, MB_ICONINFORMATION);
          return 0;
        }
        DestroyWindow(h);
        return 0;
      }
      if (id == kBack) {
        if (g_child_running) return 0;
        if (g_page == kResult) {
          if (afrosetup::BackFromFinishStartsInstall()) StartInstall();
          Go(kSummary);
          return 0;
        }
        if (g_page != kLanguage && g_page < kProgress) Go(Backward(g_page));
        return 0;
      }
      if (id == kBrowse || id == kBrowse2) {
        if (g_page == kFolder) {
          const std::wstring p = BrowseFolder(T().folder_title);
          if (!p.empty()) SetDlgItemTextW(h, kEdit, p.c_str());
        } else if (g_page == kGame && id == kBrowse) {
          const std::wstring p = BrowseIso();
          if (!p.empty()) SetDlgItemTextW(h, kEdit, p.c_str());
        } else if (g_page == kGame) {
          const std::wstring p = BrowseFolder(T().game_dir);
          if (!p.empty()) SetDlgItemTextW(h, kEdit2, p.c_str());
        }
        return 0;
      }
      if (id == kCheck && g_page == kLicense) {
        g_license = IsDlgButtonChecked(h, kCheck) == BST_CHECKED;
        EnableWindow(GetDlgItem(h, kNext), g_license);
        return 0;
      }
      if (id == kNext || id == IDOK) {
        if (g_child_running) return 0;
        if (g_page == kResult) {
          DestroyWindow(h);
          return 0;
        }
        ReadPage();
        if (g_page == kSummary) {
          StartInstall();
          return 0;
        }
        if (g_page == kFolder && g_install.empty()) return 0;
        if (g_page == kFolder && g_update && (!ExistingInstall() || AfrorecompRunning())) return 0;
        if (g_page == kGame && g_use_iso && g_iso.empty()) return 0;
        if (g_page == kGame && !g_use_iso && !FileExists(g_xexdir + L"\\default.xex")) return 0;
        if (g_page == kLicense && !g_license) return 0;
        if (g_page < kSummary) Go(Forward(g_page));
        return 0;
      }
      return 0;
    }
    case WM_CLOSE:
      if (g_child_running) {
        MessageBoxW(h, T().run_close, T().title, MB_ICONINFORMATION);
        return 0;
      }
      DestroyWindow(h);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    case WM_DPICHANGED: {
      g_dpi = HIWORD(w);
      Layout();
      return 0;
    }
  }
  return DefWindowProcW(h, m, w, l);
}

void SelfTest(const std::wstring& out_path) {
  std::vector<afrosetup::PayloadFile> files;
  std::string error;
  const bool ok = LoadOwnPayload(files, error);
  std::string ver;
  for (const auto& f : files)
    if (f.path == "app/VERSION") ver.assign(f.bytes.begin(), f.bytes.end());
  while (!ver.empty() && (ver.back() == '\n' || ver.back() == '\r')) ver.pop_back();
  FILE* f = nullptr;
  _wfopen_s(&f, out_path.c_str(), L"wb");
  if (!f) return;
  const std::string report = std::string(ok ? "payload=ok\n" : "payload=bad\n") + "version=" + ver +
                             "\nspawned=0\nkill=never\n";
  fwrite(report.data(), 1, report.size(), f);
  fclose(f);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
  g_inst = inst;
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  for (int i = 1; argv && i < argc; ++i) {
    const std::wstring a = argv[i];
    if (a.rfind(L"--self-test=", 0) == 0) {
      SelfTest(a.substr(12));
      LocalFree(argv);
      return 0;
    }
    if (a.rfind(L"--local-tree=", 0) == 0) g_cli_tree = a.substr(13);
    if (a.rfind(L"--dump-pages=", 0) == 0) g_dump_dir = a.substr(13);
  }
  if (argv) LocalFree(argv);

  std::vector<afrosetup::PayloadFile> files;
  std::string error;
  g_payload_ok = LoadOwnPayload(files, error);
  bool saw_version = false;
  std::wstring version;
  for (const auto& f : files) {
    if (f.path == "app/VERSION") {
      std::string ver(f.bytes.begin(), f.bytes.end());
      while (!ver.empty() && (ver.back() == '\n' || ver.back() == '\r' || ver.back() == ' ')) ver.pop_back();
      version.assign(ver.begin(), ver.end());
      saw_version = true;
    }
  }
  g_payload_ok = g_payload_ok && saw_version && version == afrosetup::kExpectedVersion;

  const LANGID ui = GetUserDefaultUILanguage();
  const WORD primary = PRIMARYLANGID(ui);
  if (primary == LANG_SPANISH) g_lang = kEs;
  else if (primary == LANG_FRENCH) g_lang = kFr;
  else if (primary == LANG_ITALIAN) g_lang = kIt;
  g_dpi = GetDpiForSystem();
  INITCOMMONCONTROLSEX icc{sizeof icc, ICC_PROGRESS_CLASS};
  InitCommonControlsEx(&icc);
  g_charcoal = CreateSolidBrush(kCharcoal);
  g_white = CreateSolidBrush(kField);
  g_font = CreateFontW(-Dpi(18), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
  g_link_font = CreateFontW(-Dpi(18), 0, 0, 0, FW_NORMAL, FALSE, TRUE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = g_charcoal;
  wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
  wc.lpszClassName = L"AfroSetup";
  RegisterClassW(&wc);
  g_wnd = CreateWindowExW(0, wc.lpszClassName, kText[g_lang].title, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                          CW_USEDEFAULT, CW_USEDEFAULT, Dpi(680), Dpi(520), nullptr, nullptr, inst, nullptr);
  SendMessageW(g_wnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(wc.hIcon));
  SendMessageW(g_wnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(wc.hIcon));
  g_dpi = GetDpiForWindow(g_wnd);
  auto bottom = [&](const wchar_t* text, int x, int id, bool primary) {
    HWND b = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, Dpi(x), Dpi(430),
                             Dpi(120), Dpi(32), g_wnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
    SetFont(b);
    (void)primary;
  };
  bottom(T().back, 16, kBack, false);
  bottom(T().next, 400, kNext, true);
  bottom(T().cancel, 532, kCancel, false);
  Layout();
  ShowWindow(g_wnd, show);
  if (!g_dump_dir.empty()) {
    CreateDirectoryW(g_dump_dir.c_str(), nullptr);
    const Page pages[] = {kLanguage, kMode, kFolder, kGame, kLicense, kOptions, kSummary};
    g_install = L"C:\\Juegos\\Afro";
    g_iso = L"D:\\Afro.iso";
    for (Page p : pages) {
      g_page = p;
      Layout();
      UpdateWindow(g_wnd);
      SaveClientBmp(g_dump_dir + L"\\page-" + std::to_wstring(static_cast<int>(p)) + L".bmp");
    }
    DestroyWindow(g_wnd);
  }
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(g_wnd, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  if (g_font) DeleteObject(g_font);
  if (g_link_font) DeleteObject(g_link_font);
  if (g_charcoal) DeleteObject(g_charcoal);
  if (g_white) DeleteObject(g_white);
  return static_cast<int>(msg.wParam);
}
