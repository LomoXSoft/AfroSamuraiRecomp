#pragma once
// Pure command-line and payload helpers for the graphical setup.
// No Win32 calls, so the policy tests build on any host.

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace afrosetup {

inline constexpr wchar_t kExpectedVersion[] = L"0.1.0";

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
  if (in.local_tree.empty()) errors.push_back(L"local 0.1.0 source is required");
  for (const std::wstring* p : {&in.install_dir, &in.iso, &in.gamedata_dir, &in.local_tree, &in.script_path}) {
    if (p->find(L'"') != std::wstring::npos) errors.push_back(L"paths cannot contain a double quote");
  }
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

// Closing the window never kills the build. The caller keeps the window open.
inline bool CloseShouldKillChild(bool /*child_running*/) { return false; }

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

struct SpaceEstimate {
  bool known = false;
  uint64_t bytes = 0;
};

// Sum of cleanup directories that already exist. A directory that contains the
// ISO is omitted, matching install.ps1 (that whole folder is kept). Unknown
// when the install folder or every cleanup directory is absent.
inline SpaceEstimate RecoverableBuildBytes(const std::filesystem::path& root,
                                           const std::filesystem::path& iso_inside) {
  SpaceEstimate est;
  std::error_code ec;
  if (root.empty() || !std::filesystem::exists(root, ec)) return est;
  const std::wstring iso = iso_inside.empty() ? L"" : NormalizePath(iso_inside.wstring());
  for (const wchar_t* name : kCleanupDirs) {
    const std::filesystem::path dir = root / name;
    if (!std::filesystem::exists(dir, ec)) continue;
    if (!iso.empty() && SameOrInside(NormalizePath(dir.wstring()), iso)) continue;
    est.known = true;
    for (std::filesystem::recursive_directory_iterator it(
             dir, std::filesystem::directory_options::skip_permission_denied, ec),
         end;
         it != end; it.increment(ec)) {
      if (ec) {
        ec.clear();
        continue;
      }
      if (it->is_regular_file(ec)) est.bytes += static_cast<uint64_t>(it->file_size(ec));
    }
  }
  return est;
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

inline std::wstring PrimaryProgress(int percent, const std::wstring& raw, int lang) {
  return std::to_wstring(percent) + L" % \u00b7 " + ProgressLabel(ClassifyProgress(raw), lang);
}

// Back on the finished page only changes the page. It does not start setup again.
inline bool BackFromFinishStartsInstall() { return false; }

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

}  // namespace afrosetup
