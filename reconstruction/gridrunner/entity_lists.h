// iOS list initialization (0x44810): preserve list identity and mutation timing before gameplay.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace gr::reconstruction {
enum class List { L3b0, L3bc, L3b8, L3ac, L3c0, L3c4, L3cc, L3c8, Count };
// Host ownership/linkage only. Original game classes derive from this node.
struct ListNode {
    uint64_t id = 0;
    bool dead = false;
    ListNode *previous = nullptr;
    ListNode *next = nullptr;
    List list = List::Count;
    virtual ~ListNode() = default;
    virtual void run() = 0;
};
class EntityLists {
  public:
    ListNode &prepend(List list, std::function<void(ListNode &)> run);
    ListNode &append(List list, std::function<void(ListNode &)> run);
    ListNode &prepend(List list, std::unique_ptr<ListNode> node);
    ListNode &append(List list, std::unique_ptr<ListNode> node);
    // Source field game+0xc8, not an inferred universal pause flag.
    void runUnpaused(int32_t gate);
    std::vector<uint64_t> ids(List list) const;
    ListNode *first(List list) const {
        return heads_[size_t(list)];
    }
    // Host teardown/reset, outside traversal. Game bindings must outlive nodes.
    void clear() {
        heads_.fill(nullptr);
        owned_.clear();
    }
    bool contains(uint64_t id) const {
        return owned_.find(id) != owned_.end();
    }

  private:
    ListNode &create(List list, std::unique_ptr<ListNode> node);
    void walk(List list);
    std::array<ListNode *, size_t(List::Count)> heads_{};
    std::unordered_map<uint64_t, std::unique_ptr<ListNode>> owned_;
    uint64_t nextId_ = 1;
};
} // namespace gr::reconstruction
