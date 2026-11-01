// The spell book. See spell_book.h and docs/miracles.md, "The spell book".
//
// WHY. In the plain game the player asks for a miracle at a Worship Site
// with a chain of two gestures, or with a tap on its icon. In the headset
// the gestures are hard to learn, and the pictures of the gestures that the
// game shows in a corner of the screen are not seen. The book shows the
// miracles that the player can ask for now over the hand that does not
// point, and the pointing hand takes one with the grip.
//
// THE GAME'S WAY. The list is the game's own. Each Worship Site of the player
// (GPlayer+0xA48, the Citadel, and its 6 sites at +0x34) has a miracle icon
// for each miracle of its towns. An item of the book is a miracle with an
// icon whose magic type is on (vtable slot 0xD4 of the icon). The game's
// test for a request (fn_0064BEC0, thiscall on the GPlayer with the
// GInterfaceStatus and the seed type) says whether the player can ask for it
// now: the Hand is free, the icon does not charge, and the site has prayer
// power. The take sends the request that the end of a chain of gestures
// sends (fn_005CFAE0): packet 0x25 with the seed type (fn_00550C20 on the
// GGame). The handler of the packet (fn_005DABA0) makes the test again and
// lets the best icon charge (fn_0064BDB0). So the game takes the prayer
// power and puts the Miracle in the Hand, as it does for the gestures.
//
// THE DEBUG SWITCH "All miracles" (debug_all_miracles=1, for tests). Each
// town of the player knows each miracle of the game (Town::AddMagicTypesHeld,
// the function of the setup of a land and of a reward), and its Worship Site
// gets the icons (WorshipSite::AddTownSpells). Each Worship Site of the
// player has no limit of prayer power: the flag of the site at +0x108, which
// WorshipSite::GetTotalChantsAvailable reads as 1,000,000 chants. A save of
// the game keeps the miracles that the towns know and the flag of the sites.
// The switch at 0 puts the flags back. The miracles stay.
//
// THE PICTURE shares the quad layer of the ring (kCardRing): the book does
// not open while the ring shows, and the ring closes the book.
//
// THE PULL (spell_book_pull_cm). The grip on an item does not take the
// miracle at once. The bubble of the item comes out of the book on an
// elastic band, on a quad of its own (kCardBubble), and its cell shows an
// empty socket. Both hands buzz, more as the hand goes farther. At the pull
// length the bubble snaps into the hand and the book sends the request. A
// grip that opens before that puts the miracle back: the bubble springs back
// to its cell. So the player can let go of a wrong miracle with no cost.
//
// THE KEYS of bwvr.ini:
//
//   spell_book=1             0: the first face button of the hand that does
//                            not point has no job
//   spell_book_pull_cm=10    the pull that takes a miracle out of the book,
//                            in cm of hand motion, 3 to 30. 0: the grip
//                            takes the miracle at once
//   debug_all_miracles=0     1: each Worship Site of the player has every
//                            miracle of the game, with no limit of prayer
//                            power. For tests
//   spell_book_test_save=0   a test key: the mod saves the first N pictures
//                            of the book as bwvr-book-NNNNN.bmp

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "../vr/xr_screen.h"
#include "audio.h"
#include "book_bubble.h"
#include "controller_hints.h"
#include "controllers.h"
#include "cutscene_window.h"
#include "debug_menu.h"
#include "haptics.h"
#include "radial_menu.h"
#include "spell_book.h"
#include "spell_book_math.h"
#include "temple.h"

extern void ModLog(const char* format, ...);

uint64_t ControllersFrameCount();
void ControllersGrabPulse(int hands, float amplitude, float seconds);
bool ControllersCardPose(int which, float grip_position[3],
                         float grip_orientation[4], float head[3], bool* left);
void EyeCaptureSaveUi(const char* name, uint64_t frame, int style,
                      const void* pixels, int pitch, uint32_t width,
                      uint32_t height);

// A Creature miracle: its chain starts with the inverse spiral.
bool IsCreatureType(int type);

namespace {

using namespace spell_book_math;

// --- the game --------------------------------------------------------------------

const uintptr_t kGame = 0x00D0195C;
// GGame::MyInterface, thiscall, no arguments: the GInterface of this player.
const uintptr_t kMyInterface = 0x00555850;
const int kGameInTemple = 0x205A28;       // 1 while the player is in the Temple
const int kGameHelp = 0x25005C;           // the HelpSystem
const int kHelpCinemaA = 0x45E8;          // both not 0 while a cut scene plays
const int kHelpCinemaB = 0x45EC;
const int kGameGestureList = 0x250064;    // GestureSystemDataList*
// GInterface.
const int kFaceGetPlayer = 0x1C;          // a vtable slot, thiscall
const int kFaceStatus = 0x39C;            // the local GInterfaceStatus
const int kFaceHand = 0x3A0;              // the CHand
// GInterfaceStatus: the seed type of the last miracle (fn_005DCA40), the one
// that the gesture "again" asks for.
const int kStatusLast = 0x34;
// CHand.
const int kHandState = 0x4878;
const int32_t kStateGrain = 8;            // a miracle is in the Hand
// GPlayer.
const int kPlayerCitadel = 0xA48;
const int kCitadelSites = 0x34;           // 6 WorshipSite*
const int kSites = 6;
const int kPlayerTowns = 0xA50;
const int kTownNext = 0x75C;
const int kTownSite = 0x98C;
const int kMaxTowns = 8;
// WorshipSite.
const int kSiteIcons = 0xE0;              // the first WorshipSpellIcon
const int kSiteTurn = 0xF8;               // the chants of this game turn
const int kSiteUsed = 0xFC;               // the chants that this turn used
const int kSiteNoLimit = 0x108;           // not 0: 1,000,000 chants
// WorshipSpellIcon.
const uintptr_t kIconTable = 0x0099D878;
const int kIconOn = 0xD4;                 // a vtable slot: 1 when its magic
                                          // type is on for the player
const int kIconInfo = 0x80;               // GSpellSeedInfo*
const int kIconNext = 0x110;
const int kIconCharging = 0x120;          // a byte
const int kIconStore = 0x134;             // the chants in the icon
const int kMaxIcons = 32;
// GSpellSeedInfo: 30 of 0x190 bytes.
const uintptr_t kSeedInfoTable = 0x00D9D678;
const int kSeedInfoSize = 0x190;
const int kSeedTypes = 30;
const int kInfoGestures = 0x100;          // 3 gesture types of the chain
const int kInfoMagic = 0x124;             // the magic type of the miracle
const int kMagicTypes = 0x2A;
// The gesture templates: the list at +0x08, the count at +0x0C. A template
// is 0x65C bytes: from +0x08 up to 80 corners of 5 floats (x, a pad, y, the
// turn, the direction), on a square of side 1 with y down. The count of the
// corners is the byte at +0x648, and the gesture type the byte at +0x649.
const int kListTemplates = 0x08;
const int kListCount = 0x0C;
const int kTemplateSize = 0x65C;
const int kTemplateCorners = 0x648;
const int kTemplateType = 0x649;
const int kMaxCorners = 80;
const int kGestureTypes = 24;

// fn_0064BEC0, thiscall on the GPlayer (GInterfaceStatus*, seed type), ret 8:
// 1 when a Worship Site of the player has an icon of that seed that
// ValidForRequestSpell lets the player ask for now.
const uintptr_t kCanRequest = 0x0064BEC0;
const uint8_t kCanRequestBytes[8] = {0x83, 0x7C, 0x24, 0x08,
                                     0xFF, 0x53, 0x55, 0x56};
// fn_00550C20, thiscall on the GGame (packet, a, b), ret 0xC: a packet of
// the interface. Packet 0x25 with the seed type is the request of a miracle.
const uintptr_t kSendPacket = 0x00550C20;
const uint8_t kSendPacketBytes[8] = {0x8B, 0x54, 0x24, 0x08,
                                     0x8B, 0x44, 0x24, 0x04};
const int kPacketRequest = 0x25;
// GInterfaceStatus::IsHandReadyForObject, thiscall, no arguments.
const uintptr_t kHandReady = 0x005DC890;
const uint8_t kHandReadyBytes[4] = {0x56, 0x8B, 0xF1, 0xE8};
// Town::AddMagicTypesHeld, thiscall (MAGIC_TYPE), ret 4.
const uintptr_t kTownAddMagic = 0x0073D380;
const uint8_t kTownAddMagicBytes[9] = {0x55, 0x57, 0x8B, 0x7C, 0x24,
                                       0x0C, 0x83, 0xFF, 0x2A};
// WorshipSite::AddTownSpells, thiscall (Town*), ret 4: an icon for each
// miracle of the town that the site does not have yet.
const uintptr_t kSiteAddTownSpells = 0x0077C910;
const uint8_t kSiteAddTownSpellsBytes[9] = {0x53, 0x8B, 0x5C, 0x24, 0x08,
                                            0x56, 0x57, 0x8B, 0xF9};

typedef void*(__fastcall* MyInterfaceFn)(void* game, void* unused);
typedef void*(__fastcall* VirtualPtrFn)(void* self, void* unused);
typedef int(__fastcall* VirtualIntFn)(void* self, void* unused);
typedef int(__fastcall* CanRequestFn)(void* player, void* unused, void* status,
                                      int type);
typedef void(__fastcall* SendPacketFn)(void* game, void* unused, int packet,
                                       int a, int b);
typedef int(__fastcall* IntArgFn)(void* self, void* unused, int value);
typedef void(__fastcall* PtrArgFn)(void* self, void* unused, void* value);

// The names of the seed types (miracle.cpp).
const wchar_t* const kSeedName[kSeedTypes] = {
    L"Storm",              L"Forest",             L"Fireball",
    L"Food",               L"Spiritual Shield",   L"Physical Shield",
    L"Lightning",          L"Heal",               L"Wood",
    L"Water",              L"Flock of birds",     L"Flock of animals",
    L"Creature Freeze",    L"Creature Small",     L"Creature Big",
    L"Creature Weak",      L"Creature Strong",    L"Creature Fat",
    L"Creature Thin",      L"Creature Invisible", L"Creature Compassion",
    L"Creature Angry",     L"Creature Hungry",    L"Creature Frightened",
    L"Creature Tired",     L"Creature Ill",       L"Creature Thirsty",
    L"Creature Itchy",     L"Teleport",           L"Explosion"};

const char* NarrowName(int type) {
  static char buffer[40];
  if (type < 0 || type >= kSeedTypes) return "a miracle";
  size_t n = 0;
  for (const wchar_t* w = kSeedName[type]; *w != 0 && n + 1 < sizeof(buffer);
       ++w) {
    buffer[n++] = static_cast<char>(*w);
  }
  buffer[n] = 0;
  return buffer;
}

// The word on the badge: a Creature miracle without the word "Creature".
const wchar_t* BadgeName(int type) {
  if (type < 0 || type >= kSeedTypes) return L"";
  const wchar_t* name = kSeedName[type];
  if (wcsncmp(name, L"Creature ", 9) == 0) return name + 9;
  return name;
}

void** VTable(void* object) { return *static_cast<void***>(object); }

uintptr_t TableOf(const void* object) {
  return reinterpret_cast<uintptr_t>(*static_cast<void* const*>(object));
}

bool SameBytes(uintptr_t address, const uint8_t* bytes, size_t count) {
  const void* at = reinterpret_cast<const void*>(address);
  return !IsBadReadPtr(at, count) && memcmp(at, bytes, count) == 0;
}

uint8_t* Game() {
  void* const* slot = reinterpret_cast<void* const*>(kGame);
  if (IsBadReadPtr(slot, sizeof(void*))) return nullptr;
  return static_cast<uint8_t*>(*slot);
}

uint8_t* Face() {
  uint8_t* game = Game();
  if (game == nullptr) return nullptr;
  return static_cast<uint8_t*>(
      reinterpret_cast<MyInterfaceFn>(kMyInterface)(game, nullptr));
}

uint8_t* PlayerOf(uint8_t* face) {
  return static_cast<uint8_t*>(reinterpret_cast<VirtualPtrFn>(
      VTable(face)[kFaceGetPlayer / 4])(face, nullptr));
}

template <typename T>
T* At(const uint8_t* base, int offset) {
  return *reinterpret_cast<T* const*>(base + offset);
}

int SeedTypeOf(const uint8_t* icon) {
  const uintptr_t info = reinterpret_cast<uintptr_t>(At<void>(icon, kIconInfo));
  if (info < kSeedInfoTable ||
      info >= kSeedInfoTable + kSeedInfoSize * kSeedTypes ||
      (info - kSeedInfoTable) % kSeedInfoSize != 0) {
    return -1;
  }
  return static_cast<int>((info - kSeedInfoTable) / kSeedInfoSize);
}

const uint8_t* SeedInfo(int type) {
  return reinterpret_cast<const uint8_t*>(kSeedInfoTable +
                                          type * kSeedInfoSize);
}

// The Worship Sites of the player, as fn_0064BEC0 walks them.
int Sites(uint8_t* player, uint8_t* out[kSites]) {
  int count = 0;
  if (player == nullptr || IsBadReadPtr(player + kPlayerCitadel, 4)) return 0;
  uint8_t* citadel = At<uint8_t>(player, kPlayerCitadel);
  if (citadel == nullptr ||
      IsBadReadPtr(citadel + kCitadelSites, 4 * kSites)) {
    return 0;
  }
  for (int i = 0; i < kSites; ++i) {
    uint8_t* site = At<uint8_t>(citadel, kCitadelSites + 4 * i);
    if (site != nullptr && !IsBadReadPtr(site, kSiteNoLimit + 4)) {
      out[count++] = site;
    }
  }
  return count;
}

int Icons(const uint8_t* site, uint8_t* out[kMaxIcons]) {
  int count = 0;
  uint8_t* icon = At<uint8_t>(site, kSiteIcons);
  while (icon != nullptr && count < kMaxIcons &&
         !IsBadReadPtr(icon, kIconStore + 4)) {
    if (TableOf(icon) == kIconTable) out[count++] = icon;
    icon = At<uint8_t>(icon, kIconNext);
  }
  return count;
}

int Towns(uint8_t* player, uint8_t* out[kMaxTowns]) {
  int count = 0;
  if (player == nullptr || IsBadReadPtr(player + kPlayerTowns, 4)) return 0;
  uint8_t* town = At<uint8_t>(player, kPlayerTowns);
  while (town != nullptr && count < kMaxTowns &&
         !IsBadReadPtr(town, kTownNext + 4)) {
    out[count++] = town;
    town = At<uint8_t>(town, kTownNext);
  }
  return count;
}

float SiteChants(const uint8_t* site) {
  if (*reinterpret_cast<const int32_t*>(site + kSiteNoLimit) != 0) {
    return 1000000.0f;
  }
  return *reinterpret_cast<const float*>(site + kSiteTurn) -
         *reinterpret_cast<const float*>(site + kSiteUsed);
}

// --- the settings ----------------------------------------------------------------

bool g_read = false;
bool g_on = true;
bool g_all = false;
int g_save = 0;
int g_saved = 0;
bool g_test_keys = false;
int g_pull_cm = 10;       // 0: the grip takes the miracle at once
int g_pull_ini = 10;      // the value of bwvr.ini, for the debug page
bool g_game_bubble = true;  // the game draws the bubble that the hand pulls out
bool g_game_ok = false;   // the functions of the game have their bytes

bool IniPath(char path[MAX_PATH]) {
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return false;
  char* slash = strrchr(path, '\\');
  if (slash == nullptr) return false;
  slash[1] = 0;
  if (strlen(path) + 9 >= MAX_PATH) return false;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  return true;
}

int IniKey(const char* key, int fallback) {
  char path[MAX_PATH];
  if (!IniPath(path)) return fallback;
  return static_cast<int>(GetPrivateProfileIntA("bwvr", key, fallback, path));
}

void ReadSettings() {
  if (g_read) return;
  g_read = true;
  g_on = IniKey("spell_book", 1) != 0;
  g_all = IniKey("debug_all_miracles", 0) != 0;
  g_save = IniKey("spell_book_test_save", 0);
  g_test_keys = IniKey("test_keys", 0) != 0;
  g_pull_cm = IniKey("spell_book_pull_cm", 10);
  if (g_pull_cm < 0) g_pull_cm = 0;
  if (g_pull_cm > 0 && g_pull_cm < 3) g_pull_cm = 3;
  if (g_pull_cm > 30) g_pull_cm = 30;
  g_pull_ini = g_pull_cm > 0 ? g_pull_cm : 10;
  g_game_bubble = IniKey("spell_book_bubble", 1) != 0;
  g_game_ok =
      SameBytes(kCanRequest, kCanRequestBytes, sizeof(kCanRequestBytes)) &&
      SameBytes(kSendPacket, kSendPacketBytes, sizeof(kSendPacketBytes)) &&
      SameBytes(kHandReady, kHandReadyBytes, sizeof(kHandReadyBytes));
  ModLog("spell book   : spell_book=%d: %s. debug_all_miracles=%d%s",
         g_on ? 1 : 0,
         g_on ? "the first face button of the hand that does not point opens "
                "the book (button_layout=1)"
              : "the first face button of the hand that does not point has "
                "no job",
         g_all ? 1 : 0,
         g_all ? ": each Worship Site of the player gets every miracle of the "
                 "game and no limit of prayer power (a test)"
               : "");
  if (g_pull_cm > 0) {
    ModLog("spell book   : spell_book_pull_cm=%d: the hand pulls a miracle %d "
           "cm out of the book, and then it snaps into the hand", g_pull_cm,
           g_pull_cm);
  } else {
    ModLog("spell book   : spell_book_pull_cm=0: the grip takes a miracle from "
           "the book at once");
  }
  ModLog("spell book   : spell_book_bubble=%d: %s", g_game_bubble ? 1 : 0,
         g_game_bubble ? "the game draws the bubble that the hand pulls out: "
                         "its glass ball with the miracle in it"
                       : "the bubble that the hand pulls out is a flat "
                         "picture");
  if (!g_game_ok) {
    ModLog("spell book   : the functions of the game do not have the bytes "
           "of v1.42, so the book does not open");
  }
}

// --- the frame state -------------------------------------------------------------

struct Item {
  int type;
  bool can;          // the game lets the player ask for it now
  const wchar_t* why;  // why not, for a dim item
  bool last;         // the last miracle, which the gesture "again" asks for
  bool creature;     // its chain starts with the inverse spiral
};

Item g_items[kMaxItems];
int g_count = 0;
const wchar_t* g_empty_why = L"";
bool g_open = false;
int g_hover = -1;             // the item under the pointing hand, this frame
int g_hover_type = -1;
Plane g_plane;
bool g_have_plane = false;
Layout g_layout = LayoutFor(0);
bool g_left = true;           // the book is at the left hand
bool g_button_seen_up = false;
bool g_button_before = false;
GripClaim g_grip = {};
volatile LONG g_grip_claimed = 0;  // the book holds the grip now
volatile LONG g_pluck_asked = 0;  // the pump saw a pluck
int g_pluck_type = -1;            // the type of that pluck
int g_request_type = -1;          // the request for the interface step
uint64_t g_request_frame = 0;
int g_watch_type = -1;            // a request that went out: watch the Hand
uint64_t g_watch_frame = 0;
// After a refusal the card says why for a while.
const double kNoticeSeconds = 4.0;
double g_notice_until = 0.0;
wchar_t g_notice[64] = L"";
int g_test_item = 0;              // the next item of the word "book"

// The pull of a bubble out of the book.
enum PullState { kPullNone, kPullOut, kPullBack };
struct PullNow {
  int state;
  int type;          // the seed type of the item
  float start_gap;   // the gap from the cell to the hand at the grip
  float share;       // how much of the pull is done, 0 to 1
  float most;        // the most share of this pull
  float offset[3];   // at the release: the bubble from its cell, in the axes
                     // of the book (right, up, toward the head)
  double back_at;    // the time of the release
  bool landed;       // the bubble came back to its cell one time
};
PullNow g_pull = {kPullNone, -1, 0.0f, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f}, 0.0,
                  false};
// After the snap the bubble stays at the hand until the Miracle is in the
// Hand, for at most kSnapFrames.
const uint64_t kSnapFrames = 20;
int g_snap_type = -1;
uint64_t g_snap_frame = 0;
// The counts of a report window.
uint64_t g_pulls = 0, g_snaps = 0, g_backs = 0;
uint64_t g_opens = 0, g_closes = 0, g_plucks = 0, g_refusals = 0;
uint64_t g_requests = 0, g_arrived = 0, g_rasters = 0;
double g_raster_ms = 0.0;
int g_logs = 0;
const int kMaxLogs = 400;

bool LogRoom() {
  if (g_logs >= kMaxLogs) return false;
  ++g_logs;
  return true;
}

double NowSeconds() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return static_cast<double>(count.QuadPart) /
         static_cast<double>(frequency.QuadPart);
}

// --- the list of the game --------------------------------------------------------

struct GameNow {
  uint8_t* game;
  uint8_t* face;
  uint8_t* player;
  uint8_t* status;
  bool temple;
  bool cutscene;
  bool hand_ready;
  bool grain;
};

bool ReadGame(GameNow* g) {
  memset(g, 0, sizeof(*g));
  g->game = Game();
  if (g->game == nullptr || !g_game_ok) return false;
  g->face = Face();
  if (g->face == nullptr || IsBadReadPtr(g->face, kFaceHand + 4)) return false;
  g->player = PlayerOf(g->face);
  g->status = At<uint8_t>(g->face, kFaceStatus);
  if (g->player == nullptr || g->status == nullptr) return false;
  g->temple = *reinterpret_cast<const int32_t*>(g->game + kGameInTemple) == 1;
  const uint8_t* help = At<uint8_t>(g->game, kGameHelp);
  g->cutscene =
      help != nullptr && !IsBadReadPtr(help + kHelpCinemaA, 8) &&
      *reinterpret_cast<const int32_t*>(help + kHelpCinemaA) != 0 &&
      *reinterpret_cast<const int32_t*>(help + kHelpCinemaB) != 0;
  g->hand_ready = reinterpret_cast<VirtualIntFn>(kHandReady)(g->status,
                                                             nullptr) != 0;
  const uint8_t* hand = At<uint8_t>(g->face, kFaceHand);
  g->grain = hand != nullptr && !IsBadReadPtr(hand + kHandState, 4) &&
             *reinterpret_cast<const int32_t*>(hand + kHandState) ==
                 kStateGrain;
  return true;
}

bool CanRequest(const GameNow& g, int type) {
  return reinterpret_cast<CanRequestFn>(kCanRequest)(g.player, nullptr,
                                                     g.status, type) == 1;
}

// The list of this frame: each miracle of an icon of a Worship Site of the
// player whose magic type is on, one item for each seed type. The last
// miracle comes first.
void BuildList(const GameNow& g) {
  g_count = 0;
  g_empty_why = L"";
  uint8_t* sites[kSites];
  const int site_count = Sites(g.player, sites);
  if (site_count == 0) {
    g_empty_why = L"You have no Worship Site";
    return;
  }
  bool seen[kSeedTypes] = {};
  // For each type: an icon that charges, and an icon with chants.
  bool charging[kSeedTypes] = {};
  bool power[kSeedTypes] = {};
  int order[kSeedTypes];
  int found = 0;
  for (int s = 0; s < site_count; ++s) {
    uint8_t* icons[kMaxIcons];
    const int count = Icons(sites[s], icons);
    const float chants = SiteChants(sites[s]);
    for (int i = 0; i < count; ++i) {
      const int type = SeedTypeOf(icons[i]);
      if (type < 0) continue;
      if (reinterpret_cast<VirtualIntFn>(VTable(icons[i])[kIconOn / 4])(
              icons[i], nullptr) != 1) {
        continue;
      }
      if (icons[i][kIconCharging] != 0) charging[type] = true;
      if (chants > 0.0f ||
          *reinterpret_cast<const float*>(icons[i] + kIconStore) > 0.0f) {
        power[type] = true;
      }
      if (!seen[type]) {
        seen[type] = true;
        order[found++] = type;
      }
    }
  }
  if (found == 0) {
    g_empty_why = L"Your Worship Site has no miracle";
    return;
  }
  const int last = *reinterpret_cast<const int32_t*>(g.status + kStatusLast);
  // The last miracle first, then the order of the icons.
  for (int pass = 0; pass < 2; ++pass) {
    for (int k = 0; k < found && g_count < kMaxItems; ++k) {
      const int type = order[k];
      if ((pass == 0) != (type == last)) continue;
      Item& item = g_items[g_count++];
      item.type = type;
      item.last = type == last;
      item.creature =
          *reinterpret_cast<const int32_t*>(SeedInfo(type) + kInfoGestures) ==
          2;
      item.can = g.hand_ready && CanRequest(g, type);
      item.why = item.can          ? L""
                 : !g.hand_ready   ? L"The Hand must be empty"
                 : charging[type]  ? L"The miracle charges now"
                 : !power[type]    ? L"No prayer power. Villagers must worship"
                                   : L"The game refuses it now";
    }
  }
}

// --- the gestures ----------------------------------------------------------------

// The corners of the first template of each gesture type, on a square of
// side 1 with y down. Read once, when the list of the game is there.
struct Shape {
  int count;
  float x[kMaxCorners];
  float y[kMaxCorners];
};
Shape g_shapes[kGestureTypes];
bool g_shapes_read = false;

void ReadShapes() {
  if (g_shapes_read) return;
  uint8_t* game = Game();
  if (game == nullptr) return;
  const uint8_t* list = At<uint8_t>(game, kGameGestureList);
  if (list == nullptr || IsBadReadPtr(list, kListCount + 4)) return;
  const int count = *reinterpret_cast<const int32_t*>(list + kListCount);
  const uint8_t* templates = At<uint8_t>(list, kListTemplates);
  if (templates == nullptr || count <= 0 || count > 1000 ||
      IsBadReadPtr(templates, count * kTemplateSize)) {
    return;
  }
  g_shapes_read = true;
  memset(g_shapes, 0, sizeof(g_shapes));
  int read = 0;
  for (int i = 0; i < count; ++i) {
    const uint8_t* t = templates + i * kTemplateSize;
    const int type = t[kTemplateType];
    int corners = t[kTemplateCorners];
    if (type <= 0 || type >= kGestureTypes || g_shapes[type].count > 0) {
      continue;
    }
    if (corners > kMaxCorners) corners = kMaxCorners;
    Shape& s = g_shapes[type];
    for (int c = 0; c < corners; ++c) {
      const float* f = reinterpret_cast<const float*>(t + 8 + c * 20);
      if (!(f[0] >= -0.5f && f[0] <= 1.5f && f[2] >= -0.5f && f[2] <= 1.5f)) {
        s.count = 0;
        break;
      }
      s.x[s.count] = f[0];
      s.y[s.count] = f[2];
      ++s.count;
    }
    if (s.count >= 2) ++read;
    else s.count = 0;
  }
  ModLog("spell book   : the pictures of %d gesture types come from the %d "
         "templates of Data\\Gestures.jty in the game", read, count);
}

// --- the picture -----------------------------------------------------------------

struct Rgb {
  float r, g, b;
};
const Rgb kPanel = {22.0f, 26.0f, 34.0f};
const Rgb kCellLit = {70.0f, 64.0f, 40.0f};
const Rgb kGod = {54.0f, 104.0f, 196.0f};       // a miracle of the god
const Rgb kCreatureFill = {196.0f, 120.0f, 40.0f};
const Rgb kDimFill = {90.0f, 92.0f, 98.0f};
const Rgb kHalo = {255.0f, 206.0f, 84.0f};      // the gold of a target
const Rgb kWhite = {255.0f, 255.0f, 255.0f};
const Rgb kDimInk = {160.0f, 164.0f, 172.0f};
const Rgb kNoteInk = {255.0f, 196.0f, 60.0f};

// What the picture shows. It is drawn again only when this changes.
struct Look {
  int count;
  int type[kMaxItems];
  bool can[kMaxItems];
  bool last[kMaxItems];
  int hover;
  int pulled;        // the item whose bubble is out of the book, or -1
  bool all;
  wchar_t title[48];
  wchar_t note[64];
};

HDC g_dc = nullptr;
HBITMAP g_dib = nullptr;
uint32_t* g_bits = nullptr;
HFONT g_title_font = nullptr;
HFONT g_note_font = nullptr;
HFONT g_name_font[3] = {nullptr, nullptr, nullptr};
HPEN g_pen[3] = {nullptr, nullptr, nullptr};
bool g_raster_failed = false;
uint8_t g_base[kTexture * kTexture * 4];
uint8_t g_staged[kTexture * kTexture * 4];
int g_width_px = 0, g_height_px = 0;
Look g_drawn;
bool g_have_drawn = false;
bool g_staged_new = false;
// The hole of the game's bubble in the page in this frame, and whether the
// staged picture has a hole now.
spell_book_math::Hole g_hole = {};
uint64_t g_hole_frame = 0;
bool g_staged_holed = false;

int CellSize(const Layout& l) {
  return static_cast<int>(l.cell_m * kPixelsPerMeter + 0.5f);
}

// The three sizes of a cell (6.5, 5 and 4 cm) have their own font and pen.
int SizeIndex(const Layout& l) {
  return l.cell_m > 0.06f ? 0 : (l.cell_m > 0.045f ? 1 : 2);
}

bool RasterStart() {
  if (g_dc != nullptr) return true;
  if (g_raster_failed) return false;
  g_dc = CreateCompatibleDC(nullptr);
  BITMAPINFO info;
  memset(&info, 0, sizeof(info));
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = kTexture;
  info.bmiHeader.biHeight = -kTexture;  // top first
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  if (g_dc != nullptr) {
    g_dib = CreateDIBSection(g_dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  }
  const auto font = [](int em, int weight) {
    return CreateFontW(-em, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS,
                       L"Segoe UI");
  };
  g_title_font = font(54, FW_SEMIBOLD);
  g_note_font = font(36, FW_SEMIBOLD);
  const int name_em[3] = {30, 25, 21};
  const int pen_px[3] = {5, 4, 3};
  bool fonts = g_title_font != nullptr && g_note_font != nullptr;
  for (int i = 0; i < 3; ++i) {
    g_name_font[i] = font(name_em[i], FW_BOLD);
    g_pen[i] = CreatePen(PS_SOLID, pen_px[i], RGB(255, 255, 255));
    if (g_name_font[i] == nullptr || g_pen[i] == nullptr) fonts = false;
  }
  if (g_dc == nullptr || g_dib == nullptr || bits == nullptr || !fonts) {
    g_raster_failed = true;
    ModLog("spell book   : GDI gave no picture for the book, so the book does "
           "not show. The card names the miracles");
    return false;
  }
  g_bits = static_cast<uint32_t*>(bits);
  SelectObject(g_dc, g_dib);
  SetBkMode(g_dc, TRANSPARENT);
  SetTextColor(g_dc, RGB(255, 255, 255));
  return true;
}

float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// How much of the pixel at px, py is inside a rect with round corners.
float RoundCover(float px, float py, float x0, float y0, float x1, float y1,
                 float radius) {
  const float cx = 0.5f * (x0 + x1), cy = 0.5f * (y0 + y1);
  const float qx = fabsf(px - cx) - (0.5f * (x1 - x0) - radius);
  const float qy = fabsf(py - cy) - (0.5f * (y1 - y0) - radius);
  const float ox = qx > 0.0f ? qx : 0.0f;
  const float oy = qy > 0.0f ? qy : 0.0f;
  const float inside = (qx > qy ? qx : qy) < 0.0f ? (qx > qy ? qx : qy) : 0.0f;
  const float d = sqrtf(ox * ox + oy * oy) + inside - radius;
  return Clamp01(0.5f - d);
}

// One layer over a pixel of the base: color at alpha, with the color
// multiplied by the alpha in the base.
void Over(uint8_t* out, const Rgb& color, float a) {
  if (a <= 0.0f) return;
  const float keep = 1.0f - a;
  out[0] = static_cast<uint8_t>(color.b * a + out[0] * keep + 0.5f);
  out[1] = static_cast<uint8_t>(color.g * a + out[1] * keep + 0.5f);
  out[2] = static_cast<uint8_t>(color.r * a + out[2] * keep + 0.5f);
  out[3] = static_cast<uint8_t>(255.0f * a + out[3] * keep + 0.5f);
}

// The middle of a cell in pixels of the picture, y down.
void CellPixel(const Layout& l, int i, float* x, float* y) {
  float mx = 0.0f, my = 0.0f;
  ItemCenter(l, i, &mx, &my);
  *x = 0.5f * g_width_px + mx * kPixelsPerMeter;
  *y = 0.5f * g_height_px - my * kPixelsPerMeter;
}

// The bubble of a cell: its middle and its radius in pixels.
void Bubble(const Layout& l, int i, float* x, float* y, float* r) {
  CellPixel(l, i, x, y);
  const float cell = static_cast<float>(CellSize(l));
  *y -= 0.10f * cell;
  *r = 0.33f * cell;
}

// The picture of one gesture type in a box of side `side` at x, y (its
// middle), with the pen of the size.
void DrawShape(int type, float x, float y, float side) {
  if (type <= 0 || type >= kGestureTypes || g_shapes[type].count < 2) return;
  const Shape& s = g_shapes[type];
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  for (int i = 0; i < s.count; ++i) {
    if (s.x[i] < x0) x0 = s.x[i];
    if (s.x[i] > x1) x1 = s.x[i];
    if (s.y[i] < y0) y0 = s.y[i];
    if (s.y[i] > y1) y1 = s.y[i];
  }
  const float span = (x1 - x0) > (y1 - y0) ? (x1 - x0) : (y1 - y0);
  if (!(span > 1e-4f)) return;
  const float scale = side / span;
  POINT points[kMaxCorners];
  for (int i = 0; i < s.count; ++i) {
    points[i].x = static_cast<LONG>(x + (s.x[i] - 0.5f * (x0 + x1)) * scale);
    points[i].y = static_cast<LONG>(y + (s.y[i] - 0.5f * (y0 + y1)) * scale);
  }
  Polyline(g_dc, points, s.count);
  // A dot marks the start of the gesture.
  Ellipse(g_dc, points[0].x - 5, points[0].y - 5, points[0].x + 5,
          points[0].y + 5);
}

// The two gestures of the chain of a miracle in its bubble at x, y with the
// radius r, in pixels.
void DrawBubbleShapes(int type, float x, float y, float r) {
  const uint8_t* info = SeedInfo(type);
  const int first = *reinterpret_cast<const int32_t*>(info + kInfoGestures);
  const int second = *reinterpret_cast<const int32_t*>(info + kInfoGestures + 4);
  DrawShape(first, x - 0.42f * r, y, 0.55f * r);
  DrawShape(second, x + 0.30f * r, y, 0.95f * r);
}

bool SameLook(const Look& a, const Look& b) {
  return memcmp(&a, &b, sizeof(Look)) == 0;
}

void Raster(const Look& look) {
  if (!RasterStart()) return;
  const double start = NowSeconds();
  const Layout l = LayoutFor(look.count);
  g_width_px = PixelsOf(l.width_m);
  g_height_px = PixelsOf(l.height_m);
  const int w = g_width_px, h = g_height_px;
  // The ink first, white on black: the green of a pixel is its cover.
  for (int y = 0; y < h; ++y) {
    memset(g_bits + static_cast<size_t>(y) * kTexture, 0,
           static_cast<size_t>(w) * 4);
  }
  // The band at the top: the title, or the note in its place. A book with
  // no item has the title in its upper half and the note in its lower half.
  const int title_px = static_cast<int>(kTitleM * kPixelsPerMeter);
  const bool note_in_band = look.count > 0 && look.note[0] != 0;
  {
    RECT band = {16, 2, w - 16, look.count == 0 ? h / 2 + 4 : title_px};
    SelectObject(g_dc, note_in_band ? g_note_font : g_title_font);
    DrawTextW(g_dc, note_in_band ? look.note : look.title, -1, &band,
              DT_SINGLELINE | DT_NOPREFIX | DT_CENTER | DT_VCENTER |
                  DT_END_ELLIPSIS);
  }
  if (look.count == 0 && look.note[0] != 0) {
    RECT note = {16, h / 2, w - 16, h - 6};
    SelectObject(g_dc, g_note_font);
    DrawTextW(g_dc, look.note, -1, &note,
              DT_SINGLELINE | DT_NOPREFIX | DT_CENTER | DT_VCENTER |
                  DT_END_ELLIPSIS);
  }
  const int size = SizeIndex(l);
  const float cell = static_cast<float>(CellSize(l));
  SelectObject(g_dc, g_pen[size]);
  SelectObject(g_dc, GetStockObject(WHITE_BRUSH));
  for (int i = 0; i < look.count; ++i) {
    float bx = 0.0f, by = 0.0f, br = 0.0f;
    Bubble(l, i, &bx, &by, &br);
    // The two gestures of the chain in the bubble: the start (the spiral or
    // the inverse spiral) small at the left, and the gesture of the miracle
    // larger at the right. A bubble that the hand pulls out has no gestures
    // in the book: they go with the bubble.
    if (i != look.pulled) DrawBubbleShapes(look.type[i], bx, by, br);
    float cx = 0.0f, cy = 0.0f;
    CellPixel(l, i, &cx, &cy);
    RECT name = {static_cast<LONG>(cx - 0.5f * cell + 4),
                 static_cast<LONG>(by + br + 2),
                 static_cast<LONG>(cx + 0.5f * cell - 4),
                 static_cast<LONG>(cy + 0.5f * cell - 2)};
    SelectObject(g_dc, g_name_font[size]);
    DrawTextW(g_dc, BadgeName(look.type[i]), -1, &name,
              DT_SINGLELINE | DT_NOPREFIX | DT_CENTER | DT_VCENTER |
                  DT_END_ELLIPSIS);
  }
  GdiFlush();

  // The shapes, then the ink over them.
  for (int y = 0; y < h; ++y) {
    memset(g_base + static_cast<size_t>(y) * kTexture * 4, 0,
           static_cast<size_t>(w) * 4);
  }
  for (int y = 0; y < h; ++y) {
    const float py = static_cast<float>(y) + 0.5f;
    uint8_t* row = g_base + static_cast<size_t>(y) * kTexture * 4;
    for (int x = 0; x < w; ++x) {
      const float px = static_cast<float>(x) + 0.5f;
      Over(row + x * 4, kPanel,
           0.86f * RoundCover(px, py, 0.0f, 0.0f, static_cast<float>(w),
                              static_cast<float>(h), 28.0f));
    }
  }
  for (int i = 0; i < look.count; ++i) {
    float bx = 0.0f, by = 0.0f, br = 0.0f;
    Bubble(l, i, &bx, &by, &br);
    float cx = 0.0f, cy = 0.0f;
    CellPixel(l, i, &cx, &cy);
    const bool lit = i == look.hover;
    const Rgb& fill = !look.can[i] ? kDimFill
                      : IsCreatureType(look.type[i]) ? kCreatureFill
                                                     : kGod;
    const float halo = br + (lit ? 0.16f : 0.06f) * cell;
    const int x0 = static_cast<int>(cx - 0.5f * cell);
    const int y0 = static_cast<int>(cy - 0.5f * cell);
    const int x1 = static_cast<int>(cx + 0.5f * cell);
    const int y1 = static_cast<int>(cy + 0.5f * cell);
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < h; ++y) {
      const float py = static_cast<float>(y) + 0.5f;
      uint8_t* row = g_base + static_cast<size_t>(y) * kTexture * 4;
      for (int x = x0 < 0 ? 0 : x0; x < x1 && x < w; ++x) {
        const float px = static_cast<float>(x) + 0.5f;
        uint8_t* out = row + x * 4;
        if (lit) {
          Over(out, kCellLit,
               0.9f * RoundCover(px, py, x0 + 3.0f, y0 + 3.0f, x1 - 3.0f,
                                 y1 - 3.0f, 0.12f * cell));
        }
        const float d = sqrtf((px - bx) * (px - bx) + (py - by) * (py - by));
        // The halo: a ring of gold round the bubble of the item under the
        // hand, and a thin ring of light round each other bubble.
        const float ring = Clamp01(halo + 0.5f - d) * Clamp01(d - br + 0.5f);
        if (i == look.pulled) {
          // The empty socket of a bubble that the hand pulls out.
          const float edge = Clamp01(br + 0.5f - d) * Clamp01(d - br + 3.5f);
          Over(out, kWhite, 0.30f * edge);
          continue;
        }
        if (lit) {
          Over(out, kHalo, ring);
        } else {
          Over(out, kWhite, 0.25f * ring);
        }
        Over(out, fill, (look.can[i] ? 0.95f : 0.6f) * Clamp01(br + 0.5f - d));
      }
    }
  }
  for (int y = 0; y < h; ++y) {
    uint8_t* row = g_base + static_cast<size_t>(y) * kTexture * 4;
    const uint32_t* letters = g_bits + static_cast<size_t>(y) * kTexture;
    for (int x = 0; x < w; ++x) {
      const float cover =
          static_cast<float>((letters[x] >> 8) & 0xFF) * (1.0f / 255.0f);
      if (cover <= 0.0f) continue;
      const Rgb* ink = &kWhite;
      if (look.count == 0) {
        if (y >= h / 2) ink = &kNoteInk;
      } else if (y < title_px) {
        if (note_in_band) ink = &kNoteInk;
      } else {
        // The letters of a cell: dim for an item that the game refuses now.
        const float mx = (x - 0.5f * w) / kPixelsPerMeter;
        const float my = (0.5f * h - y) / kPixelsPerMeter;
        float best = 1e9f;
        int cell_of = -1;
        for (int i = 0; i < look.count; ++i) {
          float ix = 0.0f, iy = 0.0f;
          ItemCenter(l, i, &ix, &iy);
          const float dd = (mx - ix) * (mx - ix) + (my - iy) * (my - iy);
          if (dd < best) {
            best = dd;
            cell_of = i;
          }
        }
        if (cell_of >= 0 && !look.can[cell_of]) ink = &kDimInk;
      }
      Over(row + x * 4, *ink, cover);
    }
  }
  g_drawn = look;
  g_have_drawn = true;
  g_staged_new = true;
  ++g_rasters;
  const double ms = (NowSeconds() - start) * 1000.0;
  if (ms > g_raster_ms) g_raster_ms = ms;
  if (g_saved < g_save) {
    ++g_saved;
    EyeCaptureSaveUi("book", ControllersFrameCount(), 1, g_base, kTexture * 4,
                     static_cast<uint32_t>(w), static_cast<uint32_t>(h));
  }
}

// THE BUBBLE that the hand pulls out of the book: its own small picture, with
// the gold halo, the fill of its kind and its two gestures, on a clear
// background. It is drawn one time at the start of each pull.
const int kBubbleTexture = 256;
uint8_t g_bubble[kBubbleTexture * kBubbleTexture * 4];
int g_bubble_px = 0;

bool RasterBubble(int type, const Layout& l) {
  if (!RasterStart() || type < 0 || type >= kSeedTypes || l.count <= 0) {
    return false;
  }
  const int size = SizeIndex(l);
  const float cell = static_cast<float>(CellSize(l));
  const float br = 0.33f * cell;
  const float halo = br + 0.16f * cell;
  int side = static_cast<int>(2.0f * halo + 6.0f);
  side += side & 1;
  if (side > kBubbleTexture) side = kBubbleTexture;
  const float c = 0.5f * static_cast<float>(side);
  for (int y = 0; y < side; ++y) {
    memset(g_bits + static_cast<size_t>(y) * kTexture, 0,
           static_cast<size_t>(side) * 4);
  }
  SelectObject(g_dc, g_pen[size]);
  SelectObject(g_dc, GetStockObject(WHITE_BRUSH));
  DrawBubbleShapes(type, c, c, br);
  GdiFlush();
  const Rgb& fill = IsCreatureType(type) ? kCreatureFill : kGod;
  memset(g_bubble, 0, sizeof(g_bubble));
  for (int y = 0; y < side; ++y) {
    const float py = static_cast<float>(y) + 0.5f;
    uint8_t* row = g_bubble + static_cast<size_t>(y) * kBubbleTexture * 4;
    const uint32_t* letters = g_bits + static_cast<size_t>(y) * kTexture;
    for (int x = 0; x < side; ++x) {
      const float px = static_cast<float>(x) + 0.5f;
      uint8_t* out = row + x * 4;
      const float d = sqrtf((px - c) * (px - c) + (py - c) * (py - c));
      Over(out, kHalo, Clamp01(halo + 0.5f - d) * Clamp01(d - br + 0.5f));
      Over(out, fill, 0.95f * Clamp01(br + 0.5f - d));
      const float cover =
          static_cast<float>((letters[x] >> 8) & 0xFF) * (1.0f / 255.0f);
      Over(out, kWhite, cover);
    }
  }
  g_bubble_px = side;
  // The GDI ink of the book is gone, so the next picture of the book draws
  // all of it again.
  g_have_drawn = false;
  if (g_saved < g_save) {
    ++g_saved;
    EyeCaptureSaveUi("bubble", ControllersFrameCount(), 1, g_bubble,
                     kBubbleTexture * 4, static_cast<uint32_t>(side),
                     static_cast<uint32_t>(side));
  }
  return true;
}

// The bubble card of this frame at the point `at`, facing the head.
void StageBubble(const float at[3], const float head[3]) {
  if (g_bubble_px <= 0 || bwvr::XrScreen::CardFailed(bwvr::kCardBubble)) {
    return;
  }
  bwvr::CardLayer layer = {};
  layer.visible = true;
  layer.pixels = g_bubble;
  layer.texture_width = kBubbleTexture;
  layer.texture_height = kBubbleTexture;
  layer.rect[0] = 0;
  layer.rect[1] = 0;
  layer.rect[2] = g_bubble_px;
  layer.rect[3] = g_bubble_px;
  for (int i = 0; i < 3; ++i) layer.position[i] = at[i];
  controller_math::FacePose(at, head, layer.orientation);
  layer.width = g_bubble_px / kPixelsPerMeter;
  layer.height = layer.width;
  bwvr::XrScreen::StageCard(bwvr::kCardBubble, layer);
}

void BuildLook(Look* look) {
  memset(look, 0, sizeof(*look));
  look->count = g_count;
  look->hover = g_hover;
  look->pulled = -1;
  if (g_pull.state != kPullNone) {
    for (int i = 0; i < g_count; ++i) {
      if (g_items[i].type == g_pull.type) look->pulled = i;
    }
  }
  look->all = g_all;
  for (int i = 0; i < g_count; ++i) {
    look->type[i] = g_items[i].type;
    look->can[i] = g_items[i].can;
    look->last[i] = g_items[i].last;
  }
  if (g_count == 0) {
    wcscpy_s(look->title, L"No miracle to ask for now");
    wcsncpy_s(look->note, g_empty_why, _TRUNCATE);
    return;
  }
  if (g_hover >= 0 && g_hover < g_count) {
    const Item& item = g_items[g_hover];
    swprintf_s(look->title, L"%s%s", kSeedName[item.type],
               item.last ? L" (again)" : L"");
    if (!item.can) wcsncpy_s(look->note, item.why, _TRUNCATE);
  } else {
    wcscpy_s(look->title, g_all ? L"Miracles: all (test)" : L"Miracles");
  }
  if (g_notice_until > NowSeconds() && look->note[0] == 0) {
    wcsncpy_s(look->note, g_notice, _TRUNCATE);
  }
}

// --- the debug switch ------------------------------------------------------------

// The flag of no limit that each site had before the switch set it.
struct SiteFlag {
  uint8_t* site;
  int32_t before;
};
SiteFlag g_flags[kSites * 2];
int g_flag_count = 0;
uint64_t g_all_next = 0;
bool g_all_said_no_site = false;

// The magic types of the 30 miracles of the game.
int MiracleMagicTypes(int out[kSeedTypes]) {
  int count = 0;
  for (int type = 0; type < kSeedTypes; ++type) {
    const int magic =
        *reinterpret_cast<const int32_t*>(SeedInfo(type) + kInfoMagic);
    if (magic < 0 || magic >= kMagicTypes) continue;
    bool seen = false;
    for (int k = 0; k < count; ++k) seen = seen || out[k] == magic;
    if (!seen) out[count++] = magic;
  }
  return count;
}

// Once a second while the switch is on: each town knows each miracle, its
// Worship Site has the icons, and each Worship Site of the player has no
// limit of prayer power. Each call of the game does nothing a second time.
void ApplyAll(const GameNow& g, uint64_t frame) {
  if (frame < g_all_next) return;
  g_all_next = frame + 60;
  if (!SameBytes(kTownAddMagic, kTownAddMagicBytes,
                 sizeof(kTownAddMagicBytes)) ||
      !SameBytes(kSiteAddTownSpells, kSiteAddTownSpellsBytes,
                 sizeof(kSiteAddTownSpellsBytes))) {
    return;
  }
  int magic[kSeedTypes];
  const int magic_count = MiracleMagicTypes(magic);
  uint8_t* towns[kMaxTowns];
  const int town_count = Towns(g.player, towns);
  for (int t = 0; t < town_count; ++t) {
    int added = 0;
    for (int k = 0; k < magic_count; ++k) {
      if (reinterpret_cast<IntArgFn>(kTownAddMagic)(towns[t], nullptr,
                                                    magic[k]) != 0) {
        ++added;
      }
    }
    uint8_t* site = At<uint8_t>(towns[t], kTownSite);
    int icons_before = 0, icons_after = 0;
    if (site != nullptr && !IsBadReadPtr(site, kSiteNoLimit + 4)) {
      uint8_t* icons[kMaxIcons];
      icons_before = Icons(site, icons);
      reinterpret_cast<PtrArgFn>(kSiteAddTownSpells)(site, nullptr, towns[t]);
      icons_after = Icons(site, icons);
    }
    if ((added > 0 || icons_after != icons_before) && LogRoom()) {
      ModLog("spell book   : debug_all_miracles=1: the town %p now knows %d "
             "more of the %d magic types of the miracles "
             "(Town::AddMagicTypesHeld), and its Worship Site %p has %d "
             "miracle icons (it had %d), frame %llu", towns[t], added,
             magic_count, site, icons_after, icons_before, frame);
    }
  }
  uint8_t* sites[kSites];
  const int site_count = Sites(g.player, sites);
  if (site_count == 0 && !g_all_said_no_site) {
    g_all_said_no_site = true;
    ModLog("spell book   : debug_all_miracles=1: the player has no Worship "
           "Site, so no miracle can come from the book, frame %llu", frame);
  }
  for (int s = 0; s < site_count; ++s) {
    int32_t* flag = reinterpret_cast<int32_t*>(sites[s] + kSiteNoLimit);
    if (*flag != 0) continue;
    bool known = false;
    for (int k = 0; k < g_flag_count; ++k) known = known || g_flags[k].site == sites[s];
    if (!known && g_flag_count < kSites * 2) {
      g_flags[g_flag_count].site = sites[s];
      g_flags[g_flag_count].before = *flag;
      ++g_flag_count;
    }
    *flag = 1;
    ModLog("spell book   : debug_all_miracles=1: the Worship Site %p has no "
           "limit of prayer power now (+0x108), frame %llu", sites[s], frame);
  }
}

// The switch goes to 0: each site gets its flag back.
void UndoAll(uint64_t frame) {
  GameNow g;
  if (!ReadGame(&g)) {
    g_flag_count = 0;
    return;
  }
  uint8_t* sites[kSites];
  const int site_count = Sites(g.player, sites);
  for (int k = 0; k < g_flag_count; ++k) {
    for (int s = 0; s < site_count; ++s) {
      if (sites[s] != g_flags[k].site) continue;
      *reinterpret_cast<int32_t*>(sites[s] + kSiteNoLimit) = g_flags[k].before;
      ModLog("spell book   : debug_all_miracles=0: the Worship Site %p has its "
             "limit of prayer power again. The miracles that the towns know "
             "stay, frame %llu", sites[s], frame);
    }
  }
  g_flag_count = 0;
}

// --- the frame -------------------------------------------------------------------

const char* const kCloseWords[] = {
    "", "", "", "the button", "the ring opened", "a dialog box or the game "
    "menu is up", "the pointing hand took a miracle", "the book cannot stay "
    "now"};

void SetNotice(const wchar_t* text) {
  wcsncpy_s(g_notice, text, _TRUNCATE);
  g_notice_until = NowSeconds() + kNoticeSeconds;
}

void Pluck(const GameNow& g, uint64_t frame, bool* plucked) {
  const int type = g_pluck_type;
  int index = -1;
  for (int i = 0; i < g_count; ++i) {
    if (g_items[i].type == type) index = i;
  }
  if (index < 0) return;
  const Item& item = g_items[index];
  if (item.can && g.hand_ready && CanRequest(g, type)) {
    g_request_type = type;
    g_request_frame = frame;
    ++g_plucks;
    *plucked = true;
    HapticsPulseHands(2, haptics_math::kActivate);
    AudioCue(kAudioCueRingChoose, g_left ? 0.4f : -0.4f);
    if (LogRoom()) {
      ModLog("spell book   : the pointing hand takes %s%s from the book, "
             "frame %llu", NarrowName(type), item.last ? " (the last miracle)"
                                                       : "", frame);
    }
    return;
  }
  ++g_refusals;
  HapticsPulseHands(2, haptics_math::kRefused);
  SetNotice(item.why[0] != 0 ? item.why : L"The game refuses it now");
  if (LogRoom()) {
    char why[64];
    size_t n = 0;
    for (const wchar_t* w = g_notice; *w != 0 && n + 1 < sizeof(why); ++w) {
      why[n++] = static_cast<char>(*w);
    }
    why[n] = 0;
    ModLog("spell book   : the pointing hand takes %s from the book, and the "
           "game refuses it: %s, frame %llu", NarrowName(type), why, frame);
  }
}

// After a request: the Miracle comes into the Hand (the Grain state), or the
// icon charges first. One line when it comes, or when it did not come in 10 s.
void Watch(const GameNow& g, uint64_t frame) {
  if (g_watch_type < 0) return;
  if (g.grain) {
    ++g_arrived;
    if (LogRoom()) {
      ModLog("spell book   : the Miracle %s is in the Hand, %llu frames after "
             "the request, frame %llu", NarrowName(g_watch_type),
             frame - g_watch_frame, frame);
    }
    g_watch_type = -1;
    return;
  }
  if (frame - g_watch_frame > 600) {
    if (LogRoom()) {
      ModLog("spell book   : the Miracle %s did not come into the Hand in 600 "
             "frames after the request, frame %llu", NarrowName(g_watch_type),
             frame);
    }
    g_watch_type = -1;
  }
}

// --- the pull --------------------------------------------------------------------

float Dot3(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

int ItemOfType(int type) {
  for (int i = 0; i < g_count; ++i) {
    if (g_items[i].type == type) return i;
  }
  return -1;
}

// The middle of the bubble of item i in its cell, in the room.
void CellPoint(int i, float out[3]) {
  float x = 0.0f, y = 0.0f;
  ItemCenter(g_layout, i, &x, &y);
  PointOn(g_plane, x, y, out);
}

// The buzz of the pull on both hands: the pointing hand at the strength, and
// the hand of the book at half of it. Each frame of the pull sends one, so
// the buzz does not stop while the hand pulls.
void PullBuzzNow(float share) {
  const float strength = PullBuzz(share);
  const float pct = static_cast<float>(HapticsPct()) * 0.01f;
  float amplitude = (0.03f + 0.42f * strength) * pct;
  if (amplitude > haptics_math::kMostAmplitude) {
    amplitude = haptics_math::kMostAmplitude;
  }
  if (!(amplitude > 0.0f)) return;
  ControllersGrabPulse(2, amplitude, 0.020f);
  ControllersGrabPulse(1, 0.5f * amplitude, 0.020f);
}

void StopPull() {
  g_pull.state = kPullNone;
  g_pull.type = -1;
}

// The grip closes on an item with the pull on. The miracle stays in the book
// until the hand pulls it out. A dim item is refused at once, as without the
// pull. False when the pull cannot start: then the grip takes at once.
bool StartPull(const GameNow& g, uint64_t frame) {
  const int index = ItemOfType(g_pluck_type);
  if (index < 0 || !g_have_plane || !g_items[index].can || !g.hand_ready) {
    return false;
  }
  float hand[3], turn[4], head[3];
  bool left = false;
  if (!ControllersCardPose(0, hand, turn, head, &left)) return false;
  if (!RasterBubble(g_items[index].type, g_layout)) return false;
  float cell[3];
  CellPoint(index, cell);
  g_pull.state = kPullOut;
  g_pull.type = g_items[index].type;
  g_pull.start_gap = controller_math::Distance(cell, hand);
  g_pull.share = 0.0f;
  g_pull.most = 0.0f;
  g_pull.landed = false;
  ++g_pulls;
  HapticsPulseHands(2, haptics_math::kTake);
  if (LogRoom()) {
    ModLog("spell book   : the pointing hand grips %s and pulls it out of the "
           "book: the hand is %.1f cm from its cell, and %d cm more takes it, "
           "frame %llu", NarrowName(g_pull.type), g_pull.start_gap * 100.0f,
           g_pull_cm, frame);
  }
  return true;
}

// One frame of a pull that runs: the snap, the release, or more buzz.
void PullStep(const GameNow& g, uint64_t frame, bool* plucked) {
  const int index = ItemOfType(g_pull.type);
  float hand[3], turn[4], head[3];
  bool left = false;
  if (index < 0 || !ControllersCardPose(0, hand, turn, head, &left)) {
    if (LogRoom()) {
      ModLog("spell book   : the pull of %s stops: %s, frame %llu",
             NarrowName(g_pull.type),
             index < 0 ? "the miracle left the book" : "the hand has no pose",
             frame);
    }
    StopPull();
    return;
  }
  float cell[3];
  CellPoint(index, cell);
  const float gap = controller_math::Distance(cell, hand);
  g_pull.share = PullShare(gap, g_pull.start_gap, g_pull_cm * 0.01f);
  if (g_pull.share > g_pull.most) g_pull.most = g_pull.share;
  if (InterlockedCompareExchange(&g_grip_claimed, 0, 0) == 0) {
    // The grip opened before the snap: the bubble springs back.
    float bubble[3];
    PullBubble(cell, hand, bubble);
    float d[3];
    for (int k = 0; k < 3; ++k) d[k] = bubble[k] - cell[k];
    g_pull.offset[0] = Dot3(d, g_plane.right);
    g_pull.offset[1] = Dot3(d, g_plane.up);
    g_pull.offset[2] = Dot3(d, g_plane.normal);
    g_pull.state = kPullBack;
    g_pull.back_at = NowSeconds();
    ++g_backs;
    HapticsPulseHands(2, haptics_math::kLetGo);
    if (LogRoom()) {
      ModLog("spell book   : the grip opens at %.0f percent of the pull (the "
             "most was %.0f percent), so %s springs back into the book, frame "
             "%llu", g_pull.share * 100.0f, g_pull.most * 100.0f,
             NarrowName(g_pull.type), frame);
    }
    return;
  }
  if (g_pull.share >= 1.0f) {
    const int type = g_pull.type;
    g_pluck_type = type;
    Pluck(g, frame, plucked);
    if (*plucked) {
      ++g_snaps;
      const float pct = static_cast<float>(HapticsPct()) * 0.01f;
      float amplitude = 0.65f * pct;
      if (amplitude > haptics_math::kMostAmplitude) {
        amplitude = haptics_math::kMostAmplitude;
      }
      if (amplitude > 0.0f) {
        ControllersGrabPulse(2, amplitude, 0.035f);
        ControllersGrabPulse(1, 0.4f * amplitude, 0.025f);
      }
      g_snap_type = type;
      g_snap_frame = frame;
      StopPull();
      if (LogRoom()) {
        ModLog("spell book   : %s snaps into the hand: the hand is %.1f cm "
               "from its cell, frame %llu", NarrowName(type), gap * 100.0f,
               frame);
      }
      return;
    }
    // The game refuses it now: the bubble goes back to the book.
    float d[3];
    for (int k = 0; k < 3; ++k) d[k] = (hand[k] - cell[k]) * kPullFollow;
    g_pull.offset[0] = Dot3(d, g_plane.right);
    g_pull.offset[1] = Dot3(d, g_plane.up);
    g_pull.offset[2] = Dot3(d, g_plane.normal);
    g_pull.state = kPullBack;
    g_pull.back_at = NowSeconds();
    ++g_backs;
    return;
  }
  PullBuzzNow(g_pull.share);
}

// The bubble of this frame at the point `at` of the room: the game's bubble
// in the world and its hole in the page, or the flat picture.
void PlaceBubble(int type, const float at[3], const float head[3],
                 uint64_t frame) {
  float world[3], units = 0.0f;
  if (g_game_bubble && !BookBubbleFailed() &&
      ControllersLocalToWorld(at, world, &units)) {
    BookBubbleShow(type, world, units, frame);
    if (g_have_plane && BubbleHole(g_plane, head, at, kBubbleRadiusM,
                                   &g_hole)) {
      g_hole_frame = frame;
    }
    return;
  }
  StageBubble(at, head);
}

// The book picture of this frame, with the hole of the game's bubble when it
// is in front of the page. The hole is clear in its middle and soft at its
// edge.
void StagePicture(uint64_t frame) {
  const bool holed = g_hole_frame == frame && g_hole_frame != 0;
  if (g_staged_new || holed || g_staged_holed) {
    g_staged_new = false;
    for (int y = 0; y < g_height_px; ++y) {
      memcpy(g_staged + static_cast<size_t>(y) * kTexture * 4,
             g_base + static_cast<size_t>(y) * kTexture * 4,
             static_cast<size_t>(g_width_px) * 4);
    }
  }
  g_staged_holed = holed;
  if (!holed) return;
  const float cx = 0.5f * g_width_px + g_hole.x * kPixelsPerMeter;
  const float cy = 0.5f * g_height_px - g_hole.y * kPixelsPerMeter;
  const float r = g_hole.r * kPixelsPerMeter;
  const float edge = 3.0f;
  const int x0 = static_cast<int>(floorf(cx - r - edge));
  const int x1 = static_cast<int>(ceilf(cx + r + edge));
  const int y0 = static_cast<int>(floorf(cy - r - edge));
  const int y1 = static_cast<int>(ceilf(cy + r + edge));
  for (int y = y0 < 0 ? 0 : y0; y < y1 && y < g_height_px; ++y) {
    const float py = static_cast<float>(y) + 0.5f;
    uint8_t* row = g_staged + static_cast<size_t>(y) * kTexture * 4;
    for (int x = x0 < 0 ? 0 : x0; x < x1 && x < g_width_px; ++x) {
      const float px = static_cast<float>(x) + 0.5f;
      const float d = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));
      const float keep = Clamp01((d - r) / edge + 0.5f);
      if (keep >= 1.0f) continue;
      uint8_t* out = row + x * 4;
      for (int k = 0; k < 4; ++k) {
        out[k] = static_cast<uint8_t>(out[k] * keep + 0.5f);
      }
    }
  }
}

// The bubble of this frame: out with the hand, on its way back, or in the
// hand after the snap.
void ShowBubble(const GameNow* g, uint64_t frame) {
  float hand[3], turn[4], head[3];
  bool left = false;
  if (!ControllersCardPose(0, hand, turn, head, &left)) return;
  if (g_snap_type >= 0) {
    if ((g != nullptr && g->grain) || frame - g_snap_frame > kSnapFrames) {
      g_snap_type = -1;
      return;
    }
    PlaceBubble(g_snap_type, hand, head, frame);
    return;
  }
  if (g_pull.state == kPullNone || !g_have_plane) return;
  const int index = ItemOfType(g_pull.type);
  if (index < 0) {
    StopPull();
    return;
  }
  float cell[3];
  CellPoint(index, cell);
  float at[3];
  if (g_pull.state == kPullOut) {
    PullBubble(cell, hand, at);
  } else {
    const float t = static_cast<float>(NowSeconds() - g_pull.back_at);
    if (t >= kBackSeconds) {
      StopPull();
      return;
    }
    if (!g_pull.landed && t >= BackLandSeconds()) {
      // The bubble is back in its cell: a light tick on the hand of the
      // book.
      g_pull.landed = true;
      HapticsPulseHands(1, haptics_math::kTick);
    }
    const float scale = BackScale(t);
    for (int k = 0; k < 3; ++k) {
      at[k] = cell[k] + (g_plane.right[k] * g_pull.offset[0] +
                         g_plane.up[k] * g_pull.offset[1] +
                         g_plane.normal[k] * g_pull.offset[2]) *
                            scale;
    }
  }
  PlaceBubble(g_pull.type, at, head, frame);
}

}  // namespace

bool IsCreatureType(int type) {
  if (type < 0 || type >= kSeedTypes) return false;
  return *reinterpret_cast<const int32_t*>(SeedInfo(type) + kInfoGestures) ==
         2;
}

bool SpellBookButtonOn() {
  ReadSettings();
  return g_on && g_game_ok && ControllersButtonLayout() != 0;
}

bool SpellBookOpen() { return g_open; }

bool SpellBookCanOpen() {
  ReadSettings();
  if (!g_on || g_open || RadialMenuShows()) return false;
  RingHand hand;
  return ControllersRingHand(&hand) && hand.active && hand.usable &&
         hand.book_face && !hand.dialog;
}

void SpellBookFrame() {
  ReadSettings();
  const uint64_t frame = ControllersFrameCount();
  GameNow g;
  const bool have_game = ReadGame(&g);
  if (have_game && g_all) ApplyAll(g, frame);
  if (have_game) Watch(g, frame);
  if (!g_on || !g_game_ok) {
    g_open = false;
    return;
  }
  RingHand hand;
  const bool have_hand = ControllersRingHand(&hand);
  // The first face button of the hand that does not point. A button that is
  // down when the hand wakes is not a press.
  bool press = false;
  if (have_hand && hand.active && hand.book_face) {
    const bool raw = hand.book_button;
    if (!raw) g_button_seen_up = true;
    const bool down = raw && g_button_seen_up;
    press = down && !g_button_before;
    g_button_before = down;
  } else {
    g_button_seen_up = false;
    g_button_before = false;
  }
  OpenInput in = {};
  in.press = press;
  in.can_open = have_hand && hand.active && hand.usable && hand.book_face &&
                have_game && !g.temple && !g.cutscene &&
                !CutsceneWindowFlat();
  in.ring = RadialMenuShows();
  in.menu = have_hand && hand.dialog;
  bool plucked = false;
  if (InterlockedExchange(&g_pluck_asked, 0) != 0 && g_open && have_game &&
      g_pull.state == kPullNone) {
    if (g_pull_cm <= 0 || !StartPull(g, frame)) Pluck(g, frame, &plucked);
  }
  if (g_pull.state == kPullOut && g_open && have_game) {
    PullStep(g, frame, &plucked);
  }
  in.plucked = plucked;
  const bool was_open = g_open;
  const int event = OpenStep(&g_open, in);
  if (event == kOpened) {
    ++g_opens;
    g_left = hand.left;
    g_hover = -1;
    g_hover_type = -1;
    g_notice_until = 0.0;
    g_test_item = 0;
    HapticsPulseHands(1, haptics_math::kTick);
  } else if (event == kRefused) {
    ++g_refusals;
    HapticsPulseHands(1, haptics_math::kRefused);
    if (LogRoom()) {
      ModLog("spell book   : the book does not open: %s, frame %llu",
             !have_hand || !hand.active ? "the hand has no pose"
             : !hand.usable             ? "the controllers cannot act now"
             : in.menu                  ? "a dialog box or the game menu is up"
             : in.ring                  ? "the ring shows"
             : !have_game               ? "no game runs"
             : g.temple                 ? "the player is in the Temple"
                                        : "a cut scene plays",
             frame);
    }
  } else if (event != kNothing) {
    ++g_closes;
    if (event == kClosedButton) HapticsPulseHands(1, haptics_math::kTick);
    if (LogRoom()) {
      ModLog("spell book   : the book closes: %s, frame %llu",
             kCloseWords[event], frame);
    }
  }
  if (!g_open) {
    g_hover = -1;
    g_hover_type = -1;
    if (g_pull.state != kPullNone) {
      if (LogRoom()) {
        ModLog("spell book   : the book closes while the hand pulls %s, so "
               "the miracle stays in the book, frame %llu",
               NarrowName(g_pull.type), frame);
      }
      StopPull();
    }
    if (was_open) g_have_plane = false;
    ShowBubble(have_game ? &g : nullptr, frame);
    return;
  }

  // The list of the game, the place of the book, and the pick.
  ReadShapes();
  BuildList(g);
  g_layout = LayoutFor(g_count);
  float grip[3], orientation[4], head[3];
  bool left = true;
  if (!ControllersCardPose(1, grip, orientation, head, &left)) return;
  float center[3];
  BookCenter(grip, orientation, left, center);
  PlaneAt(center, head, &g_plane);
  g_have_plane = true;
  if (event == kOpened && LogRoom()) {
    char words[400] = "";
    for (int i = 0; i < g_count; ++i) {
      char one[64];
      sprintf_s(one, "%s%s%s", i > 0 ? ", " : "", NarrowName(g_items[i].type),
                g_items[i].can ? "" : " (dim)");
      strncat_s(words, one, _TRUNCATE);
    }
    char empty[64] = "";
    if (g_count == 0) {
      size_t n = 0;
      for (const wchar_t* w = g_empty_why; *w != 0 && n + 1 < sizeof(empty);
           ++w) {
        empty[n++] = static_cast<char>(*w);
      }
      empty[n] = 0;
    }
    ModLog("spell book   : the book opens over the %s hand, at %.3f %.3f %.3f "
           "m, %.0f cm from the head, %.0f by %.0f cm. %d miracles: %s%s, "
           "frame %llu", left ? "left" : "right", center[0], center[1],
           center[2], controller_math::Distance(center, head) * 100.0f,
           g_layout.width_m * 100.0f, g_layout.height_m * 100.0f, g_count,
           g_count > 0 ? words : "none. ", empty, frame);
  }
  // The item under the pointing hand: the item of the last frame keeps its
  // place by its type, so a change of the list does not move the pick.
  int current = -1;
  for (int i = 0; i < g_count; ++i) {
    if (g_items[i].type == g_hover_type) current = i;
  }
  float point[3], point_turn[4], unused_head[3];
  bool unused_left = false;
  int hover = -1;
  if (g_pull.state != kPullNone) {
    // While the bubble is out of the book, or on its way back, the pick
    // stays on its item.
    hover = ItemOfType(g_pull.type);
  } else if (ControllersCardPose(0, point, point_turn, unused_head,
                                 &unused_left)) {
    float in_plane[3];
    InPlane(g_plane, point, in_plane);
    hover = Pick(g_layout, in_plane, current);
  }
  const int hover_type = hover >= 0 ? g_items[hover].type : -1;
  if (hover_type != g_hover_type) {
    if (hover >= 0) {
      // A light pulse on the pointing hand and a soft sound when the
      // highlight goes to a new item.
      HapticsPulseHands(2, haptics_math::kTick);
      AudioCue(kAudioCueRingMove, g_left ? 0.4f : -0.4f);
    }
    if (LogRoom()) {
      if (hover >= 0) {
        ModLog("spell book   : the pointing hand is at %s%s, frame %llu",
               NarrowName(hover_type),
               g_items[hover].can ? "" : ", which the game refuses now",
               frame);
      } else {
        ModLog("spell book   : the pointing hand is at no miracle of the "
               "book, frame %llu", frame);
      }
    }
  }
  g_hover = hover;
  g_hover_type = hover_type;

  // The picture of this frame.
  Look look;
  BuildLook(&look);
  if (!g_have_drawn || !SameLook(look, g_drawn)) Raster(look);
  ShowBubble(have_game ? &g : nullptr, frame);
  if (!g_have_drawn || g_raster_failed ||
      bwvr::XrScreen::CardFailed(bwvr::kCardRing) || RadialMenuShows()) {
    return;
  }
  StagePicture(frame);
  bwvr::CardLayer layer = {};
  layer.visible = true;
  layer.pixels = g_staged;
  layer.texture_width = kTexture;
  layer.texture_height = kTexture;
  layer.rect[0] = 0;
  layer.rect[1] = 0;
  layer.rect[2] = g_width_px;
  layer.rect[3] = g_height_px;
  for (int i = 0; i < 3; ++i) layer.position[i] = g_plane.center[i];
  for (int i = 0; i < 4; ++i) layer.orientation[i] = g_plane.orientation[i];
  layer.width = g_width_px / kPixelsPerMeter;
  layer.height = g_height_px / kPixelsPerMeter;
  bwvr::XrScreen::StageCard(bwvr::kCardRing, layer);
}

bool SpellBookPumpGrip(bool grip_down) {
  if (!g_on) return false;
  bool pluck = false;
  const bool claim = GripStep(&g_grip, grip_down, g_open, g_hover, &pluck);
  InterlockedExchange(&g_grip_claimed, claim ? 1 : 0);
  if (pluck) {
    g_pluck_type = g_hover_type;
    InterlockedExchange(&g_pluck_asked, 1);
  }
  return claim;
}

void SpellBookInterfaceStep(void* face_in) {
  if (g_request_type < 0) return;
  const int type = g_request_type;
  g_request_type = -1;
  GameNow g;
  if (!ReadGame(&g) || g.face != face_in) return;
  const uint64_t frame = ControllersFrameCount();
  // The same tests as the end of a chain of gestures: the Hand is free, and
  // a Worship Site has an icon of the miracle that the player can ask for.
  if (!g.hand_ready || !CanRequest(g, type)) {
    ++g_refusals;
    HapticsPulseHands(2, haptics_math::kRefused);
    SetNotice(!g.hand_ready ? L"The Hand must be empty"
                            : L"The game refuses it now");
    if (LogRoom()) {
      ModLog("spell book   : the request of %s does not go to the game: %s, "
             "frame %llu", NarrowName(type),
             !g.hand_ready ? "the Hand is not free" : "the game test says no",
             frame);
    }
    return;
  }
  reinterpret_cast<SendPacketFn>(kSendPacket)(g.game, nullptr, kPacketRequest,
                                              type, 0);
  ++g_requests;
  g_watch_type = type;
  g_watch_frame = frame;
  if (LogRoom()) {
    ModLog("spell book   : the request of %s goes to the game: packet 0x25 "
           "with the seed type %d, as at the end of a chain of gestures, "
           "%llu frames after the take, frame %llu", NarrowName(type), type,
           frame - g_request_frame, frame);
  }
}

int SpellBookHint(MiracleLine lines[kMiracleLines]) {
  const bool notice = g_notice_until > NowSeconds();
  if (!g_open && !notice) return 0;
  static wchar_t take[64];
  static wchar_t dim[96];
  int count = 0;
  if (g_open && g_count == 0) {
    lines[count++] = {-1, L"No miracle to ask for now"};
    if (g_empty_why[0] != 0) lines[count++] = {-1, g_empty_why};
    return count;
  }
  if (g_open && g_pull.state == kPullOut) {
    lines[count++] = {-1, L"Pull farther to take it"};
    lines[count++] = {kWordGrab, L"Open to put it back"};
    return count;
  }
  if (g_open && g_hover >= 0 && g_hover < g_count) {
    const Item& item = g_items[g_hover];
    if (item.can) {
      swprintf_s(take, g_pull_cm > 0 ? L"Hold and pull out %s" : L"Take %s",
                 kSeedName[item.type]);
      lines[count++] = {kWordGrab, take};
    } else {
      swprintf_s(dim, L"%s: %s", kSeedName[item.type], item.why);
      lines[count++] = {-1, dim};
    }
  } else if (g_open) {
    lines[count++] = {-1, L"Put your hand on a miracle"};
  }
  // The line of a dim item already gives the reason of a refusal of that
  // item, so the card does not show the same reason two times.
  const bool said = count > 0 && wcsstr(lines[count - 1].text, g_notice) != nullptr;
  if (notice && !said && count < kMiracleLines) lines[count++] = {-1, g_notice};
  return count;
}

bool SpellBookTestItem(float out[3]) {
  if (!g_open || !g_have_plane || g_count == 0) return false;
  const int i = g_test_item % g_count;
  ++g_test_item;
  float x = 0.0f, y = 0.0f;
  ItemCenter(g_layout, i, &x, &y);
  PointOn(g_plane, x, y, out);
  ModLog("spell book   : test: the word \"book\" steps the pointing hand to "
         "item %d (%s) of the book, frame %llu", i,
         NarrowName(g_items[i].type), ControllersFrameCount());
  return true;
}

int SpellBookValue() {
  ReadSettings();
  return g_game_ok ? (g_on ? 1 : 0) : kDebugCannot;
}

void SpellBookChange(const char* by) {
  ReadSettings();
  g_on = !g_on;
  if (!g_on && g_open) {
    g_open = false;
    ++g_closes;
  }
  ControllersGrabPulse(1, 0.5f, g_on ? 0.40f : 0.08f);
  ModLog("spell book   : %s: spell_book=%d. %s, frame %llu", by, g_on ? 1 : 0,
         g_on ? "The first face button of the hand that does not point opens "
                "the book"
              : "The first face button of the hand that does not point has "
                "no job",
         ControllersFrameCount());
}

int SpellBookPullValue() {
  ReadSettings();
  return g_game_ok ? (g_pull_cm > 0 ? 1 : 0) : kDebugCannot;
}

void SpellBookPullChange(const char* by) {
  ReadSettings();
  g_pull_cm = g_pull_cm > 0 ? 0 : g_pull_ini;
  if (g_pull.state != kPullNone) StopPull();
  ControllersGrabPulse(1, 0.5f, g_pull_cm > 0 ? 0.40f : 0.08f);
  ModLog("spell book   : %s: spell_book_pull_cm=%d. %s, frame %llu", by,
         g_pull_cm,
         g_pull_cm > 0 ? "The hand pulls a miracle out of the book"
                       : "The grip takes a miracle from the book at once",
         ControllersFrameCount());
}

int SpellBookBubbleValue() {
  ReadSettings();
  return g_game_ok && !BookBubbleFailed() ? (g_game_bubble ? 1 : 0)
                                          : kDebugCannot;
}

void SpellBookBubbleChange(const char* by) {
  ReadSettings();
  g_game_bubble = !g_game_bubble;
  ControllersGrabPulse(1, 0.5f, g_game_bubble ? 0.40f : 0.08f);
  ModLog("spell book   : %s: spell_book_bubble=%d. %s, frame %llu", by,
         g_game_bubble ? 1 : 0,
         g_game_bubble ? "The game draws the bubble that the hand pulls out"
                       : "The bubble that the hand pulls out is a flat picture",
         ControllersFrameCount());
}

int SpellBookAllValue() {
  ReadSettings();
  return g_game_ok ? (g_all ? 1 : 0) : kDebugCannot;
}

void SpellBookAllChange(const char* by) {
  ReadSettings();
  g_all = !g_all;
  g_all_next = 0;
  ControllersGrabPulse(1, 0.5f, g_all ? 0.40f : 0.08f);
  ModLog("spell book   : %s: debug_all_miracles=%d%s, frame %llu", by,
         g_all ? 1 : 0,
         g_all ? ": each Worship Site of the player gets every miracle of the "
                 "game and no limit of prayer power (a test)"
               : "", ControllersFrameCount());
  if (!g_all) UndoAll(ControllersFrameCount());
}

void SpellBookReport() {
  if (g_opens + g_closes + g_refusals + g_requests + g_pulls == 0) return;
  ModLog("spell book   : the book opened %llu times and closed %llu times "
         "this window: %llu pulls (%llu snaps, %llu back into the book), "
         "%llu miracles taken, %llu requests to the game, %llu "
         "Miracles in the Hand, %llu refusals. The picture was drawn %llu "
         "times, the longest draw took %.2f ms",
         g_opens, g_closes, g_pulls, g_snaps, g_backs, g_plucks, g_requests,
         g_arrived, g_refusals, g_rasters, g_raster_ms);
  int shown = 0, draws = 0, made = 0;
  BookBubbleCounts(&shown, &draws, &made);
  if (shown + draws + made > 0) {
    ModLog("spell book   : the game's bubble showed in %d frames this window, "
           "with %d draws (one for each eye pass). The game made %d parts of "
           "it (the glass ball and the graphic of a miracle)", shown, draws,
           made);
  }
  g_pulls = g_snaps = g_backs = 0;
  g_opens = g_closes = g_plucks = g_requests = g_arrived = g_refusals = 0;
  g_rasters = 0;
  g_raster_ms = 0.0;
}
