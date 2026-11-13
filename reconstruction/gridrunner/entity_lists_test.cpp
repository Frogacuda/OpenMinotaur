#ifdef NDEBUG
#undef NDEBUG
#endif
#include "entity_lists.h"
#include <cassert>
#include <iostream>
using namespace gr::reconstruction;
int main() {
    // Literal order and gate boundaries from iOS 0x44810, independent of saved port output.
    for (int gate : {-1, 0, 1}) {
        EntityLists lists;
        std::vector<int> visited;
        for (int i = 0; i < 8; i++)
            lists.prepend(List(i), [&, i](ListNode &) { visited.push_back(i); });
        lists.runUnpaused(gate);
        const std::vector<int> expected = gate < 0    ? std::vector<int>{1}
                                          : gate == 0 ? std::vector<int>{1, 2, 3, 4, 5, 6, 7}
                                                      : std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7};
        assert(visited == expected);
    }
    {
        EntityLists lists;
        std::vector<int> visited;
        bool born = false;
        lists.prepend(List::L3c4, [&](ListNode &) { visited.push_back(1); });
        const auto parent = lists
                                .prepend(List::L3c4,
                                         [&](ListNode &e) {
                                             visited.push_back(2);
                                             if (!born) {
                                                 born = true;
                                                 lists.prepend(List::L3c4, [&](ListNode &) {
                                                     visited.push_back(3);
                                                 });
                                             }
                                             e.dead = true;
                                         })
                                .id;
        lists.runUnpaused(1);
        assert((visited == std::vector<int>{2, 1}));
        assert(!lists.contains(parent) && lists.ids(List::L3c4).size() == 2);
        visited.clear();
        lists.runUnpaused(1);
        assert((visited == std::vector<int>{3, 1}));
    }
    {
        EntityLists lists;
        std::vector<int> visited;
        lists.prepend(List::L3bc, [&](ListNode &e) {
            visited.push_back(1);
            e.dead = true;
            lists.prepend(List::L3c4, [&](ListNode &child) {
                visited.push_back(2);
                child.dead = true;
            });
        });
        lists.runUnpaused(1);
        assert((visited == std::vector<int>{1, 2}));
        assert(lists.ids(List::L3bc).empty() && lists.ids(List::L3c4).empty());
    }
    {
        EntityLists lists;
        std::vector<int> visited;
        auto &future = lists.prepend(List::L3c4, [&](ListNode &e) {
            visited.push_back(2);
            assert(e.dead);
        });
        const auto futureId = future.id;
        lists.prepend(List::L3c4, [&](ListNode &) {
            visited.push_back(1);
            future.dead = true;
        });
        lists.runUnpaused(1);
        assert((visited == std::vector<int>{1, 2}));
        assert(!lists.contains(futureId));
    }
    {
        EntityLists lists;
        std::vector<int> visited;
        lists.prepend(List::L3c4, [&](ListNode &e) {
            visited.push_back(1);
            e.dead = true;
            lists.append(List::L3c4, [&](ListNode &child) {
                visited.push_back(2);
                child.dead = true;
            });
        });
        lists.runUnpaused(1);
        assert((visited == std::vector<int>{1, 2}));
        assert(lists.ids(List::L3c4).empty());
    }
    std::cout << "iOS list order/gates, prepend/append births and post-run deletion passed\n";
}
