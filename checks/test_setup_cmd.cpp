#include "../tools/afrosetup_cmd.h"

#include <algorithm>
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
  using afrosetup::CancelAnswer;
  using afrosetup::ConfirmedStop;
  using afrosetup::DescendantPids;
  using afrosetup::MayMarkStepDone;
  using afrosetup::PartialCountsAsVerified;
  using afrosetup::PidIsInRun;
  using afrosetup::JoinCommandLine;
  using afrosetup::MaySpawn;
  using afrosetup::QuoteWinArg;
  using afrosetup::SameOrInside;
  using afrosetup::Validate;

  Expect(afrosetup::EnsureInstallRoot(L"D:\\") == L"D:\\AfroSamuraiRecomp", "drive root becomes the subfolder");
  Expect(afrosetup::EnsureInstallRoot(L"D:") == L"D:\\AfroSamuraiRecomp", "bare drive letter becomes the subfolder");
  Expect(afrosetup::EnsureInstallRoot(L"D:/") == L"D:\\AfroSamuraiRecomp", "forward slash drive root becomes the subfolder");
  Expect(afrosetup::EnsureInstallRoot(L"D:\\Juegos") == L"D:\\Juegos\\AfroSamuraiRecomp", "a parent gains one subfolder");
  Expect(afrosetup::EnsureInstallRoot(L"D:\\Juegos\\") == L"D:\\Juegos\\AfroSamuraiRecomp", "a trailing slash is not a second folder");
  Expect(afrosetup::EnsureInstallRoot(L"D:\\AfroSamuraiRecomp") == L"D:\\AfroSamuraiRecomp",
         "an existing install name is not nested");
  Expect(afrosetup::EnsureInstallRoot(L"D:\\afrosamurairecomp\\") == L"D:\\afrosamurairecomp",
         "the install name match ignores case and a trailing slash");
  Expect(afrosetup::EnsureInstallRoot(L"C:\\My Games") == L"C:\\My Games\\AfroSamuraiRecomp", "spaces stay in the parent");
  Expect(afrosetup::EnsureInstallRoot(L"D:\\\u00d1o\u00f1os") == L"D:\\\u00d1o\u00f1os\\AfroSamuraiRecomp",
         "unicode in the parent is kept");
  Expect(afrosetup::EnsureInstallRoot(L"\\\\server\\share") == L"\\\\server\\share\\AfroSamuraiRecomp",
         "a UNC share root gains the subfolder");
  Expect(afrosetup::EnsureInstallRoot(L"\\\\server\\share\\Juegos") == L"\\\\server\\share\\Juegos\\AfroSamuraiRecomp",
         "a UNC directory is a parent");
  Expect(afrosetup::IsVolumeOrShareRoot(L"D:\\") && afrosetup::IsVolumeOrShareRoot(L"\\\\server\\share\\"),
         "drive and share roots are recognized");
  Expect(!afrosetup::IsVolumeOrShareRoot(L"D:\\AfroSamuraiRecomp"), "a subfolder is not a root");
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
  Expect(!ConfirmedStop(CancelAnswer::kContinue) && ConfirmedStop(CancelAnswer::kStop),
         "only a confirmed stop ends the run");
  Expect(!MayMarkStepDone(true) && MayMarkStepDone(false), "a cancelled step is not marked done");
  Expect(!PartialCountsAsVerified(true, false) && !PartialCountsAsVerified(false, true) &&
             PartialCountsAsVerified(true, true),
         "a partial download is verified only after the hash matches and it is renamed");
  const std::vector<std::pair<uint32_t, uint32_t>> edges = {{11, 10}, {12, 11}, {99, 1}, {50, 1}};
  const auto tree = DescendantPids(10, edges);
  Expect(PidIsInRun(10, tree) && PidIsInRun(11, tree) && PidIsInRun(12, tree),
         "this run includes the process it started and its children");
  Expect(!PidIsInRun(99, tree) && !PidIsInRun(50, tree),
         "a game and another installer are not part of this run");

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
  Expect(short_es == L"Generando c\u00f3digo", "code generation has no frozen percent");
  Expect(afrosetup::ProgressIndeterminate(ev.status), "code generation is an indeterminate bar");
  Expect(afrosetup::ProgressBarValue(ev.percent, ev.status) == 38, "generation does not invent a file fraction");
  Expect(short_es.find(L"Paso") == std::wstring::npos && short_es.find(L"10-40") == std::wstring::npos,
         "primary status hides the step counter and the time range");
  const std::wstring compiling =
      L"[ 55%] Paso 12/14: Compilar afrorecomp.exe (generacion de codigo + compilacion, 10-40 min) (compilando 10/400)";
  Expect(afrosetup::ParseProgressLine(compiling, ev), "compile detail parses");
  Expect(afrosetup::PrimaryProgress(ev.percent, ev.status, 1) == L"Compilando el juego \u00b7 10/400",
         "compile detail shows the real file count");
  Expect(afrosetup::ProgressBarValue(ev.percent, ev.status) == 2, "the bar is files done over files total");
  Expect(!afrosetup::ProgressIndeterminate(ev.status), "a real file count is not indeterminate");
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

  Expect(!afrosetup::AutoLaunchOnSuccess(), "a finished setup does not start the game by itself");
  const bool setup_ok = true, bat_ok = true;
  const auto play_ask = afrosetup::ResultActionFor(afrosetup::ResultButton::kPlay, setup_ok, bat_ok,
                                                   afrosetup::PlayOutcome::kNotAttempted);
  Expect(play_ask.launch && !play_ask.close_window, "Play launches and does not close before the command is accepted");
  const auto play_ok = afrosetup::ResultActionFor(afrosetup::ResultButton::kPlay, setup_ok, bat_ok,
                                                 afrosetup::PlayOutcome::kAccepted);
  const auto finish = afrosetup::ResultActionFor(afrosetup::ResultButton::kFinish, setup_ok, bat_ok,
                                                afrosetup::PlayOutcome::kAccepted);
  Expect(play_ok.launch && play_ok.close_window, "Play closes after the launch is accepted");
  Expect(!finish.launch && finish.close_window, "Finish closes and does not launch");
  Expect(play_ok.close_window && finish.close_window, "Play and Finish close the same way once Play has launched");
  const auto play_fail = afrosetup::ResultActionFor(afrosetup::ResultButton::kPlay, setup_ok, bat_ok,
                                                    afrosetup::PlayOutcome::kRejected);
  Expect(play_fail.launch && !play_fail.close_window, "a rejected launch leaves the installer open");
  const auto play_missing = afrosetup::ResultActionFor(afrosetup::ResultButton::kPlay, true, false,
                                                       afrosetup::PlayOutcome::kAccepted);
  Expect(!play_missing.launch && !play_missing.close_window, "Play without game\\Play.bat does not launch or close");
  const auto play_failed_setup = afrosetup::ResultActionFor(afrosetup::ResultButton::kPlay, false, true,
                                                            afrosetup::PlayOutcome::kAccepted);
  Expect(!play_failed_setup.launch && !play_failed_setup.close_window,
         "Play after a failed setup does not launch or close");
  const auto closed = afrosetup::ResultActionFor(afrosetup::ResultButton::kClose, true, true,
                                                 afrosetup::PlayOutcome::kAccepted);
  Expect(!closed.launch && closed.close_window, "the window X closes and never launches");
  const std::wstring stock_args = afrosetup::StockGameArguments();
  Expect(stock_args == L"--game_data_root=\"%~dp0..\\gamedata\" --gpu_plugin=xenos --log_file=\"%~dp0..\\afro.log\"",
         "stock Play.bat arguments are the four the game requires");
  const std::wstring stock_bat = L"start \"\" \"%~dp0afrorecomp.exe\" " + stock_args +
                                 L"\r\nif not defined AFRO_PLAY_NOPAUSE pause\r\nexit /b 1\r\n";
  Expect(afrosetup::PlayBatClosesItself(stock_bat), "the stock bat starts the game and can exit");
  const std::wstring old_bat = L"afrorecomp.exe " + stock_args + L"\r\n";
  Expect(!afrosetup::PlayBatClosesItself(old_bat) && old_bat.find(stock_args) != std::wstring::npos,
         "the old waiting bat still carries the same arguments");
  const std::wstring custom_bat = L"afrorecomp.exe --game_data_root=\"D:\\My Disc\\gamedata\" --gpu_plugin=xenos\r\n";
  Expect(!afrosetup::PlayBatClosesItself(custom_bat), "an edited bat is not treated as the stock one");
  const auto command = afrosetup::PlayCommandFor(L"C:\\Games\\Afro Samurai", L"C:\\Windows", true);
  Expect(command.file == L"C:\\Windows\\System32\\cmd.exe", "Jugar runs Play.bat through cmd");
  Expect(command.directory == L"C:\\Games\\Afro Samurai\\game", "Play.bat starts in the game folder");
  Expect(command.parameters.find(L"\"C:\\Games\\Afro Samurai\\game\\Play.bat\"") != std::wstring::npos,
         "Play.bat with spaces is quoted");
  Expect(!command.new_console, "the stock bat does not keep a command window");
  const auto custom = afrosetup::PlayCommandFor(L"C:\\Games\\Afro Samurai", L"C:\\Windows", false);
  Expect(custom.new_console, "an edited bat keeps the console its author wrote");
  Expect(!command.inherit_handles, "the game does not inherit the installer handles");
  Expect(afrosetup::PlaySurvivesInstallerExit(command) && afrosetup::PlaySurvivesInstallerExit(custom),
         "the accepted launch is detached from the installer");

  afrosetup::ToolReuse reused;
  reused.git = reused.cmake = reused.ninja = reused.python = reused.llvm = reused.msvc = true;
  reused.sdk = reused.rexglue = reused.gamedata = true;
  const auto update_steps = afrosetup::LogicalSetupSteps(reused);
  Expect(update_steps.size() == 5, "an update that reuses tools has five real steps");
  Expect(update_steps[0] == "repo" && update_steps[1] == "codegen" && update_steps[2] == "compile" &&
             update_steps[3] == "gpu" && update_steps[4] == "install",
         "update steps are source, codegen, compile, graphics, copy");
  for (const auto& name : update_steps)
    Expect(name != "git" && name != "llvm" && name != "msvc" && name != "sdk", "reused tools are not steps");
  const auto fresh_steps = afrosetup::LogicalSetupSteps({});
  Expect(fresh_steps.size() == 14, "a new install counts every tool that will actually run");
  Expect(fresh_steps[0] == "git" && fresh_steps[6] == "repo" && fresh_steps[10] == "codegen",
         "fresh install order matches the script");

  afrosetup::StepEvent step;
  Expect(afrosetup::ParseAfroStep(L"AFROSTEP 2/5 codegen -1 0 0 generate", step), "codegen step parses");
  const auto codegen_es = afrosetup::ViewStep(step, 1);
  Expect(codegen_es.indeterminate && codegen_es.bar == 0, "codegen has no measured percent");
  Expect(codegen_es.label == L"Paso 2 de 5 \u00b7 Compilar \u00b7 Generando c\u00f3digo",
         "Spanish codegen step keeps its name and has no invented total");
  Expect(afrosetup::ViewStep(step, 0).label == L"Step 2 of 5 \u00b7 Build \u00b7 Generating code",
         "English codegen step label");
  Expect(afrosetup::ViewStep(step, 2).label == L"\u00c9tape 2 sur 5 \u00b7 Compilation \u00b7 G\u00e9n\u00e9ration du code",
         "French codegen step label");
  Expect(afrosetup::ViewStep(step, 3).label == L"Passo 2 di 5 \u00b7 Compilazione \u00b7 Generazione del codice",
         "Italian codegen step label");
  Expect(afrosetup::ParseAfroStep(L"AFROSTEP 3/5 compile -1 10 400 compile", step), "compile step parses");
  const auto compile_es = afrosetup::ViewStep(step, 1);
  Expect(!compile_es.indeterminate && compile_es.bar == 2 && compile_es.index == 3,
         "the compile bar is that step's file ratio and the step number advanced");
  Expect(compile_es.label == L"Paso 3 de 5 \u00b7 Compilar \u00b7 Compilando \u00b7 10/400",
         "compile keeps the step name and the completed file count");
  Expect(afrosetup::ParseAfroStep(L"AFROSTEP 1/14 llvm -1 524288000 943718400 download", step),
         "a byte download parses");
  const auto dl = afrosetup::ViewStep(step, 1);
  Expect(!dl.indeterminate && dl.bar == 55, "the download bar is bytes done over bytes total");
  Expect(dl.label == L"Paso 1 de 14 \u00b7 LLVM/Clang \u00b7 Descargando \u00b7 500/900 MB",
         "a download shows the step name and completed/total megabytes");
  Expect(afrosetup::ParseAfroStep(L"AFROSTEP 5/14 llvm -1 395313152 939524096 download", step),
         "the agreed LLVM example parses");
  Expect(afrosetup::ViewStep(step, 1).label == L"Paso 5 de 14 \u00b7 LLVM/Clang \u00b7 Descargando \u00b7 377/896 MB",
         "Spanish LLVM line is step, name, action, completed/total");
  Expect(afrosetup::ViewStep(step, 0).label == L"Step 5 of 14 \u00b7 LLVM/Clang \u00b7 Downloading \u00b7 377/896 MB",
         "English LLVM line uses the same order");
  Expect(afrosetup::ViewStep(step, 2).label ==
             L"\u00c9tape 5 sur 14 \u00b7 LLVM/Clang \u00b7 T\u00e9l\u00e9chargement \u00b7 377/896 MB",
         "French LLVM line uses the same order");
  Expect(afrosetup::ViewStep(step, 3).label == L"Passo 5 di 14 \u00b7 LLVM/Clang \u00b7 Download \u00b7 377/896 MB",
         "Italian LLVM line uses the same order");

  Expect(afrosetup::ParseAfroStep(L"AFROSTEP 5/14 llvm -1 53 53 unpack", step), "xz zip count parses");
  const auto xz_zip = afrosetup::ViewStep(step, 1);
  Expect(!xz_zip.indeterminate && xz_zip.bar == 100 && xz_zip.label.find(L"53/53") != std::wstring::npos &&
             xz_zip.label.find(L"LLVM/Clang") != std::wstring::npos &&
             xz_zip.label.find(L"Extrayendo") == std::wstring::npos,
         "53/53 is the decompressor zip, not LLVM extraction");
  Expect(afrosetup::ParseAfroStep(L"AFROSTEP 5/14 llvm -1 0 943718400 decompress", step),
         "decompress starts from the archive length");
  const auto dec0 = afrosetup::ViewStep(step, 1);
  Expect(!dec0.indeterminate && dec0.bar == 0 && dec0.label.find(L"53") == std::wstring::npos &&
             dec0.label.find(L"Descomprimiendo") != std::wstring::npos,
         "a new measured phase starts empty and drops the previous 53/53");
  Expect(afrosetup::ViewStep(step, 0).label.find(L"Decompressing") != std::wstring::npos &&
             afrosetup::ViewStep(step, 2).label.find(L"D\u00e9compression") != std::wstring::npos &&
             afrosetup::ViewStep(step, 3).label.find(L"Decompressione") != std::wstring::npos,
         "decompress keeps the LLVM step name in four languages");
  Expect(afrosetup::ParseAfroStep(L"AFROSTEP 5/14 llvm -1 471859200 943718400 decompress", step),
         "mid-archive decompress parses");
  const auto dec_mid = afrosetup::ViewStep(step, 1);
  Expect(!dec_mid.indeterminate && dec_mid.bar == 50 && dec_mid.bar < 100,
         "half the compressed bytes is 50, not a finished bar");
  Expect(afrosetup::ParseAfroStep(L"AFROSTEP 5/14 llvm -1 943718399 943718400 decompress", step),
         "the last reported decompress byte stays short of the end");
  Expect(!afrosetup::ViewStep(step, 1).indeterminate && afrosetup::ViewStep(step, 1).bar < 100,
         "one byte short of the archive is not 100 percent");
  Expect(afrosetup::ParseAfroStep(L"AFROSTEP 5/14 llvm -1 0 0 extract", step), "tar starts with no file total");
  const auto tar = afrosetup::ViewStep(step, 1);
  Expect(tar.indeterminate && tar.bar == 0 && tar.label.find(L"53") == std::wstring::npos &&
             tar.label.find(L"Extrayendo") != std::wstring::npos &&
             tar.label.find(L"LLVM/Clang") != std::wstring::npos,
         "tar extraction is an animated bar and does not keep the zip fraction");
  Expect(afrosetup::ViewStep(step, 0).label.find(L"Extracting") != std::wstring::npos &&
             afrosetup::ViewStep(step, 2).label.find(L"Extraction") != std::wstring::npos &&
             afrosetup::ViewStep(step, 3).label.find(L"Estrazione") != std::wstring::npos,
         "the unmeasured extract label stays translated");
  Expect(afrosetup::ParseAfroStep(L"AFROSTEP 2/5 codegen -1 0 0 generate", step) &&
             afrosetup::ViewStep(step, 1).indeterminate,
         "codegen stays unmeasured");

  {
    const wchar_t* trace[] = {
        L"AFROSTEP 1/5 repo -1 0 0",
        L"AFROSTEP 2/5 codegen -1 0 0 generate",
        L"[ 38%] Paso 12/14: Compilar afrorecomp.exe (generacion de codigo + compilacion, 10-40 min)",
        L"AFROSTEP 3/5 compile -1 0 0 compile",
        L"AFROSTEP 3/5 compile -1 10 400 compile",
        L"AFROSTEP 3/5 compile -1 200 400 compile",
        L"AFROSTEP 4/5 gpu -1 2 20 graphics",
        L"AFROSTEP 5/5 install -1 0 0 check",
        L"AFROSTEP 5/5 install -1 4 8 copy",
    };
    afrosetup::StepView view;
    bool saw_codegen = false;
    for (const wchar_t* line : trace) {
      const bool applied = afrosetup::ApplyInstallerLine(line, view, 1);
      if (!applied) {
        Expect(saw_codegen && view.index == 2 && view.indeterminate && view.label.find(L"38") == std::wstring::npos,
               "a global percent between codegen and compile does not move the step bar");
        std::cout << "step kept " << view.index << "/" << view.count << " unmeasured\n";
        continue;
      }
      if (view.index == 2) saw_codegen = true;
      std::cout << "step " << view.index << "/" << view.count << " bar " << view.bar
                << (view.indeterminate ? " unmeasured" : " measured") << "\n";
    }
    Expect(saw_codegen, "the trace shows the codegen step");
    Expect(view.index == 5 && view.count == 5 && !view.indeterminate && view.bar == 50,
           "the last step bar is the copy ratio, not an overall percent");
    Expect(view.label == L"Paso 5 de 5 \u00b7 Instalar \u00b7 Copiando \u00b7 4/8",
           "the copy step keeps its name and completed count");
  }

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
    bool version_ok = false, ps1_ok = false, bat_ok = false, hooks_ok = false, manifest_lf = false;
    for (const auto& f : packed) {
      if (f.path == "app/VERSION") {
        std::string text(f.bytes.begin(), f.bytes.end());
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' '))
          text.pop_back();
        version_ok = text == "0.3.2";
      }
      if (f.path == "tools/install.ps1") {
        std::string text(f.bytes.begin(), f.bytes.end());
        ps1_ok = text.find("LocalTree") != std::string::npos &&
                 text.find("$LOCAL_TREE_VERSION = '0.3.2'") != std::string::npos &&
                 text.find("Refusing -Ref main") != std::string::npos &&
                 text.find("AFROSTEP ") != std::string::npos &&
                 text.find("AFROPHASE compile") != std::string::npos &&
                 text.find("Set-PhasePlan") != std::string::npos;
      }
      if (f.path == "install.bat") bat_ok = true;
      if (f.path == "app/afrorecomp_manifest.toml")
        manifest_lf = std::find(f.bytes.begin(), f.bytes.end(), uint8_t('\r')) == f.bytes.end();
      if (f.path == "app/afro_hooks.toml") {
        std::string text(f.bytes.begin(), f.bytes.end());
        hooks_ok = text.find("0x82663180") != std::string::npos &&
                   text.find("AfroPlayerMeterDeathGate") != std::string::npos;
      }
    }
    Expect(version_ok, "bundled VERSION is 0.3.2");
    Expect(ps1_ok, "bundled install.ps1 is the 0.3.2 local-tree script");
    Expect(bat_ok, "bundled tree still contains install.bat");
    Expect(hooks_ok, "bundled hooks include the meter death gate");
    Expect(manifest_lf, "bundled codegen manifest has LF line ends, as rexglue writes it");
  }

  auto read_text = [](const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::string();
    return std::string(std::istreambuf_iterator<char>(in), {});
  };
  std::string script_text, window_cpp, build_bat;
  for (const char* dir : {"/tmp/afro-0.3.2", "..", "."}) {
    if (script_text.empty()) script_text = read_text(std::string(dir) + "/tools/install.ps1");
    if (window_cpp.empty()) window_cpp = read_text(std::string(dir) + "/tools/afrosetup.cpp");
    if (build_bat.empty()) build_bat = read_text(std::string(dir) + "/tools/build_win.bat");
  }
  Expect(!script_text.empty() && !window_cpp.empty() && !build_bat.empty(), "progress and launch sources are readable");
  Expect(script_text.find("10-40 min") == std::string::npos, "the build step no longer quotes 10-40 min");
  Expect(script_text.find("Content-Length") != std::string::npos &&
             script_text.find("Receiving objects:") != std::string::npos &&
             script_text.find("Expand-ZipCounted") != std::string::npos,
         "downloads, git objects and zip entries are real counts");
  Expect(!afrosetup::ButtonStyleKeepsCheck(afrosetup::kBtnOwnerDraw),
         "BS_OWNERDRAW does not keep BM_SETCHECK");
  Expect(afrosetup::ButtonStyleKeepsCheck(afrosetup::kBtnAutoRadio) &&
             afrosetup::ButtonStyleKeepsCheck(afrosetup::kBtnAutoCheck),
         "a real radio or checkbox style does keep the mark");
  afrosetup::ChoiceMark lost;
  Expect(afrosetup::ReportedCheck(afrosetup::kBtnOwnerDraw, 0, lost) == 0,
         "without our store an owner-draw circle stays empty");
  afrosetup::ChoiceMark kept;
  afrosetup::RememberChoice(kept, 1);
  Expect(afrosetup::ReportedCheck(afrosetup::kBtnOwnerDraw, 0, kept) == 1,
         "the stored mark is what the circle paints");
  int group[4] = {1, 0, 0, 0};
  afrosetup::ApplyRadioGroup(group, 4, 2);
  Expect(group[0] == 0 && group[1] == 0 && group[2] == 1 && group[3] == 0,
         "choosing one radio clears the others");
  const bool none[] = {false, false, false, false};
  Expect(afrosetup::SelectedRadioOrKeep(none, 4, 1) == 1,
         "if the buttons report nothing, the page default stays");
  const bool spanish[] = {false, true, false, false};
  Expect(afrosetup::SelectedRadioOrKeep(spanish, 4, 0) == 1, "a reported radio wins over the default");
  Expect(afrosetup::ToggleCheck(0) == 1 && afrosetup::ToggleCheck(1) == 0, "a checkbox toggles both ways");
  const afrosetup::WizardPage choice_pages[] = {
      afrosetup::WizardPage::kLanguage, afrosetup::WizardPage::kMode, afrosetup::WizardPage::kGame,
      afrosetup::WizardPage::kLicense, afrosetup::WizardPage::kOptions};
  for (afrosetup::WizardPage page : choice_pages) {
    const afrosetup::PageChoices n = afrosetup::DarkChoiceControls(page);
    Expect(n.radios + n.checks > 0, "each dark choice page has a radio or a checkbox");
  }
  Expect(afrosetup::DarkChoiceControls(afrosetup::WizardPage::kLanguage).radios == 4, "language has four radios");
  Expect(afrosetup::DarkChoiceControls(afrosetup::WizardPage::kOptions).checks == 1 &&
             afrosetup::DarkChoiceControls(afrosetup::WizardPage::kLicense).checks == 1,
         "license and options keep a real checkbox");
  Expect(afrosetup::NextStopFor(afrosetup::WizardPage::kGame, false, true, false, false, true) ==
             afrosetup::NextStop::kProceed,
         "ISO selected with a path continues even if the extracted folder is empty");
  Expect(afrosetup::NextStopFor(afrosetup::WizardPage::kGame, false, true, true, true, true) ==
             afrosetup::NextStop::kNeedIso,
         "ISO selected with an empty box does not continue");
  Expect(afrosetup::NextStopFor(afrosetup::WizardPage::kGame, false, false, false, false, true) ==
             afrosetup::NextStop::kNeedXex,
         "folder mode without default.xex does not continue");
  Expect(afrosetup::NextStopFor(afrosetup::WizardPage::kGame, false, false, true, true, true) ==
             afrosetup::NextStop::kProceed,
         "folder mode with default.xex continues");
  Expect(afrosetup::NextStopFor(afrosetup::WizardPage::kFolder, true, true, true, false, false) ==
             afrosetup::NextStop::kNeedFolder,
         "an empty install folder does not continue");
  Expect(afrosetup::NextStopFor(afrosetup::WizardPage::kLicense, false, true, false, false, false) ==
             afrosetup::NextStop::kNeedLicense,
         "the license page requires the checkbox");
  Expect(window_cpp.find("BM_SETCHECK") != std::string::npos && window_cpp.find("BM_GETCHECK") != std::string::npos &&
             window_cpp.find("StoreChoice") != std::string::npos &&
             window_cpp.find("HIWORD(w) == BN_CLICKED") != std::string::npos,
         "the window stores the mark and ignores focus notifications");
  Expect(afrosetup::HeaderHeightForClientWidth(680) == 136, "a 680px client keeps the 1200x240 header");
  Expect(afrosetup::HeaderHeightForClientWidth(850) == 170, "header height follows the client width");
  Expect(script_text.find("System32\\tar.exe") != std::string::npos, "plain tar is the System32 program");
  Expect(script_text.find("tools\\xz\\xz.exe") == std::string::npos &&
             script_text.find("Join-Path $dir 'xz.exe'") != std::string::npos,
         "xz.exe is the managed copy, not a PATH lookup");
  Expect(script_text.find("Expand-TarXz") != std::string::npos &&
             script_text.find("$psi.Arguments = '-dc'") != std::string::npos &&
             script_text.find("Get-Command xz") == std::string::npos &&
             script_text.find("RedirectStandardInput = $true") != std::string::npos,
         "a tar.xz is fed to managed xz on stdin before tar sees a plain tar");
  Expect(script_text.find("Expand-ZipCounted $zip $stage 0 'unpack'") != std::string::npos &&
             script_text.find("Emit-Step $script:PhaseKey -1 0 $compressed 'decompress'") != std::string::npos &&
             script_text.find("Emit-Step $script:PhaseKey -1 0 0 'extract'") != std::string::npos &&
             script_text.find("if ($shown -ge $compressed) { $shown = $compressed - 1 }") != std::string::npos,
         "xz zip, decompress bytes and tar are separate phases and decompress never reports 100");
  Expect(window_cpp.find("PBM_SETPOS, 0, 0") != std::string::npos &&
             window_cpp.find("PBM_SETPOS, 0, 0") < window_cpp.find("PBM_SETMARQUEE, TRUE"),
         "an unmeasured bar is cleared before the animation starts");
  Expect(script_text.find("'-xf', $arc") == std::string::npos &&
             script_text.find("'-xf', $tarOut") != std::string::npos,
         "tar -xf is not given the .tar.xz");
  const std::wstring xz_signed =
      L"62fdfde73d5c5d293bbb4a96211b29d09adbd56bc6736976e4c9fc9942ae3c67";
  const std::wstring xz_typo =
      L"62fdfde73d5c5c293bbb4a96211b29d09adbd56bc6736976e4c9fc9942ae3c67";
  Expect(afrosetup::Sha256Equal(L"62FDFDE73D5C5D293BBB4A96211B29D09ADBD56BC6736976E4C9FC9942AE3C67", xz_signed),
         "SHA-256 compare folds case");
  Expect(!afrosetup::Sha256Equal(xz_typo, xz_signed), "one different digit is a real mismatch");
  Expect(script_text.find("62fdfde73d5c5d293bbb4a96211b29d09adbd56bc6736976e4c9fc9942ae3c67") != std::string::npos &&
             script_text.find("62fdfde73d5c5c293bbb4a96211b29d09adbd56bc6736976e4c9fc9942ae3c67") == std::string::npos,
         "the xz pin is the signed digest, not the one-digit typo");
  Expect(script_text.find("Same-Sha256") != std::string::npos && script_text.find("-ceq") != std::string::npos,
         "the script compares the normalized digest");
  Expect(script_text.find("if ($fail) { throw $fail }") != std::string::npos,
         "a checksum or tool failure is not replaced with a generic tar message");
  Expect(script_text.find("$nested") != std::string::npos && script_text.find("StepFrac") != std::string::npos,
         "a nested download does not replace the phase measurement");
  Expect(script_text.find("Same-Sha256 (Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash $d.sha") != std::string::npos,
         "a verified download is reused and not fetched again");
  Expect(script_text.find("$script:PhasePlan.Count") != std::string::npos,
         "the log step number uses the same plan as AFROSTEP");
  const auto tools = script_text.find("foreach ($n in @('git','cmake','ninja','python','llvm','msvc'))");
  const auto repo = script_text.find("$script:PhasePlan += 'repo'", tools);
  const auto later = script_text.find("foreach ($n in @('sdk','rexglue-src','gamedata'))", repo);
  const auto tail = script_text.find("$script:PhasePlan += @('codegen','compile','gpu','install')", later);
  Expect(tools != std::string::npos && repo != std::string::npos && later != std::string::npos &&
             tail != std::string::npos,
         "install.ps1 counts the same steps as LogicalSetupSteps");
  Expect(script_text.find("if (Done $n) { Say (T 'skip'") != std::string::npos, "a finished tool is not emitted as a step");
  Expect(script_text.find("AFROPHASE codegen") != std::string::npos && script_text.find("AFROPHASE compile") != std::string::npos,
         "the script separates codegen from compile");
  Expect(build_bat.find("echo AFROPHASE codegen") != std::string::npos && build_bat.find("echo AFROPHASE compile") != std::string::npos,
         "the build announces codegen and then compile");
  Expect(build_bat.find("> ..\\..\\codegen_win.log") == std::string::npos, "rexglue output is not redirected away from the installer");
  Expect(window_cpp.find("StartSpaceJob") == std::string::npos &&
             window_cpp.find("RecoverableBuildBytes") == std::string::npos &&
             window_cpp.find("SpaceThread") == std::string::npos &&
             window_cpp.find("space_pending") == std::string::npos,
         "the options page does not walk the disk for a space estimate");
  Expect(build_bat.find("CODEGEN FAILED & exit /b 1") == std::string::npos &&
             build_bat.find("CODEGEN FAILED exit %GEN_EXIT%") != std::string::npos &&
             build_bat.find("set RC=") == std::string::npos &&
             build_bat.find("check_pe_imports.py") != std::string::npos,
         "codegen keeps its exit code without writing the RC variable CMake uses");
  Expect(build_bat.find("CMAKE_RC_COMPILER:[A-Za-z]*=0$") != std::string::npos &&
             build_bat.find("if \"%RC%\"==\"0\" set \"RC=\"") != std::string::npos,
         "a cache that stored RC=0 is removed and that value is cleared before CMake");
  Expect(afrosetup::CmakeRcCacheLineIsExitCode(L"CMAKE_RC_COMPILER:FILEPATH=0") &&
             afrosetup::CmakeRcCacheLineIsExitCode(L"CMAKE_RC_COMPILER:STRING=0"),
         "RC=0 in the CMake cache is the exit code, not rc.exe");
  Expect(!afrosetup::CmakeRcCacheLineIsExitCode(
             L"CMAKE_RC_COMPILER:FILEPATH=E:/AfroSamuraiRecomp/tools/llvm/bin/llvm-rc.exe") &&
             !afrosetup::CmakeRcCacheLineIsExitCode(L"# CMAKE_RC_COMPILER is the resource compiler"),
         "a real resource compiler path and a comment are kept");
  Expect(script_text.find("Sync-SourceTree $src $dest @('app\\out', 'app\\generated'") != std::string::npos &&
             script_text.find("afro-keep-") == std::string::npos,
         "an update syncs the source tree and keeps the build folder and codegen output");
  Expect(script_text.find("SetLastWriteTimeUtc($target, [DateTime]::UtcNow)") != std::string::npos &&
             script_text.find("Test-SameBytes $from $target") != std::string::npos,
         "only differing files are written, and they are newer than the old objects");
  Expect(build_bat.find("--max_jump_table_entries=") == std::string::npos,
         "build_win.bat runs codegen with the same flags as the Ninja rule");
  Expect(window_cpp.find("ResultButton::kPlay") != std::string::npos && window_cpp.find("ResultButton::kFinish") != std::string::npos &&
             window_cpp.find("ResultButton::kClose") != std::string::npos,
         "Play, Finish and X go through the result policy");
  Expect(window_cpp.find("PlayOutcome::kNotAttempted") != std::string::npos &&
             window_cpp.find("PlayOutcome::kAccepted") != std::string::npos &&
             window_cpp.find("PlayOutcome::kRejected") != std::string::npos,
         "Play closes only after the launch is accepted");
  Expect(window_cpp.find("StartInstalledGame") != std::string::npos, "Play starts the installed game");
  Expect(window_cpp.find("ShellExecuteW(g_wnd, L\"open\", play") == std::string::npos, "Play does not use ShellExecute");
  Expect(window_cpp.find("CREATE_BREAKAWAY_FROM_JOB") != std::string::npos &&
             window_cpp.find("CREATE_NO_WINDOW") != std::string::npos &&
             window_cpp.find("CREATE_NEW_PROCESS_GROUP") != std::string::npos &&
             window_cpp.find("FALSE, flags, env") != std::string::npos &&
             window_cpp.find("AFRO_PLAY_NOPAUSE") != std::string::npos &&
             window_cpp.find("PlayBatClosesItself") != std::string::npos,
         "Jugar runs Play.bat, hides the stock console, and inherits no installer handles");
  Expect(script_text.find("start \"\" \"%~dp0afrorecomp.exe\" --game_data_root=\"%~dp0..\\gamedata\" --gpu_plugin=xenos --log_file=\"%~dp0..\\afro.log\"") !=
                 std::string::npos &&
             script_text.find("if not defined AFRO_PLAY_NOPAUSE pause") != std::string::npos &&
             script_text.find("Play.bat left as the user wrote it") != std::string::npos,
         "the stock Play.bat keeps every required argument, closes, and an edited bat is kept");
  Expect(window_cpp.find("AutoLaunchOnSuccess() is false") != std::string::npos, "success does not launch on its own");
  Expect(window_cpp.find("L\"Play\"") != std::string::npos && window_cpp.find("L\"Jugar\"") != std::string::npos &&
             window_cpp.find("L\"Jouer\"") != std::string::npos && window_cpp.find("L\"Gioca\"") != std::string::npos,
         "Play keeps its four language captions");
  Expect(window_cpp.find("The game did not start. The installer stays open.") != std::string::npos &&
             window_cpp.find("El juego no se ha iniciado. El instalador sigue abierto.") != std::string::npos &&
             window_cpp.find("L'installateur reste ouvert.") != std::string::npos &&
             window_cpp.find("L'installatore resta aperto.") != std::string::npos,
         "a rejected launch has a message in EN, ES, FR and IT");
  const auto play_call = window_cpp.find("StartInstalledGame");
  const auto finish_btn = window_cpp.find("ResultButton::kFinish");
  Expect(play_call != std::string::npos && finish_btn != std::string::npos &&
             window_cpp.find("StartInstalledGame", finish_btn) == std::string::npos,
         "Finish does not start the game");

  Expect(afrosetup::IsoAnswerFromDialog(6) == afrosetup::IsoPromptAnswer::kContinue &&
             afrosetup::IsoContinueChosen(afrosetup::IsoPromptAnswer::kContinue),
         "only an explicit continue proceeds");
  Expect(afrosetup::IsoAnswerFromDialog(7) == afrosetup::IsoPromptAnswer::kStop &&
             afrosetup::IsoAnswerFromDialog(2) == afrosetup::IsoPromptAnswer::kStop &&
             afrosetup::IsoAnswerFromDialog(0) == afrosetup::IsoPromptAnswer::kStop &&
             !afrosetup::IsoContinueChosen(afrosetup::IsoPromptAnswer::kStop),
         "No, Cancel and the window X stop");
  Expect(!afrosetup::IsoOverrideMarksVerified(), "continuing does not mark the ISO verified");
  afrosetup::IsoUnverifiedNote noted;
  noted.ok = true;
  noted.path = L"C:\\Games\\Afro Samurai (Europe) (En,Fr,Es,It).iso";
  noted.actual = L"56D1C955B07C8DCF3439362946592FC9677FA9CB";
  noted.expected = L"29394DB1854DFC59C767D768C8D35E0A08ACB73D";
  noted.length = 7835492352ULL;
  Expect(afrosetup::IsoHashMarksVerified(afrosetup::IsoHashResult::kMatch) &&
             !afrosetup::IsoHashStopsInstall(afrosetup::IsoHashResult::kMatch) &&
             !afrosetup::IsoHashPrompts(afrosetup::IsoHashResult::kMatch),
         "a matching SHA-1 is verified and does not ask");
  Expect(!afrosetup::IsoHashMarksVerified(afrosetup::IsoHashResult::kMismatch) &&
             !afrosetup::IsoHashStopsInstall(afrosetup::IsoHashResult::kMismatch) &&
             !afrosetup::IsoHashPrompts(afrosetup::IsoHashResult::kMismatch),
         "a different SHA-1 continues with no prompt and no verified stamp");
  Expect(afrosetup::IsoHashStopsInstall(afrosetup::IsoHashResult::kUnreadable) &&
             !afrosetup::IsoHashMarksVerified(afrosetup::IsoHashResult::kUnreadable),
         "an unreadable ISO still stops the install");
  Expect(afrosetup::UnverifiedNoteSkipsReread(noted, noted.path, noted.length),
         "the same file is not hashed again");
  Expect(!afrosetup::UnverifiedNoteSkipsReread(noted, L"D:\\other.iso", noted.length),
         "a different path is hashed");
  Expect(!afrosetup::UnverifiedNoteSkipsReread(noted, noted.path, noted.length - 1),
         "a changed file is hashed again");
  noted.actual = noted.expected;
  Expect(!afrosetup::UnverifiedNoteSkipsReread(noted, noted.path, noted.length),
         "a note that claims a match is not an unverified skip");
  afrosetup::IsoUnverifiedNote no_hash = noted;
  no_hash.ok = true;
  no_hash.actual.clear();
  Expect(!afrosetup::UnverifiedNoteSkipsReread(no_hash, no_hash.path, no_hash.length),
         "length alone does not accept an ISO");
  bool saw_override = false;
  for (const auto& a : BuildArgv(ok))
    if (a == L"-AcceptUnverifiedIso") saw_override = true;
  Expect(!saw_override, "the window does not pass an ISO override flag");
  Expect(script_text.find("Get-FileHash -LiteralPath $isoFull -Algorithm SHA1") != std::string::npos &&
             script_text.find("$ISO_SHA1 = '29394DB1854DFC59C767D768C8D35E0A08ACB73D'") != std::string::npos,
         "the ISO is still one full-file SHA-1 against the same pin");
  Expect(script_text.find("if ($h -eq $ISO_SHA1) {") != std::string::npos &&
             script_text.find("Mark 'iso-hash'") != std::string::npos,
         "only the matching hash is marked verified");
  Expect(script_text.find("exit 42") == std::string::npos &&
             script_text.find("ISO_UNVERIFIED_CHOICE") == std::string::npos &&
             script_text.find("YesNo (T 'cont')") == std::string::npos &&
             script_text.find("-AcceptUnverifiedIso") == std::string::npos &&
             script_text.find("iso_continue") != std::string::npos,
         "a hash mismatch is a log line, not a question or a restart");
  Expect(script_text.find("Not verified. Continuing anyway.") != std::string::npos &&
             script_text.find("No verificada. Se continua igualmente.") != std::string::npos &&
             script_text.find("Non verifiee. Poursuite malgre tout.") != std::string::npos &&
             script_text.find("Non verificata. Si continua comunque.") != std::string::npos,
         "the log line exists in four languages");
  Expect(script_text.find("throw (T 'iso_nf'") != std::string::npos,
         "a missing ISO is still an error");
  Expect(window_cpp.find("AskIsoDialog") == std::string::npos &&
             window_cpp.find("g_exit_code == 42") == std::string::npos,
         "the window does not pop up or restart on a hash mismatch");
  Expect(window_cpp.find("7,9 GB") != std::string::npos, "the fixed 7.9 GB tools line stays");

  std::vector<std::wstring> safe;
  Expect(afrosetup::GitSafeDirectoriesFor(L"D:\\AfroSamuraiRecomp\\rexglue-sdk", safe) && safe.size() == 2 &&
             safe[0] == L"D:/AfroSamuraiRecomp/rexglue-sdk" &&
             safe[1] == L"D:/AfroSamuraiRecomp/rexglue-sdk/*",
         "the owned repo and only its children are marked safe");
  Expect(afrosetup::GitSafeDirectoriesFor(L"D:\\Afro Samurai\\rexglue-sdk", safe) &&
             safe[0] == L"D:/Afro Samurai/rexglue-sdk",
         "a space in the install path stays in safe.directory");
  Expect(afrosetup::GitSafeDirectoriesFor(L"D:\\\u00d1o\u00f1os\\rexglue-sdk", safe) &&
             safe[0] == L"D:/\u00d1o\u00f1os/rexglue-sdk" &&
             safe[1] == L"D:/\u00d1o\u00f1os/rexglue-sdk/*",
         "unicode in the owned path is kept");
  Expect(!afrosetup::GitSafeDirectoriesFor(L"*", safe) && !afrosetup::GitSafeDirectoriesFor(L"D:\\*", safe) &&
             !afrosetup::GitSafeDirectoriesFor(L"D:\\", safe) &&
             !afrosetup::GitSafeDirectoriesFor(L"\\\\server\\share", safe),
         "a wildcard, a drive root, or a share root is not marked safe");
  Expect(safe.empty() || safe[0] != L"*", "a rejected directory does not become safe.directory=*");
  Expect(afrosetup::DecideCloneResume(true, true, true, false) == afrosetup::CloneAction::kResumeCheckout &&
             afrosetup::DecideCloneResume(true, true, false, true) == afrosetup::CloneAction::kResumeCheckout,
         "an existing .git is checked out again and is not deleted");
  Expect(afrosetup::DecideCloneResume(false, false, true, false) == afrosetup::CloneAction::kCloneNew,
         "a missing folder is cloned");
  Expect(afrosetup::DecideCloneResume(true, false, true, false) == afrosetup::CloneAction::kReplaceIncomplete,
         "only an incomplete owned clone directory may be replaced");
  Expect(afrosetup::DecideCloneResume(true, false, false, false) == afrosetup::CloneAction::kRefuse &&
             afrosetup::DecideCloneResume(true, false, true, true) == afrosetup::CloneAction::kRefuse,
         "another folder or a reparse point is not deleted");
  Expect(afrosetup::GitStatusPropagates(128) && afrosetup::GitStatusPropagates(1) &&
             !afrosetup::GitStatusPropagates(0),
         "dubious ownership and any other git failure fail the step");
  Expect(script_text.find("safe.directory=$value") != std::string::npos &&
             script_text.find("safe.directory=*") == std::string::npos &&
             script_text.find("--global") == std::string::npos &&
             script_text.find("Remove-IncompleteClone") != std::string::npos &&
             script_text.find("Refusing to delete") != std::string::npos &&
             script_text.find("if ($code -ne 0) { throw") != std::string::npos,
         "git exceptions stay on this process, and a failed git command still throws");

  if (g_fails) {
    std::cerr << g_fails << " failure(s)\n";
    return 1;
  }
  std::cout << "setup command policy: all checks passed\n";
  return 0;
}
