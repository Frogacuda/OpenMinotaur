#pragma once
// Optional adapter at the host boundary. The widget core does not include or link
// the light-synth read/response implementation. Collections may be added by the host.
#include "widget_runtime.h"

namespace neon::widgets {
template <class ScalarSnapshot> inline Snapshot fromReadSnapshot(const ScalarSnapshot &read) {
    Snapshot out;
    out.game = read.game;
    out.session = read.session;
    out.sequence = read.sequence;
    for (auto &item : read.values)
        out.values[item.first] =
            std::visit([](const auto &v) -> Value { return Value(v); }, item.second);
    return out;
}
} // namespace neon::widgets
