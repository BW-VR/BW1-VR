// The input channel from the headset app to the mod, a test
// (src/common/input_channel.h, src/mod/xr_input_math.h). main.cpp includes
// this file after Check and Near, just before main.
#pragma once

#include <stddef.h>

#include <atomic>
#include <thread>
#include <vector>

#include "../../src/common/input_channel.h"
#include "../../src/mod/xr_input_math.h"

namespace input_channel_checks {

namespace ic = input_channel;
namespace xm = xr_input_math;

// A slot in which each value follows from k, so a reader can see a torn copy.
inline ic::SlotData Pattern(uint32_t k) {
  ic::SlotData d;
  memset(&d, 0, sizeof(d));
  d.frame = k;
  d.flags = k & 7u;
  d.display_time = static_cast<int64_t>(k) * 1000003;
  d.display_period = 13888888;
  d.recenters = k / 100;
  for (int e = 0; e < 2; ++e) {
    for (int i = 0; i < 3; ++i) d.eyes[e].position[i] = static_cast<float>(k + i);
    d.eyes[e].orientation[3] = static_cast<float>(k);
    d.eyes[e].valid = 1;
  }
  for (int h = 0; h < 2; ++h) {
    d.hands[h].flags = k;
    d.hands[h].trigger = static_cast<float>(k);
    d.hands[h].serial = k;
  }
  d.reserved[3] = k;
  return d;
}

inline bool PatternWhole(const ic::SlotData& d) {
  const uint32_t k = d.frame;
  ic::SlotData want = Pattern(k);
  return memcmp(&want, &d, sizeof(d)) == 0;
}

inline void InputChannelChecks() {
  printf("\nThe input channel from the headset app to the mod\n");

  // THE LAYOUT has one form on 32 and on 64 bits. static_assert in the
  // header holds each size and offset. These lines put them in the output.
  Check(sizeof(ic::File) == 4096 && offsetof(ic::File, slot) == 256 &&
            offsetof(ic::File, events) == 1024,
        "the file has one layout for the mod and the app",
        static_cast<double>(sizeof(ic::File)), 4096);
  Check(offsetof(ic::Slot, data) == 8 && sizeof(ic::SlotData) == 448 &&
            offsetof(ic::SlotData, eyes) == 48 &&
            offsetof(ic::SlotData, hands) == 144 &&
            offsetof(ic::Hand, profile) == 80,
        "the slot has one layout for the mod and the app",
        static_cast<double>(sizeof(ic::SlotData)), 448);

  std::vector<uint8_t> bytes(ic::kFileBytes, 0);
  ic::File* f = reinterpret_cast<ic::File*>(bytes.data());
  Check(!ic::HeadOk(f, bytes.size()), "a file of zeros has no head", 0, 0);
  ic::MakeHead(f);
  Check(ic::HeadOk(f, bytes.size()), "a new head is whole", 1, 1);
  Check(!ic::HeadOk(f, bytes.size() - 1),
        "a file that is shorter than the channel is not a channel", 0, 0);

  // THE SLOT.
  {
    ic::SlotData out = {};
    uint32_t torn = 0;
    ic::Write(&f->slot, Pattern(5));
    bool read = ic::Read(&f->slot, &out, 4, &torn);
    Check(read && PatternWhole(out) && out.frame == 5 && torn == 0,
          "a slot that the writer wrote is read whole at the first try",
          out.frame, 5);
    Check(f->slot.seq == 2, "a write makes the sequence odd and then even",
          f->slot.seq, 2);

    // A TORN READ: the writer begins while the reader copies.
    const uint32_t begin = ic::ReadBegin(&f->slot);
    memcpy(&out, &f->slot.data, sizeof(out));
    ic::WriteBegin(&f->slot);
    f->slot.data = Pattern(6);
    Check(!ic::ReadEnd(&f->slot, begin),
          "a copy while the writer begins is torn", 0, 0);
    ic::SlotData during = {};
    Check(!ic::TryRead(&f->slot, &during),
          "a read while the sequence is odd is torn", f->slot.seq, 3);
    torn = 0;
    read = ic::Read(&f->slot, &during, 3, &torn);
    Check(!read && torn == 3, "each of 3 tries is torn while the writer writes",
          torn, 3);
    ic::WriteEnd(&f->slot);
    torn = 0;
    read = ic::Read(&f->slot, &out, 3, &torn);
    Check(read && out.frame == 6 && torn == 0,
          "after the end of the write the read is whole", out.frame, 6);
    // A copy that a whole write passed: the sequence moved by 2.
    const uint32_t before = ic::ReadBegin(&f->slot);
    ic::Write(&f->slot, Pattern(7));
    Check(!ic::ReadEnd(&f->slot, before),
          "a copy that a whole write passed is torn", 0, 0);
  }

  // THE EVENTS and their wrap.
  {
    ic::MakeHead(f);
    f->head.event_write = f->head.event_read = 0xFFFFFFF0u;
    ic::Event e = {};
    int pushed = 0;
    for (uint32_t i = 0; i < ic::kEvents + 3; ++i) {
      e.frame = i;
      if (ic::PushEvent(f, e)) ++pushed;
    }
    Check(pushed == static_cast<int>(ic::kEvents) && f->head.events_dropped == 3,
          "a full ring takes 64 events and drops the next 3", pushed, 64);
    ic::Event out[ic::kEvents];
    bool broken = true;
    uint32_t n = ic::TakeEvents(f, out, 10, &broken);
    bool order = n == 10 && !broken;
    for (uint32_t i = 0; i < n; ++i) order = order && out[i].frame == i;
    Check(order, "the app takes 10 events in their order", n, 10);
    // The indices pass 2^32 here: 0xFFFFFFF0 + 64 is 0x30.
    Check(f->head.event_write == 0x30u, "the write index wraps at 2^32",
          f->head.event_write, 0x30);
    e.frame = 1000;
    Check(ic::PushEvent(f, e), "after the take there is room again", 1, 1);
    n = ic::TakeEvents(f, out, ic::kEvents, &broken);
    order = n == ic::kEvents - 10 + 1 && !broken;
    for (uint32_t i = 0; i + 1 < n; ++i) order = order && out[i].frame == 10 + i;
    order = order && n > 0 && out[n - 1].frame == 1000;
    Check(order, "the rest comes in its order, over the end of the ring", n,
          ic::kEvents - 9);
    n = ic::TakeEvents(f, out, ic::kEvents, &broken);
    Check(n == 0 && !broken, "an empty ring gives no event", n, 0);
    f->head.event_write = f->head.event_read + ic::kEvents + 5;
    n = ic::TakeEvents(f, out, ic::kEvents, &broken);
    Check(n == 0 && broken && f->head.event_read == f->head.event_write,
          "indices that are not valid give no event, and the app takes the "
          "write index", n, 0);
    f->head.event_write += 4;
    Check(ic::DropOldEvents(f) == 4 && f->head.event_read == f->head.event_write,
          "the app drops the 4 events of an earlier run at its start", 4, 4);
  }

  // THE VALUES OF XRSCREEN. A round trip keeps each value.
  {
    bwvr::EyeView eye = {{0.1f, 1.6f, -0.2f}, {0.0f, 0.38f, 0.0f, 0.92f},
                         -0.9f, 0.8f, 0.85f, -0.95f, true};
    ic::Eye wire;
    xm::FromEye(eye, &wire);
    bwvr::EyeView back;
    xm::ToEye(wire, &back);
    Check(memcmp(back.position, eye.position, sizeof(eye.position)) == 0 &&
              memcmp(back.orientation, eye.orientation,
                     sizeof(eye.orientation)) == 0 &&
              back.fov_left == eye.fov_left && back.fov_right == eye.fov_right &&
              back.fov_up == eye.fov_up && back.fov_down == eye.fov_down &&
              back.valid,
          "an eye goes through the channel with each value", back.fov_down,
          eye.fov_down);
    bwvr::HandInput hand;
    memset(&hand, 0, sizeof(hand));
    hand.active = hand.aim_valid = hand.grip_tracked = true;
    hand.secondary = hand.menu = true;
    hand.aim_position[2] = -0.35f;
    hand.grip_orientation[3] = 1.0f;
    hand.trigger = 0.25f;
    hand.squeeze = 0.59f;
    hand.stick[1] = -1.0f;
    hand.serial = 77;
    ic::Hand hw;
    xm::FromHand(hand, "oculus/touch_controller", &hw);
    bwvr::HandInput hb;
    xm::ToHand(hw, &hb);
    Check(memcmp(&hb, &hand, sizeof(hand)) == 0 &&
              strcmp(hw.profile, "oculus/touch_controller") == 0,
          "a hand goes through the channel with each value and its profile",
          hb.squeeze, hand.squeeze);
    const char long_name[] =
        "a/profile/with/a/name/that/is/longer/than/the/sixty/four/chars/of/it";
    xm::FromHand(hand, long_name, &hw);
    Check(strlen(hw.profile) == ic::kProfileChars - 1,
          "a profile name that is too long is cut to 63 characters",
          static_cast<double>(strlen(hw.profile)), ic::kProfileChars - 1);
    xm::NoHand(&hb);
    Check(!hb.active && !hb.secondary && hb.trigger == 0.0f &&
              hb.aim_orientation[3] == 1.0f && hb.serial == 77,
          "a hand of no input is not active, with each button up",
          static_cast<double>(hb.serial),
          77);
  }

  // THE RECENTER: one recenter for each new count, none for the first slot.
  {
    bool have = false;
    uint32_t seen = 0;
    const bool first = xm::TakeRecenter(3, &have, &seen);
    const bool same = xm::TakeRecenter(3, &have, &seen);
    const bool next = xm::TakeRecenter(4, &have, &seen);
    const bool again = xm::TakeRecenter(4, &have, &seen);
    Check(!first && !same && next && !again,
          "a recenter of the app recenters the mod one time", next ? 1 : 0, 1);
  }

  // A NEW SESSION OF THE APP: its count of the recenters starts at 0. The
  // mod takes it as it is, with no recenter of the player.
  {
    bool have_session = false, have = false;
    uint32_t session = 0, seen = 0;
    const bool first = xm::NewSession(77, &have_session, &session);
    xm::TakeRecenter(3, &have, &seen);
    const bool same = xm::NewSession(77, &have_session, &session);
    const bool other = xm::NewSession(91, &have_session, &session);
    if (other) have = false;
    const bool recenter = xm::TakeRecenter(0, &have, &seen);
    const bool next = xm::TakeRecenter(1, &have, &seen);
    Check(!first && !same && other && !recenter && next,
          "a new session of the app gives no recenter, and its next recenter "
          "counts", recenter ? 1 : 0, 0);
  }

  // THE LOCKSTEP of a test: a slot of frame + 1 is for the frame.
  Check(!xm::SlotFor(0, 0) && xm::SlotFor(1, 0) && !xm::SlotFor(5, 5) &&
            xm::SlotFor(6, 5) && !xm::SlotFor(9, 5),
        "the slot of a frame is the slot with the frame + 1, and no other", 1,
        1);

  // TWO THREADS: a writer writes 200000 slots, and a reader reads all the
  // time. Each read that says whole is whole, and the frames do not go back.
  {
    std::vector<uint8_t> shared(ic::kFileBytes, 0);
    ic::File* g = reinterpret_cast<ic::File*>(shared.data());
    ic::MakeHead(g);
    const uint32_t writes = 200000;
    std::atomic<bool> done(false);
    std::thread writer([g, writes, &done]() {
      for (uint32_t k = 1; k <= writes; ++k) ic::Write(&g->slot, Pattern(k));
      done = true;
    });
    uint64_t reads = 0, whole = 0, bad = 0, back = 0, torn_reads = 0;
    uint32_t last = 0;
    while (!done || last < writes) {
      ic::SlotData d;
      uint32_t torn = 0;
      ++reads;
      if (!ic::Read(&g->slot, &d, 1, &torn)) {
        ++torn_reads;
        continue;
      }
      if (d.frame == 0) continue;  // before the first write
      ++whole;
      if (!PatternWhole(d)) ++bad;
      if (d.frame < last) ++back;
      last = d.frame;
    }
    writer.join();
    printf("  (%llu reads, %llu whole, %llu torn)\n",
           static_cast<unsigned long long>(reads),
           static_cast<unsigned long long>(whole),
           static_cast<unsigned long long>(torn_reads));
    Check(bad == 0 && back == 0 && last == writes && whole > 0,
          "two threads: each whole read of the slot is one write, and the "
          "frames do not go back", static_cast<double>(bad), 0);

    // The events in two threads: 100000 events through a ring of 64.
    ic::MakeHead(g);
    const uint32_t events = 100000;
    std::thread pusher([g, events]() {
      for (uint32_t i = 0; i < events;) {
        ic::Event e = {i, i & 1u, 0.5f, 0.01f};
        if (ic::PushEvent(g, e)) {
          ++i;
        } else {
          std::this_thread::yield();
        }
      }
    });
    uint32_t next = 0;
    bool in_order = true;
    ic::Event got[ic::kEvents];
    while (next < events) {
      bool broken = false;
      const uint32_t n = ic::TakeEvents(g, got, ic::kEvents, &broken);
      if (broken) in_order = false;
      for (uint32_t i = 0; i < n; ++i) {
        if (got[i].frame != next || got[i].hand != (next & 1u)) in_order = false;
        ++next;
      }
      if (n == 0) std::this_thread::yield();
    }
    pusher.join();
    Check(in_order && next == events,
          "two threads: 100000 events go through a ring of 64, each in its "
          "order", next, events);
  }
}

}  // namespace input_channel_checks
