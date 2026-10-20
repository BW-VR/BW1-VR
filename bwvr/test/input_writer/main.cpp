// THE TEST WRITER OF THE INPUT CHANNEL (docs/android.md, "The input
// channel"). A Windows program for a desktop run, with the channel code of
// the mod and the app (src/common/input_channel.h). It does what the headset
// app does, with the input of a record in place of a session.
//
//   bwvr_input_writer play <channel> <record> <seconds>
//     Waits for the head of the mod in the channel file. Then for each frame
//     of the controllers that the mod latches (Head::want), it writes the
//     input of the same frame of the record into the slot, with the frame +
//     1 as its tag (the lockstep of xr_input_lockstep_ms). After the end of
//     the record it writes the last frame of the record. It takes each pulse
//     of the ring of the events and writes one line for it. It ends after
//     <seconds>.
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

#include <vector>

#include "../../src/common/input_channel.h"

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

int Play(const char* channel, const char* record, int seconds) {
  std::vector<ic::SlotData> frames;
  uint32_t source = 0;
  if (!ReadRecord(record, &frames, &source) || frames.empty()) return 2;
  printf("the record has %zu frames of the controllers (source %u)\n",
         frames.size(), source);
  fflush(stdout);
  const ULONGLONG end = GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000;
  Mapped m;
  while (GetTickCount64() < end) {
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
    printf("no channel of the mod at %s in %d s\n", channel, seconds);
    return 3;
  }
  ic::File* f = m.view;
  ic::StoreRelease(&f->head.app_session, GetCurrentProcessId());
  ic::DropOldEvents(f);
  printf("the mod made the channel (session %u). The writer writes the "
         "record\n", f->head.mod_session);
  fflush(stdout);

  uint64_t writes = 0;
  uint32_t last_want = 0;
  uint32_t pulses = 0;
  bool seen_want = false;
  while (GetTickCount64() < end) {
    const uint32_t want = ic::LoadAcquire(&f->head.want);
    const uint32_t tag = want + 1;
    if (f->slot.data.frame != tag) {
      ic::SlotData d = frames[want < frames.size() ? want : frames.size() - 1];
      d.frame = tag;
      ic::Write(&f->slot, d);
      ++writes;
      last_want = want;
      seen_want = true;
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
  m.Close();
  return writes > 0 ? 0 : 4;
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
    return Play(argv[2], argv[3], atoi(argv[4]));
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
  printf("bwvr_input_writer play <channel> <record> <seconds>\n"
         "bwvr_input_writer compare <record A> <record B>\n"
         "bwvr_input_writer dump <channel>\n");
  return 9;
}
