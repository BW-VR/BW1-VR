// WINE IN THE HEADSET APP (docs/android.md, "Wine in the app").
//
// The APK carries a Wine tree for each ABI (assets/wine/<abi>.tar) with its
// build id (assets/wine/<abi>.id). At each start a thread of the app:
//
//   1. unpacks the tree of its own ABI into the internal folder of the app
//      when the tree is not there or has another build id.
//   2. makes the Wine prefix with wineboot the first time: no display driver
//      of Wine (user.reg), the CPU DLL for x86 in system32 (arm64-v8a).
//   3. links the drive G: of the prefix to the game folder.
//   4. with wine_probe=1 runs the test program bwvr_wine_probe.exe of the
//      game folder one time.
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
  // At the end of the app: stops the setup, waits for its thread, and stops
  // each process of Wine (wineserver -k).
  void Stop();

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
  // stdout and stderr into wine.log, and waits up to timeout_s seconds. The
  // exit code, or -1 when it did not start or did not end. stoppable: the
  // end of the app (Stop) stops the program.
  int Spawn(const std::vector<std::string>& args, int timeout_s, double* ms,
            bool stoppable = true);

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
};
