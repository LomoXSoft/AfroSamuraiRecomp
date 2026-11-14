#pragma once
// Pure command-line and payload helpers for the graphical setup.
// No Win32 calls, so the policy tests build on any host.

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace afrosetup {

inline constexpr wchar_t kExpectedVersion[] = L"0.3.4";

struct Request {
  std::wstring powershell_exe;
  std::wstring script_path;
  std::wstring install_dir;
  std::wstring iso;
  std::wstring gamedata_dir;
  std::wstring language;  // en, es, fr, it
  bool accept_license = false;
  bool license_recorded = false;
  bool shortcut = false;
  bool keep_build_files = true;
  bool update = false;
  std::wstring local_tree;
  // Nonce from this install folder's hash of this ISO. Empty means no override.
  std::wstring accept_unverified_iso;
};

// Windows command-line quoting (CommandLineToArgvW rules).
inline std::wstring QuoteWinArg(const std::wstring& arg) {
  if (arg.empty()) return L"\"\"";
  bool need = false;
  for (wchar_t c : arg) {
    if (c == L' ' || c == L'\t' || c == L'"') need = true;
  }
  if (!need) return arg;
  std::wstring out = L"\"";
  int backslashes = 0;
  for (wchar_t c : arg) {
    if (c == L'\\') {
      ++backslashes;
      continue;
    }
    if (c == L'"') {
      out.append(static_cast<size_t>(backslashes) * 2 + 1, L'\\');
      out.push_back(L'"');
      backslashes = 0;
      continue;
    }
    if (backslashes) {
      out.append(static_cast<size_t>(backslashes), L'\\');
      backslashes = 0;
    }
    out.push_back(c);
  }
  out.append(static_cast<size_t>(backslashes) * 2, L'\\');
  out.push_back(L'"');
  return out;
}

inline std::wstring JoinCommandLine(const std::vector<std::wstring>& argv) {
  std::wstring line;
  for (size_t i = 0; i < argv.size(); ++i) {
    if (i) line.push_back(L' ');
    line += QuoteWinArg(argv[i]);
  }
  return line;
}

inline bool LanguageOk(const std::wstring& language) {
  return language == L"en" || language == L"es" || language == L"fr" || language == L"it";
}

// A new install takes the folder the person picked and creates
// AfroSamuraiRecomp inside it. D:\ is a valid parent: the install root is
// D:\AfroSamuraiRecomp. A path that already ends in that name is left as is.
inline std::wstring InstallFolderName() { return L"AfroSamuraiRecomp"; }

inline std::wstring SlashPath(std::wstring path) {
  for (wchar_t& c : path)
    if (c == L'/') c = L'\\';
  size_t i = 0;
  while (i < path.size() && (path[i] == L' ' || path[i] == L'\t')) ++i;
  if (i) path.erase(0, i);
  while (!path.empty() && (path.back() == L' ' || path.back() == L'\t')) path.pop_back();
  return path;
}

inline bool IsVolumeOrShareRoot(const std::wstring& path) {
  const std::wstring p = SlashPath(path);
  if (p.size() == 2 && p[1] == L':') return true;
  if (p.size() == 3 && p[1] == L':' && p[2] == L'\\') return true;
  if (p.size() < 5 || p[0] != L'\\' || p[1] != L'\\') return false;
  std::wstring body = p;
  while (!body.empty() && body.back() == L'\\') body.pop_back();
  const size_t slash = body.find(L'\\', 2);
  return slash != std::wstring::npos && body.find(L'\\', slash + 1) == std::wstring::npos;
}

inline bool LeafIsInstallName(const std::wstring& path) {
  const std::wstring p = SlashPath(path);
  size_t end = p.size();
  while (end > 0 && p[end - 1] == L'\\') --end;
  if (end == 0) return false;
  size_t start = p.find_last_of(L'\\', end - 1);
  start = start == std::wstring::npos ? 0 : start + 1;
  const std::wstring leaf = p.substr(start, end - start);
  if (leaf.size() != InstallFolderName().size()) return false;
  for (size_t i = 0; i < leaf.size(); ++i) {
    wchar_t a = leaf[i], b = InstallFolderName()[i];
    if (a >= L'A' && a <= L'Z') a = static_cast<wchar_t>(a - L'A' + L'a');
    if (b >= L'A' && b <= L'Z') b = static_cast<wchar_t>(b - L'A' + L'a');
    if (a != b) return false;
  }
  return true;
}

// New install only. Update keeps the existing folder the person selected.
inline std::wstring EnsureInstallRoot(const std::wstring& chosen) {
  std::wstring p = SlashPath(chosen);
  if (p.empty()) return p;
  while (p.size() > 3 && p.back() == L'\\') p.pop_back();
  if (p.size() == 2 && p[1] == L':') p.push_back(L'\\');
  if (LeafIsInstallName(p) && !IsVolumeOrShareRoot(p)) {
    while (p.size() > 3 && p.back() == L'\\') p.pop_back();
    return p;
  }
  if (IsVolumeOrShareRoot(p)) {
    if (p.back() != L'\\') p.push_back(L'\\');
    return p + InstallFolderName();
  }
  return p + L"\\" + InstallFolderName();
}

// The header art is 1200x240. Its height on screen is the client width
// times that ratio, so the separator stays at the bottom of the picture.
inline std::wstring NormalizeSha256(std::wstring hex) {
  std::wstring out;
  for (wchar_t c : hex) {
    if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') continue;
    if (c >= L'a' && c <= L'f') c = static_cast<wchar_t>(c - L'a' + L'A');
    out.push_back(c);
  }
  return out;
}

inline bool Sha256Hex(const std::wstring& hex) {
  if (hex.size() != 64) return false;
  for (wchar_t c : hex) {
    const bool digit = c >= L'0' && c <= L'9';
    const bool upper = c >= L'A' && c <= L'F';
    if (!digit && !upper) return false;
  }
  return true;
}

// Case does not matter. A different digit does.
inline bool Sha256Equal(const std::wstring& got, const std::wstring& expected) {
  const std::wstring a = NormalizeSha256(got);
  const std::wstring b = NormalizeSha256(expected);
  return Sha256Hex(a) && a == b;
}

inline int HeaderHeightForClientWidth(int client_width_px) {
  constexpr int kArtW = 1200;
  constexpr int kArtH = 240;
  if (client_width_px < 1) return 0;
  return client_width_px * kArtH / kArtW;
}

// BUTTON styles share the low nibble. BS_OWNERDRAW is 0xB, so it cannot also
// be BS_AUTORADIOBUTTON (0x9) or BS_AUTOCHECKBOX (0x3). The system button
// then ignores BM_SETCHECK, and BM_GETCHECK stays unchecked.
inline constexpr unsigned kBtnCheck = 0x2;
inline constexpr unsigned kBtnAutoCheck = 0x3;
inline constexpr unsigned kBtnRadio = 0x4;
inline constexpr unsigned kBtn3State = 0x5;
inline constexpr unsigned kBtnAuto3 = 0x6;
inline constexpr unsigned kBtnAutoRadio = 0x9;
inline constexpr unsigned kBtnOwnerDraw = 0xB;

inline bool ButtonStyleKeepsCheck(unsigned style) {
  switch (style & 0xF) {
    case kBtnCheck:
    case kBtnAutoCheck:
    case kBtnRadio:
    case kBtn3State:
    case kBtnAuto3:
    case kBtnAutoRadio:
      return true;
    default:
      return false;
  }
}

// Mark kept by our button procedure, because the owner-draw button drops it.
struct ChoiceMark {
  bool set = false;
  int value = 0;
};

inline void RememberChoice(ChoiceMark& mark, int value) {
  if (value < 0 || value > 2) value = 0;
  mark.set = true;
  mark.value = value;
}

inline int RecallChoice(const ChoiceMark& mark) { return mark.set ? mark.value : 0; }

// system_check is what the stock button would return. Owner-draw returns the
// mark we stored; a radio or checkbox style returns the stock value.
inline int ReportedCheck(unsigned style, int system_check, const ChoiceMark& mark) {
  if (ButtonStyleKeepsCheck(style)) return system_check;
  return RecallChoice(mark);
}

// Radios: one selection. If every control reports unchecked, the owner-draw
// button dropped the mark, so keep the value the click or the page default set.
inline int SelectedRadioOrKeep(const bool* checked, int count, int keep) {
  if (!checked || count < 1) return keep;
  for (int i = 0; i < count; ++i)
    if (checked[i]) return i;
  return keep;
}

inline void ApplyRadioGroup(int* marks, int count, int index) {
  if (!marks || count < 1) return;
  for (int i = 0; i < count; ++i) marks[i] = i == index ? 1 : 0;
}

inline int ToggleCheck(int current) { return current == 1 ? 0 : 1; }

enum class WizardPage { kLanguage, kMode, kFolder, kGame, kLicense, kOptions, kSummary, kOther };

struct PageChoices {
  int radios = 0;
  int checks = 0;
};

inline PageChoices DarkChoiceControls(WizardPage page) {
  switch (page) {
    case WizardPage::kLanguage:
      return {4, 0};
    case WizardPage::kMode:
      return {2, 0};
    case WizardPage::kGame:
      return {2, 0};
    case WizardPage::kLicense:
      return {0, 1};
    case WizardPage::kOptions:
      return {2, 1};
    default:
      return {};
  }
}

enum class NextStop { kProceed, kNeedFolder, kNeedIso, kNeedXex, kNeedLicense };

// Why Next stays on this page. The ISO decision is which radio is on and
// whether that box is empty. The file name is not inspected.
inline NextStop NextStopFor(WizardPage page, bool folder_empty, bool use_iso, bool iso_empty, bool xex_present,
                            bool license) {
  if (page == WizardPage::kFolder && folder_empty) return NextStop::kNeedFolder;
  if (page == WizardPage::kGame) {
    if (use_iso) return iso_empty ? NextStop::kNeedIso : NextStop::kProceed;
    return xex_present ? NextStop::kProceed : NextStop::kNeedXex;
  }
  if (page == WizardPage::kLicense && !license) return NextStop::kNeedLicense;
  return NextStop::kProceed;
}

inline std::wstring NormalizePath(std::wstring path) {
  for (wchar_t& c : path) {
    if (c == L'/') c = L'\\';
    if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
  }
  while (path.size() > 3 && (path.back() == L'\\' || path.back() == L' ')) path.pop_back();
  return path;
}

// True when child is the same path as parent or a directory inside it.
inline bool SameOrInside(const std::wstring& parent, const std::wstring& child) {
  const std::wstring a = NormalizePath(parent);
  const std::wstring b = NormalizePath(child);
  if (a.empty() || b.empty()) return false;
  if (a == b) return true;
  if (b.size() <= a.size()) return false;
  if (b.compare(0, a.size(), a) != 0) return false;
  return b[a.size()] == L'\\';
}

// Dialog buttons match Win32: IDYES 6 continues, IDNO 7 / IDCANCEL 2 / 0 stop.
enum class IsoPromptAnswer { kStop, kContinue };

inline bool IsSha1Hex(const std::wstring& s) {
  if (s.size() != 40) return false;
  for (wchar_t c : s) {
    if (!((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F'))) return false;
  }
  return true;
}

inline bool IsIsoNonce(const std::wstring& s) {
  if (s.size() != 32) return false;
  for (wchar_t c : s) {
    if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F'))) return false;
  }
  return true;
}

inline std::wstring UpperHex(std::wstring s) {
  for (wchar_t& c : s) {
    if (c >= L'a' && c <= L'z') c = static_cast<wchar_t>(c - L'a' + L'A');
  }
  return s;
}

// Continue is the only answer that proceeds. Stop, Cancel and the window X do not.
inline bool IsoContinueChosen(IsoPromptAnswer answer) { return answer == IsoPromptAnswer::kContinue; }

inline IsoPromptAnswer IsoAnswerFromDialog(int pressed) {
  if (pressed == 6) return IsoPromptAnswer::kContinue;
  return IsoPromptAnswer::kStop;
}

// An override is not a verified hash. The iso-hash stamp stays absent.
inline bool IsoOverrideMarksVerified() { return false; }

enum class IsoHashResult { kMatch, kMismatch, kUnreadable };

// A different SHA-1 is recorded and the install continues. A failed read does not.
inline bool IsoHashMarksVerified(IsoHashResult result) { return result == IsoHashResult::kMatch; }
inline bool IsoHashStopsInstall(IsoHashResult result) { return result == IsoHashResult::kUnreadable; }
inline bool IsoHashPrompts(IsoHashResult) { return false; }

struct IsoUnverifiedNote {
  std::wstring path;
  std::wstring actual;
  std::wstring expected;
  uint64_t length = 0;
  bool ok = false;
};

// Same path and the same length mean the bytes already hashed are still the file.
// Length never accepts an ISO by itself: the note must already hold both hashes.
inline bool UnverifiedNoteSkipsReread(const IsoUnverifiedNote& note, const std::wstring& path, uint64_t length) {
  if (!note.ok || !IsSha1Hex(note.actual) || !IsSha1Hex(note.expected)) return false;
  if (note.actual == note.expected) return false;
  if (note.length == 0 || length == 0 || note.length != length) return false;
  return NormalizePath(note.path) == NormalizePath(path);
}

struct IsoRequest {
  std::wstring path;
  std::wstring actual;
  std::wstring expected;
  std::wstring nonce;
  uint64_t length = 0;
  bool ok = false;
};

// One record written after the single SHA-1 of this ISO. Keys are path, length,
// actual, expected, nonce. A missing or partial record cannot authorize a skip.
inline std::wstring Utf8ToWide(const std::string& s) {
  std::wstring out;
  for (size_t n = 0; n < s.size();) {
    const unsigned char c = static_cast<unsigned char>(s[n]);
    if (c < 0x80) {
      out.push_back(static_cast<wchar_t>(c));
      n += 1;
    } else if ((c & 0xE0) == 0xC0 && n + 1 < s.size()) {
      out.push_back(static_cast<wchar_t>(((c & 0x1F) << 6) |
                                         (static_cast<unsigned char>(s[n + 1]) & 0x3F)));
      n += 2;
    } else if ((c & 0xF0) == 0xE0 && n + 2 < s.size()) {
      out.push_back(static_cast<wchar_t>(((c & 0x0F) << 12) |
                                         ((static_cast<unsigned char>(s[n + 1]) & 0x3F) << 6) |
                                         (static_cast<unsigned char>(s[n + 2]) & 0x3F)));
      n += 3;
    } else {
      out.push_back(L'?');
      n += 1;
    }
  }
  return out;
}

inline IsoRequest ParseIsoRequest(std::string text) {
  IsoRequest out;
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
    text.erase(0, 3);
  }
  std::wstring path, actual, expected, nonce;
  uint64_t length = 0;
  bool saw_length = false;
  size_t i = 0;
  while (i < text.size()) {
    size_t end = text.find('\n', i);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(i, end - i);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t eq = line.find('=');
    if (eq != std::string::npos && eq > 0) {
      const std::string key = line.substr(0, eq);
      const std::string value = line.substr(eq + 1);
      std::wstring wide(value.begin(), value.end());
      if (key == "path") path = Utf8ToWide(value);
      else if (key == "actual") actual = UpperHex(wide);
      else if (key == "expected") expected = UpperHex(wide);
      else if (key == "nonce") nonce = wide;
      else if (key == "length") {
        saw_length = true;
        length = 0;
        for (char c : value) {
          if (c < '0' || c > '9') {
            saw_length = false;
            break;
          }
          length = length * 10 + static_cast<uint64_t>(c - '0');
        }
      }
    }
    i = end + 1;
  }
  out.path = path;
  out.actual = actual;
  out.expected = expected;
  out.nonce = nonce;
  out.length = length;
  out.ok = !path.empty() && saw_length && length > 0 && IsSha1Hex(actual) && IsSha1Hex(expected) &&
           actual != expected && IsIsoNonce(nonce);
  return out;
}

// Skip the second full read only when the nonce, path and length are the ones
// just hashed in this install folder. Another path, another length, or no
// record still requires the SHA-1. Unattended without this match rejects.
inline bool ConsentSkipsReread(const IsoRequest& recorded, const std::wstring& nonce,
                               const std::wstring& current_path, uint64_t current_length) {
  if (!recorded.ok || !IsIsoNonce(nonce)) return false;
  if (UpperHex(recorded.nonce) != UpperHex(nonce)) return false;
  if (current_length == 0 || recorded.length != current_length) return false;
  if (NormalizePath(recorded.path) != NormalizePath(current_path)) return false;
  return true;
}

inline std::vector<std::wstring> Validate(const Request& in) {
  std::vector<std::wstring> errors;
  if (in.powershell_exe.empty() || in.script_path.empty())
    errors.push_back(L"missing powershell or install.ps1 path");
  if (in.install_dir.empty()) errors.push_back(L"install folder is empty");
  if (in.update) {
    if (!in.iso.empty() && !in.gamedata_dir.empty())
      errors.push_back(L"choose either an ISO or a folder that contains default.xex");
  } else if (in.iso.empty() == in.gamedata_dir.empty()) {
    errors.push_back(L"choose either an ISO or a folder that contains default.xex");
  }
  if (!in.accept_license && !in.license_recorded)
    errors.push_back(L"the Build Tools license checkbox is required");
  if (!LanguageOk(in.language)) errors.push_back(L"language must be en, es, fr, or it");
  if (in.local_tree.empty()) errors.push_back(L"local 0.3.4 source is required");
  for (const std::wstring* p : {&in.install_dir, &in.iso, &in.gamedata_dir, &in.local_tree, &in.script_path,
                                 &in.accept_unverified_iso}) {
    if (p->find(L'"') != std::wstring::npos) errors.push_back(L"paths cannot contain a double quote");
  }
  if (!in.accept_unverified_iso.empty() && !IsIsoNonce(in.accept_unverified_iso))
    errors.push_back(L"unverified ISO consent is not valid for this attempt");
  if (!in.install_dir.empty() && !in.local_tree.empty() &&
      (SameOrInside(in.install_dir, in.local_tree) || SameOrInside(in.local_tree, in.install_dir))) {
    errors.push_back(L"install folder and source folder must not contain each other");
  }
  return errors;
}

// Argv for CreateProcessW. Never passes -Ref or -RepoUrl. Public main is not a choice.
inline std::vector<std::wstring> BuildArgv(const Request& in) {
  std::vector<std::wstring> argv;
  argv.push_back(in.powershell_exe);
  argv.push_back(L"-NoProfile");
  argv.push_back(L"-NonInteractive");
  argv.push_back(L"-ExecutionPolicy");
  argv.push_back(L"Bypass");
  argv.push_back(L"-File");
  argv.push_back(in.script_path);
  argv.push_back(L"-InstallDir");
  argv.push_back(in.install_dir);
  argv.push_back(L"-Language");
  argv.push_back(in.language);
  argv.push_back(L"-Shortcut");
  argv.push_back(in.shortcut ? L"yes" : L"no");
  if (in.accept_license) argv.push_back(L"-AcceptMsvcLicense");
  argv.push_back(in.keep_build_files ? L"-KeepBuildFiles" : L"-CleanupAfterInstall");
  argv.push_back(L"-LocalTree");
  argv.push_back(in.local_tree);
  argv.push_back(L"-NonInteractive");
  if (in.update) argv.push_back(L"-Update");
  if (!in.iso.empty()) {
    argv.push_back(L"-Iso");
    argv.push_back(in.iso);
  } else if (!in.gamedata_dir.empty()) {
    argv.push_back(L"-GameDataDir");
    argv.push_back(in.gamedata_dir);
  }
  return argv;
}

inline bool ArgvUsesPublicMain(const std::vector<std::wstring>& argv) {
  for (size_t i = 0; i < argv.size(); ++i) {
    if (argv[i] == L"-Ref" || argv[i] == L"-RepoUrl") return true;
    if (argv[i] == L"main" && i > 0 && argv[i - 1] == L"-Ref") return true;
    if (argv[i].find(L"github.com/LomoXSoft/AfroSamuraiRecomp") != std::wstring::npos) return true;
  }
  return false;
}

// The summary page must be confirmed, and a child must not already be running.
inline bool MaySpawn(bool summary_confirmed, bool child_running) {
  return summary_confirmed && !child_running;
}

enum class CancelAnswer { kContinue, kStop };

// Stop is the only answer that ends this run. Continue leaves it running.
inline bool ConfirmedStop(CancelAnswer answer) { return answer == CancelAnswer::kStop; }

// A cancelled or failed run must not stamp the step that was in progress.
inline bool MayMarkStepDone(bool run_cancelled) { return !run_cancelled; }

// A .part file counts only after the hash matches and it is renamed into place.
inline bool PartialCountsAsVerified(bool hash_ok, bool moved_into_place) {
  return hash_ok && moved_into_place;
}

// Pids of this run: the process we created and its descendants.
// Edges are (pid, parent). A game or another installer is not in the list.
inline std::vector<uint32_t> DescendantPids(
    uint32_t root, const std::vector<std::pair<uint32_t, uint32_t>>& pid_parent) {
  std::vector<uint32_t> out;
  std::vector<uint32_t> queue{root};
  size_t i = 0;
  while (i < queue.size()) {
    const uint32_t cur = queue[i++];
    bool seen = false;
    for (uint32_t have : out)
      if (have == cur) seen = true;
    if (seen || cur == 0) continue;
    out.push_back(cur);
    for (const auto& edge : pid_parent)
      if (edge.second == cur && edge.first != cur) queue.push_back(edge.first);
  }
  return out;
}

inline bool PidIsInRun(uint32_t pid, const std::vector<uint32_t>& tree) {
  for (uint32_t have : tree)
    if (have == pid) return true;
  return false;
}

// Success never starts the game by itself. Play is the only control that does.
enum class ResultButton { kPlay, kFinish, kClose };
enum class PlayOutcome { kNotAttempted, kAccepted, kRejected };

struct ResultAction {
  bool launch = false;
  bool close_window = false;
};

inline bool AutoLaunchOnSuccess() { return false; }

// Play launches only after a successful setup whose game\Play.bat is present.
// The window closes only once that launch command has been accepted.
// Finish and the window X close the same way and never launch.
inline ResultAction ResultActionFor(ResultButton button, bool setup_ok, bool play_bat_exists, PlayOutcome outcome) {
  ResultAction action;
  if (button != ResultButton::kPlay) {
    action.launch = false;
    action.close_window = true;
    return action;
  }
  if (!setup_ok || !play_bat_exists) {
    action.launch = false;
    action.close_window = false;
    return action;
  }
  action.launch = true;
  action.close_window = outcome == PlayOutcome::kAccepted;
  return action;
}

// Arguments the stock Play.bat passes to afrorecomp.exe. %~dp0 is the game folder.
inline std::wstring StockGameArguments() {
  return L"--game_data_root=\"%~dp0..\\gamedata\" --gpu_plugin=xenos --log_file=\"%~dp0..\\afro.log\"";
}

// The stock bat uses start, so its console exits. Any other bat is run as written.
inline bool PlayBatClosesItself(const std::wstring& bat) {
  const std::wstring launch = L"start \"\" \"%~dp0afrorecomp.exe\" " + StockGameArguments();
  return bat.find(launch) != std::wstring::npos && bat.find(L"if not defined AFRO_PLAY_NOPAUSE pause") != std::wstring::npos &&
         bat.find(L"exit /b 1") != std::wstring::npos;
}

// Jugar runs Play.bat. It does not rebuild the game's arguments.
// A bat that closes itself has no leftover console. Any other bat keeps a console.
struct PlayCommand {
  std::wstring file;
  std::wstring parameters;
  std::wstring directory;
  bool inherit_handles = false;
  bool new_console = false;
  bool break_from_job = false;
  bool new_process_group = false;
};

inline PlayCommand PlayCommandFor(const std::wstring& install_dir, const std::wstring& windows_dir, bool bat_closes_itself) {
  PlayCommand command;
  command.file = windows_dir + L"\\System32\\cmd.exe";
  command.directory = install_dir + L"\\game";
  command.parameters = L"/d /c " + QuoteWinArg(command.directory + L"\\Play.bat");
  command.inherit_handles = false;
  command.new_console = !bat_closes_itself;
  command.break_from_job = true;
  command.new_process_group = true;
  return command;
}

inline bool PlaySurvivesInstallerExit(const PlayCommand& command) {
  const std::wstring suffix = L"\\game";
  const bool game_dir = command.directory.size() > suffix.size() &&
                        command.directory.compare(command.directory.size() - suffix.size(), suffix.size(), suffix) == 0;
  const bool cmd = command.file.size() >= 7 && command.file.compare(command.file.size() - 7, 7, L"cmd.exe") == 0;
  return game_dir && cmd && !command.inherit_handles && command.break_from_job && command.new_process_group &&
         command.parameters.find(L"Play.bat") != std::wstring::npos;
}

inline std::vector<std::wstring> PathWarnings(const std::wstring& path) {
  std::vector<std::wstring> w;
  bool non_ascii = false;
  bool space = false;
  for (wchar_t c : path) {
    if (c == L' ') space = true;
    if (c > 0x7E) non_ascii = true;
  }
  if (space) w.push_back(L"path contains spaces; it will be quoted");
  if (non_ascii) w.push_back(L"path contains non-ASCII characters; the toolchain may fail");
  if (path.size() > 80) w.push_back(L"path is long; prefer a short folder such as C:\\Games\\Afro");
  return w;
}

struct PayloadFile {
  std::string path;
  std::vector<uint8_t> bytes;
};

inline constexpr char kPayloadMagic[8] = {'A', 'F', 'S', 'T', 'P', '0', '0', '1'};

inline bool PayloadPathOk(const std::string& path) {
  if (path.empty() || path[0] == '/' || path[0] == '\\') return false;
  std::string seg;
  for (size_t i = 0; i <= path.size(); ++i) {
    const char c = i == path.size() ? '/' : path[i];
    if (c == '\\') return false;
    if (c == '/') {
      if (seg.empty() || seg == "." || seg == "..") return false;
      for (unsigned char ch : seg) {
        const bool ok = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                        (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-' || ch == '+';
        if (!ok) return false;
      }
      seg.clear();
      continue;
    }
    seg.push_back(c);
  }
  return true;
}

inline void AppendU16(std::vector<uint8_t>& o, uint16_t v) {
  o.push_back(static_cast<uint8_t>(v));
  o.push_back(static_cast<uint8_t>(v >> 8));
}
inline void AppendU32(std::vector<uint8_t>& o, uint32_t v) {
  for (int i = 0; i < 4; ++i) o.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
inline void AppendU64(std::vector<uint8_t>& o, uint64_t v) {
  for (int i = 0; i < 8; ++i) o.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
inline bool ReadU16(const uint8_t* p, const uint8_t* end, uint16_t& v) {
  if (end - p < 2) return false;
  v = static_cast<uint16_t>(p[0] | (p[1] << 8));
  return true;
}
inline bool ReadU32(const uint8_t* p, const uint8_t* end, uint32_t& v) {
  if (end - p < 4) return false;
  v = static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
  return true;
}
inline bool ReadU64(const uint8_t* p, const uint8_t* end, uint64_t& v) {
  if (end - p < 8) return false;
  v = 0;
  for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(p[i]) << (8 * i);
  return true;
}

inline std::vector<uint8_t> EncodePayload(const std::vector<PayloadFile>& files) {
  std::vector<uint8_t> body;
  AppendU32(body, static_cast<uint32_t>(files.size()));
  for (const PayloadFile& f : files) {
    if (!PayloadPathOk(f.path) || f.path.size() > 65535) return {};
    AppendU16(body, static_cast<uint16_t>(f.path.size()));
    body.insert(body.end(), f.path.begin(), f.path.end());
    AppendU64(body, f.bytes.size());
    body.insert(body.end(), f.bytes.begin(), f.bytes.end());
  }
  return body;
}

// `image` is the whole file, including the 24-byte footer.
inline bool ParsePayload(const uint8_t* image, size_t n, std::vector<PayloadFile>& out, std::string& error) {
  out.clear();
  if (n < 24) {
    error = "file is too small for a payload footer";
    return false;
  }
  const uint8_t* footer = image + (n - 24);
  uint64_t offset = 0, size = 0;
  if (!ReadU64(footer, footer + 8, offset) || !ReadU64(footer + 8, footer + 16, size)) {
    error = "footer truncated";
    return false;
  }
  if (std::string(reinterpret_cast<const char*>(footer + 16), 8) != std::string(kPayloadMagic, 8)) {
    error = "payload magic missing";
    return false;
  }
  if (offset > n || size > n || offset + size > n - 24) {
    error = "payload range is outside the file";
    return false;
  }
  const uint8_t* p = image + offset;
  const uint8_t* end = p + size;
  uint32_t count = 0;
  if (!ReadU32(p, end, count)) {
    error = "payload count truncated";
    return false;
  }
  p += 4;
  if (count > 100000) {
    error = "payload count is unreasonable";
    return false;
  }
  for (uint32_t i = 0; i < count; ++i) {
    uint16_t len = 0;
    if (!ReadU16(p, end, len)) {
      error = "path length truncated";
      return false;
    }
    p += 2;
    if (static_cast<size_t>(end - p) < len) {
      error = "path truncated";
      return false;
    }
    PayloadFile file;
    file.path.assign(reinterpret_cast<const char*>(p), len);
    p += len;
    if (!PayloadPathOk(file.path)) {
      error = "unsafe payload path: " + file.path;
      return false;
    }
    uint64_t nbytes = 0;
    if (!ReadU64(p, end, nbytes)) {
      error = "file size truncated";
      return false;
    }
    p += 8;
    if (static_cast<uint64_t>(end - p) < nbytes) {
      error = "file bytes truncated";
      return false;
    }
    file.bytes.assign(p, p + nbytes);
    p += nbytes;
    out.push_back(std::move(file));
  }
  if (p != end) {
    error = "payload has trailing bytes";
    return false;
  }
  error.clear();
  return true;
}

// install.ps1 deletes these on cleanup. game, gamedata, logs, and saves are not in the list.
inline const wchar_t* const kCleanupDirs[] = {
    L"tools", L"downloads", L"rexglue-sdk", L"sdk", L"afrorecomp", L".install"};
inline const wchar_t* const kKeptInstallDirs[] = {L"game", L"gamedata"};

inline bool IsCleanupDirName(const std::wstring& name) {
  for (const wchar_t* n : kCleanupDirs)
    if (name == n) return true;
  return false;
}

inline bool IsKeptInstallDirName(const std::wstring& name) {
  for (const wchar_t* n : kKeptInstallDirs)
    if (name == n) return true;
  return false;
}

// CMake uses the environment variable RC as the resource compiler path.
// A cache line whose value is the exit code 0 is that mistake, not rc.exe.
inline bool CmakeRcCacheLineIsExitCode(const std::wstring& line) {
  const std::wstring key = L"CMAKE_RC_COMPILER:";
  if (line.size() < key.size() || line.compare(0, key.size(), key) != 0) return false;
  const size_t eq = line.find(L'=', key.size());
  if (eq == std::wstring::npos) return false;
  std::wstring value = line.substr(eq + 1);
  while (!value.empty() && (value.back() == L'\r' || value.back() == L' ' || value.back() == L'\t'))
    value.pop_back();
  return value == L"0";
}

// Binary units, one decimal for GB, from the measured byte count.
inline std::wstring FormatByteAmount(uint64_t bytes) {
  const uint64_t gb = 1024ull * 1024ull * 1024ull;
  const uint64_t mb = 1024ull * 1024ull;
  if (bytes >= gb) {
    const uint64_t tenths = (bytes * 10 + gb / 2) / gb;
    return std::to_wstring(tenths / 10) + L"." + std::to_wstring(tenths % 10) + L" GB";
  }
  return std::to_wstring((bytes + mb / 2) / mb) + L" MB";
}

inline std::wstring NormalizeConsole(const std::string& bytes) {
  std::wstring out;
  out.reserve(bytes.size());
  for (size_t i = 0; i < bytes.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(bytes[i]);
    if (c == '\r') {
      out += L"\r\n";
      if (i + 1 < bytes.size() && bytes[i + 1] == '\n') ++i;
    } else if (c == '\n') {
      out += L"\r\n";
    } else {
      out.push_back(static_cast<wchar_t>(c));
    }
  }
  return out;
}

struct ProgressEvent {
  bool has_percent = false;
  int percent = 0;
  std::wstring status;
  bool download = false;
  bool compile = false;
  bool has_ratio = false;
  int ratio_done = 0;
  int ratio_total = 0;
};

// Reads install.ps1's own "[ 12%] Paso 3/15: title (detail)" lines.
// The status text is kept. A download detail is not reported as compiling.
inline bool ParseProgressLine(const std::wstring& line, ProgressEvent& ev) {
  const size_t open = line.find(L'[');
  const size_t close = line.find(L"%]", open == std::wstring::npos ? 0 : open);
  if (open == std::wstring::npos || close == std::wstring::npos || close < open) return false;
  int value = 0;
  bool digit = false;
  for (size_t i = open + 1; i < close; ++i) {
    if (line[i] == L' ') continue;
    if (line[i] < L'0' || line[i] > L'9') return false;
    digit = true;
    value = value * 10 + (line[i] - L'0');
  }
  if (!digit || value > 100) return false;
  ev = {};
  ev.has_percent = true;
  ev.percent = value;
  std::wstring rest = line.substr(close + 2);
  while (!rest.empty() && (rest.front() == L' ' || rest.front() == L'\t')) rest.erase(rest.begin());
  while (!rest.empty() && (rest.back() == L'\r' || rest.back() == L'\n' || rest.back() == L' ')) rest.pop_back();
  ev.status = rest;
  const std::wstring lower = [&] {
    std::wstring s = rest;
    for (wchar_t& c : s)
      if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    return s;
  }();
  ev.download = lower.find(L"download") != std::wstring::npos || lower.find(L"descarg") != std::wstring::npos ||
                lower.find(L"telecharg") != std::wstring::npos;
  ev.compile = !ev.download && (lower.find(L"compil") != std::wstring::npos);
  int done = 0, total = 0, acc = 0;
  bool in_num = false, saw_slash = false;
  for (size_t i = 0; i < rest.size(); ++i) {
    const wchar_t c = rest[i];
    if (c >= L'0' && c <= L'9') {
      in_num = true;
      acc = acc * 10 + (c - L'0');
      continue;
    }
    if (in_num && c == L'/' ) {
      done = acc;
      acc = 0;
      saw_slash = true;
      in_num = false;
      continue;
    }
    if (saw_slash && in_num) {
      total = acc;
      if (total > 0) {
        ev.has_ratio = true;
        ev.ratio_done = done;
        ev.ratio_total = total;
      }
    }
    in_num = false;
    saw_slash = false;
    acc = 0;
  }
  return true;
}

enum class ProgressActivity { kWorking, kDownload, kExtract, kGenerate, kCompile, kGraphics, kCheck, kFinish };

// Turns the installer's verbose percent line into one short activity.
// A step title that mentions both code generation and a time range stays
// "generating code" until the parenthetical detail is actually compiling.
inline ProgressActivity ClassifyProgress(const std::wstring& raw) {
  std::wstring s = raw;
  for (wchar_t& c : s)
    if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
  const size_t paren = s.rfind(L'(');
  const std::wstring detail = paren == std::wstring::npos ? L"" : s.substr(paren);
  auto has = [](const std::wstring& h, const wchar_t* n) { return h.find(n) != std::wstring::npos; };
  if (has(detail, L"compil") && !has(detail, L"genera") && !has(detail, L"codegen"))
    return ProgressActivity::kCompile;
  if (has(s, L"generacion") || has(s, L"generation") || has(s, L"generazione") || has(s, L"codegen"))
    return ProgressActivity::kGenerate;
  if (has(s, L"descarg") || has(s, L"download") || has(s, L"telecharg")) return ProgressActivity::kDownload;
  if (has(s, L"extract") || has(s, L"extr") || has(s, L"estraz")) return ProgressActivity::kExtract;
  if (has(s, L"gpu") || has(s, L"runtime") || has(s, L"plugin")) return ProgressActivity::kGraphics;
  if (has(s, L"verif") || has(s, L"comprob") || has(s, L"controllo")) return ProgressActivity::kCheck;
  if (has(s, L"termin") || has(s, L"finished") || has(s, L"100%")) return ProgressActivity::kFinish;
  if (has(s, L"compil")) return ProgressActivity::kCompile;
  return ProgressActivity::kWorking;
}

inline const wchar_t* ProgressLabel(ProgressActivity activity, int lang) {
  static const wchar_t* kEn[] = {L"Preparing", L"Downloading", L"Extracting", L"Generating code",
                                 L"Compiling the game", L"Compiling graphics", L"Checking", L"Finishing"};
  static const wchar_t* kEs[] = {L"Preparando", L"Descargando", L"Extrayendo", L"Generando c\u00f3digo",
                                 L"Compilando el juego", L"Compilando gr\u00e1ficos", L"Comprobando", L"Terminando"};
  static const wchar_t* kFr[] = {L"Pr\u00e9paration", L"T\u00e9l\u00e9chargement", L"Extraction", L"G\u00e9n\u00e9ration du code",
                                 L"Compilation du jeu", L"Compilation graphique", L"V\u00e9rification", L"Fin"};
  static const wchar_t* kIt[] = {L"Preparazione", L"Download", L"Estrazione", L"Generazione del codice",
                                 L"Compilazione del gioco", L"Compilazione grafica", L"Controllo", L"Fine"};
  const wchar_t* const* table = kEn;
  if (lang == 1) table = kEs;
  else if (lang == 2) table = kFr;
  else if (lang == 3) table = kIt;
  return table[static_cast<int>(activity)];
}

// Code generation has no file count, so the bar must not sit on a frozen percent.
inline bool ProgressIndeterminate(const std::wstring& raw) {
  return ClassifyProgress(raw) == ProgressActivity::kGenerate;
}

// During a real compile, the bar is files done / files total. Otherwise it is
// the installer's own overall percent. Nothing is interpolated.
inline int ProgressBarValue(int percent, const std::wstring& raw) {
  if (ClassifyProgress(raw) != ProgressActivity::kCompile) return percent;
  ProgressEvent ev;
  if (ParseProgressLine(std::wstring(L"[ ") + std::to_wstring(percent) + L"%] " + raw, ev) && ev.has_ratio &&
      ev.ratio_total > 0)
    return ev.ratio_done * 100 / ev.ratio_total;
  return percent;
}

inline std::wstring PrimaryProgress(int percent, const std::wstring& raw, int lang) {
  const ProgressActivity activity = ClassifyProgress(raw);
  ProgressEvent ev;
  if (activity == ProgressActivity::kCompile &&
      ParseProgressLine(std::wstring(L"[ ") + std::to_wstring(percent) + L"%] " + raw, ev) && ev.has_ratio &&
      ev.ratio_total > 0) {
    return std::wstring(ProgressLabel(activity, lang)) + L" \u00b7 " + std::to_wstring(ev.ratio_done) + L"/" +
           std::to_wstring(ev.ratio_total);
  }
  if (activity == ProgressActivity::kGenerate) return ProgressLabel(activity, lang);
  return std::to_wstring(percent) + L" % \u00b7 " + ProgressLabel(activity, lang);
}

// Back on the finished page only changes the page. It does not start setup again.
inline bool BackFromFinishStartsInstall() { return false; }

struct StepEvent {
  int index = 0;
  int count = 0;
  std::wstring key;
  int percent = -1;
  long long done = 0;
  long long total = 0;
  std::wstring activity;
};

struct StepView {
  int index = 0;
  int count = 0;
  bool indeterminate = true;
  int bar = 0;
  std::wstring label;
};

inline bool ParseLeadingInt(const std::wstring& text, size_t& i, long long& out) {
  if (i >= text.size()) return false;
  int sign = 1;
  if (text[i] == L'-') {
    sign = -1;
    ++i;
  }
  if (i >= text.size() || text[i] < L'0' || text[i] > L'9') return false;
  long long value = 0;
  while (i < text.size() && text[i] >= L'0' && text[i] <= L'9') {
    value = value * 10 + (text[i] - L'0');
    ++i;
  }
  out = sign * value;
  return true;
}

inline bool ParseLeadingInt(const std::wstring& text, size_t& i, int& out) {
  if (i >= text.size()) return false;
  int sign = 1;
  if (text[i] == L'-') {
    sign = -1;
    ++i;
  }
  if (i >= text.size() || text[i] < L'0' || text[i] > L'9') return false;
  int value = 0;
  while (i < text.size() && text[i] >= L'0' && text[i] <= L'9') {
    value = value * 10 + (text[i] - L'0');
    ++i;
  }
  out = sign * value;
  return true;
}

// AFROSTEP index/count key percent done total [activity]
// percent < 0 and total == 0 means this step has no measured fraction.
inline bool ParseAfroStep(const std::wstring& line, StepEvent& ev) {
  size_t i = 0;
  while (i < line.size() && (line[i] == L' ' || line[i] == L'\t')) ++i;
  const std::wstring tag = L"AFROSTEP ";
  if (line.compare(i, tag.size(), tag) != 0) return false;
  i += tag.size();
  int index = 0, count = 0, percent = 0;
  long long done = 0, total = 0;
  if (!ParseLeadingInt(line, i, index) || i >= line.size() || line[i] != L'/') return false;
  ++i;
  if (!ParseLeadingInt(line, i, count) || index < 1 || count < index) return false;
  if (i >= line.size() || line[i] != L' ') return false;
  ++i;
  const size_t key_end = line.find(L' ', i);
  if (key_end == std::wstring::npos) return false;
  const std::wstring key = line.substr(i, key_end - i);
  i = key_end + 1;
  if (!ParseLeadingInt(line, i, percent)) return false;
  if (i >= line.size() || line[i] != L' ') return false;
  ++i;
  if (!ParseLeadingInt(line, i, done)) return false;
  if (i >= line.size() || line[i] != L' ') return false;
  ++i;
  if (!ParseLeadingInt(line, i, total)) return false;
  std::wstring activity;
  if (i < line.size() && line[i] == L' ') {
    ++i;
    activity = line.substr(i);
    while (!activity.empty() && (activity.back() == L'\r' || activity.back() == L' ')) activity.pop_back();
  }
  ev = {};
  ev.index = index;
  ev.count = count;
  ev.key = key;
  ev.percent = percent;
  ev.done = done;
  ev.total = total;
  ev.activity = activity;
  return true;
}

inline std::wstring DefaultActivity(const std::wstring& key) {
  if (key == L"repo") return L"prepare";
  if (key == L"codegen") return L"generate";
  if (key == L"compile") return L"compile";
  if (key == L"gpu") return L"graphics";
  if (key == L"install") return L"copy";
  if (key == L"gamedata") return L"extract";
  return L"download";
}

inline std::wstring MeasuredAmount(long long done, long long total, const std::wstring& activity) {
  // Completed/total. Byte totals are whole MB. Counts stay n/m.
  // decompress is bytes read from the compressed archive, whose length is known.
  // Nothing here is a time estimate or a guessed total.
  const bool bytes = (activity == L"download" || activity == L"copybytes" || activity == L"decompress") &&
                     total >= 1024 * 1024;
  if (!bytes) return std::to_wstring(done) + L"/" + std::to_wstring(total);
  auto mb = [](long long value) {
    return std::to_wstring((value + (1024 * 1024) / 2) / (1024 * 1024));
  };
  return mb(done) + L"/" + mb(total) + L" MB";
}

inline const wchar_t* StepName(const std::wstring& key, int lang) {
  struct Row {
    const wchar_t* id;
    const wchar_t* en;
    const wchar_t* es;
    const wchar_t* fr;
    const wchar_t* it;
  };
  static const Row kRows[] = {
      {L"git", L"MinGit", L"MinGit", L"MinGit", L"MinGit"},
      {L"cmake", L"CMake", L"CMake", L"CMake", L"CMake"},
      {L"ninja", L"Ninja", L"Ninja", L"Ninja", L"Ninja"},
      {L"python", L"Python", L"Python", L"Python", L"Python"},
      {L"llvm", L"LLVM/Clang", L"LLVM/Clang", L"LLVM/Clang", L"LLVM/Clang"},
      {L"msvc", L"MSVC", L"MSVC", L"MSVC", L"MSVC"},
      {L"repo", L"Source", L"C\u00f3digo", L"Sources", L"Sorgenti"},
      {L"sdk", L"SDK", L"SDK", L"SDK", L"SDK"},
      {L"rexglue-src", L"ReXGlue", L"ReXGlue", L"ReXGlue", L"ReXGlue"},
      {L"gamedata", L"Game", L"Juego", L"Jeu", L"Gioco"},
      {L"codegen", L"Build", L"Compilar", L"Compilation", L"Compilazione"},
      {L"compile", L"Build", L"Compilar", L"Compilation", L"Compilazione"},
      {L"gpu", L"Graphics", L"Gr\u00e1ficos", L"Graphismes", L"Grafica"},
      {L"install", L"Install", L"Instalar", L"Installation", L"Installazione"},
  };
  const Row* row = nullptr;
  for (const Row& candidate : kRows)
    if (key == candidate.id) row = &candidate;
  if (!row) return key.c_str();
  if (lang == 1) return row->es;
  if (lang == 2) return row->fr;
  if (lang == 3) return row->it;
  return row->en;
}

inline const wchar_t* ActivityText(const std::wstring& activity, int lang) {
  struct Row {
    const wchar_t* id;
    const wchar_t* en;
    const wchar_t* es;
    const wchar_t* fr;
    const wchar_t* it;
  };
  static const Row kRows[] = {
      {L"prepare", L"Preparing the code", L"Preparando el c\u00f3digo", L"Pr\u00e9paration du code",
       L"Preparazione del codice"},
      {L"download", L"Downloading", L"Descargando", L"T\u00e9l\u00e9chargement", L"Download"},
      {L"extract", L"Extracting", L"Extrayendo", L"Extraction", L"Estrazione"},
      {L"unpack", L"Preparing the decompressor", L"Preparando el descompresor",
       L"Pr\u00e9paration du d\u00e9compresseur", L"Preparazione del decompressore"},
      {L"decompress", L"Decompressing", L"Descomprimiendo", L"D\u00e9compression", L"Decompressione"},
      {L"generate", L"Generating code", L"Generando c\u00f3digo", L"G\u00e9n\u00e9ration du code",
       L"Generazione del codice"},
      {L"compile", L"Compiling", L"Compilando", L"Compilation", L"Compilazione"},
      {L"graphics", L"Compiling", L"Compilando", L"Compilation", L"Compilazione"},
      {L"check", L"Checking", L"Comprobando", L"V\u00e9rification", L"Controllo"},
      {L"copy", L"Copying", L"Copiando", L"Copie", L"Copia"},
  };
  const Row* row = &kRows[0];
  for (const Row& candidate : kRows)
    if (activity == candidate.id) row = &candidate;
  if (lang == 1) return row->es;
  if (lang == 2) return row->fr;
  if (lang == 3) return row->it;
  return row->en;
}

inline std::wstring StepTitle(int index, int count, int lang) {
  const std::wstring n = std::to_wstring(index);
  const std::wstring total = std::to_wstring(count);
  if (lang == 1) return L"Paso " + n + L" de " + total;
  if (lang == 2) return L"\u00c9tape " + n + L" sur " + total;
  if (lang == 3) return L"Passo " + n + L" di " + total;
  return L"Step " + n + L" of " + total;
}

// One bar: the fraction of the current step. A file count is that fraction.
// A step with no measurement stays indeterminate and shows no percent.
inline StepView ViewStep(const StepEvent& ev, int lang) {
  StepView view;
  view.index = ev.index;
  view.count = ev.count;
  std::wstring activity = ev.activity.empty() ? DefaultActivity(ev.key) : ev.activity;
  if (activity == L"fetch") activity = L"download";
  if (activity == L"copybytes") activity = L"copy";
  const std::wstring amount_activity = ev.activity.empty() ? activity : ev.activity;
  // Paso X de Y · step name · action · completed/total. No amount without a real total.
  view.label = StepTitle(ev.index, ev.count, lang) + L" \u00b7 " + StepName(ev.key, lang) + L" \u00b7 " +
               ActivityText(activity, lang);
  if (ev.total > 0) {
    view.indeterminate = false;
    int bar = static_cast<int>((static_cast<long long>(ev.done) * 100) / ev.total);
    if (bar < 0) bar = 0;
    if (bar > 100) bar = 100;
    view.bar = bar;
    view.label += L" \u00b7 " + MeasuredAmount(ev.done, ev.total, amount_activity);
  } else {
    view.indeterminate = true;
    view.bar = 0;
  }
  return view;
}

// Legacy overall-percent lines do not move the step bar.
inline bool ApplyInstallerLine(const std::wstring& line, StepView& view, int lang) {
  StepEvent ev;
  if (!ParseAfroStep(line, ev)) return false;
  view = ViewStep(ev, lang);
  return true;
}

// Tools that already exist are not steps. Codegen and compile are separate.
// Build and the final copy always run: an update clears those stamps.
struct ToolReuse {
  bool git = false;
  bool cmake = false;
  bool ninja = false;
  bool python = false;
  bool llvm = false;
  bool msvc = false;
  bool sdk = false;
  bool rexglue = false;
  bool gamedata = false;
};

inline std::vector<std::string> LogicalSetupSteps(const ToolReuse& reuse) {
  std::vector<std::string> steps;
  auto add = [&](bool reused, const char* name) {
    if (!reused) steps.emplace_back(name);
  };
  add(reuse.git, "git");
  add(reuse.cmake, "cmake");
  add(reuse.ninja, "ninja");
  add(reuse.python, "python");
  add(reuse.llvm, "llvm");
  add(reuse.msvc, "msvc");
  steps.emplace_back("repo");
  add(reuse.sdk, "sdk");
  add(reuse.rexglue, "rexglue-src");
  add(reuse.gamedata, "gamedata");
  steps.emplace_back("codegen");
  steps.emplace_back("compile");
  steps.emplace_back("gpu");
  steps.emplace_back("install");
  return steps;
}

// Update may start only from the explicit button, with the game closed.
inline bool MayStartUpdate(bool update_clicked, bool setup_running, bool game_running, bool layout_ok) {
  return update_clicked && !setup_running && !game_running && layout_ok;
}

inline std::wstring VersionFromBuildRev(const std::wstring& rev) {
  const std::wstring prefix = L"local-";
  if (rev.compare(0, prefix.size(), prefix) != 0) return L"";
  const std::wstring rest = rev.substr(prefix.size());
  const size_t dash = rest.find(L'-');
  const std::wstring ver = dash == std::wstring::npos ? rest : rest.substr(0, dash);
  if (ver.find(L'.') == std::wstring::npos) return L"";
  return ver;
}

inline bool SameVersionRefresh(const std::wstring& installed, const std::wstring& bundled) {
  return !installed.empty() && installed == bundled;
}

inline bool NeedsGameMedia(bool update, bool extracted_xex) { return !update || !extracted_xex; }

// A prior acceptance counts only when the compiler and its stamp are both still there.
inline bool LicenseAlreadyRecorded(bool update, bool msvc_compiler, bool msvc_stamp) {
  return update && msvc_compiler && msvc_stamp;
}

enum class ToolPlan { kDownloadAll, kReuseAll, kRecoverMissing };

inline ToolPlan PlanTools(int present, int total) {
  if (total <= 0 || present <= 0) return ToolPlan::kDownloadAll;
  if (present >= total) return ToolPlan::kReuseAll;
  return ToolPlan::kRecoverMissing;
}

// Stamp name -> file that must exist to reuse a download. Build stamps are separate
// and are always cleared on update so a same-version refresh compiles.
struct ToolStamp {
  const char* stamp;
  const char* relative_file;
};

inline const ToolStamp kReuseStamps[] = {
    {"git", "tools/git/cmd/git.exe"},
    {"cmake", "tools/cmake/bin/cmake.exe"},
    {"ninja", "tools/ninja/ninja.exe"},
    {"python", "tools/python/python.exe"},
    {"llvm", "tools/llvm/bin/clang++.exe"},
    {"msvc", "tools/msvc/setup_x64.bat"},
    {"sdk", "sdk/win-amd64/bin/rexglue.exe"},
    {"rexglue-src", "rexglue-sdk/.git"},
    {"gamedata", "gamedata/default.xex"},
};

inline const char* const kRebuildStamps[] = {"build-app", "build-gpu", "build"};

inline bool ShouldClearReuseStamp(bool stamp_exists, bool file_exists) {
  return stamp_exists && !file_exists;
}

inline bool ShouldMarkReuseStamp(bool stamp_exists, bool file_exists) { return file_exists && !stamp_exists; }

// safe.directory values for one installer-owned repository. The exact path
// covers that repo. A trailing /* covers only its submodules. Never "*".
inline std::wstring GitOwnedSlashPath(std::wstring path) {
  for (wchar_t& c : path)
    if (c == L'\\') c = L'/';
  while (path.size() > 3 && (path.back() == L'/' || path.back() == L' ')) path.pop_back();
  return path;
}

inline bool GitSafeDirectoryIsBroad(const std::wstring& value) {
  if (value.empty() || value == L"*" || value == L"/*") return true;
  const std::wstring path = GitOwnedSlashPath(value);
  if (path == L"*" || path == L"/*") return true;
  if (path.size() == 2 && path[1] == L':') return true;
  if (path.size() == 3 && path[1] == L':' && path[2] == L'/') return true;
  if (path.size() >= 2 && path[0] == L'/' && path[1] == L'/') {
    int slashes = 0;
    for (wchar_t c : path)
      if (c == L'/') ++slashes;
    if (slashes <= 3) return true;
  }
  return false;
}

inline bool GitSafeDirectoriesFor(const std::wstring& repo, std::vector<std::wstring>& values) {
  values.clear();
  const std::wstring path = GitOwnedSlashPath(repo);
  if (GitSafeDirectoryIsBroad(path) || path.find(L'*') != std::wstring::npos) return false;
  values.push_back(path);
  values.push_back(path + L"/*");
  return !GitSafeDirectoryIsBroad(values[0]) && !GitSafeDirectoryIsBroad(values[1]);
}

// A non-zero git status, including dubious ownership (128), must fail the step.
inline bool GitStatusPropagates(int exit_code) { return exit_code != 0; }

enum class CloneAction { kCloneNew, kResumeCheckout, kReplaceIncomplete, kRefuse };

// .git present: keep it and checkout. An incomplete folder is removed only when
// it is exactly this installer's rexglue-sdk or afrorecomp directory.
inline CloneAction DecideCloneResume(bool folder_exists, bool git_exists, bool exact_owned_dir, bool reparse) {
  if (git_exists) return CloneAction::kResumeCheckout;
  if (!folder_exists) return CloneAction::kCloneNew;
  if (reparse || !exact_owned_dir) return CloneAction::kRefuse;
  return CloneAction::kReplaceIncomplete;
}

}  // namespace afrosetup
