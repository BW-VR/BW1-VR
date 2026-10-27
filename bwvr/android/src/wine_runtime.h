// WINE IN THE HEADSET APP (docs/android.md, "Wine in the app").
//
// The APK carries a Wine tree for each ABI (assets/wine/<abi>.tar) with its
// build id (assets/wine/<abi>.id). At each start a thread of the app:
//
//   1. unpacks the tree of its own ABI into the internal folder of the app
//      when the tree is not there or has another build id.
//   2. makes the Wine prefix with wineboot when the prefix has no marker (a
//      new app, or a wineboot that stopped or failed): no display driver of
//      Wine (user.reg), the CPU DLLs for x86 in system32 (arm64-v8a:
//      wowbox64.dll and libwow64fex.dll).
//   3. links the drive G: of the prefix to the game folder.
//   4. with wine_probe=1 runs the test program bwvr_wine_probe.exe of the
//      game folder one time.
//
// At "Play" a second thread starts the game (StartGame, play_rules.h).
//
// The thread does not hold the frame loop: the menu shows what it does in
// its status line. The rules (the paths, the environment, the build id, the
// DLLs that stay out) are in wine_rules.h.
#pragma once

#include <android/asset_manager.h>
#include <sys/types.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "wine_rules.h"

class WineRuntime {
 public:
  struct Options {
    // wine_setup=1 (the default): the thread sets up Wine when the APK has
    // a tree.
    bool setup = true;   // wine_setup: 0 is the app with no Wine
    bool probe = false;  // wine_probe: run the test program after the setup
    std::string debug;   // wine_debug: the value of WINEDEBUG
    // wine_audio=1 (the default): the prefix has the audio driver of the
    // tree (wineaaudio, "Audio" = "aaudio"). 0: no audio driver, no sound.
    bool audio = true;
    // wine_audio_low_latency=1 (the default): the audio driver asks AAudio
    // for the low latency mode first. 0: the mode none
    // (WINEAAUDIO_LOW_LATENCY of the environment).
    bool low_latency = true;
    // wine_owner_check=2 (the default): Wine accepts each owner of its
    // folders only when the internal folder of the app has another owner
    // than the app (wine_rules::AnyOwner). 1: the test of Wine. 0: each
    // owner.
    int owner_check = 2;
    // wine_sigsys=2 (the default): a system call that a seccomp filter
    // traps gives -ENOSYS only in that container (wine_rules::SigsysEnosys).
    // 0: SIGSYS stops the process. 1: always -ENOSYS.
    int sigsys = 2;
  };

  // The ABI of this build of the app: arm64-v8a or x86_64.
  static const char* Abi();

  // Starts the thread of the setup. files: the internal folder of the app.
  // app_folder: the folder of the app on the shared storage (wine.log).
  void Start(AAssetManager* assets, const std::string& files, const std::string& app_folder,
             const std::string& game_folder, const Options& options);
  // What the status line of the menu says about Wine. Empty when the APK
  // has no Wine tree or wine_setup=0.
  std::string Status();
  // At the end of the app: stops the setup and the game, waits for their
  // threads, and stops each process of Wine (wineserver -k). With no thread
  // (wine_setup=0, or no internal folder) it does nothing.
  void Stop();
  // True when the APK has a Wine tree for the ABI of this build.
  static bool ApkHasTree(AAssetManager* assets);

  // THE GAME (docs/android.md, "Play in the app"). After a setup that ended
  // with "Wine is ready", StartGame starts a thread that puts the files of
  // the mod from the APK into the 32 bit system folder of the prefix,
  // imports the registry of the game (play_rules.h, GameRegistry) and runs
  // runblack.exe from the drive G: with the game folder as its current
  // folder. StopGame ("Quit game") stops runblack.exe and then each process
  // of Wine (wineserver -k).
  struct Game {
    bool new_game = false;  // -NEWGAME
    int width = 0;          // ScreenW
    int height = 0;         // ScreenH
    // The profile of the PC (play_rules.h, ProfileRegistry): the keys that
    // go after the registry of the game in the same import, the stamp of
    // bwvr-profile.reg for pc-profile.done after a good import, and the name
    // of the profile. Empty: no import.
    std::string profile_reg;
    std::string profile_stamp;
    std::string profile_name;
    // The variables of Box64 for the game (play_rules.h, Box64Vars):
    // box64_strongmem of the menu and box64_env of bwvr.ini.
    int box64_strongmem = 0;
    std::string box64_env;
    // The CPU of the 32 bit code on arm64-v8a (wine_rules.h, Cpu): wow64_cpu
    // of the menu. And the variables of FEX for the game (play_rules.h,
    // FexVars): fex_env of bwvr.ini.
    wine_rules::Cpu cpu = wine_rules::Cpu::kBox64;
    std::string fex_env;
    // fex_disk_cache of bwvr.ini (play_rules.h, FexVars): FEX keeps the code
    // that it translates in files of the prefix for the next start.
    bool fex_disk_cache = false;
  };
  enum class GameState { kIdle, kStarting, kRunning, kEnded };
  bool Ready() const { return ready_; }
  // False when Wine is not ready or a game runs.
  bool StartGame(const Game& game);
  // The state, and for kEnded the exit code (-1: a signal or a fault) and
  // what ended the game.
  GameState State(int* exit_code, std::string* why);
  void StopGame();

 private:
  void Run();
  void SetStatus(const std::string& text);
  bool Fault(const std::string& text);
  bool Unpack(const std::string& apk_id);
  bool CheckTree();
  // Makes a new prefix. Its last step writes the marker with the build id
  // of the tree.
  bool MakePrefix(const std::string& tree_id);
  bool LinkGame();
  void Probe();
  // Runs a program of the tree with the environment of the prefix, with its
  // stdout and stderr into wine.log, and waits up to timeout_s seconds (0:
  // no limit). The exit code, or -1 when it did not start or did not end.
  // stop: a flag that stops the program (SIGKILL), or null. cwd: the
  // current folder of the program, or empty for the folder of the app. env:
  // the environment, or null for the environment of each program. say_pid:
  // the log names the process that the app started.
  int Spawn(const std::vector<std::string>& args, int timeout_s, double* ms,
            const std::atomic<bool>* stop, const std::string& cwd = std::string(),
            const std::vector<std::string>* env = nullptr, bool say_pid = false);
  void RunGame(Game game);
  bool PutModFiles();
  // Copies the CPU DLLs of the tree (wine_rules.h, CopiedCpuDlls) into
  // system32 of the prefix. False at a fault.
  bool CopyCpuDlls();
  void EndGame(int code, const std::string& why);

  AAssetManager* assets_ = nullptr;
  std::string files_;
  std::string game_;
  std::string abi_;
  Options options_;
  wine_rules::Paths paths_;
  std::vector<std::string> env_;
  std::thread thread_;
  std::mutex lock_;
  std::string status_;
  std::atomic<bool> stop_{false};
  bool log_started_ = false;  // a program wrote into wine.log in this start
  std::atomic<bool> ready_{false};  // the setup ended with "Wine is ready"

  // The game. state_, code_ and why_ are under lock_.
  std::thread game_thread_;
  std::atomic<bool> game_stop_{false};
  GameState state_ = GameState::kIdle;
  int code_ = 0;
  std::string why_;
};
