// THE GPU PARTS OF A WALK (gpu_parts, docs/android.md, "The GPU parts of the
// walk"). The pure rules, with no Vulkan, so the math test of the PC checks
// them (test/controller_math/walk_parts_checks.h). walk_probe.cpp gives the
// calls of the renderer of the mod to a Recording: a timestamp goes in at the
// start of each command buffer, before each start of a pass
// (vkCmdBeginRendering), after each end of a pass and before the end of the
// command buffer. Each part is the time between two timestamps that follow.
// A part of a pass has the key of the pass (its views, its size, its clear
// and its map of the rates). A part between two passes has the kind of the
// most costly command in it: uploads, copies and blits, read backs, or only
// barriers.
#pragma once

#include <stdint.h>

namespace walk_parts {

// The most timestamps in one command buffer. After the last one the rest of
// the command buffer is one part (kAfterLimit).
constexpr uint32_t kMostMarks = 48;
// The most keys of passes that the totals keep apart. A pass with another
// key goes into kOtherPasses.
constexpr int kMostPassKeys = 10;

enum Gap : int {
  kUploads = 0,  // vkCmdCopyBufferToImage
  kCopies,       // vkCmdBlitImage, vkCmdCopyImage
  kReadBacks,    // vkCmdCopyImageToBuffer
  kBarriers,     // only barriers, the start and the end
  kAfterLimit,   // the rest after kMostMarks timestamps
  kOtherPasses,  // the passes with a key that did not fit
  kGaps
};

struct PassKey {
  uint32_t views = 0;  // the bits of the view mask: 0 is one layer with no eye
  uint32_t width = 0;
  uint32_t height = 0;
  bool clear = false;  // the color starts cleared, else it is loaded
  bool rate = false;   // the pass has the map of the shading rates
  bool density = false;  // the pass has a fragment density map (the one pass)
  bool Same(const PassKey& o) const {
    return views == o.views && width == o.width && height == o.height && clear == o.clear &&
           rate == o.rate && density == o.density;
  }
};

inline uint32_t ViewCount(uint32_t mask) {
  uint32_t n = 0;
  for (; mask != 0; mask &= mask - 1) ++n;
  return n;
}

// The counts of a command buffer, also with no timestamp.
struct Counts {
  uint64_t passes = 0;
  uint64_t draws = 0;
  uint64_t pipelines = 0;     // vkCmdBindPipeline
  uint64_t sets = 0;          // vkCmdBindDescriptorSets
  uint64_t writes = 0;        // the descriptor writes of vkUpdateDescriptorSets
  uint64_t uploads = 0;       // the regions of vkCmdCopyBufferToImage
  uint64_t upload_bytes = 0;  // their texels, 4 bytes each
  void Add(const Counts& o) {
    passes += o.passes;
    draws += o.draws;
    pipelines += o.pipelines;
    sets += o.sets;
    writes += o.writes;
    uploads += o.uploads;
    upload_bytes += o.upload_bytes;
  }
};

// Part i: from timestamp i to timestamp i + 1.
struct Part {
  bool pass = false;
  PassKey key;   // for a pass
  int gap = kBarriers;  // for a part between passes
  uint64_t draws = 0;
};

// ONE COMMAND BUFFER IN RECORDING. Each call that gives true wants a
// timestamp now, with the index *mark (0 is the first timestamp of the
// command buffer).
class Recording {
 public:
  bool Begin(uint32_t* mark) {
    marks_ = 0;
    open_ = -1;
    limit_ = false;
    counts_ = Counts();
    in_pass_ = false;
    return Mark(mark, false, PassKey());
  }
  bool PassBegin(const PassKey& key, uint32_t* mark) {
    ++counts_.passes;
    const bool m = Mark(mark, true, key);
    in_pass_ = true;
    return m;
  }
  bool PassEnd(uint32_t* mark) {
    in_pass_ = false;
    return Mark(mark, false, PassKey());
  }
  // The end of the command buffer: the last timestamp closes the last part.
  bool End(uint32_t* mark) {
    if (marks_ >= kMostMarks || marks_ == 0) return false;
    *mark = marks_++;
    return true;
  }
  void Draw() {
    ++counts_.draws;
    if (open_ >= 0) ++parts_[open_].draws;
  }
  void Upload(uint64_t regions, uint64_t texels) {
    counts_.uploads += regions;
    counts_.upload_bytes += texels * 4;
    Between(kUploads);
  }
  void Copy() { Between(kCopies); }
  void ReadBack() { Between(kReadBacks); }
  void Pipeline() { ++counts_.pipelines; }
  void Sets() { ++counts_.sets; }
  void Writes(uint64_t n) { counts_.writes += n; }

  // The timestamps of the command buffer. The parts are 0 to Marks() - 2.
  uint32_t Marks() const { return marks_; }
  const Part& PartAt(uint32_t i) const { return parts_[i]; }
  const Counts& CountsOf() const { return counts_; }

 private:
  bool Mark(uint32_t* mark, bool pass, const PassKey& key) {
    if (marks_ >= kMostMarks - 1) {
      // The last free timestamp stays for End: the part from here on is one.
      if (open_ >= 0 && !limit_) {
        limit_ = true;
        parts_[open_].pass = false;
        parts_[open_].gap = kAfterLimit;
      }
      return false;
    }
    *mark = marks_;
    Part& p = parts_[marks_];
    p = Part();
    p.pass = pass;
    p.key = key;
    open_ = static_cast<int>(marks_);
    ++marks_;
    return true;
  }
  // A command between passes: the part gets the kind of the most costly one
  // (an upload before a copy before a read back before a barrier).
  void Between(int kind) {
    if (in_pass_ || open_ < 0 || limit_) return;
    Part& p = parts_[open_];
    if (!p.pass && kind < p.gap) p.gap = kind;
  }

  uint32_t marks_ = 0;
  int open_ = -1;
  bool in_pass_ = false;
  bool limit_ = false;
  Counts counts_;
  Part parts_[kMostMarks];
};

// A stat of one part in the totals: the ms over all walks, the most ms in
// one walk, and the count of the parts with this label.
struct PartStat {
  double ms = 0.0;
  double most = 0.0;
  uint64_t count = 0;
  uint64_t draws = 0;
};

// THE TOTALS of the walks since the last Reset. A walk is a command buffer of
// the renderer with one pass or more. A command buffer with no pass (the
// uploads of a walk, a read back) adds its parts and its counts, but it is no
// walk.
class Totals {
 public:
  // The parts of a finished command buffer, with the ms of each part
  // (ms[i] for the part i, Marks() - 1 values), or null when it has no times.
  void Add(const Recording& r, const double* ms) {
    const Counts& c = r.CountsOf();
    counts_.Add(c);
    if (c.passes > 0) ++walks_;
    if (ms == nullptr || r.Marks() < 2) {
      if (c.passes > 0) ++untimed_;
      return;
    }
    double walk_pass[kMostPassKeys] = {};
    double walk_gap[kGaps] = {};
    for (uint32_t i = 0; i + 1 < r.Marks(); ++i) {
      const Part& p = r.PartAt(i);
      const double t = ms[i] < 0.0 ? 0.0 : ms[i];
      PartStat* s = nullptr;
      double* w = nullptr;
      if (p.pass) {
        const int k = KeyIndex(p.key);
        if (k >= 0) {
          s = &pass_[k];
          w = &walk_pass[k];
        } else {
          s = &gap_[kOtherPasses];
          w = &walk_gap[kOtherPasses];
        }
      } else {
        s = &gap_[p.gap];
        w = &walk_gap[p.gap];
      }
      s->ms += t;
      s->draws += p.draws;
      ++s->count;
      *w += t;
      total_ms_ += t;
    }
    for (int k = 0; k < keys_; ++k) {
      if (walk_pass[k] > pass_[k].most) pass_[k].most = walk_pass[k];
    }
    for (int g = 0; g < kGaps; ++g) {
      if (walk_gap[g] > gap_[g].most) gap_[g].most = walk_gap[g];
    }
  }
  void Reset() {
    // The keys stay, so a key keeps its place in each line.
    for (PartStat& s : pass_) s = PartStat();
    for (PartStat& s : gap_) s = PartStat();
    counts_ = Counts();
    walks_ = untimed_ = 0;
    total_ms_ = 0.0;
  }
  uint64_t Walks() const { return walks_; }
  uint64_t Untimed() const { return untimed_; }
  double TotalMs() const { return total_ms_; }
  const Counts& CountsOf() const { return counts_; }
  int Keys() const { return keys_; }
  const PassKey& Key(int k) const { return key_[k]; }
  const PartStat& Pass(int k) const { return pass_[k]; }
  const PartStat& GapStat(int g) const { return gap_[g]; }

 private:
  int KeyIndex(const PassKey& key) {
    for (int k = 0; k < keys_; ++k) {
      if (key_[k].Same(key)) return k;
    }
    if (keys_ >= kMostPassKeys) return -1;
    key_[keys_] = key;
    return keys_++;
  }
  PassKey key_[kMostPassKeys];
  int keys_ = 0;
  PartStat pass_[kMostPassKeys];
  PartStat gap_[kGaps];
  Counts counts_;
  uint64_t walks_ = 0;
  uint64_t untimed_ = 0;
  double total_ms_ = 0.0;
};

// gpu_parts of bwvr.ini: each value but 0 is on.
inline bool On(int key) { return key != 0; }

}  // namespace walk_parts
