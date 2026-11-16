// The path of a file in the VirtualStore, a test (save_store_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <string.h>

#include "../../src/mod/save_store_math.h"

namespace save_store_checks {

inline void SaveStoreChecks() {
  using namespace save_store;
  printf("\nThe path of a file in the VirtualStore\n");

  const char* roots[] = {"C:\\Program Files (x86)", "C:\\Program Files",
                         "C:\\ProgramData", "C:\\WINDOWS"};
  const char* store = "C:\\Users\\Buck\\AppData\\Local\\VirtualStore";
  char out[260];

  const bool slot = StorePath(
      "C:\\Program Files (x86)\\Lionhead Studios Ltd\\Black & White\\Profiles"
      "\\_B_u_c_k\\Saved Games\\0\\SaveGame.inf",
      roots, 4, store, out, sizeof(out));
  Check(slot && strcmp(out,
                       "C:\\Users\\Buck\\AppData\\Local\\VirtualStore\\Program "
                       "Files (x86)\\Lionhead Studios Ltd\\Black & White\\"
                       "Profiles\\_B_u_c_k\\Saved Games\\0\\SaveGame.inf") ==
                0,
        "a file of a slot in Program Files (x86) has its copy in the "
        "VirtualStore at the path with no drive", slot ? 1 : 0, 1);

  Check(StorePath("C:\\Program Files\\Game\\a.sav", roots, 4, store, out,
                  sizeof(out)) &&
            strcmp(out, "C:\\Users\\Buck\\AppData\\Local\\VirtualStore\\"
                        "Program Files\\Game\\a.sav") == 0,
        "Program Files is not taken for Program Files (x86)", 1, 1);

  Check(StorePath("c:\\program files (x86)\\x\\y.inf", roots, 4, store, out,
                  sizeof(out)) &&
            strcmp(out, "C:\\Users\\Buck\\AppData\\Local\\VirtualStore\\"
                        "program files (x86)\\x\\y.inf") == 0,
        "capital and small letters are the same for a root", 1, 1);

  Check(!StorePath("C:\\Program Files (x86)Other\\a.sav", roots, 4, store,
                   out, sizeof(out)),
        "a folder whose name only starts with a root is not in it", 0, 0);
  Check(!StorePath("C:\\Program Files (x86)", roots, 4, store, out,
                   sizeof(out)) &&
            !StorePath("C:\\Program Files (x86)\\", roots, 4, store, out,
                       sizeof(out)),
        "a root itself has no copy", 0, 0);
  Check(!StorePath("C:\\Black & White\\Profiles\\a.sav", roots, 4, store, out,
                   sizeof(out)),
        "a game folder out of the roots has no copy", 0, 0);
  Check(!StorePath("D:\\Program Files (x86)\\Game\\a.sav", roots, 4, store,
                   out, sizeof(out)),
        "a folder on a different drive is not in a root of drive C", 0, 0);
  Check(!StorePath("Profiles\\a.sav", roots, 4, store, out, sizeof(out)) &&
            !StorePath("\\\\server\\share\\a.sav", roots, 4, store, out,
                       sizeof(out)),
        "a path with no drive has no copy", 0, 0);

  const char* slash_roots[] = {"C:\\Program Files (x86)\\"};
  Check(StorePath("C:\\Program Files (x86)\\Game\\a.sav", slash_roots, 1,
                  "C:\\Store\\", out, sizeof(out)) &&
            strcmp(out, "C:\\Store\\Program Files (x86)\\Game\\a.sav") == 0,
        "a backslash at the end of a root or of the store has no effect", 1,
        1);

  char small[20];
  Check(!StorePath("C:\\Program Files (x86)\\Game\\a.sav", roots, 4, store,
                   small, sizeof(small)) &&
            small[0] == 0,
        "a path that does not fit gives no copy and an empty text", 0, 0);
  const size_t fit = strlen("C:\\Store\\Program Files (x86)\\G\\a") + 1;
  char exact[64];
  Check(StorePath("C:\\Program Files (x86)\\G\\a", roots, 4, "C:\\Store",
                  exact, fit),
        "a path that just fits with its end zero gives a copy", 1, 1);
  Check(!StorePath("C:\\Program Files (x86)\\G\\a", roots, 4, "C:\\Store",
                   exact, fit - 1),
        "one character less does not fit", 0, 0);

  Check(RetryError(2) && RetryError(3) && RetryError(5),
        "the mod makes a call again after the errors 2, 3 and 5", 1, 1);
  Check(!RetryError(0) && !RetryError(32) && !RetryError(183) &&
            !RetryError(112),
        "a file in use, a file that is there and a full disk are not errors "
        "of the VirtualStore", 0, 0);
}

}  // namespace save_store_checks
