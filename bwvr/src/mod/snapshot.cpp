// The snapshot for a report. See snapshot.h and docs/controllers.md, "The
// snapshot".
//
// WHY. A player in the headset who sees a fault has no keyboard for F12 and
// does not know where the log is. The item "snapshot" of the ring saves what
// a report needs in one folder: the eye pictures of that moment and the log
// of the run until then.
//
// HOW. The item asks the eye capture (eye_capture.cpp) for the next stereo
// frame, as F12 does. That frame writes its pictures beside runblack.exe as
// bwvr-<kind>-<frame>.bmp: the eyes of the frame, the images of the Vulkan
// renderer, and in a session the images of the eye swapchains. 1 s later a
// thread of its own moves each picture of that frame into the folder
// bwvr\captures\snapshot-<date>-<time> of the game folder. It writes each one
// as a PNG with no alpha (Windows Imaging Component), and it deletes the
// BMP. A picture that WIC cannot write stays a BMP in the folder. The
// picture "before" (the target before the eye passes) is not a picture of
// the view, so it stays where it is. Then the thread copies bwvr.log into
// the folder. The game thread does no file work, so the frame does not stop.
//
//   snapshot_item=1        0: the word "snapshot" of the ring is an empty
//                          slot
//   snapshot_test_dir      a test key: the folders go into this folder, and
//                          not into bwvr\captures of the game folder

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wincodec.h>

#include "snapshot.h"

extern void ModLog(const char* format, ...);

// eye_capture.cpp
void EyeCaptureArmNext();
uint64_t EyeCaptureNextArmed();

namespace {

bool g_read = false;
bool g_on = true;
char g_dir[MAX_PATH] = "";       // the game folder, with a backslash
char g_base[MAX_PATH] = "";      // the folder of the snapshot folders

enum Stage { kIdle, kWaitFrame, kWaitFiles, kWriting };
int g_stage = kIdle;
ULONGLONG g_since = 0;           // the tick of the start, or of the arm
uint64_t g_asked_frame = 0;      // the frame of the controllers
HANDLE g_thread = nullptr;
volatile LONG g_result = 0;      // the thread: 0 runs, 1 saved, -1 failed
wchar_t g_note[48] = L"";

// The pictures wait this long for a frame, and the files this long after it.
const ULONGLONG kFrameWaitMs = 3000;
const ULONGLONG kFileWaitMs = 1000;

// The work of the thread, and what it did.
struct Job {
  char dir[MAX_PATH];
  char folder[MAX_PATH];
  uint64_t frame;     // the stereo frame of the pictures, 0 for none
  FILETIME since;     // a picture older than this is not of this snapshot
  int png;
  int bmp;
  int failed;
  bool log;
  double ms;
};
Job g_job;

int IniKey(const char* key, int fallback) {
  char path[MAX_PATH];
  lstrcpynA(path, g_dir, MAX_PATH);
  lstrcatA(path, "bwvr.ini");
  return static_cast<int>(GetPrivateProfileIntA("bwvr", key, fallback, path));
}

void ReadSettings() {
  if (g_read) return;
  g_read = true;
  GetModuleFileNameA(nullptr, g_dir, MAX_PATH);
  char* slash = strrchr(g_dir, '\\');
  if (slash != nullptr) slash[1] = 0;
  g_on = IniKey("snapshot_item", 1) != 0;
  char path[MAX_PATH], test[MAX_PATH] = "";
  lstrcpynA(path, g_dir, MAX_PATH);
  lstrcatA(path, "bwvr.ini");
  GetPrivateProfileStringA("bwvr", "snapshot_test_dir", "", test, MAX_PATH,
                           path);
  if (test[0] != 0) {
    lstrcpynA(g_base, test, MAX_PATH);
  } else {
    _snprintf_s(g_base, MAX_PATH, _TRUNCATE, "%sbwvr\\captures", g_dir);
  }
  ModLog("snapshot     : snapshot_item=%d. %s%s", g_on ? 1 : 0,
         g_on ? "The ring item \"snapshot\" saves the eye pictures and a copy "
                "of the log in a folder in "
              : "No snapshot item",
         g_on ? g_base : "");
}

double NowMs() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return static_cast<double>(count.QuadPart) * 1000.0 /
         static_cast<double>(frequency.QuadPart);
}

// Makes each folder of the path that is not there.
bool MakeFolders(const char* path) {
  char part[MAX_PATH];
  lstrcpynA(part, path, MAX_PATH);
  for (char* p = part + 3; *p != 0; ++p) {
    if (*p != '\\' && *p != '/') continue;
    const char keep = *p;
    *p = 0;
    CreateDirectoryA(part, nullptr);
    *p = keep;
  }
  CreateDirectoryA(part, nullptr);
  const DWORD a = GetFileAttributesA(part);
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

template <typename T>
void Release(T*& p) {
  if (p != nullptr) p->Release();
  p = nullptr;
}

// A BMP as a PNG of 24 bits: no alpha, because the alpha of some eye
// pictures is 0.
bool WritePng(IWICImagingFactory* factory, const char* from, const char* to) {
  wchar_t wfrom[MAX_PATH], wto[MAX_PATH];
  if (MultiByteToWideChar(CP_ACP, 0, from, -1, wfrom, MAX_PATH) == 0 ||
      MultiByteToWideChar(CP_ACP, 0, to, -1, wto, MAX_PATH) == 0) {
    return false;
  }
  IWICBitmapDecoder* decoder = nullptr;
  IWICBitmapFrameDecode* source = nullptr;
  IWICFormatConverter* converter = nullptr;
  IWICStream* stream = nullptr;
  IWICBitmapEncoder* encoder = nullptr;
  IWICBitmapFrameEncode* target = nullptr;
  IPropertyBag2* bag = nullptr;
  UINT w = 0, h = 0;
  WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
  bool ok =
      SUCCEEDED(factory->CreateDecoderFromFilename(
          wfrom, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
          &decoder)) &&
      SUCCEEDED(decoder->GetFrame(0, &source)) &&
      SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
      SUCCEEDED(converter->Initialize(source, GUID_WICPixelFormat24bppBGR,
                                      WICBitmapDitherTypeNone, nullptr, 0.0,
                                      WICBitmapPaletteTypeCustom)) &&
      SUCCEEDED(converter->GetSize(&w, &h)) &&
      SUCCEEDED(factory->CreateStream(&stream)) &&
      SUCCEEDED(stream->InitializeFromFilename(wto, GENERIC_WRITE)) &&
      SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr,
                                       &encoder)) &&
      SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
      SUCCEEDED(encoder->CreateNewFrame(&target, &bag)) &&
      SUCCEEDED(target->Initialize(bag)) &&
      SUCCEEDED(target->SetSize(w, h)) &&
      SUCCEEDED(target->SetPixelFormat(&format)) &&
      IsEqualGUID(format, GUID_WICPixelFormat24bppBGR) &&
      SUCCEEDED(target->WriteSource(converter, nullptr)) &&
      SUCCEEDED(target->Commit()) && SUCCEEDED(encoder->Commit());
  Release(bag);
  Release(target);
  Release(encoder);
  Release(stream);
  Release(converter);
  Release(source);
  Release(decoder);
  if (!ok) DeleteFileA(to);
  return ok;
}

// A copy of the log. The mod keeps bwvr.log open for its writes, so the read
// must share the write.
bool CopyLog(const char* dir, const char* folder) {
  char from[MAX_PATH], to[MAX_PATH];
  _snprintf_s(from, MAX_PATH, _TRUNCATE, "%sbwvr.log", dir);
  _snprintf_s(to, MAX_PATH, _TRUNCATE, "%s\\bwvr.log", folder);
  HANDLE in = CreateFileA(from, GENERIC_READ,
                          FILE_SHARE_READ | FILE_SHARE_WRITE |
                              FILE_SHARE_DELETE,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                          nullptr);
  if (in == INVALID_HANDLE_VALUE) return false;
  HANDLE out = CreateFileA(to, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
  if (out == INVALID_HANDLE_VALUE) {
    CloseHandle(in);
    return false;
  }
  static char buffer[64 * 1024];
  bool ok = true;
  for (;;) {
    DWORD got = 0, put = 0;
    if (!ReadFile(in, buffer, sizeof(buffer), &got, nullptr)) {
      ok = false;
      break;
    }
    if (got == 0) break;
    if (!WriteFile(out, buffer, got, &put, nullptr) || put != got) {
      ok = false;
      break;
    }
  }
  CloseHandle(out);
  CloseHandle(in);
  return ok;
}

DWORD WINAPI Work(void*) {
  Job& j = g_job;
  const double start = NowMs();
  if (!MakeFolders(j.folder)) {
    j.ms = NowMs() - start;
    InterlockedExchange(&g_result, -1);
    return 0;
  }
  const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  IWICImagingFactory* factory = nullptr;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                              CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&factory)))) {
    factory = nullptr;
  }
  if (j.frame != 0) {
    char pattern[MAX_PATH];
    _snprintf_s(pattern, MAX_PATH, _TRUNCATE, "%sbwvr-*-%05llu.bmp", j.dir,
                j.frame);
    WIN32_FIND_DATAA found;
    const HANDLE find = FindFirstFileA(pattern, &found);
    if (find != INVALID_HANDLE_VALUE) {
      do {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            strncmp(found.cFileName, "bwvr-before-", 12) == 0 ||
            CompareFileTime(&found.ftLastWriteTime, &j.since) < 0) {
          continue;
        }
        // bwvr-vk-left-01234.bmp is vk-left.png in the folder.
        char kind[MAX_PATH];
        lstrcpynA(kind, found.cFileName + 5, MAX_PATH);
        char* dash = strrchr(kind, '-');
        if (dash != nullptr) *dash = 0;
        char from[MAX_PATH], to[MAX_PATH];
        _snprintf_s(from, MAX_PATH, _TRUNCATE, "%s%s", j.dir,
                    found.cFileName);
        _snprintf_s(to, MAX_PATH, _TRUNCATE, "%s\\%s.png", j.folder, kind);
        if (factory != nullptr && WritePng(factory, from, to)) {
          DeleteFileA(from);
          ++j.png;
          continue;
        }
        _snprintf_s(to, MAX_PATH, _TRUNCATE, "%s\\%s.bmp", j.folder, kind);
        if (MoveFileExA(from, to, MOVEFILE_COPY_ALLOWED |
                                      MOVEFILE_REPLACE_EXISTING)) {
          ++j.bmp;
        } else {
          ++j.failed;
        }
      } while (FindNextFileA(find, &found));
      FindClose(find);
    }
  }
  Release(factory);
  if (SUCCEEDED(com)) CoUninitialize();
  // The last line before the copy says what the folder has.
  ModLog("snapshot     : %d pictures as PNG and %d as BMP in %s (%d could not "
         "move). The copy of the log ends with this line", j.png, j.bmp,
         j.folder, j.failed);
  j.log = CopyLog(j.dir, j.folder);
  j.ms = NowMs() - start;
  InterlockedExchange(&g_result, j.log && j.png + j.bmp > 0 ? 1 : -1);
  return 0;
}

void StartWork(uint64_t stereo_frame) {
  memset(&g_job, 0, sizeof(g_job));
  lstrcpynA(g_job.dir, g_dir, MAX_PATH);
  SYSTEMTIME t;
  GetLocalTime(&t);
  _snprintf_s(g_job.folder, MAX_PATH, _TRUNCATE,
              "%s\\snapshot-%04d%02d%02d-%02d%02d%02d", g_base, t.wYear,
              t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
  g_job.frame = stereo_frame;
  // A picture of an earlier run with the same number is not of this one.
  // The clock of a file can be some seconds off, so 10 s before the ask.
  FILETIME now;
  GetSystemTimeAsFileTime(&now);
  ULARGE_INTEGER v;
  v.LowPart = now.dwLowDateTime;
  v.HighPart = now.dwHighDateTime;
  v.QuadPart -= 10ull * 10000000ull;
  g_job.since.dwLowDateTime = v.LowPart;
  g_job.since.dwHighDateTime = v.HighPart;
  InterlockedExchange(&g_result, 0);
  g_thread = CreateThread(nullptr, 0, &Work, nullptr, 0, nullptr);
  if (g_thread == nullptr) InterlockedExchange(&g_result, -1);
  g_stage = kWriting;
}

}  // namespace

bool SnapshotOn() {
  ReadSettings();
  return g_on;
}

bool SnapshotBusy() { return g_stage != kIdle; }

bool SnapshotStart(uint64_t frame) {
  ReadSettings();
  if (!g_on || g_stage != kIdle) return false;
  EyeCaptureArmNext();
  g_stage = kWaitFrame;
  g_since = GetTickCount64();
  g_asked_frame = frame;
  g_note[0] = 0;
  ModLog("snapshot     : the player asked for a snapshot, frame %llu. The next "
         "frame saves the eye pictures", frame);
  return true;
}

const wchar_t* SnapshotNote() { return g_note; }

int SnapshotFrame() {
  if (g_stage == kIdle) return 0;
  const ULONGLONG now = GetTickCount64();
  if (g_stage == kWaitFrame) {
    const uint64_t armed = EyeCaptureNextArmed();
    if (armed != 0) {
      g_stage = kWaitFiles;
      g_since = now;
      g_job.frame = armed;
    } else if (now - g_since > kFrameWaitMs) {
      // No stereo frame asked the eye capture: the folder gets the log.
      ModLog("snapshot     : no frame saved the eye pictures in %llu ms, so "
             "the folder gets the log only", kFrameWaitMs);
      StartWork(0);
    }
    return 0;
  }
  if (g_stage == kWaitFiles) {
    if (now - g_since >= kFileWaitMs) StartWork(g_job.frame);
    return 0;
  }
  const LONG result = InterlockedCompareExchange(&g_result, 0, 0);
  if (result == 0) return 0;
  if (g_thread != nullptr) {
    WaitForSingleObject(g_thread, INFINITE);
    CloseHandle(g_thread);
    g_thread = nullptr;
  }
  g_stage = kIdle;
  if (result > 0) {
    wcscpy_s(g_note, L"Saved in bwvr\\captures");
    ModLog("snapshot     : saved, asked at frame %llu: the pictures of stereo "
           "frame %llu (%d as PNG, %d as BMP) and the log in %s, in %.0f ms on "
           "a thread of its own", g_asked_frame, g_job.frame, g_job.png,
           g_job.bmp, g_job.folder, g_job.ms);
    return 1;
  }
  wcscpy_s(g_note, g_job.log ? L"No picture to save" : L"The save failed");
  ModLog("snapshot     : the snapshot of frame %llu did not save all of it: %d "
         "pictures as PNG, %d as BMP, %d that could not move, the log %s, the "
         "folder %s", g_asked_frame, g_job.png, g_job.bmp, g_job.failed,
         g_job.log ? "copied" : "not copied", g_job.folder);
  return -1;
}
