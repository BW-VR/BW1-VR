// THE GAME FOLDER ON THE HEADSET (docs/android.md, "The game folder").
//
// The app does not contain the game. The player copies the folder of the
// game (Black & White with the patches up to the unofficial patch v1.42) to
// the folder `game` in the folder of the app on the shared storage:
// Android/data/<package>/files/game. bwvr.ini is in that folder, beside
// runblack.exe, as on the PC.
#pragma once

#include <string>

struct GameFolder {
  std::string folder;    // the folder of the game
  std::string ini_path;  // bwvr.ini in it
  bool has_exe = false;  // runblack.exe is there
  bool exe_right = false;  // and it is the v1.42 file
  std::string message;   // what the status line says about the folder
};

// Looks at the folder `game` in `app_folder`, and makes it when it is not
// there. Reads runblack.exe one time (about 50 ms for 8.6 MB).
GameFolder FindGameFolder(const std::string& app_folder);

// The text of a file, or empty when it is not there.
std::string ReadTextFile(const std::string& path);
// Writes the text into a temp file and renames it over the file, so a stop
// in the write leaves the old file. False when a step fails.
bool WriteTextFile(const std::string& path, const std::string& text);
