#include "stream_reader.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../../src/mod/stream_ring.h"
#include "../../src/mod/stream_split.h"
#include "app_log.h"
#include "game_folder.h"

namespace {

namespace sr = stream_ring;
namespace ss = scene_stream;

constexpr size_t kLimit = 256u << 20;  // the largest record that can be in the stream

double NowMs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

void SleepUs(long us) {
  timespec t = {us / 1000000, (us % 1000000) * 1000};
  nanosleep(&t, nullptr);
}

struct Map {
  int fd = -1;
  uint8_t* view = nullptr;
  size_t bytes = 0;
  sr::Head* head = nullptr;
  void Close() {
    if (view != nullptr) munmap(view, bytes);
    if (fd >= 0) close(fd);
    *this = Map();
  }
};

// The ring file with a whole head, or false.
bool Open(const std::string& path, Map* m) {
  m->fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
  if (m->fd < 0) return false;
  struct stat st;
  if (fstat(m->fd, &st) != 0 || st.st_size < static_cast<off_t>(sr::kHeadBytes)) {
    m->Close();
    return false;
  }
  m->bytes = static_cast<size_t>(st.st_size);
  void* v = mmap(nullptr, m->bytes, PROT_READ | PROT_WRITE, MAP_SHARED, m->fd, 0);
  if (v == MAP_FAILED) {
    AppLog("live         : mmap of %s failed (errno %d)", path.c_str(), errno);
    m->Close();
    return false;
  }
  m->view = static_cast<uint8_t*>(v);
  m->head = reinterpret_cast<sr::Head*>(m->view);
  if (!sr::HeadOk(m->head, m->bytes)) {
    m->Close();
    return false;
  }
  return true;
}

}  // namespace

bool StreamReader::Start(const std::string& path, size_t frames, size_t queue_bytes) {
  path_ = path;
  whole_frames_ = frames;
  queue_max_bytes_ = queue_bytes;
  stop_ = false;
  started_ = pthread_create(&thread_, nullptr, &StreamReader::Main, this) == 0;
  if (!started_) AppLog("live         : the reader thread did not start");
  return started_;
}

void StreamReader::Stop() {
  if (!started_) return;
  stop_ = true;
  pthread_join(thread_, nullptr);
  started_ = false;
}

size_t StreamReader::MostBytes() {
  pthread_mutex_lock(&lock_);
  const size_t most = most_bytes_;
  pthread_mutex_unlock(&lock_);
  return most;
}

bool StreamReader::TakeAll(std::vector<QueuedFrame>* frames) {
  frames->clear();
  pthread_mutex_lock(&lock_);
  ++takes_;
  while (!queue_.empty()) {
    frames->push_back(std::move(queue_.front()));
    queue_.pop_front();
  }
  queue_bytes_ = 0;
  pthread_mutex_unlock(&lock_);
  return !frames->empty();
}

bool StreamReader::WaitAll(std::vector<QueuedFrame>* frames, int timeout_ms) {
  pthread_mutex_lock(&lock_);
  if (queue_.empty() && timeout_ms > 0) {
    timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_nsec += static_cast<long>(timeout_ms % 1000) * 1000000L;
    until.tv_sec += timeout_ms / 1000 + until.tv_nsec / 1000000000L;
    until.tv_nsec %= 1000000000L;
    while (queue_.empty() && !stop_) {
      if (pthread_cond_timedwait(&arrived_cond_, &lock_, &until) != 0) break;
    }
  }
  pthread_mutex_unlock(&lock_);
  return TakeAll(frames);
}

void* StreamReader::Main(void* self) {
  TimeLogNameThread("bwvr reader");
  static_cast<StreamReader*>(self)->Run();
  return nullptr;
}

void StreamReader::Run() {
  enum class State { kNoRing, kAsk, kWait, kRead, kPaused };
  State state = State::kNoRing;
  Map m;
  uint32_t session = 0, lost = 0, ask = 0;
  double asked = 0.0;
  bool said_wait = false;
  std::vector<uint8_t> pending;
  std::vector<uint8_t> chunk(4u << 20);
  uint64_t frames = 0, bytes = 0, stripped = 0;
  size_t queue_most = 0;
  int32_t test_frame = 0;
  std::vector<uint8_t> state_only;
  size_t queue_most_bytes = 0;
  uint64_t takes_at_pause = 0;
  uint64_t pauses = 0;
  while (!stop_) {
    if (feed_ > 0) {
      int status = 0;
      if (waitpid(feed_, &status, WNOHANG) == feed_) {
        AppLog("live         : the feed process ended with the status %d",
               WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        feed_ = 0;
      }
    }
    if (state == State::kNoRing) {
      if (!Open(path_, &m)) {
        if (!said_wait) {
          AppLog("live         : waits for the ring %s of the mod", path_.c_str());
          said_wait = true;
        }
        SleepUs(100000);
        continue;
      }
      session = m.head->session;
      lost = sr::LoadAcquire(&m.head->lost);
      AppLog("live         : the ring is there: %u MB, session %08x", m.head->capacity >> 20,
             session);
      state = State::kAsk;
    }
    if (sr::LoadAcquire(&m.head->magic) != sr::kMagic || m.head->session != session) {
      AppLog("live         : the writer made a new head. The app maps the ring again");
      m.Close();
      pending.clear();
      state = State::kNoRing;
      continue;
    }
    if (sr::LoadAcquire(&m.head->lost) != lost) {
      lost = sr::LoadAcquire(&m.head->lost);
      AppLog("live         : the writer gave up on the reader. The app asks again");
      state = State::kAsk;
    }
    if (state == State::kPaused) {
      // The session took no frame while the queue filled. It takes frames
      // again: ask for the whole state.
      pthread_mutex_lock(&lock_);
      const bool again = takes_ != takes_at_pause;
      pthread_mutex_unlock(&lock_);
      if (!again) {
        SleepUs(2000);
        continue;
      }
      AppLog("live         : the session takes frames again. The app asks for the whole state");
      state = State::kAsk;
    }
    if (state == State::kAsk) {
      pending.clear();
      ask = sr::Ask(m.head);
      asked = NowMs();
      state = State::kWait;
    }
    if (state == State::kWait) {
      uint32_t sync = 0;
      if (!sr::Answered(m.head, ask, &sync)) {
        SleepUs(500);
        continue;
      }
      sr::StoreRelease(&m.head->read, sync);
      AppLog("live         : the writer answered the ask after %.0f ms, at its frame %u",
             NowMs() - asked, sr::LoadAcquire(&m.head->frame));
      state = State::kRead;
    }
    // One frame can be larger than the limit, so the limit holds only when
    // a frame waits. With a bound of frames the reader takes each frame.
    pthread_mutex_lock(&lock_);
    const size_t waiting = queue_.empty() ? 0 : queue_bytes_;
    pthread_mutex_unlock(&lock_);
    if (whole_frames_ == 0 && queue_max_bytes_ > 0 && waiting >= queue_max_bytes_) {
      SleepUs(500);
      continue;
    }
    bool broken = false;
    const uint32_t n = sr::Take(m.head, m.view + sr::kHeadBytes, chunk.data(),
                                static_cast<uint32_t>(chunk.size()), &broken);
    if (broken) {
      AppLog("live         : the indices of the ring are not valid. The app asks again");
      state = State::kAsk;
      continue;
    }
    if (n == 0) {
      SleepUs(500);
      continue;
    }
    pending.insert(pending.end(), chunk.begin(), chunk.begin() + n);
    size_t used = 0;
    for (;;) {
      bool bad = false, whole = false;
      uint64_t frame = 0;
      const size_t len = sr::UnitBytes(pending.data() + used, pending.size() - used,
                                       kLimit, &bad, &frame, &whole);
      if (bad) {
        AppLog("live         : a record in the ring is not of the stream. The app asks again");
        state = State::kAsk;
        break;
      }
      if (len == 0) break;
      QueuedFrame q;
      q.bytes.assign(pending.begin() + used, pending.begin() + used + len);
      const sr::UnitInfo info = sr::Inspect(q.bytes.data(), q.bytes.size());
      if (info.start) test_frame = info.test_frame;
      q.frame = info.frame;
      q.start = info.start;
      q.test_frame = test_frame;
      q.picture = info.capture ||
                  (test_frame > 0 && info.frame == static_cast<uint64_t>(test_frame));
      used += len;
      ++frames;
      bytes += len;
      if (time_log_) {
        timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        q.arrival_ms = now.tv_sec * 1000.0 + now.tv_nsec / 1e6;
        last_arrival_ms_.store(now.tv_sec * 1000.0 + now.tv_nsec / 1e6,
                               std::memory_order_relaxed);
        arrived_.fetch_add(1, std::memory_order_relaxed);
      }
      pthread_mutex_lock(&lock_);
      // THE LIMIT: a queue that the session does not take (a session that
      // does not run) is cleared, and the reader leaves the ring.
      if (whole_frames_ > 0 &&
          stream_split::QueueFull(queue_bytes_, q.bytes.size(), queue_max_bytes_)) {
        queue_.clear();
        queue_bytes_ = 0;
        takes_at_pause = takes_;
        pthread_mutex_unlock(&lock_);
        sr::Leave(m.head);
        ++pauses;
        AppLog("live         : the queue passed %zu MB, and the session took no frame. "
               "The app clears the queue, leaves the ring and asks again when the "
               "session takes frames",
               queue_max_bytes_ >> 20);
        state = State::kPaused;
        break;
      }
      queue_bytes_ += q.bytes.size();
      queue_.push_back(std::move(q));
      // THE BOUND: the newest frames stay whole. The one frame that passes
      // the bound keeps only its records of state.
      if (whole_frames_ > 0) {
        const size_t i = stream_split::PassesBound(queue_.size(), whole_frames_);
        if (i < queue_.size()) {
          QueuedFrame& old = queue_[i];
          if (old.whole &&
              !stream_split::KeepWalks(i, queue_.size(), whole_frames_, old.picture)) {
            stream_split::StateOnly(old.bytes.data(), old.bytes.size(), &state_only);
            queue_bytes_ -= old.bytes.size() - state_only.size();
            old.bytes.swap(state_only);
            old.whole = false;
            ++stripped;
          }
        }
      }
      if (queue_.size() > queue_most) queue_most = queue_.size();
      if (queue_bytes_ > queue_most_bytes) queue_most_bytes = queue_bytes_;
      most_bytes_ = queue_most_bytes;
      pthread_cond_broadcast(&arrived_cond_);
      pthread_mutex_unlock(&lock_);
    }
    if (state == State::kRead) pending.erase(pending.begin(), pending.begin() + used);
    if (state == State::kPaused) pending.clear();
  }
  if (m.head != nullptr) sr::Leave(m.head);
  AppLog("live         : the reader leaves the ring: %llu frames, %llu bytes, at most %zu "
         "frames and %zu bytes waited for the session (the limit is %zu bytes), %llu "
         "frames lost their walks in the queue, %llu times the queue passed the limit",
         static_cast<unsigned long long>(frames), static_cast<unsigned long long>(bytes),
         queue_most, queue_most_bytes, whole_frames_ > 0 ? queue_max_bytes_ : 0,
         static_cast<unsigned long long>(stripped), static_cast<unsigned long long>(pauses));
  m.Close();
}

bool StreamReader::StartFeed(const std::string& ring, const std::string& feed,
                             int pace_ms) {
  // Everything that the child needs is made before the fork: the child of a
  // process with threads may call no function that takes a lock (malloc, the
  // log).
  static std::string s_bytes;
  struct Unit {
    size_t at, bytes;
    uint32_t frame;
  };
  static std::vector<Unit> s_units;
  s_bytes = ReadTextFile(feed);
  ss::Reader check(reinterpret_cast<const uint8_t*>(s_bytes.data()), s_bytes.size());
  if (!check.Ok()) {
    AppLog("live         : the feed %s is not a stream (%zu bytes)", feed.c_str(),
           s_bytes.size());
    return false;
  }
  const uint8_t* data = reinterpret_cast<const uint8_t*>(s_bytes.data());
  size_t at = 8;
  while (at < s_bytes.size()) {
    bool bad = false, whole = false;
    uint64_t frame = 0;
    const size_t len = sr::UnitBytes(data + at, s_bytes.size() - at, kLimit, &bad, &frame,
                                     &whole);
    if (bad || len == 0) break;
    s_units.push_back({at, len, static_cast<uint32_t>(frame)});
    at += len;
  }
  const std::string path = ring;
  const size_t file_bytes = sr::kHeadBytes + sr::kCapacity;
  const pid_t pid = fork();
  if (pid < 0) {
    AppLog("live         : fork failed (errno %d)", errno);
    return false;
  }
  if (pid == 0) {
    // THE CHILD: only system calls and the ring code.
    const int fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0660);
    if (fd < 0 || ftruncate(fd, static_cast<off_t>(file_bytes)) != 0) _exit(2);
    void* v = mmap(nullptr, file_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (v == MAP_FAILED) _exit(3);
    sr::Head* h = static_cast<sr::Head*>(v);
    uint8_t* ring_data = static_cast<uint8_t*>(v) + sr::kHeadBytes;
    sr::MakeHead(h, sr::kCapacity, static_cast<uint32_t>(getpid()));
    uint32_t answered = 0;
    double start = NowMs();
    while (!sr::TakeAsk(h, &answered)) {
      if (NowMs() - start > 300000.0) _exit(4);
      SleepUs(1000);
    }
    for (const Unit& u : s_units) {
      sr::StoreRelease(&h->frame, u.frame);
      uint32_t done = 0;
      double last = NowMs();
      while (done < u.bytes) {
        const uint32_t k = sr::Put(h, ring_data, data + u.at + done,
                                   static_cast<uint32_t>(u.bytes - done));
        done += k;
        if (k > 0) {
          last = NowMs();
        } else {
          if (NowMs() - last > 300000.0) {
            sr::StoreRelease(&h->lost, 1);
            _exit(5);
          }
          SleepUs(200);
        }
      }
      sr::StoreRelease(&h->units, h->units + 1);
      if (pace_ms > 0) SleepUs(pace_ms * 1000L);
    }
    // The end: the reader took each byte (0), or not in 5 minutes (6). The
    // write of a picture of a test in the emulator holds the reader for
    // about 2 minutes.
    start = NowMs();
    while (sr::LoadAcquire(&h->read) != h->write) {
      if (NowMs() - start > 300000.0) _exit(6);
      SleepUs(1000);
    }
    _exit(0);
  }
  feed_ = pid;
  AppLog("live         : stream_live_feed=1. The process %d writes the %zu frames of "
         "%s into the ring, one each %d ms",
         static_cast<int>(pid), s_units.size(), feed.c_str(), pace_ms);
  return true;
}
