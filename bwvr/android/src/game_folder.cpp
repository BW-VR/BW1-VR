#include "game_folder.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <vector>

#include "../../src/common/exe_sha1.h"
#include "app_log.h"

namespace {

bool Exists(const std::string& path) {
  struct stat s;
  return stat(path.c_str(), &s) == 0;
}

}  // namespace

std::string ReadTextFile(const std::string& path) {
  std::string text;
  FILE* f = fopen(path.c_str(), "rb");
  if (f == nullptr) return text;
  char buffer[16384];
  size_t got;
  while ((got = fread(buffer, 1, sizeof(buffer), f)) > 0) text.append(buffer, got);
  fclose(f);
  return text;
}

bool WriteTextFile(const std::string& path, const std::string& text) {
  const std::string temp = path + ".new";
  FILE* f = fopen(temp.c_str(), "wb");
  if (f == nullptr) return false;
  const bool wrote = fwrite(text.data(), 1, text.size(), f) == text.size();
  const bool closed = fclose(f) == 0;
  if (!wrote || !closed) {
    remove(temp.c_str());
    return false;
  }
  return rename(temp.c_str(), path.c_str()) == 0;
}

GameFolder FindGameFolder(const std::string& app_folder) {
  GameFolder g;
  g.folder = app_folder + "/game";
  g.ini_path = g.folder + "/bwvr.ini";
  if (!Exists(g.folder) && mkdir(g.folder.c_str(), 0770) != 0) {
    AppLog("game folder  : cannot make %s (errno %d)", g.folder.c_str(), errno);
  }
  const std::string exe = g.folder + "/runblack.exe";
  FILE* f = fopen(exe.c_str(), "rb");
  if (f == nullptr) {
    g.message = "Copy your Black & White folder (patch v1.42) to "
                "Android/data/org.bwvr.quest/files/game.";
    AppLog("game folder  : no runblack.exe in %s", g.folder.c_str());
    return g;
  }
  g.has_exe = true;
  bwvr::detail::Sha1 sha;
  bwvr::detail::Sha1Start(&sha);
  std::vector<uint8_t> buffer(64 * 1024);
  uint64_t size = 0;
  size_t got;
  while ((got = fread(buffer.data(), 1, buffer.size(), f)) > 0) {
    bwvr::detail::Sha1Add(&sha, buffer.data(), static_cast<uint32_t>(got));
    size += got;
  }
  fclose(f);
  char hex[41];
  bwvr::detail::Sha1Finish(&sha, hex);
  g.exe_right = size == bwvr::kGameExeSize && strcmp(hex, bwvr::kGameExeSha1) == 0;
  if (g.exe_right) {
    g.message = "The game folder has runblack.exe of the unofficial patch v1.42.";
  } else {
    const char* name = bwvr::KnownBuildName(hex);
    g.message = std::string("This Black & White is ") +
                (name != nullptr ? name : "a build the mod does not know") +
                ". The mod needs the unofficial v1.42 patch.";
  }
  AppLog("game folder  : runblack.exe in %s, %llu bytes, SHA-1 %s (%s)",
         g.folder.c_str(), static_cast<unsigned long long>(size), hex,
         g.exe_right ? "v1.42" : "not v1.42");
  return g;
}
