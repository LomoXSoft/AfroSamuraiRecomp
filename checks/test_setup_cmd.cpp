#include "../tools/afrosetup_cmd.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void Expect(bool cond, const char* msg) {
  if (cond) return;
  std::cerr << "FAIL: " << msg << "\n";
  ++g_fails;
}

afrosetup::Request Sample() {
  afrosetup::Request r;
  r.powershell_exe = L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
  r.script_path = L"C:\\Users\\Mar\u00eda Jes\u00fas\\AppData\\Local\\Temp\\afrosetup\\tools\\install.ps1";
  r.install_dir = L"C:\\Games\\Afro Samurai";
  r.iso = L"D:\\Disc Images\\Afro Samurai.iso";
  r.language = L"es";
  r.accept_license = true;
  r.shortcut = true;
  r.keep_build_files = true;
  r.local_tree = L"C:\\src\\afro-0.1.0";
  return r;
}

}  // namespace

int main(int argc, char** argv) {
  using afrosetup::ArgvUsesPublicMain;
  using afrosetup::BuildArgv;
  using afrosetup::CloseShouldKillChild;
  using afrosetup::JoinCommandLine;
  using afrosetup::MaySpawn;
  using afrosetup::QuoteWinArg;
  using afrosetup::SameOrInside;
  using afrosetup::Validate;

  Expect(QuoteWinArg(L"C:\\Games\\Afro") == L"C:\\Games\\Afro", "simple path stays bare");
  Expect(QuoteWinArg(L"C:\\Games\\Afro Samurai") == L"\"C:\\Games\\Afro Samurai\"", "spaces are quoted");
  Expect(QuoteWinArg(L"D:\\My Disc\\") == L"\"D:\\My Disc\\\\\"", "trailing backslash is doubled inside quotes");
  Expect(QuoteWinArg(L"say \"hi\"") == L"\"say \\\"hi\\\"\"", "embedded quotes are escaped");
  Expect(QuoteWinArg(L"") == L"\"\"", "empty arg is quoted");

  const afrosetup::Request ok = Sample();
  Expect(Validate(ok).empty(), "sample request is valid");
  const std::vector<std::wstring> args = BuildArgv(ok);
  Expect(!ArgvUsesPublicMain(args), "local 0.1.0 command does not use public main");
  bool saw_tree = false, saw_license = false, saw_script_flag = false, saw_ps_nonint = false;
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == L"-LocalTree") {
      saw_tree = true;
      Expect(i + 1 < args.size() && args[i + 1] == ok.local_tree, "LocalTree value");
    }
    if (args[i] == L"-AcceptMsvcLicense") saw_license = true;
    if (args[i] == L"-NonInteractive" && i > 5) saw_script_flag = true;
    if (args[i] == L"-NonInteractive" && i < 5) saw_ps_nonint = true;
    if (args[i] == L"-Ref" || args[i] == L"-RepoUrl") Expect(false, "ref/url flag present");
  }
  Expect(saw_tree && saw_license && saw_script_flag && saw_ps_nonint, "required flags present");
  Expect(args[0] == ok.powershell_exe, "powershell is not taken from PATH");
  Expect(args[5] == L"-File", "-File is used so paths are arguments");
  const std::wstring line = JoinCommandLine(args);
  Expect(line.find(L"\"C:\\Games\\Afro Samurai\"") != std::wstring::npos, "install dir is quoted");
  Expect(line.find(L"Mar\u00eda Jes\u00fas") != std::wstring::npos, "unicode path is preserved");

  afrosetup::Request no_lic = ok;
  no_lic.accept_license = false;
  const std::vector<std::wstring> nolic = BuildArgv(no_lic);
  for (const auto& a : nolic)
    if (a == L"-AcceptMsvcLicense") Expect(false, "unchecked license must not be passed");
  Expect(!Validate(no_lic).empty(), "unchecked license is rejected");

  afrosetup::Request both = ok;
  both.gamedata_dir = L"C:\\xex";
  Expect(!Validate(both).empty(), "ISO and gamedata together are rejected");
  afrosetup::Request neither = ok;
  neither.iso.clear();
  Expect(!Validate(neither).empty(), "missing game files are rejected");

  afrosetup::Request nested = ok;
  nested.install_dir = L"C:\\src\\afro-0.1.0\\build";
  Expect(!Validate(nested).empty(), "install dir inside the source is rejected");

  Expect(!MaySpawn(false, false), "summary not confirmed does not spawn");
  Expect(!MaySpawn(true, true), "a running child does not spawn again");
  Expect(MaySpawn(true, false), "confirmed summary may spawn");
  Expect(!CloseShouldKillChild(false) && !CloseShouldKillChild(true), "close never kills the build");

  Expect(SameOrInside(L"C:/Games", L"C:\\games\\Afro"), "case and slash insensitive containment");
  Expect(!SameOrInside(L"C:\\Games\\Afro", L"C:\\Games\\AfroExtra"), "prefix is not containment");

  Expect(!afrosetup::BackFromFinishStartsInstall(), "Back on the finished page does not install");
  Expect(afrosetup::IsCleanupDirName(L"tools") && afrosetup::IsCleanupDirName(L".install"), "cleanup names");
  Expect(!afrosetup::IsCleanupDirName(L"game") && !afrosetup::IsCleanupDirName(L"gamedata"),
         "game and gamedata are not cleanup");
  Expect(afrosetup::IsKeptInstallDirName(L"game") && afrosetup::IsKeptInstallDirName(L"gamedata"), "kept names");
  Expect(afrosetup::FormatByteAmount(1024ull * 1024ull * 1024ull) == L"1.0 GB", "1 GiB formats from bytes");
  Expect(afrosetup::FormatByteAmount(1536ull * 1024ull * 1024ull) == L"1.5 GB", "1.5 GiB formats from bytes");
  Expect(afrosetup::NormalizeConsole("a\nb\r\nc\rd") == L"a\r\nb\r\nc\r\nd", "CR and LF become edit-control breaks");

  afrosetup::ProgressEvent ev;
  Expect(afrosetup::ParseProgressLine(L"[ 12%] Paso 3/15: CMake (descargando cmake.zip: 4/20 MB)", ev),
         "progress line parses");
  Expect(ev.percent == 12 && ev.download && !ev.compile, "a download line is not compiling");
  Expect(ev.status.find(L"descargando") != std::wstring::npos, "download status text is kept");
  Expect(afrosetup::ParseProgressLine(L"[ 80%] Step 14/15: Build (compiling 3/40)", ev) && ev.compile && !ev.download,
         "compile line stays compile");
  Expect(afrosetup::ParseProgressLine(L"[100%] Todos los pasos terminados.", ev) && ev.percent == 100, "done line");
  const std::wstring noisy =
      L"[ 38%] Paso 12/14: Compilar afrorecomp.exe (generacion de codigo + compilacion, 10-40 min)";
  Expect(afrosetup::ParseProgressLine(noisy, ev), "noisy build line parses");
  const std::wstring short_es = afrosetup::PrimaryProgress(ev.percent, ev.status, 1);
  Expect(short_es == L"38 % \u00b7 Generando c\u00f3digo", "primary status is a short generate label");
  Expect(short_es.find(L"Paso") == std::wstring::npos && short_es.find(L"10-40") == std::wstring::npos,
         "primary status hides the step counter and the time range");
  const std::wstring compiling =
      L"[ 55%] Paso 12/14: Compilar afrorecomp.exe (generacion de codigo + compilacion, 10-40 min) (compilando 10/400)";
  Expect(afrosetup::ParseProgressLine(compiling, ev), "compile detail parses");
  Expect(afrosetup::PrimaryProgress(ev.percent, ev.status, 1) == L"55 % \u00b7 Compilando el juego",
         "compile detail becomes compiling the game");
  Expect(afrosetup::ClassifyProgress(ev.status) == afrosetup::ProgressActivity::kCompile, "compile is not generate");
  Expect(afrosetup::PrimaryProgress(12, L"Paso 3/15: CMake (portable) (descargando cmake.zip: 4/20 MB)", 1) ==
             L"12 % \u00b7 Descargando",
         "a download is not labeled as compiling");

  Expect(afrosetup::VersionFromBuildRev(L"local-0.1.0") == L"0.1.0", "stamp version");
  Expect(afrosetup::VersionFromBuildRev(L"local-0.1.0-3bde90d") == L"0.1.0", "stamp version ignores git suffix");
  Expect(afrosetup::VersionFromBuildRev(L"3bde90d").empty(), "a bare git id is not a product version");
  Expect(afrosetup::SameVersionRefresh(L"0.1.0", L"0.1.0"), "same-version refresh is 0.1.0 to 0.1.0");
  Expect(!afrosetup::SameVersionRefresh(L"", L"0.1.0"), "a missing installed version is not pretended to be 0.1.0");
  Expect(!afrosetup::NeedsGameMedia(true, true), "update with extracted game does not ask for an ISO");
  Expect(afrosetup::NeedsGameMedia(true, false), "update without extracted game still needs an ISO");
  Expect(afrosetup::NeedsGameMedia(false, true), "a new install still asks for the game files");
  Expect(afrosetup::LicenseAlreadyRecorded(true, true, true), "update reuses a compiler that is still installed");
  Expect(!afrosetup::LicenseAlreadyRecorded(true, false, true), "a stamp without the compiler is not acceptance");
  Expect(!afrosetup::LicenseAlreadyRecorded(false, true, true), "a new install does not skip the checkbox");
  Expect(afrosetup::PlanTools(0, 9) == afrosetup::ToolPlan::kDownloadAll, "no tools means download");
  Expect(afrosetup::PlanTools(9, 9) == afrosetup::ToolPlan::kReuseAll, "complete tools are reused");
  Expect(afrosetup::PlanTools(4, 9) == afrosetup::ToolPlan::kRecoverMissing, "only missing tools are fetched");
  Expect(afrosetup::ShouldClearReuseStamp(true, false) && !afrosetup::ShouldClearReuseStamp(true, true),
         "a stamp with a missing file is cleared");
  Expect(afrosetup::ShouldMarkReuseStamp(false, true) && !afrosetup::ShouldMarkReuseStamp(true, true),
         "an existing file is reused without downloading again");
  Expect(!afrosetup::MayStartUpdate(false, false, false, true), "Update does not start before the button");
  Expect(!afrosetup::MayStartUpdate(true, false, true, true), "a running game blocks update and is not killed");
  Expect(afrosetup::MayStartUpdate(true, false, false, true), "an explicit Update with the game closed may start");

  afrosetup::Request refresh = ok;
  refresh.update = true;
  refresh.iso.clear();
  refresh.accept_license = false;
  refresh.license_recorded = true;
  Expect(Validate(refresh).empty(), "update with existing extract needs no ISO");
  const auto refresh_args = BuildArgv(refresh);
  Expect(!ArgvUsesPublicMain(refresh_args), "update stays off public main");
  bool saw_update = false, saw_iso = false, saw_accept = false;
  for (const auto& a : refresh_args) {
    if (a == L"-Update") saw_update = true;
    if (a == L"-Iso" || a == L"-GameDataDir") saw_iso = true;
    if (a == L"-AcceptMsvcLicense") saw_accept = true;
  }
  Expect(saw_update && !saw_iso && !saw_accept, "refresh passes -Update and neither ISO nor a new license");

  {
    const auto root = std::filesystem::temp_directory_path() / "afrosetup-space-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "tools");
    std::filesystem::create_directories(root / "game");
    {
      std::ofstream(root / "tools" / "blob", std::ios::binary).write("0123456789", 10);
      std::ofstream(root / "game" / "big", std::ios::binary).write("xxxxxxxxxx", 10);
      std::ofstream(root / "game" / "more", std::ios::binary).write("yyyyy", 5);
    }
    const auto missing = afrosetup::RecoverableBuildBytes(root / "nope", {});
    Expect(!missing.known, "missing install folder has no fake size");
    const auto est = afrosetup::RecoverableBuildBytes(root, {});
    Expect(est.known && est.bytes == 10, "only the tools blob is recoverable, not game");
    const auto iso_blocks = afrosetup::RecoverableBuildBytes(root, root / "tools" / "blob");
    Expect(!iso_blocks.known, "a cleanup directory that holds the ISO is not counted");
    std::filesystem::remove_all(root);
  }

  std::vector<afrosetup::PayloadFile> files(2);
  files[0].path = "app/VERSION";
  const std::string ver = "0.1.0\n";
  files[0].bytes.assign(ver.begin(), ver.end());
  files[1].path = "tools/install.ps1";
  const std::string ps1 = "LocalTree\n";
  files[1].bytes.assign(ps1.begin(), ps1.end());
  const std::vector<uint8_t> body = afrosetup::EncodePayload(files);
  Expect(!body.empty(), "payload encodes");
  std::vector<uint8_t> image = {'P', 'E'};
  const uint64_t offset = image.size();
  image.insert(image.end(), body.begin(), body.end());
  const uint64_t size = body.size();
  for (int i = 0; i < 8; ++i) image.push_back(static_cast<uint8_t>(offset >> (8 * i)));
  for (int i = 0; i < 8; ++i) image.push_back(static_cast<uint8_t>(size >> (8 * i)));
  image.insert(image.end(), afrosetup::kPayloadMagic, afrosetup::kPayloadMagic + 8);
  std::vector<afrosetup::PayloadFile> parsed;
  std::string error;
  Expect(afrosetup::ParsePayload(image.data(), image.size(), parsed, error), error.c_str());
  Expect(parsed.size() == 2 && parsed[0].path == "app/VERSION", "payload roundtrip");

  files[0].path = "../app/VERSION";
  Expect(afrosetup::EncodePayload(files).empty(), "parent segments are rejected");

  if (argc > 1) {
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> exe((std::istreambuf_iterator<char>(in)), {});
    std::vector<afrosetup::PayloadFile> packed;
    std::string perr;
    Expect(afrosetup::ParsePayload(exe.data(), exe.size(), packed, perr), perr.c_str());
    bool version_ok = false, ps1_ok = false, bat_ok = false, hooks_ok = false;
    for (const auto& f : packed) {
      if (f.path == "app/VERSION") {
        std::string text(f.bytes.begin(), f.bytes.end());
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' '))
          text.pop_back();
        version_ok = text == "0.1.0";
      }
      if (f.path == "tools/install.ps1") {
        std::string text(f.bytes.begin(), f.bytes.end());
        ps1_ok = text.find("LocalTree") != std::string::npos &&
                 text.find("$LOCAL_TREE_VERSION = '0.1.0'") != std::string::npos &&
                 text.find("Refusing -Ref main") != std::string::npos;
      }
      if (f.path == "install.bat") bat_ok = true;
      if (f.path == "app/afro_hooks.toml") {
        std::string text(f.bytes.begin(), f.bytes.end());
        hooks_ok = text.find("0x82663180") != std::string::npos &&
                   text.find("AfroPlayerMeterDeathGate") != std::string::npos;
      }
    }
    Expect(version_ok, "bundled VERSION is 0.1.0");
    Expect(ps1_ok, "bundled install.ps1 is the 0.1.0 local-tree script");
    Expect(bat_ok, "bundled tree still contains install.bat");
    Expect(hooks_ok, "bundled hooks include the meter death gate");
  }

  if (g_fails) {
    std::cerr << g_fails << " failure(s)\n";
    return 1;
  }
  std::cout << "setup command policy: all checks passed\n";
  return 0;
}
