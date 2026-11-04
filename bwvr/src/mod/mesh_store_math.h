// THE STORE OF THE MESH DATA IN THE RENDERER (docs/software-tnl.md, "The meshes
// on the GPU"). Pure logic with no Vulkan, so the renderer and the checks in
// bwvr/test/controller_math/ use it.
//
// - The list of data that wait: data that found no room in the arena wait
//   here, one entry for each mesh. A newer version of a mesh replaces its
//   entry, and a direct store of a mesh removes its entry, so a flush never
//   puts an older version over a newer one.
// - The gate of a walk: an action that may run once in each walk.
// - The guard of a draw with groups: the shader reads one matrix for each group
//   of the mesh data, so the draw must have at least that many.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

namespace mesh_store_math {

// Item has `uint64_t id` and `size_t size` (the bytes that it holds).
template <typename Item>
class WaitList {
 public:
  explicit WaitList(size_t most_bytes) : most_(most_bytes) {}
  // False when the list has no room: the item is dropped (an older entry of the
  // same mesh goes out too, because it is older than the item).
  bool Put(Item&& item) {
    Remove(item.id);
    if (bytes_ + item.size > most_) return false;
    bytes_ += item.size;
    items_.push_back(std::move(item));
    return true;
  }
  bool Remove(uint64_t id) {
    for (size_t i = 0; i < items_.size(); ++i) {
      if (items_[i].id != id) continue;
      bytes_ -= items_[i].size;
      items_.erase(items_.begin() + static_cast<ptrdiff_t>(i));
      return true;
    }
    return false;
  }
  bool Has(uint64_t id) const {
    for (const Item& item : items_) {
      if (item.id == id) return true;
    }
    return false;
  }
  bool Empty() const { return items_.empty(); }
  size_t Count() const { return items_.size(); }
  size_t Bytes() const { return bytes_; }
  std::vector<Item>& Items() { return items_; }
  // Removes the items for which done(item) is true.
  template <typename Done>
  void RemoveIf(Done done) {
    size_t keep = 0;
    bytes_ = 0;
    for (size_t i = 0; i < items_.size(); ++i) {
      if (done(items_[i])) continue;
      if (keep != i) items_[keep] = std::move(items_[i]);
      bytes_ += items_[keep].size;
      ++keep;
    }
    items_.resize(keep);
  }

 private:
  std::vector<Item> items_;
  size_t bytes_ = 0;
  size_t most_;
};

// True the first time it is asked in a walk.
struct WalkGate {
  uint64_t last = ~0ull;
  bool Once(uint64_t walk) {
    if (walk == last) return false;
    last = walk;
    return true;
  }
};

// A draw with groups is for a mesh whose data have `mesh_groups` groups, and
// it has `draw_groups` matrices. The shader reads the matrix of the ordinal of
// each vertex, so the draw needs at least as many as the data have. A draw of
// a mesh with no groups in its data is not a draw with groups.
inline bool GroupDrawOk(uint32_t mesh_groups, uint32_t draw_groups, uint32_t most) {
  return mesh_groups != 0 && draw_groups >= mesh_groups && draw_groups <= most;
}

}  // namespace mesh_store_math
