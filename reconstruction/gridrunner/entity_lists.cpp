#include "neon/entity_list.h"
#include "entity_lists.h"
#include <stdexcept>
#include <utility>
namespace gr::reconstruction {
namespace {
struct OriginalLinks {
    static ListNode *&previous(ListNode &n) {
        return n.previous;
    }
    static ListNode *&next(ListNode &n) {
        return n.next;
    }
};
class FunctionNode final : public ListNode {
    std::function<void(ListNode &)> callback_;

  public:
    explicit FunctionNode(std::function<void(ListNode &)> callback)
        : callback_(std::move(callback)) {
        if (!callback_)
            throw std::invalid_argument("Missing callback");
    }
    void run() override {
        callback_(*this);
    }
};
} // namespace
ListNode &EntityLists::create(List list, std::unique_ptr<ListNode> node) {
    if (int(list) < 0 || int(list) >= int(List::Count) || !node || node->list != List::Count)
        throw std::invalid_argument("Invalid entity/list");
    node->id = nextId_++;
    node->list = list;
    auto &ref = *node;
    owned_.emplace(ref.id, std::move(node));
    return ref;
}
ListNode &EntityLists::prepend(List list, std::function<void(ListNode &)> run) {
    return prepend(list, std::make_unique<FunctionNode>(std::move(run)));
}
ListNode &EntityLists::append(List list, std::function<void(ListNode &)> run) {
    return append(list, std::make_unique<FunctionNode>(std::move(run)));
}
ListNode &EntityLists::prepend(List list, std::unique_ptr<ListNode> value) {
    auto &node = create(list, std::move(value));
    auto &head = heads_[size_t(list)];
    node.next = nullptr;
    neon::entities::prepend<ListNode, OriginalLinks>(head, node);
    return node;
}
ListNode &EntityLists::append(List list, std::unique_ptr<ListNode> value) {
    auto &node = create(list, std::move(value));
    auto &head = heads_[size_t(list)];
    neon::entities::append<ListNode, OriginalLinks>(head, node);
    return node;
}
void EntityLists::walk(List list) {
    auto dispose = [this](ListNode *node) { owned_.erase(node->id); };
    neon::entities::runAndCull<ListNode, decltype(dispose), OriginalLinks>(heads_[size_t(list)],
                                                                           dispose);
}
void EntityLists::runUnpaused(int32_t gate) {
    if (gate > 0)
        walk(List::L3b0);
    walk(List::L3bc);
    if (gate >= 0) {
        walk(List::L3b8);
        walk(List::L3ac);
        walk(List::L3c0);
        walk(List::L3c4);
        walk(List::L3cc);
        walk(List::L3c8);
    }
}
std::vector<uint64_t> EntityLists::ids(List list) const {
    if (int(list) < 0 || int(list) >= int(List::Count))
        throw std::invalid_argument("Invalid list");
    std::vector<uint64_t> result;
    for (auto *node = heads_[size_t(list)]; node; node = node->next)
        result.push_back(node->id);
    return result;
}
} // namespace gr::reconstruction
