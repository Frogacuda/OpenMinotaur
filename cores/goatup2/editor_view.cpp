#include "editor_view.h"
#include "editor_tables.h"
#include "actor_tables.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace goatup2 {
namespace {
neon::colour rgba(uint32_t v) {
    const auto c = unpackParticleColour(v);
    return {c.r, c.g, c.b, c.a};
}
void sound(StageRuntime &w, int n, int shift = 9) {
    if (!w.medium.splashSoundSuppressed)
        w.playSample(n, uint32_t(w.medium.sampleVolume) << shift, 0);
}
} // namespace
void EditorView::open(const Stage &stage) {
    document.open(stage);
    mode = previousMode = 0;
    phase = velocity = {0, 0};
    column = lastColumn_ = stage.declaredColumns / 2;
    row = lastRow_ = (stage.declaredRows - 2) / 2;
    changed = stroke = false;
    error.clear();
    cancel();
}
void EditorView::cancel() {
    pads_.clear();
    fingers_ = {};
    gesture_ = 0;
    finger_ = -1;
    multi_ = false;
    armed_ = false;
    navigation_ = {};
    canvas_ = false;
    painted_ = false;
    toolRepeat_ = 0;
    pickerFinger_ = -1;
    pickerVelocity = {0, 0};
}
void EditorView::pads(const neon::input::ControllerState *p, int n) {
    pads_.captureLatched(p, n);
}
void EditorView::touch(neon::input::TouchPhase action, uint32_t id, neon::vec2d p) {
    using P = neon::input::TouchPhase;
    for (auto &f : fingers_)
        if (f.held && f.id == id) {
            f.point = p;
            if (action == P::Up || action == P::Cancel)
                f.held = false;
            return;
        }
    if (action == P::Down)
        for (auto &f : fingers_)
            if (!f.held) {
                f = {id, true, true, p, p};
                return;
            }
}
std::vector<EditorView::Button> EditorView::buttons() const {
    if (mode == 4)
        return {{Mode, 18, {.43, .41}}};
    std::vector<Button> b = {{Mode, 18, {.43, .41}}, {Test, 21, {.43, -.41}}};
    if (mode == 2)
        b.push_back({Return, 42, {-.1, .41}});
    if (mode == 3) {
        b.push_back({Clear, 40, {-.43, .41}});
        b.push_back({Rename, 98, {.12, .41}});
        b.push_back({ShiftLeft, 101, {-.3, .41}});
        b.push_back({ShiftUp, 102, {-.2, .41}});
        b.push_back({ShiftDown, 103, {-.1, .41}});
        b.push_back({ShiftRight, 100, {0, .41}});
    }
    if (document.canUndo())
        b.push_back({Undo, 99, {.31, .41}});
    return b;
}
bool EditorView::consumes(neon::vec2d p) const {
    for (const auto &b : buttons())
        if (std::abs(p.x - b.position.x) < .048 && std::abs(p.y - b.position.y) < .048)
            return true;
    return false;
}
void EditorView::mapping(StageRuntime &w) {
    const auto &s = document.stage();
    phase.x = std::clamp(phase.x, -.5, .5);
    phase.y = std::clamp(phase.y, -.5, double(float(.85f - .5f)));
    column = std::clamp(int((phase.x + .5) * double(float(s.declaredColumns))), 0,
                        s.declaredColumns - 1);
    row = std::clamp(int(((phase.y + .5) / .85) * double(float(s.declaredRows))), 0,
                     s.declaredRows - 1);
    w.gridOffset = {-w.layout.viewportGap.x * (phase.x * 1.1 + .5),
                    ((phase.y * 1.1 + .5) / -.77) * w.layout.viewportGap.y};
}
void EditorView::remove(StageRuntime &w, int state) {
    for (const auto &p : document.placements.entries())
        if (p.type >= 0 && p.state == state)
            w.editorRemovalFlash({p.x, p.y});
    int count = 0;
    for (const auto &p : document.placements.entries())
        count += p.type >= 0 && p.state == state;
    changed |= document.removeSelected(state);
    for (int i = 0; i < count; ++i)
        sound(w, 39, 12);
}
int EditorView::tick(StageRuntime &w) {
    const auto input = pads_.consume();
    if (mode != 4)
        mapping(w);
    const auto controls = buttons();
    int action = None;
    bool any = false;
    for (const auto &f : fingers_)
        any |= f.held;
    for (const auto &p : input)
        if (p.connected)
            any |= p.buttons || p.pressed || std::abs(p.lx) > .2 || std::abs(p.ly) > .2 ||
                   std::abs(p.rx) > .5 || std::abs(p.ry) > .5;
    if (!armed_) {
        if (!any)
            armed_ = true;
        for (auto &f : fingers_)
            f.down = false;
        return None;
    }
    for (auto &f : fingers_)
        if (f.down) {
            for (const auto &b : controls)
                if (std::abs(f.point.x - b.position.x) < .048 &&
                    std::abs(f.point.y - b.position.y) < .048) {
                    action = b.action;
                    f.held = false;
                    f.down = false;
                }
        }
    float dx = 0, dy = 0;
    bool nativeStroke = false, nativePaint = false;
    for (const auto &p : input)
        if (p.connected) {
            const auto b = p.buttons;
            const float nx =
                (b & neon::input::Right ? 1.f : 0.f) - (b & neon::input::Left ? 1.f : 0.f);
            const float ny =
                (b & neon::input::Down ? 1.f : 0.f) - (b & neon::input::Up ? 1.f : 0.f);
            if (nx || ny)
                canvas_ = false;
            if (std::abs(p.lx) > .2 || std::abs(p.ly) > .2) {
                canvas_ = true;
                dx = p.lx * .006f;
                dy = p.ly * .006f;
            }
            std::vector<neon::input::MenuItem> items;
            for (const auto &c : controls) {
                neon::input::MenuItem item;
                item.label = c.action;
                item.cx = c.position.x;
                item.cy = c.position.y;
                items.push_back(item);
            }
            const int selected = navigation_.update(items, mode, nx, ny,
                                                    !canvas_ && (p.pressed & neon::input::South),
                                                    false, 0, {Mode}, {});
            if (selected)
                action = selected;
            if ((p.pressed & neon::input::East) && mode != 4)
                action = Return;
            if (mode < 2) {
                const bool tool = std::abs(p.rx) > .5 || std::abs(p.ry) > .5;
                if (tool &&
                    (toolRepeat_ == 0 || (toolRepeat_ >= 20 && (toolRepeat_ - 20) % 8 == 0))) {
                    if (std::abs(p.rx) > std::abs(p.ry))
                        document.category(p.rx > 0 ? -1 : 1, mode == 1);
                    else
                        document.variation(p.ry > 0 ? -1 : 1, mode == 1,
                                           (b & neon::input::West) != 0);
                    sound(w, 8, 8);
                }
                toolRepeat_ = tool ? toolRepeat_ + 1 : 0;
                nativeStroke |= canvas_ && ((mode == 0 && (b & neon::input::South)) ||
                                            (mode == 1 && (b & neon::input::North)));
                nativePaint |= canvas_ && mode == 0 && (b & neon::input::South);
                if (canvas_ && (p.pressed & neon::input::South)) {
                    if (mode == 0) {
                        document.paint(column, row);
                        changed = true;
                        sound(w, 7);
                    } else {
                        changed |= document.place(worldToStage({phase.x * .6, phase.y * .78},
                                                               w.gridOffset, w.layout, w.bounds));
                        sound(w, 9, 7);
                    }
                }
                if (canvas_ && !tool && (p.pressed & neon::input::West)) {
                    if (mode == 0) {
                        document.sample(column, row);
                        sound(w, 67, 12);
                    } else {
                        document.selectNearest(worldToStage({phase.x * .6, phase.y * .78},
                                                            w.gridOffset, w.layout, w.bounds),
                                               true);
                        remove(w, 1);
                    }
                }
                if (canvas_ && mode == 1 && (p.pressed & neon::input::North))
                    document.selectNearest(worldToStage({phase.x * .6, phase.y * .78}, w.gridOffset,
                                                        w.layout, w.bounds));
                if (mode == 1 && (b & neon::input::North) && (dx || dy))
                    changed |= document.moveSelected({dx, dy}, w.bounds);
            }
        }
    if (mode < 2) {
        int touches = 0;
        for (const auto &f : fingers_)
            touches += f.held;
        const bool multi = touches > 1;
        if (multi && !multi_) {
            if (mode == 0) {
                document.sample(column, row);
                sound(w, 67, 12);
            } else {
                bool selected = false;
                for (const auto &p : document.placements.entries())
                    selected |= p.type >= 0 && p.state == 2;
                sound(w, selected ? 10 : 8, 12);
            }
        }
        if (mode == 1 && multi_ && !multi) {
            remove(w, 1);
            if (painted_)
                changed = true;
            painted_ = false;
        }
        multi_ = multi;
        if (gesture_ == 0)
            for (size_t i = 0; i < fingers_.size(); ++i)
                if (fingers_[i].held && fingers_[i].down &&
                    fingers_[i].point.y < double(.85f - .5f)) {
                    auto &f = fingers_[i];
                    finger_ = int(i);
                    velocity = {0, 0};
                    axisX_ = axisY_ = false;
                    const auto cursor = neon::vec2d{phase.x * .6, phase.y * .78};
                    const auto reach = mode == 0 ? w.layout.cellSize * .8 : neon::vec2d{.04, .04};
                    const bool hit = std::abs(f.point.x - cursor.x) < reach.x &&
                                     std::abs(f.point.y - cursor.y) < reach.y;
                    gesture_ = hit ? (mode == 0 ? 3 : 1) : 2;
                    if (hit) {
                        if (mode == 0) {
                            document.paint(column, row);
                            painted_ = true;
                            sound(w, 7);
                        } else {
                            changed |= document.place(
                                worldToStage(cursor, w.gridOffset, w.layout, w.bounds));
                            sound(w, 9, 7);
                        }
                    }
                    break;
                }
        if (finger_ >= 0) {
            auto &f = fingers_[size_t(finger_)];
            auto delta = f.point - f.last;
            if (!f.held) {
                if (gesture_ == 3 && painted_) {
                    changed = true;
                    painted_ = false;
                }
                gesture_ = 0;
                finger_ = -1;
                if (std::abs(velocity.x) < .004)
                    velocity.x = 0;
                if (std::abs(velocity.y) < .004)
                    velocity.y = 0;
            } else if (gesture_ == 2) {
                if (std::abs(delta.x) < .05)
                    velocity.x = 0;
                if (std::abs(delta.y) < .05)
                    velocity.y = 0;
                velocity += delta * .5;
            } else if (gesture_ == 3) {
                int axes = document.sampled
                               ? 3
                               : editor_data::Brushes[document.brush].axes[document.style];
                if (axes == 1 || (axes == 3 && axisX_)) {
                    delta.y = 0;
                    delta.x = std::clamp(delta.x, -double(float(w.layout.cellSize.x * .2)),
                                         double(float(w.layout.cellSize.x * .2)));
                }
                if (axes == 2 || (axes == 3 && axisY_)) {
                    delta.x = 0;
                    delta.y = std::clamp(delta.y, -double(float(w.layout.cellSize.y * .2)),
                                         double(float(w.layout.cellSize.y * .2)));
                }
                velocity = delta;
                if (column != lastColumn_ || row != lastRow_) {
                    document.paint(column, row);
                    painted_ = true;
                    sound(w, 7);
                    if (column != lastColumn_)
                        axisX_ = true;
                    if (row != lastRow_ && !axisX_)
                        axisY_ = true;
                }
            }
            if (mode == 1 && multi) {
                bool selected = false;
                for (const auto &p : document.placements.entries())
                    selected |= p.type >= 0 && p.state == 2;
                if (selected) {
                    painted_ |= document.moveSelected(delta, w.bounds);
                } else
                    document.selectNearest(worldToStage({phase.x * .6, phase.y * .78}, w.gridOffset,
                                                        w.layout, w.bounds),
                                           true);
            }
        }
        // 0x53fd8: selector integrates component-scaled deltas (16,46),
        // determines the dominant axis after five ticks, then wraps at 0/1.
        if (pickerFinger_ < 0) {
            pickerVelocity = {0, 0};
            for (size_t i = 0; i < fingers_.size(); ++i)
                if (fingers_[i].held && fingers_[i].down &&
                    fingers_[i].point.y > double(.85f + .05 - .5)) {
                    auto &f = fingers_[i];
                    if (pickerRecent_ < 1)
                        pickerPosition = {f.point.x > 0 ? .75 : .25, f.point.y + .5};
                    pickerRecent_ = 1;
                    pickerLockTicks_ = 5;
                    pickerTravel = {0, 0};
                    pickerBlockX_ = pickerBlockY_ = true;
                    pickerFinger_ = int(i);
                    document.sampled = false;
                    break;
                }
            if (pickerFinger_ < 0 && pickerRecent_ > 0)
                --pickerRecent_;
        } else {
            auto &f = fingers_[size_t(pickerFinger_)];
            const auto delta = f.point - f.last;
            if (!f.held) {
                pickerFinger_ = -1;
                pickerVelocity = {0, 0};
            } else {
                if (pickerLockTicks_ > 0) {
                    pickerTravel += delta;
                    if (--pickerLockTicks_ == 0) {
                        pickerBlockX_ = std::abs(pickerTravel.x) <= std::abs(pickerTravel.y);
                        pickerBlockY_ = std::abs(pickerTravel.y) < std::abs(pickerTravel.x);
                    }
                }
                if (std::abs(delta.x) < .05 || pickerBlockX_)
                    pickerVelocity.x = 0;
                if (std::abs(delta.y) < .05 || pickerBlockY_)
                    pickerVelocity.y = 0;
                pickerVelocity += delta * neon::vec2d{16, 46};
                if (pickerBlockX_)
                    pickerVelocity.x = 0;
                if (pickerBlockY_)
                    pickerVelocity.y = 0;
                pickerPosition += pickerVelocity;
                if (pickerPosition.x < 0) {
                    pickerPosition.x += 1;
                    document.category(1, mode == 1);
                    sound(w, 8, 8);
                }
                if (pickerPosition.x >= 1) {
                    pickerPosition.x -= 1;
                    document.category(-1, mode == 1);
                    sound(w, 8, 8);
                }
                if (pickerPosition.y < 0) {
                    pickerPosition.y += 1;
                    document.variation(1, mode == 1, pickerPosition.x > .5);
                    sound(w, 10, 8);
                }
                if (pickerPosition.y >= 1) {
                    pickerPosition.y -= 1;
                    document.variation(-1, mode == 1, pickerPosition.x > .5);
                    sound(w, 10, 8);
                }
            }
        }
        phase += velocity + neon::vec2d{dx, dy};
        velocity = velocity * .96;
        mapping(w);
        if (nativePaint && (column != lastColumn_ || row != lastRow_)) {
            changed |= document.paint(column, row);
            sound(w, 7);
        }
        if (mode == 1 && !multi && !changed)
            document.selectNearest(
                worldToStage({phase.x * .6, phase.y * .78}, w.gridOffset, w.layout, w.bounds));
    }
    stroke = nativeStroke || gesture_ == 3 || (mode == 1 && multi_);
    lastColumn_ = column;
    lastRow_ = row;
    for (auto &f : fingers_) {
        f.down = false;
        f.last = f.point;
    }
    if (action != None) {
        const int sample = action == Mode     ? 8
                           : action == Test   ? 9
                           : action == Return ? 57
                           : action == Clear  ? 56
                           : action == Rename ? 54
                           : action == Undo   ? 10
                                              : 53;
        sound(w, sample,
              action == Test || action == Return    ? 12
              : action == Clear || action == Rename ? 9
                                                    : 8);
    }
    return action;
}
void EditorView::objectDraw(StageRuntime &w, const Placement &p, bool cursor) {
    if (p.type < 0 || p.type > 9)
        return;
    const auto first = draws_.size();
    const auto &descriptor = editor_data::Objects[p.type];
    int sprite = descriptor.sprite;
    auto colour = neon::colour{1, 1, 1, 1};
    auto scale = neon::vec2d{1, 1};
    float radius = descriptor.radius;
    double angle = 0;
    bool firebar = false;
    const unsigned variant = p.flags & 255U, transform = p.flags >> 8;
    if (p.type == 2) {
        sprite = pickupSpriteForStage(w.stageIndex, int(variant));
        if (variant == 3)
            scale = scale * 2.;
        if (variant == 0 || variant == 11)
            colour = w.terrainPalette[size_t(w.fruitPaletteIndex)];
        else if (variant == 8)
            colour = w.terrainPalette[0];
    } else if (p.type == 3) {
        const std::array<int32_t, 16> *records[] = {&actor_data::Device0, &actor_data::Device1,
                                                    &actor_data::Device2, &actor_data::Device3,
                                                    &actor_data::Device4, &actor_data::Device5};
        const auto &r = *records[variant];
        sprite = std::abs(r[0]);
        colour = variant < 2 ? w.terrainPalette[0] : rgba(uint32_t(r[2]));
        if (variant > 3)
            scale = scale * .7;
        if (variant == 5)
            scale.y = -scale.y;
        if (r[0] < 0)
            scale.y = -scale.y;
    } else if (p.type >= 4 && p.type <= 8) {
        const auto *r = actor_data::find(p.type, int(variant));
        if (!r)
            return;
        sprite = (*r)[0];
        const uint32_t packed = uint32_t((*r)[2]);
        colour = packed < 32 ? w.terrainPalette[packed] : rgba(packed);
        scale = scale * double(float((*r)[11]) / 256.f);
        if (uint32_t((*r)[6]) & 64U) {
            angle = double(transform & 3U) * 1.570796327;
            if (uint32_t((*r)[6]) & 128U)
                angle -= .7853981635;
        } else {
            if (transform & 1U)
                scale.x = -scale.x;
            if (transform & 2U)
                scale.y = -scale.y;
        }
        firebar = p.type == 8 && (*r)[7] == 3;
    }
    scale = scale * w.spriteScale(sprite);
    if (p.type == 2) {
        if (transform & 1U)
            scale.x = -scale.x;
        if (transform & 2U)
            scale.y = -scale.y;
    }
    if ((p.type == 0 || p.type == 1) && variant)
        scale.y = -scale.y;
    if (p.state == 1)
        colour = w.terrainPalette[1];
    if (p.state == 2)
        colour.a = (w.animationFrameFlags & 8U) ? .2 : 1.;
    if (cursor)
        for (const auto &existing : document.placements.entries())
            if (existing.type == p.type) {
                colour.a = .6;
                break;
            }
    const auto pos = cursor ? neon::vec2d{phase.x * .6, phase.y * .78}
                            : stageToWorld({p.x, p.y}, w.gridOffset, w.layout, w.bounds);
    draws_.push_back({pos, scale * (double(radius) * 2.), colour, angle, sprite + 2});
    if (firebar) {
        // 0x55028 repeats the static body before its four-to-seven balls.
        draws_.push_back(draws_.back());
        const float rotation = -float(double(w.sourceTime) + p.x * 10. + p.y * 10.);
        neon::vec2d step{0, .03};
        neon::rotateClockwise(step.x, step.y, double(rotation),
                              neon::ZeroAngleRule::PreserveCoordinates);
        auto ball = pos;
        for (unsigned i = 0; i < ((transform & 3U) | 4U); ++i) {
            ball += step;
            draws_.push_back({ball, {.03, .03}, colour, double(rotation * 8.f), 108});
        }
    }
    for (auto n = first; n < draws_.size(); ++n)
        draws_[n].editorObject = true;
}
void EditorView::prepare(StageRuntime &w, StageHud &hud) {
    draws_.clear();
    hud.text.clear();
    hud.clearRows = {3, 5, 6, 40, 41, 42, 43, 44};
    auto text = [&](int x, int row, std::string value, int ink) {
        hud.text.push_back({x, row, std::move(value), w.terrainPalette[size_t(ink)]});
    };
    if (mode != 4)
        for (const auto &p : document.placements.entries())
            objectDraw(w, p, false);
    if (mode < 2) {
        if (mode == 1)
            objectDraw(w,
                       {document.object, 0, 0,
                        uint32_t(document.variant) | (uint32_t(document.transform) << 8), 0, 0},
                       true);
        else {
            const auto cells = document.previewCells();
            for (int i = 0; i < 3; ++i)
                if (cells[size_t(i)] || (document.sampled && i == 1)) {
                    StageDraw d;
                    d.position = {phase.x * .6,
                                  phase.y * .78 + double(i - 1) * w.layout.cellSize.y};
                    d.size = w.layout.cellSize;
                    d.brushTile = true;
                    d.brushWord = cells[size_t(i)];
                    draws_.push_back(d);
                }
            draws_.push_back(
                {{phase.x * .6, phase.y * .78}, w.layout.cellSize, {1, 1, 1, 1}, 0, 16});
        }
        char line[80];
        std::snprintf(line, sizeof line, "X: %d Y: %d", column, row);
        if (mode == 0) {
            const auto &b = editor_data::Brushes[document.brush];
            for (int i = 0; i < 3; ++i)
                text(6, 40 + i, b.description[i], 0);
            text(6, 44, b.names[document.style], 0);
            double y = .415 - w.layout.cellSize.y;
            for (const auto cell : document.previewCells())
                if (cell) {
                    StageDraw d;
                    d.position = {-.42, y};
                    d.size = w.layout.cellSize;
                    d.brushTile = true;
                    d.brushWord = cell;
                    draws_.push_back(d);
                    y += w.layout.cellSize.y;
                }
            if (b.colours > 0) {
                StageDraw d{{0, .36}, {.01, .13}, w.terrainPalette[0], 0, 2};
                d.flags = 0;
                draws_.push_back(d);
            }
        } else {
            const auto &b = editor_data::Objects[document.object];
            for (int i = 0; i < 4; ++i)
                text(6, 40 + i, b.description[i], 0);
            text(6, 44, b.names[document.variant], 0);
            int used = 0;
            for (const auto &p : document.placements.entries())
                used += p.type == document.object;
            std::snprintf(line, sizeof line, "X: %d Y: %d        USED: %d  MAX: %d", column, row,
                          used, b.limit);
            if (b.transforms > 0) {
                StageDraw d{{0, .36}, {.01, .13}, w.terrainPalette[0], 0, 2};
                d.flags = 0;
                draws_.push_back(d);
            }
        }
        text(1, 3, line, 0);
    } else if (mode < 4) {
        text((42 - int(bankName.size())) / 2, 3, bankName, 0);
        text((42 - int(levelName.size())) / 2, 5, levelName, 0);
        if (mode == 2) {
            text(1, 41, "BACK TO LEVEL", 0);
            text(1, 42, "SELECT SCREEN", 0);
        }
    }
    if (!error.empty())
        text(1, 6, error, 2);
    for (const auto &b : buttons()) {
        auto colour =
            ((pads_.connected() || keyboardNavigation) && navigation_.sel == b.action && !canvas_)
                ? w.terrainPalette[0]
                : neon::colour{1, 1, 1, 1};
        draws_.push_back({b.position, w.spriteScale(b.sprite) * .08, colour, 0, b.sprite + 2});
    }
}
} // namespace goatup2
