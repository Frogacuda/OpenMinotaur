#pragma once
namespace neon::entities {
template <class Node> struct PrevNextLinks {
    static Node *&previous(Node &node) {
        return node.prev;
    }
    static Node *&next(Node &node) {
        return node.next;
    }
};
// Original early insertion conditionally writes next when there is a head.
// The caller owns initialization of a detached node's next pointer.
template <class Node, class Links = PrevNextLinks<Node>> void prepend(Node *&head, Node &node) {
    Links::previous(node) = nullptr;
    if (head) {
        Links::next(node) = head;
        Links::previous(*head) = &node;
    }
    head = &node;
}
template <class Node, class Links = PrevNextLinks<Node>> void append(Node *&head, Node &node) {
    if (!head)
        head = &node;
    else {
        auto *tail = head;
        while (Links::next(*tail))
            tail = Links::next(*tail);
        Links::next(*tail) = &node;
        Links::previous(node) = tail;
    }
}
template <class Node, class Dispose, class Links = PrevNextLinks<Node>>
void runAndCull(Node *&head, Dispose dispose) {
    auto *node = head;
    while (node) {
        node->run(); // source runs even a node already marked dead by a neighbor
        auto *next = Links::next(*node); // AFTER callback; appended children may run now
        if (node->dead) {
            auto *previous = Links::previous(*node);
            if (previous)
                Links::next(*previous) = next;
            else
                head = next;
            if (next)
                Links::previous(*next) = previous;
            dispose(node); // next was captured before destructor/ownership release
        }
        node = next;
    }
}
template <class Node, class Dispose> void destroyList(Node *&head, Dispose dispose) {
    auto *node = head;
    while (node) {
        auto *next = node->next;
        dispose(node);
        node = next;
    }
    head = nullptr;
}
} // namespace neon::entities
