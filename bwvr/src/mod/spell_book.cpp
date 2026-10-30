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
// THE KEYS of bwvr.ini:
//
//   spell_book=1             0: the first face button of the hand that does
//                            not point has no job
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
// The counts of a report window.
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
    // larger at the right.
    const uint8_t* info = SeedInfo(look.type[i]);
    const int first = *reinterpret_cast<const int32_t*>(info + kInfoGestures);
    const int second =
        *reinterpret_cast<const int32_t*>(info + kInfoGestures + 4);
    DrawShape(first, bx - 0.42f * br, by, 0.55f * br);
    DrawShape(second, bx + 0.30f * br, by, 0.95f * br);
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

void BuildLook(Look* look) {
  memset(look, 0, sizeof(*look));
  look->count = g_count;
  look->hover = g_hover;
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
  if (InterlockedExchange(&g_pluck_asked, 0) != 0 && g_open && have_game) {
    Pluck(g, frame, &plucked);
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
    if (was_open) g_have_plane = false;
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
  if (ControllersCardPose(0, point, point_turn, unused_head, &unused_left)) {
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
  if (!g_have_drawn || g_raster_failed ||
      bwvr::XrScreen::CardFailed(bwvr::kCardRing) || RadialMenuShows()) {
    return;
  }
  if (g_staged_new) {
    g_staged_new = false;
    for (int y = 0; y < g_height_px; ++y) {
      memcpy(g_staged + static_cast<size_t>(y) * kTexture * 4,
             g_base + static_cast<size_t>(y) * kTexture * 4,
             static_cast<size_t>(g_width_px) * 4);
    }
  }
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
  if (g_open && g_hover >= 0 && g_hover < g_count) {
    const Item& item = g_items[g_hover];
    if (item.can) {
      swprintf_s(take, L"Take %s", kSeedName[item.type]);
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
  if (g_opens + g_closes + g_refusals + g_requests == 0) return;
  ModLog("spell book   : the book opened %llu times and closed %llu times "
         "this window: %llu miracles taken, %llu requests to the game, %llu "
         "Miracles in the Hand, %llu refusals. The picture was drawn %llu "
         "times, the longest draw took %.2f ms",
         g_opens, g_closes, g_plucks, g_requests, g_arrived, g_refusals,
         g_rasters, g_raster_ms);
  g_opens = g_closes = g_plucks = g_requests = g_arrived = g_refusals = 0;
  g_rasters = 0;
  g_raster_ms = 0.0;
}
