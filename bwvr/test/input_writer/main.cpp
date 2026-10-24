// THE TEST WRITER OF THE INPUT CHANNEL (docs/android.md, "The input
// channel"). A Windows program for a desktop run, with the channel code of
// the mod and the app (src/common/input_channel.h). It does what the headset
// app does, with the input of a record in place of a session.
//
//   bwvr_input_writer play <channel> <record> <seconds> [<head seconds>] [<hz>] [app]
//     Waits up to <head seconds> (<seconds> when not given) for the head of
//     the mod in the channel file, so that a run can first wait for an idle
//     PC. Then for each frame
//     of the controllers that the mod latches (Head::want), it writes the
//     input of the same frame of the record into the slot, with the frame +
//     1 as its tag (the lockstep of xr_input_lockstep_ms). After the end of
//     the record it writes the last frame of the record. With <hz> above 0
//     it also writes the last slot again <hz> times a second, with the same
//     bytes, as the app writes a slot at each frame of its session: these
//     writes pace the frames of the mod with vk_pace=1. With the word app
//     (and <hz> above 0) it writes as the app does: only at <hz>, the input
//     of the frame that the mod latched last, with a frame of the writer that
//     is one more for each write, and no answer to a latch (no lockstep). It
//     takes each pulse
//     of the ring of the events and writes one line for it. It ends
//     <seconds> after the head of the mod came.
//
//   bwvr_input_writer script <channel> <script> <seconds> <head seconds> <hz>
//                            <log> <render width> <render height>
//                            [<distance cm> <width cm>]
//     Waits for the head of the mod, as play does. Then it writes a new slot
//     <hz> times a second, as the app writes a slot at each frame of its
//     session: a head at the origin of the LOCAL space and two Touch
//     controllers, 20 cm to the side of the head, 30 cm down and 30 cm ahead,
//     which follow the steps of the script, one step after the other:
//       wait <text>          the next step waits until <log> (bwvr.log of
//                            the game folder) has the text
//       aim <hand> <x> <y>   the aim of the hand goes to the pixel x, y of
//                            the screen quad of the mod (the quad of its
//                            kScreen for a render of <render width> by
//                            <render height>, <distance cm> in front of the
//                            origin, 200 by default, <width cm> wide, 260
//                            by default), or after the step panel to the
//                            pixel x, y of the panel image on the menu
//                            panel quad
//       panel                the next step waits until <log> has the line
//                            of the mod with the menu panel quad of a
//                            dialog box in a frame of the scene, and aim
//                            goes to that quad from then on
//       panelquad <the rest of the line of the mod>
//                            aim goes to the menu panel quad of the text
//                            after "The menu panel quad at " of that line,
//                            with no wait (a run with panel_pointer=0, in
//                            which the mod writes no such line)
//       screen               aim goes to the screen quad again
//       trigger <hand> <0|1> the trigger of the hand
//       slots <n>            the next step waits for n slots
//       key <word>           one key into the ring of the keys, as the
//                            keyboard of the app: a letter or a digit,
//                            space, back or enter
//     <hand> is left or right. A line that starts with # is a note. Each
//     step writes one line. It ends <seconds> after the head of the mod
//     came, and it writes each pulse of the mod, as play does. Exit code 0
//     when it did each step.
//
//   bwvr_input_writer compare <record A> <record B>
//     Compares the input of each frame that is in both records. Exit code 0
//     when each one has the same bytes.
//
//   bwvr_input_writer dump <channel>
//     Writes the head and the slot of a channel file as text.
//
// No game code. A record holds no pixels and no file of the game.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "../../src/common/input_channel.h"
#include "../../src/mod/controller_math.h"

namespace {

namespace ic = input_channel;

bool ReadRecord(const char* path, std::vector<ic::SlotData>* frames,
                uint32_t* source) {
  FILE* f = nullptr;
  if (fopen_s(&f, path, "rb") != 0 || f == nullptr) {
    printf("no record at %s\n", path);
    return false;
  }
  ic::RecordHead head = {};
  bool ok = fread(&head, sizeof(head), 1, f) == 1 &&
            head.magic == ic::kRecordMagic && head.version == ic::kVersion &&
            head.slot_bytes == sizeof(ic::SlotData);
  if (!ok) {
    printf("%s is not a record of this version\n", path);
    fclose(f);
    return false;
  }
  *source = head.source;
  frames->clear();
  ic::SlotData d;
  while (fread(&d, sizeof(d), 1, f) == 1) frames->push_back(d);
  fclose(f);
  return true;
}

struct Mapped {
  HANDLE file = INVALID_HANDLE_VALUE;
  HANDLE mapping = nullptr;
  ic::File* view = nullptr;
  void Close() {
    if (view != nullptr) UnmapViewOfFile(view);
    if (mapping != nullptr) CloseHandle(mapping);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    *this = Mapped();
  }
};

bool Map(const char* path, Mapped* m) {
  m->file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (m->file == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size = {};
  if (!GetFileSizeEx(m->file, &size) || size.QuadPart < ic::kFileBytes) {
    m->Close();
    return false;
  }
  m->mapping = CreateFileMappingA(m->file, nullptr, PAGE_READWRITE, 0,
                                  ic::kFileBytes, nullptr);
  if (m->mapping != nullptr) {
    m->view = static_cast<ic::File*>(
        MapViewOfFile(m->mapping, FILE_MAP_ALL_ACCESS, 0, 0, ic::kFileBytes));
  }
  if (m->view == nullptr) {
    m->Close();
    return false;
  }
  return true;
}

int Play(const char* channel, const char* record, int seconds,
         int head_seconds, int hz, bool app) {
  std::vector<ic::SlotData> frames;
  uint32_t source = 0;
  if (!ReadRecord(record, &frames, &source) || frames.empty()) return 2;
  printf("the record has %zu frames of the controllers (source %u)\n",
         frames.size(), source);
  fflush(stdout);
  const ULONGLONG head_end =
      GetTickCount64() + static_cast<ULONGLONG>(head_seconds) * 1000;
  Mapped m;
  while (GetTickCount64() < head_end) {
    if (Map(channel, &m)) {
      if (ic::HeadOk(m.view, ic::kFileBytes) &&
          ic::LoadAcquire(&m.view->head.mod_session) != 0) {
        break;
      }
      m.Close();
    }
    Sleep(20);
  }
  if (m.view == nullptr) {
    printf("no channel of the mod at %s in %d s\n", channel, head_seconds);
    return 3;
  }
  const ULONGLONG end = GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000;
  ic::File* f = m.view;
  ic::StoreRelease(&f->head.app_session, GetCurrentProcessId());
  ic::DropOldEvents(f);
  printf("the mod made the channel (session %u). The writer writes the "
         "record\n", f->head.mod_session);
  fflush(stdout);

  uint64_t writes = 0;
  uint64_t again = 0;
  uint32_t last_want = 0;
  uint32_t pulses = 0;
  bool seen_want = false;
  // The writes again at <hz>, on the clock of QueryPerformanceCounter.
  LARGE_INTEGER frequency, now;
  QueryPerformanceFrequency(&frequency);
  const int64_t tick = hz > 0 ? frequency.QuadPart / hz : 0;
  QueryPerformanceCounter(&now);
  int64_t next = now.QuadPart + tick;
  ic::SlotData last = {};
  uint32_t app_frame = 0;
  while (GetTickCount64() < end) {
    const uint32_t want = ic::LoadAcquire(&f->head.want);
    const uint32_t tag = want + 1;
    if (app) {
      // As the app: one write at each tick, a new frame of the writer.
      QueryPerformanceCounter(&now);
      if (now.QuadPart >= next) {
        ic::SlotData d = frames[want < frames.size() ? want : frames.size() - 1];
        d.frame = ++app_frame;
        ic::Write(&f->slot, d);
        ++writes;
        last_want = want;
        seen_want = true;
        next += tick;
        if (next <= now.QuadPart) next = now.QuadPart + tick;
      }
    } else if (f->slot.data.frame != tag) {
      ic::SlotData d = frames[want < frames.size() ? want : frames.size() - 1];
      d.frame = tag;
      ic::Write(&f->slot, d);
      last = d;
      ++writes;
      last_want = want;
      seen_want = true;
    }
    if (tick > 0 && !app) {
      QueryPerformanceCounter(&now);
      if (now.QuadPart >= next) {
        if (seen_want) {
          ic::Write(&f->slot, last);
          ++again;
        }
        next += tick;
        if (next <= now.QuadPart) next = now.QuadPart + tick;
      }
    }
    ic::Event events[ic::kEvents];
    bool broken = false;
    const uint32_t n = ic::TakeEvents(f, events, ic::kEvents, &broken);
    for (uint32_t i = 0; i < n; ++i) {
      ++pulses;
      printf("pulse frame %u hand %u, %.0f ms at %.2f\n", events[i].frame,
             events[i].hand, events[i].seconds * 1000.0f, events[i].amplitude);
    }
    if (broken) printf("the indices of the events are not valid\n");
    if (n > 0) fflush(stdout);
    YieldProcessor();
  }
  printf("the writer wrote %llu slots, the last for frame %u of the "
         "controllers%s. %u pulses came. The mod read the slot %u times, read "
         "%u torn copies again, and dropped %u pulses\n",
         static_cast<unsigned long long>(writes), last_want,
         seen_want ? "" : " (the mod asked for no frame)", pulses,
         f->head.latches, f->head.torn, f->head.events_dropped);
  if (hz > 0 && app) {
    printf("the writer wrote as the app at %d Hz for %d s, %u frames of the "
           "writer\n", hz, seconds, app_frame);
  } else if (hz > 0) {
    printf("the writer wrote the last slot again %llu times at %d Hz in %d s\n",
           static_cast<unsigned long long>(again), hz, seconds);
  }
  m.Close();
  return writes > 0 ? 0 : 4;
}

// THE SCRIPT of the command script.
struct ScriptStep {
  std::string word;
  int hand = 1;
  float a = 0.0f, b = 0.0f;
  std::string text;
};

bool ReadScript(const char* path, std::vector<ScriptStep>* steps) {
  FILE* f = nullptr;
  if (fopen_s(&f, path, "r") != 0 || f == nullptr) {
    printf("no script at %s\n", path);
    return false;
  }
  char line[512];
  while (fgets(line, sizeof(line), f) != nullptr) {
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ')) {
      line[--n] = 0;
    }
    if (n == 0 || line[0] == '#') continue;
    ScriptStep s;
    char word[32] = {0}, hand[16] = {0};
    if (sscanf_s(line, "%31s", word, static_cast<unsigned>(sizeof(word))) != 1) continue;
    s.word = word;
    const char* rest = line + strlen(word);
    while (*rest == ' ') ++rest;
    if (s.word == "aim") {
      sscanf_s(rest, "%15s %f %f", hand, static_cast<unsigned>(sizeof(hand)), &s.a, &s.b);
    } else if (s.word == "trigger") {
      sscanf_s(rest, "%15s %f", hand, static_cast<unsigned>(sizeof(hand)), &s.a);
    } else if (s.word == "slots") {
      s.a = static_cast<float>(atof(rest));
    } else if (s.word == "wait" || s.word == "key" || s.word == "panelquad") {
      s.text = rest;
    } else if (s.word == "panel" || s.word == "screen") {
    } else {
      printf("the script has a word that the writer does not know: %s\n", line);
      fclose(f);
      return false;
    }
    s.hand = strcmp(hand, "left") == 0 ? 0 : 1;
    steps->push_back(s);
  }
  fclose(f);
  return true;
}

// The rest of the first line of the file after the text, or false when the
// file does not have the text. The mod writes its log with a shared read.
bool LogFind(const char* path, const std::string& text, std::string* rest) {
  HANDLE h = CreateFileA(path, GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  std::string all;
  char buffer[65536];
  DWORD got = 0;
  while (ReadFile(h, buffer, sizeof(buffer), &got, nullptr) && got > 0) {
    all.append(buffer, got);
  }
  CloseHandle(h);
  const size_t at = all.find(text);
  if (at == std::string::npos) return false;
  if (rest != nullptr) {
    const size_t from = at + text.size();
    const size_t end = all.find('\n', from);
    *rest = all.substr(from, end == std::string::npos ? std::string::npos : end - from);
  }
  return true;
}
bool LogHas(const char* path, const std::string& text) {
  return LogFind(path, text, nullptr);
}

// THE MENU PANEL QUAD of a dialog box in a frame of the scene, from the line
// of the mod (controllers.cpp, PanelFrame).
struct PanelQuad {
  float position[3];
  float orientation[4];
  float size[2];
  float rect[4];
};
const char* const kPanelLine =
    "controllers  : panel quads: a dialog box is up in a frame of the scene";
bool ReadPanelQuad(const std::string& rest, PanelQuad* q) {
  const size_t at = rest.find("The menu panel quad at ");
  if (at == std::string::npos) return false;
  return sscanf_s(rest.c_str() + at,
                  "The menu panel quad at %f %f %f m, turn %f %f %f %f, %f by %f m, "
                  "the rect %f %f %f %f",
                  &q->position[0], &q->position[1], &q->position[2], &q->orientation[0],
                  &q->orientation[1], &q->orientation[2], &q->orientation[3], &q->size[0],
                  &q->size[1], &q->rect[0], &q->rect[1], &q->rect[2], &q->rect[3]) == 13 &&
         q->rect[2] > 0.0f && q->rect[3] > 0.0f;
}

// The code of a key of the script, or 0.
uint32_t KeyCode(const std::string& word) {
  if (word == "space") return ' ';
  if (word == "back") return ic::kKeyBack;
  if (word == "enter") return ic::kKeyEnter;
  if (word.size() == 1) return static_cast<uint8_t>(word[0]);
  return 0;
}

int Script(const char* channel, const char* script, int seconds, int head_seconds,
           int hz, const char* log, int render_w, int render_h, float distance_m,
           float width_m) {
  std::vector<ScriptStep> steps;
  if (!ReadScript(script, &steps)) return 2;
  if (hz <= 0 || render_w <= 0 || render_h <= 0) {
    printf("the script needs a rate and a render size\n");
    return 2;
  }
  printf("the script has %zu steps. A slot %d times a second, the screen quad %.2f m "
         "wide at %.2f m for a render of %d by %d\n",
         steps.size(), hz, width_m, distance_m, render_w, render_h);
  fflush(stdout);
  const ULONGLONG head_end =
      GetTickCount64() + static_cast<ULONGLONG>(head_seconds) * 1000;
  Mapped m;
  while (GetTickCount64() < head_end) {
    if (Map(channel, &m)) {
      if (ic::HeadOk(m.view, ic::kFileBytes) &&
          ic::LoadAcquire(&m.view->head.mod_session) != 0) {
        break;
      }
      m.Close();
    }
    Sleep(20);
  }
  if (m.view == nullptr) {
    printf("no channel of the mod at %s in %d s\n", channel, head_seconds);
    return 3;
  }
  ic::File* f = m.view;
  ic::StoreRelease(&f->head.app_session, GetCurrentProcessId());
  ic::DropOldEvents(f);
  printf("the mod made the channel (session %u). The writer runs the script\n",
         f->head.mod_session);
  fflush(stdout);

  // The head and the hands of the slots.
  ic::SlotData d;
  memset(&d, 0, sizeof(d));
  d.flags = ic::kFocused | ic::kRunning;
  d.display_period = 1000000000LL / hz;
  d.recommended_width = static_cast<uint32_t>(render_w);
  d.recommended_height = static_cast<uint32_t>(render_h);
  for (int e = 0; e < 2; ++e) {
    d.eyes[e].position[0] = e == 0 ? -0.032f : 0.032f;
    d.eyes[e].orientation[3] = 1.0f;
    d.eyes[e].fov[0] = -0.82f;
    d.eyes[e].fov[1] = 0.82f;
    d.eyes[e].fov[2] = 0.82f;
    d.eyes[e].fov[3] = -0.82f;
    d.eyes[e].valid = 1;
  }
  for (int h = 0; h < 2; ++h) {
    ic::Hand& x = d.hands[h];
    x.flags = ic::kActive | ic::kAimValid | ic::kGripValid | ic::kAimTracked |
              ic::kGripTracked;
    const float side = h == 0 ? -0.2f : 0.2f;
    x.aim_position[0] = x.grip_position[0] = side;
    x.aim_position[1] = x.grip_position[1] = -0.3f;
    x.aim_position[2] = x.grip_position[2] = -0.3f;
    x.aim_orientation[3] = x.grip_orientation[3] = 1.0f;
    strcpy_s(x.profile, "oculus/touch_controller");
  }
  const float quad_h = width_m * static_cast<float>(render_h) / static_cast<float>(render_w);

  LARGE_INTEGER frequency, now;
  QueryPerformanceFrequency(&frequency);
  const int64_t tick = frequency.QuadPart / hz;
  QueryPerformanceCounter(&now);
  int64_t next = now.QuadPart;
  const ULONGLONG end = GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000;
  size_t step = 0;
  uint64_t wait_slots = 0;
  bool waiting_log = false;
  ULONGLONG next_look = 0;
  uint32_t frame = 0, pulses = 0, keys = 0;
  PanelQuad panel = {};
  bool on_panel = false;
  while (GetTickCount64() < end) {
    // The steps that do not wait.
    while (step < steps.size() && wait_slots == 0) {
      const ScriptStep& s = steps[step];
      if (s.word == "wait") {
        const ULONGLONG t = GetTickCount64();
        if (!waiting_log) {
          waiting_log = true;
          printf("slot %u: wait for \"%s\" in the log\n", frame, s.text.c_str());
          fflush(stdout);
        }
        if (t < next_look) break;
        next_look = t + 100;
        if (!LogHas(log, s.text)) break;
        waiting_log = false;
        printf("slot %u: the log has \"%s\"\n", frame, s.text.c_str());
      } else if (s.word == "panel") {
        const ULONGLONG t = GetTickCount64();
        if (!waiting_log) {
          waiting_log = true;
          printf("slot %u: wait for the menu panel quad in the log\n", frame);
          fflush(stdout);
        }
        if (t < next_look) break;
        next_look = t + 100;
        std::string rest;
        if (!LogFind(log, kPanelLine, &rest)) break;
        waiting_log = false;
        if (!ReadPanelQuad(rest, &panel)) {
          printf("slot %u: the line of the menu panel quad has no quad: %s\n", frame,
                 rest.c_str());
          break;
        }
        on_panel = true;
        printf("slot %u: the menu panel quad is at %.4f %.4f %.4f m, %.4f by %.4f m, the "
               "rect %.0f %.0f %.0f %.0f\n",
               frame, panel.position[0], panel.position[1], panel.position[2], panel.size[0],
               panel.size[1], panel.rect[0], panel.rect[1], panel.rect[2], panel.rect[3]);
      } else if (s.word == "panelquad") {
        on_panel = ReadPanelQuad(s.text, &panel);
        printf("slot %u: %s\n", frame,
               on_panel ? "aim goes to the menu panel quad of the script"
                        : "the script has no menu panel quad after panelquad");
      } else if (s.word == "screen") {
        on_panel = false;
        printf("slot %u: aim goes to the screen quad\n", frame);
      } else if (s.word == "aim") {
        ic::Hand& x = d.hands[s.hand];
        float target[3] = {(s.a / render_w - 0.5f) * width_m,
                           (0.5f - s.b / render_h) * quad_h, -distance_m};
        if (on_panel) {
          controller_math::QuadPoint(panel.position, panel.orientation, panel.size,
                                     (s.a - panel.rect[0]) / panel.rect[2],
                                     (s.b - panel.rect[1]) / panel.rect[3], target);
        }
        const float dir[3] = {target[0] - x.aim_position[0], target[1] - x.aim_position[1],
                              target[2] - x.aim_position[2]};
        controller_math::AimAlong(dir, x.aim_orientation);
        memcpy(x.grip_orientation, x.aim_orientation, sizeof(x.grip_orientation));
        printf("slot %u: the %s hand aims at the pixel %.0f %.0f of the %s\n", frame,
               s.hand == 0 ? "left" : "right", s.a, s.b,
               on_panel ? "panel image on the menu panel quad" : "screen quad");
      } else if (s.word == "trigger") {
        d.hands[s.hand].trigger = s.a > 0.5f ? 1.0f : 0.0f;
        printf("slot %u: the %s trigger %s\n", frame, s.hand == 0 ? "left" : "right",
               s.a > 0.5f ? "closes" : "opens");
      } else if (s.word == "slots") {
        wait_slots = static_cast<uint64_t>(s.a);
      } else if (s.word == "key") {
        const uint32_t code = KeyCode(s.text);
        const ic::KeyEvent k = {code, frame};
        const bool pushed = code != 0 && ic::PushKey(f, k);
        if (pushed) ++keys;
        printf("slot %u: the key %s %s\n", frame, s.text.c_str(),
               pushed ? "goes into the ring" : "does not go into the ring");
      }
      fflush(stdout);
      ++step;
    }
    // One slot at each tick of the rate.
    QueryPerformanceCounter(&now);
    if (now.QuadPart >= next) {
      ++frame;
      d.frame = frame;
      for (int h = 0; h < 2; ++h) d.hands[h].serial = frame;
      d.display_time = now.QuadPart / frequency.QuadPart * 1000000000LL +
                       now.QuadPart % frequency.QuadPart * 1000000000LL / frequency.QuadPart;
      ic::Write(&f->slot, d);
      if (wait_slots > 0) --wait_slots;
      next += tick;
      if (next <= now.QuadPart) next = now.QuadPart + tick;
    }
    ic::Event events[ic::kEvents];
    bool broken = false;
    const uint32_t n = ic::TakeEvents(f, events, ic::kEvents, &broken);
    for (uint32_t i = 0; i < n; ++i) {
      ++pulses;
      printf("pulse frame %u hand %u, %.0f ms at %.2f\n", events[i].frame, events[i].hand,
             events[i].seconds * 1000.0f, events[i].amplitude);
    }
    if (n > 0) fflush(stdout);
    Sleep(0);
  }
  printf("the writer wrote %u slots and %u keys, and did %zu of %zu steps of the script. "
         "%u pulses came. The mod read the slot %u times and %u keys\n",
         frame, keys, step, steps.size(), pulses, f->head.latches, f->head.key_read);
  fflush(stdout);
  m.Close();
  return step == steps.size() ? 0 : 5;
}

const char* FirstField(const ic::SlotData& a, const ic::SlotData& b) {
  if (a.flags != b.flags) return "the focus";
  if (a.display_time != b.display_time || a.display_period != b.display_period)
    return "the display time";
  if (memcmp(a.eyes, b.eyes, sizeof(a.eyes)) != 0) return "the eyes";
  for (int h = 0; h < 2; ++h) {
    const ic::Hand& x = a.hands[h];
    const ic::Hand& y = b.hands[h];
    if (x.flags != y.flags) return h ? "the buttons of the right hand" : "the buttons of the left hand";
    if (memcmp(x.aim_position, y.aim_position, sizeof(float) * 14) != 0)
      return h ? "the poses of the right hand" : "the poses of the left hand";
    if (x.trigger != y.trigger || x.squeeze != y.squeeze ||
        x.stick[0] != y.stick[0] || x.stick[1] != y.stick[1])
      return h ? "the values of the right hand" : "the values of the left hand";
    if (strcmp(x.profile, y.profile) != 0) return "a profile";
  }
  return "another value";
}

int Compare(const char* a_path, const char* b_path) {
  std::vector<ic::SlotData> a, b;
  uint32_t sa = 0, sb = 0;
  if (!ReadRecord(a_path, &a, &sa) || !ReadRecord(b_path, &b, &sb)) return 2;
  const size_t n = a.size() < b.size() ? a.size() : b.size();
  size_t same = 0, order = 0;
  long first = -1;
  const char* what = "";
  for (size_t i = 0; i < n; ++i) {
    if (a[i].frame == i && b[i].frame == i) ++order;
    // The serial counts the syncs of a session, and the record keeps it.
    if (memcmp(&a[i], &b[i], sizeof(ic::SlotData)) == 0) {
      ++same;
    } else if (first < 0) {
      first = static_cast<long>(i);
      what = FirstField(a[i], b[i]);
    }
  }
  printf("compare: record A has %zu frames (source %u), record B %zu frames "
         "(source %u). %zu frames in both, %zu with their frame in order, %zu "
         "with the same bytes",
         a.size(), sa, b.size(), sb, n, order, same);
  if (first >= 0) printf(". The first difference is at frame %ld: %s", first, what);
  printf("\n");
  return n > 0 && same == n && order == n ? 0 : 1;
}

void PrintHand(int h, const ic::Hand& x) {
  printf("hand %d flags 0x%03X aim %.4f %.4f %.4f %.4f %.4f %.4f %.4f grip "
         "%.4f %.4f %.4f %.4f %.4f %.4f %.4f trigger %.2f squeeze %.2f stick "
         "%.2f %.2f serial %u profile \"%s\"\n",
         h, x.flags, x.aim_position[0], x.aim_position[1], x.aim_position[2],
         x.aim_orientation[0], x.aim_orientation[1], x.aim_orientation[2],
         x.aim_orientation[3], x.grip_position[0], x.grip_position[1],
         x.grip_position[2], x.grip_orientation[0], x.grip_orientation[1],
         x.grip_orientation[2], x.grip_orientation[3], x.trigger, x.squeeze,
         x.stick[0], x.stick[1], x.serial, x.profile);
}

int Dump(const char* channel) {
  FILE* file = nullptr;
  if (fopen_s(&file, channel, "rb") != 0 || file == nullptr) {
    printf("no channel at %s\n", channel);
    return 2;
  }
  std::vector<uint8_t> bytes(ic::kFileBytes, 0);
  const size_t got = fread(bytes.data(), 1, bytes.size(), file);
  fclose(file);
  const ic::File* f = reinterpret_cast<const ic::File*>(bytes.data());
  const bool ok = ic::HeadOk(f, got);
  printf("head %s, %zu bytes, version %u, app session %u, mod session %u, "
         "events written %u, read %u, dropped %u\n",
         ok ? "whole" : "not whole", got, f->head.version, f->head.app_session,
         f->head.mod_session, f->head.event_write, f->head.event_read,
         f->head.events_dropped);
  if (!ok) return 1;
  const ic::SlotData& d = f->slot.data;
  printf("slot seq %u frame %u flags 0x%X time %lld period %lld recenters %u "
         "size %u %u gaze %.3f %.3f %.3f\n",
         f->slot.seq, d.frame, d.flags, static_cast<long long>(d.display_time),
         static_cast<long long>(d.display_period), d.recenters,
         d.recommended_width, d.recommended_height, d.gaze[0], d.gaze[1],
         d.gaze[2]);
  for (int e = 0; e < 2; ++e) {
    const ic::Eye& x = d.eyes[e];
    printf("eye %d valid %u position %.4f %.4f %.4f orientation %.4f %.4f %.4f "
           "%.4f fov %.4f %.4f %.4f %.4f\n",
           e, x.valid, x.position[0], x.position[1], x.position[2],
           x.orientation[0], x.orientation[1], x.orientation[2],
           x.orientation[3], x.fov[0], x.fov[1], x.fov[2], x.fov[3]);
  }
  for (int h = 0; h < 2; ++h) PrintHand(h, d.hands[h]);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
  if (argc >= 5 && strcmp(argv[1], "play") == 0) {
    const int seconds = atoi(argv[4]);
    return Play(argv[2], argv[3], seconds,
                argc >= 6 ? atoi(argv[5]) : seconds,
                argc >= 7 ? atoi(argv[6]) : 0,
                argc >= 8 && strcmp(argv[7], "app") == 0);
  }
  if (argc >= 10 && strcmp(argv[1], "script") == 0) {
    const int code = Script(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]), atoi(argv[6]),
                            argv[7], atoi(argv[8]), atoi(argv[9]),
                            argc >= 11 ? static_cast<float>(atof(argv[10])) / 100.0f : 2.0f,
                            argc >= 12 ? static_cast<float>(atof(argv[11])) / 100.0f : 2.6f);
    fflush(stdout);
    return code;
  }
  if (argc >= 4 && strcmp(argv[1], "compare") == 0) {
    const int code = Compare(argv[2], argv[3]);
    fflush(stdout);
    return code;
  }
  if (argc >= 3 && strcmp(argv[1], "dump") == 0) {
    const int code = Dump(argv[2]);
    fflush(stdout);
    return code;
  }
  printf("bwvr_input_writer play <channel> <record> <seconds> [<head seconds>] [<hz>] [app]\n"
         "bwvr_input_writer script <channel> <script> <seconds> <head seconds> <hz> <log> "
         "<render width> <render height> [<distance cm> <width cm>]\n"
         "bwvr_input_writer compare <record A> <record B>\n"
         "bwvr_input_writer dump <channel>\n");
  return 9;
}
