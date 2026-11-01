// The gaze of an eye tracker from OSC messages over UDP. See eye_osc.h and
// docs/backend.md, "Foveated rendering".

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "eye_osc.h"
#include "osc_gaze_math.h"

void ModLog(const char* format, ...);

namespace {

// The thread waits this long for a packet, then looks at the stop flag.
const DWORD kReceiveMs = 200;
// The log gets a line for each new address, for this many addresses.
const int kAddressLines = 16;

HANDLE g_thread = nullptr;
SOCKET g_socket = INVALID_SOCKET;
volatile LONG g_stop = 0;
int g_port = 0;
bool g_any = false;

// THE LAST GAZE. g_lock guards each field of this group.
SRWLOCK g_lock = SRWLOCK_INIT;
bool g_have_gaze = false;
float g_gaze[3] = {0.0f, 0.0f, -1.0f};
LARGE_INTEGER g_gaze_at = {};

// The counts since the last report. The thread writes them.
volatile LONG g_packets = 0;
volatile LONG g_messages = 0;
volatile LONG g_gazes = 0;

// The addresses that the log has, so that each gets one line.
char g_addresses[kAddressLines][64];
int g_address_count = 0;

void LogAddress(const osc_gaze_math::Message& m, bool gaze) {
  for (int i = 0; i < g_address_count; ++i) {
    if (strncmp(g_addresses[i], m.address, sizeof(g_addresses[i]) - 1) == 0) {
      return;
    }
  }
  if (g_address_count >= kAddressLines) return;
  strncpy_s(g_addresses[g_address_count], m.address, _TRUNCATE);
  ++g_address_count;
  ModLog("eye osc      : a message %s with %d numbers%s", m.address, m.count,
         gaze ? ", a gaze that the mod uses"
              : (m.other ? " and other arguments, not a gaze"
                         : ", not a gaze"));
}

struct Visit {
  void operator()(const osc_gaze_math::Message& m) {
    float direction[3];
    const bool gaze = osc_gaze_math::GazeOf(m, direction);
    LogAddress(m, gaze);
    if (!gaze) return;
    InterlockedIncrement(&g_gazes);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    AcquireSRWLockExclusive(&g_lock);
    for (int i = 0; i < 3; ++i) g_gaze[i] = direction[i];
    g_gaze_at = now;
    g_have_gaze = true;
    ReleaseSRWLockExclusive(&g_lock);
  }
};

DWORD WINAPI ReceiveThread(void*) {
  static uint8_t packet[8192];
  Visit visit;
  while (InterlockedCompareExchange(&g_stop, 0, 0) == 0) {
    const int bytes = recv(g_socket, reinterpret_cast<char*>(packet),
                           sizeof(packet), 0);
    if (bytes == SOCKET_ERROR) {
      const int error = WSAGetLastError();
      // A wait with no packet, or a packet that is larger than the buffer.
      if (error == WSAETIMEDOUT || error == WSAEMSGSIZE) continue;
      // An ICMP of a port that is not reachable, for a packet that this
      // socket did not send. It does not stop the receive.
      if (error == WSAECONNRESET) continue;
      if (InterlockedCompareExchange(&g_stop, 0, 0) == 0) {
        ModLog("eye osc      : the receive failed (WSA error %d). The gaze "
               "of OSC stops", error);
      }
      break;
    }
    InterlockedIncrement(&g_packets);
    const int messages = osc_gaze_math::ForEachMessage(
        packet, static_cast<size_t>(bytes), visit);
    InterlockedExchangeAdd(&g_messages, messages);
  }
  return 0;
}

}  // namespace

void EyeOscStart(int port, bool any_address) {
  if (g_thread != nullptr || port <= 0) return;
  if (port > 65535) {
    ModLog("eye osc      : eye_osc_port=%d is not a port. The mod reads no "
           "gaze from OSC", port);
    return;
  }
  WSADATA wsa;
  const int started = WSAStartup(MAKEWORD(2, 2), &wsa);
  if (started != 0) {
    ModLog("eye osc      : WSAStartup failed (%d). The mod reads no gaze from "
           "OSC", started);
    return;
  }
  g_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (g_socket == INVALID_SOCKET) {
    ModLog("eye osc      : no UDP socket (WSA error %d). The mod reads no "
           "gaze from OSC", WSAGetLastError());
    WSACleanup();
    return;
  }
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<u_short>(port));
  address.sin_addr.s_addr = htonl(any_address ? INADDR_ANY : INADDR_LOOPBACK);
  if (bind(g_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) ==
      SOCKET_ERROR) {
    ModLog("eye osc      : port %d is not free (WSA error %d), for example "
           "because VRChat or a tool of the eye tracker uses it. Set another "
           "port in eye_osc_port and in the program that sends the gaze. The "
           "mod reads no gaze from OSC", port, WSAGetLastError());
    closesocket(g_socket);
    g_socket = INVALID_SOCKET;
    WSACleanup();
    return;
  }
  const DWORD timeout = kReceiveMs;
  setsockopt(g_socket, SOL_SOCKET, SO_RCVTIMEO,
             reinterpret_cast<const char*>(&timeout), sizeof(timeout));
  InterlockedExchange(&g_stop, 0);
  g_thread = CreateThread(nullptr, 0, ReceiveThread, nullptr, 0, nullptr);
  if (g_thread == nullptr) {
    ModLog("eye osc      : no thread (error %lu). The mod reads no gaze from "
           "OSC", GetLastError());
    closesocket(g_socket);
    g_socket = INVALID_SOCKET;
    WSACleanup();
    return;
  }
  g_port = port;
  g_any = any_address;
  ModLog("eye osc      : receives the gaze of an eye tracker as OSC on %s:%d "
         "(eye_osc_port%s). Steam Link of the Steam Frame and other "
         "senders of /tracking/eye/... can send to it",
         any_address ? "all addresses of the PC" : "127.0.0.1", port,
         any_address ? ", eye_osc_any=1" : "");
}

void EyeOscStop() {
  if (g_thread == nullptr) return;
  InterlockedExchange(&g_stop, 1);
  if (WaitForSingleObject(g_thread, kReceiveMs * 5) != WAIT_OBJECT_0) {
    // closesocket ends a recv that waits.
    closesocket(g_socket);
    g_socket = INVALID_SOCKET;
    WaitForSingleObject(g_thread, kReceiveMs * 5);
  }
  CloseHandle(g_thread);
  g_thread = nullptr;
  if (g_socket != INVALID_SOCKET) closesocket(g_socket);
  g_socket = INVALID_SOCKET;
  WSACleanup();
  ModLog("eye osc      : stopped on port %d", g_port);
}

bool EyeOscGaze(float direction[3]) {
  if (g_thread == nullptr) return false;
  LARGE_INTEGER now, frequency;
  QueryPerformanceCounter(&now);
  QueryPerformanceFrequency(&frequency);
  AcquireSRWLockShared(&g_lock);
  const bool have = g_have_gaze;
  const LARGE_INTEGER at = g_gaze_at;
  for (int i = 0; i < 3; ++i) direction[i] = g_gaze[i];
  ReleaseSRWLockShared(&g_lock);
  if (!have) return false;
  const double age_ms = static_cast<double>(now.QuadPart - at.QuadPart) *
                        1000.0 / static_cast<double>(frequency.QuadPart);
  return osc_gaze_math::Fresh(age_ms);
}

void EyeOscReport() {
  if (g_thread == nullptr) return;
  const LONG packets = InterlockedExchange(&g_packets, 0);
  const LONG messages = InterlockedExchange(&g_messages, 0);
  const LONG gazes = InterlockedExchange(&g_gazes, 0);
  // With no sender the log gets one line, and not one for each report.
  static LONG s_last_packets = -1;
  const bool quiet = packets == 0 && s_last_packets == 0;
  s_last_packets = packets;
  if (quiet) return;
  ModLog("eye osc      : %ld packets, %ld messages, %ld gazes on %s:%d", packets,
         messages, gazes, g_any ? "all addresses" : "127.0.0.1", g_port);
}
