#include "stage_presentation.h"
#include "campaign_actors.h"
#include "presentation_tables.h"
#include "terrain_tables.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace goatup2 {
namespace {
neon::colour packed(uint32_t value) {
    const auto c = unpackParticleColour(value);
    return {c.r, c.g, c.b, c.a};
}
neon::colour dynamic(int type, uint32_t frame) {
    const auto c = neon::paletteColour(type, 1, frame, {neon::WashRule::TriangularRainbow, 12, 4});
    return {c.r, c.g, c.b, c.a};
}
} // namespace
void StageStarfield::reset(neon::DarwinRandom &random) {
    points_.clear();
    depth_ = 0;
    // 0x401ac: unit extents, zero offsets/rotations, only depth wrapping.
    // 0x4ca24 sets the edge-fade width to .5f before creating the stage stars.
    const auto coordinate = [&]() { return 2. * (double(random.next()) / 2147483647.) - 1.; };
    for (int remaining = 100; remaining > 0; --remaining) {
        Point p;
        p.x = coordinate();
        p.y = coordinate();
        p.z = coordinate();
        p.size = double(.05f);
        // 0x4cb50 passes the FIFTH argument, opaque white, on the stack.
        // The preceding register argument 2 selects the sprite, not its ink.
        p.colour = remaining <= 2 ? 0xffc663ffU : 0xffffffffU;
        p.sprite = remaining == 1 ? 179 : remaining == 2 ? 178 : 2;
        // Constructor 0x401ac clears +29: 0x4ffb0 uses the depth-sorted
        // path. Its incoming key narrows to float, while stored Z stays double.
        // Retain the original midpoint walk and tie placement, rather than
        // sorting by an all-double comparator after construction.
        const double key = double(float(p.z));
        if (points_.empty() || key > points_.front().z)
            points_.insert(points_.begin(), p);
        else if (key <= points_.back().z)
            points_.push_back(p);
        else {
            size_t cursor = 0, span = points_.size();
            bool forward = true;
            for (;;) {
                const auto step = span / 2;
                if (span > 1)
                    cursor = forward ? cursor + step : cursor - step;
                if (key < points_[cursor].z && points_[cursor + 1].z < key)
                    break;
                forward = key < points_[cursor].z;
                const bool again = span > 3;
                span = step;
                if (!again)
                    break;
            }
            points_.insert(points_.begin() + cursor + 1, p);
        }
    }
}
void StageStarfield::advance(bool paused) {
    if (paused)
        return;
    // Use binary64 .001, matching the original 0x42f28 arithmetic.
    depth_ += .001;
    if (depth_ > 1.)
        depth_ -= 2.;
    if (depth_ < -1.)
        depth_ += 2.;
}
void StageStarfield::append(std::vector<StageDraw> &out,
                            const std::array<neon::colour, 32> &palette) const {
    // 0x41928, normal stage's zero rotations. 0x50528 writes the X/Y camera
    // coordinates too; depth wrapping alone does not replace that parallax.
    // Projection and size
    // narrow to binary32 separately; colour is chosen after the visibility test.
    for (const auto &p : points_) {
        double z = p.z - depth_;
        if (z < -1.)
            z += 2.;
        if (z > 1.)
            z -= 2.;
        const double projectedDepth = z + z;
        if (projectedDepth <= 0)
            continue;
        const double inverse = double(float(1. / (projectedDepth * 16. + .01)));
        const double x = double(float((p.x - cameraPosition_.x) * inverse * 4.));
        const double y = double(float((p.y - cameraPosition_.y) * inverse * 3.));
        if (!(x > -.6 && x < .6 && y > -.6 && y < .6))
            continue;
        auto colour = p.colour < 16 ? palette.at(p.colour) : packed(p.colour);
        const float distance = float(std::abs(z));
        if (distance > .5f)
            colour.a *= double(1.f - (distance - .5f) / .5f);
        const double size = p.size * double(float(1. / (projectedDepth * 8. + .4)));
        out.push_back({{x, y}, {size, size}, colour, 0, p.sprite + 2});
    }
}
void updateStagePalette(StageRuntime &w, uint32_t frame) {
    const float phase = float((frame & 31U) << 1) / 63.f;
    w.terrainPalette[0] =
        packed(sampleColourRamp(phase, {presentation_data::RainbowStops.data(), 14}));
    constexpr int types[] = {8, 5, 0, 4, 2};
    for (int i = 0; i < 5; ++i)
        w.terrainPalette[size_t(i + 1)] = dynamic(types[i], frame);
    const std::array<uint32_t, 16> *flashes[] = {
        &presentation_data::Flash0, &presentation_data::Flash1, &presentation_data::Flash2,
        &presentation_data::Flash3, &presentation_data::Flash4, &presentation_data::Flash5,
        &presentation_data::Flash6, &presentation_data::Flash7};
    for (int i = 0; i < 8; ++i)
        w.terrainPalette[size_t(i + 6)] = packed((*flashes[i])[frame & 15U]);
    w.rainbow = w.terrainPalette[1];
}
StagePresentation::StagePresentation() {
    for (size_t i = 0; i < 16; ++i)
        tilePalette_[i] = packed(presentation_data::TerrainPalette[i]);
}
void StagePresentation::attach(StageRuntime &w, int vw, int vh) {
    // 0x48ffc measures before 0x46f70 adds tutorial text. Labels must not
    // enlarge the scroll limits even when they lie beyond occupied terrain.
    visibleColumns_ = vw;
    visibleRows_ = vh;
    camera.measure(w.layout, {w.initialBackup, w.initialBackup}, vw, vh);
}
void StagePresentation::requestCamera(const StageRuntime &w) {
    if (w.goat) {
        camera.request(attract && w.attractTicks > 0 ? w.goat->attractCameraTarget
                                                     : w.goat->stagePosition,
                       w.layout, w.bounds, w.goat->drawScale.x, w.goat->velocity.y);
        stars.setCamera(camera.backgroundRequest);
    }
}
void StagePresentation::baseDraw(StageActor &a, neon::vec2d scale, neon::colour tint) {
    // Base entity draw 0x30798: flash and delayed-submission counters mutate
    // even when no vertices are submitted. Redraws consume prepared packets.
    if (a.sprite < 0)
        return;
    if (a.flashTicks != 0) {
        --a.flashTicks;
        const double f = double(float(uint32_t(a.flashTicks) & 7U) / 7.f);
        tint.r = f * .5 + .5;
        tint.g = f;
        tint.b = 0;
    }
    const float diameter = a.radius + a.radius;
    if (a.drawDelay > 0) {
        --a.drawDelay;
        return;
    }
    draws_.push_back(
        {a.worldPosition, scale * double(diameter), tint, double(a.heading), a.sprite + 2});
}
void StagePresentation::scoreDraw(FloatingScoreActor &a) {
    // 0x4bf48. Glyph spacing includes 1.2, glyph geometry does not. Source
    // digits[firstDigit+1..15] include the explicitly initialized final zero.
    auto glyph = a.drawScale * double(a.radius);
    const auto spacing = glyph * 1.2;
    const int count = (a.iconSprite ? 16 : 15) - a.firstDigit;
    double x = (spacing.x * .5 + a.worldPosition.x) + double(float(count)) * spacing.x * -.5;
    const double y = spacing.y * -.5 + a.worldPosition.y - .04;
    if (a.iconSprite) {
        auto tint = a.owner().terrainPalette.at(size_t(a.iconPaletteIndex));
        tint.a = a.colour.a;
        const auto size = a.glyphScale * double(a.radius * a.layoutScale);
        draws_.push_back({{x, y}, size, tint, double(a.heading), a.iconSprite + 2});
        x += spacing.x;
    }
    for (int i = a.firstDigit + 1; i <= 15; ++i) {
        draws_.push_back(
            {{x, y}, glyph, a.colour, double(a.heading), a.digits[size_t(i)] + a.firstGlyph + 2});
        x += spacing.x;
    }
}
void StagePresentation::actor(StageRuntime &w, StageActor &a) {
    if (auto *machine = dynamic_cast<EnemyActor *>(&a);
        machine && !machine->patroller && machine->behavior == 4) {
        // 0x5dbd8 emits the tether from the ORIGINAL position, independently
        // of the ordinary stage fade, before the moving actor artwork.
        auto origin = stageToWorld(machine->originalPosition, w.gridOffset, w.layout, w.bounds);
        origin = origin - machine->drawScale * double(machine->radius);
        StageDraw beam{origin,
                       {double(machine->radius) * machine->drawScale.x * 2.,
                        machine->stagePosition.y - machine->originalPosition.y},
                       {1, 1, 1, 1},
                       0,
                       135};
        beam.flags = 0;
        draws_.push_back(beam);
    }
    if (auto *machine = dynamic_cast<EnemyActor *>(&a);
        machine && !machine->patroller && machine->behavior == 3) {
        // 0x5dbd8: body uses stage drawing, but each fireball is a direct base
        // draw. The hazard compares PREPARED world coordinates while drawing.
        auto tint = a.freezeTicks > 0 ? w.rainbow : a.colour;
        tint.a *= double(w.displayFade);
        baseDraw(a, (a.drawScale * double(a.stageDrawScale)) * double(a.stretch), tint);
        if (a.freezeTicks > 0)
            --a.freezeTicks;
        const auto centre = a.worldPosition, scale = a.drawScale;
        const float savedRadius = a.radius, savedAngle = a.heading;
        float angle =
            float(double(w.sourceTime) + a.stagePosition.x * 10. + a.stagePosition.y * 10.);
        if (machine->placementTransform & 4U)
            angle = -angle;
        neon::vec2d step{0, .03};
        neon::rotateClockwise(step.x, step.y, double(angle),
                              neon::ZeroAngleRule::PreserveCoordinates);
        a.radius = .02f;
        a.drawScale = {1, 1};
        a.heading = angle * 8.f;
        a.sprite = 106;
        for (unsigned i = 0; i < ((machine->placementTransform & 3U) | 4U); ++i) {
            a.worldPosition = a.worldPosition + step;
            if (w.goat && (w.goat->worldPosition - a.worldPosition).length() < .03)
                w.goat->hazard = true;
            baseDraw(a, a.drawScale, a.colour);
        }
        a.sprite = 105;
        a.worldPosition = centre;
        a.drawScale = scale;
        a.radius = savedRadius;
        a.heading = savedAngle;
        return;
    }
    if (auto *label = dynamic_cast<FloatingScoreActor *>(&a)) {
        scoreDraw(*label);
        return;
    }
    if (auto *goat = dynamic_cast<GoatActor *>(&a)) {
        if (goat->state >= 3)
            return;
        if (goat->state == 0) {
            // 0x5b428 draws the fifty history samples before the goat. Keep
            // their base-draw counter effects, and restore its live geometry.
            const auto savedPosition = a.worldPosition;
            const int savedSprite = a.sprite;
            uint32_t index = goat->followerCursor;
            for (float phase = 1.f; phase > 0; phase = float(double(phase) - .02)) {
                --index;
                const auto &sample = goat->followerHistory[index & 1023U];
                const auto local = sample.position + (w.gridOffset * w.layout.displayReciprocal);
                a.worldPosition = stageToWorld(local, w.gridOffset, w.layout, w.bounds);
                a.sprite = 119;
                const float alpha = phase * sample.speed;
                baseDraw(a, a.drawScale * (double(phase) * 1.4 * double(sample.speed)),
                         {1, 1, 1, std::clamp(double(alpha), 0., 1.)});
            }
            a.sprite = savedSprite;
            a.worldPosition = savedPosition;
        }
        if (goat->state == 1)
            return;
        auto tint = goat->invulnerability > 0 ? w.rainbow : neon::colour{1, 1, 1, 1};
        tint.a = goat->colour.a;
        baseDraw(a, a.drawScale * double(goat->zoom + 1.f), tint);
        return;
    }
    if (a.directWorldDraw) {
        baseDraw(a, a.drawScale, a.colour);
        return;
    }
    if (auto *pickup = dynamic_cast<PickupActor *>(&a)) {
        if (pickup->drawMarker)
            draws_.push_back({pickup->markerWorldPosition, {.02, .02}, {0, 1, 1, 1}, 0, 4});
        if (!pickup->drawMain)
            return;
    }
    auto tint = a.freezeTicks > 0 ? w.rainbow : a.colour;
    tint.a = tint.a * double(w.displayFade);
    baseDraw(a, (a.drawScale * double(a.stageDrawScale)) * double(a.stretch), tint);
    if (a.freezeTicks > 0)
        --a.freezeTicks;
}
void StagePresentation::actors(StageRuntime &w, StageActor *a) {
    for (; a; a = a->next)
        actor(w, *a);
}
void StagePresentation::prepareEditor(StageRuntime &w, const std::vector<StageDraw> &overlay,
                                      bool overview) {
    editor = true;
    overview_ = overview;
    draws_.clear();
    stars.append(draws_, w.terrainPalette);
    terrain(w, overview);
    actors(w, w.foregroundEffects);
    for (auto d : overlay) {
        if (overview && d.editorObject) {
            d.position =
                (d.position - (w.gridOffset * w.layout.cellSize - neon::vec2d{.5, .5})) * .27 +
                overviewOrigin(w);
            d.size = d.size * .27;
        }
        // 0x50818 reads template cursor +b0/+b4 and cached index +b44.
        // Terrain's loops leave (columns,rows) and its final visited index;
        // neither brush caller rewrites them to the editing cursor.
        if (d.brushTile) {
            if (tileWord(w, w.layout.columns, w.layout.rows, d.brushWord, d,
                         (w.layout.rows - 1) * 100 + w.layout.columns - 1))
                draws_.push_back(d);
        } else
            draws_.push_back(d);
    }
    editor = false;
    overview_ = false;
}
void StagePresentation::prepareMenu(StageRuntime &w, const std::vector<StageDraw> &overlay,
                                    bool preview) {
    draws_.clear();
    stars.append(draws_, w.terrainPalette);
    if (preview)
        terrain(w, true); // 0x48cbc(preview=1): template only, no actors/HUD
    appendOverlay(overlay);
}
void StagePresentation::prepare(StageRuntime &w) {
    draws_.clear();
    // Background callback 0x40ee8: preserve source layer/list order.
    stars.append(draws_, w.terrainPalette);
    actors(w, w.ordinaryEffects);
    actors(w, w.enemies);
    actors(w, w.grassBonuses);
    actors(w, w.contactObjects);
    actors(w, w.foregroundEffects);
    if (w.goat)
        actor(w, *w.goat);
    terrain(w);
    actors(w, w.placementOverlays);
    hudGraphics(w);
    actors(w, w.overlayEffects);
}
void StagePresentation::hudGraphics(StageRuntime &w) {
    // Tail of background callback 0x40ee8, AFTER terrain/placement holder,
    // BEFORE +3b4 overlays (including totalizer) and cellGrid's text pass.
    draws_.push_back({{0, .431}, {1, .128}, {0, 0, 0, 1}, 0, 2});
    if (editor || attract || !w.goat)
        return;
    auto &goat = *w.goat;
    if (w.sharedGrace > 0) {
        StageDraw d{
            {-.5, -.305}, {double(float(w.sharedGrace) / 1000.f), .01}, w.terrainPalette[8], 0, 2};
        d.flags = 0;
        draws_.push_back(d);
    }
    if (w.playerVisible) {
        auto band = [&](double x, double length, neon::colour c) {
            StageDraw d{{x, .405}, {length, .03}, c, 0, 2};
            d.flags = 0;
            draws_.push_back(d);
        };
        band(-.5, double(goat.hunger) > .2 ? double(.2f) : double(goat.hunger), {1, 0, 0, .4});
        const float middle = std::min(.6f, float(double(goat.hunger) - .2));
        if (middle > 0)
            band(-.3, double(middle), {0, 1, 0, .4});
        const float top = std::min(.2f, float(double(goat.hunger) - .8));
        if (top > 0)
            band(.3, double(top), {1, 1, 0, .4});
    }
    const neon::vec2d spareSize{std::fabs(goat.drawScale.x) * .05,
                                std::fabs(goat.drawScale.y) * .05};
    float x = -.47f;
    for (int i = 0; i < w.lives; ++i) {
        draws_.push_back({{double(x), .47}, spareSize, {1, 1, 1, 1}, 0, goat.sprite + 2});
        x = float(double(x) + .06);
    }
    if (w.exitContact) {
        const double y = rightHandedTouch ? -.30000000000000004 : -.4;
        draws_.push_back(
            {{.45, y}, {.06, .08}, w.terrainPalette[0], (goat.worldPosition.y - y) * 2.2, 167});
    }
    const int32_t fruit = fruitSpriteForStage(w.stageIndex);
    auto tint = w.terrainPalette.at(size_t(w.fruitPaletteIndex));
    draws_.push_back({{.31, .465}, w.spriteScale(fruit) * .04, tint, 0, fruit + 2});
}
void StagePresentation::decayTile(StageRuntime &w, int index, uint32_t raw, bool visible) {
    // Common visible/offscreen collapse mutation, 0x506c0 / 0x50818. Neighbor
    // addresses use SOURCE declaredColumns, even though rows physically stride
    // 100. Retain the odd asymmetry: horizontal +1, vertical +16 phase steps.
    if ((w.originalPauseFlag && !visible) || (raw & 0x840000U) != 0x840000U)
        return;
    uint32_t phase = (raw & 0x3fffffffU) >> 24;
    if (!phase)
        return;
    if (!w.originalPauseFlag)
        phase += 2;
    auto cell = [&](int i) -> uint32_t & {
        return physicalTerrainWord({w.working, w.backup}, i, 0);
    };
    if (phase <= ((raw & 0x600000U) ? 63U : 31U)) {
        cell(index) = (cell(index) & 0xc0ffffffU) | (phase << 24);
        return;
    }
    cell(index) = 0;
    if (((cell(index - w.layout.columns) & 255U) | 2U) == 6U)
        cell(index - w.layout.columns) = 0;
    if (((cell(index + w.layout.columns) & 255U) | 2U) == 7U)
        cell(index + w.layout.columns) = 0;
    for (int neighbor : {index - 1, index + 1, index - 100, index + 100}) {
        auto &value = cell(neighbor);
        if ((value & 0x840000U) == 0x840000U) {
            const uint32_t increment =
                (neighbor == index - 1 || neighbor == index + 1) ? 0x1000000U : 0x10000000U;
            value = (value & 0xc0ffffffU) | (((value + increment) >> 24 & 63U) << 24);
            if (!w.medium.splashSoundSuppressed)
                w.sounds.push_back({65, uint32_t(w.medium.sampleVolume) << 8, 0});
        }
    }
}
bool StagePresentation::tile(StageRuntime &w, int x, int y, StageDraw &out) {
    return tileWord(w, x, y, w.working.at(size_t(y) * 100 + size_t(x)), out);
}
bool StagePresentation::tileWord(StageRuntime &w, int x, int y, uint32_t raw, StageDraw &out,
                                 int mutationIndex) {
    const int index = mutationIndex < 0 ? y * 100 + x : mutationIndex;
    if (raw == 0 && !editor)
        return false;
    out.colour = tilePalette_.at((raw & 0x3fffU) >> 8);
    if (raw & 0x4000U)
        out.colour.a = .7;
    const uint32_t id = raw & 255U;
    if (raw & 0x80000000U) {
        const uint32_t phase = (terrainFrames + ((raw >> 21) & 0x7f8U)) & 127U;
        if (id == 26 || id == 10)
            w.glyphLookup[id] =
                (uint32_t(id == 26 ? 29 : 11) + ((terrainFrames & 4U) ? 1U : 0U)) | 0x80000U;
        if ((id == 26 || id == 10) && uint32_t(phase - 16U) > 32U)
            w.glyphLookup[id] = 0;
        if (id == 2) {
            w.glyphLookup[id] = ((terrainFrames >> 5) & 1U) | 0x10002U;
            if (phase > 96U)
                w.glyphLookup[id] = 0;
        }
    }
    const uint32_t glyph = w.glyphLookup.at(id);
    if (!(raw & 0x80000000U) && id == 45) {
        auto a = out.position, b = out.position;
        a.x += double(std::sin(float(a.y * 5. + double(w.sourceTime) * .7)));
        b.x += double(std::sin(float(b.y * 4. + double(w.sourceTime) * .9)));
        const double blue = double(std::sin(float((a - orbit_[1]).length() * 5.))) * .05 + .1;
        const double red = double(std::sin(float((b - orbit_[0]).length() * 4.))) * .05 + .1;
        out.colour = {red, 0, blue, (blue * red * -20. + 1.) * .6};
    } else if (!(raw & 0x80000000U) && id == 44)
        out.colour = w.rainbow;
    else if (!(raw & 0x80000000U) && id == 43)
        out.colour = {0, double(std::sin(float((orbit_[0] - orbit_[1]).length() * 3.))) * .05 + .1,
                      double(std::sin(float((out.position - orbit_[0]).length() * 6.))) * .3 + .7,
                      double(std::cos(float((orbit_[1] - out.position).length() * 9.))) * .2 + .3};
    float squash = 0;
    if (raw & 0x40000U) {
        const uint32_t phase = (raw & 0x3fffffffU) >> 24;
        const float displacement = float(phase) * .015625f;
        decayTile(w, index, raw, true);
        if (!(raw & 0x600000U)) {
            squash = displacement + displacement;
            out.position.y += double(displacement) * w.layout.cellSize.y;
        } else {
            squash = displacement;
            if (raw & 0x200000U)
                out.position.y += double(displacement) * w.layout.cellSize.y;
        }
        out.size.y -= double(squash) * w.layout.cellSize.y;
        out.colour.a = .8;
    }
    if ((raw & 0x840000U) == 0x800000U) {
        const bool upward = (raw & 0x1000000U) != 0;
        const uint32_t neighbor =
            physicalTerrainWord({w.working, w.backup}, x, y + (upward ? -1 : 1));
        if (!neighbor)
            physicalTerrainWord({w.working, w.backup}, x, y) = 0;
        else if ((neighbor & 0x40000U) &&
                 (upward ? ((neighbor & 0x400000U) || !(neighbor & 0x200000U))
                         : ((neighbor & 0x200000U) || !(neighbor & 0x400000U)))) {
            const float d = float((neighbor & 0x3fffffffU) >> 24) * .015625f;
            out.position.y += (upward ? -double(d) : double(d)) * w.layout.cellSize.y;
        }
    }
    float uvX = 0, uvY = 0;
    if (id == 47)
        uvY = 1.f - w.downwardCurrentPhase;
    else if (id == 46)
        uvY = w.downwardCurrentPhase;
    if ((raw & 0x240000U) == 0x200000U) {
        if ((glyph & 255U) == 16U)
            uvX = 1.f - w.upwardCurrentPhase;
        else if ((glyph & 255U) == 14U)
            uvX = w.downwardCurrentPhase;
    }
    if ((glyph & 0x80000U) && w.goat && (out.position - w.goat->worldPosition).length() < .035)
        w.goat->hazard = true;
    uint32_t tileId = glyph & 255U;
    if (!tileId || (raw & 0x440000U) == 0x400000U) {
        if (!editor || overview_)
            return false;
        out.colour = packed(((x ^ y) & 4) ? 0x0000ff60U : 0xa0007f50U);
        tileId = 0;
        if (raw & 0x400000U)
            out.colour = w.terrainPalette[0];
    }
    // 0x50818: truncate the float-scaled pixel displacement BEFORE normalizing.
    out.tile = true;
    out.frame = 0;
    out.u = double(int(float((tileId % 28U) * 18U) + uvX * 18.f) + 1) / 512.;
    out.v = double(int(float((tileId / 28U) * 18U + 419U) + uvY * 18.f) + 1) / 512.;
    out.du = 16. / 512.;
    out.dv = double(16.f * (1.f - squash)) / 512.;
    return true;
}
neon::vec2d StagePresentation::overviewOrigin(const StageRuntime &w) const {
    const auto cell = w.layout.cellSize * .27;
    const int left = int(3 - camera.upper.x), right = int(visibleColumns_ - 3 - camera.lower.x);
    const int top = int(4 - camera.upper.y), bottom = int(visibleRows_ - 6 - camera.lower.y);
    const int x = ((w.layout.columns - right) + left) / 2 - left,
              y = ((w.layout.rows - bottom) + top) / 2 - top;
    return {cell.x * double(float(x)) - double(float(w.layout.columns)) * .5 * cell.x,
            cell.y * (-4.) - double(float(w.layout.rows)) * .5 * cell.y +
                cell.y * double(float(y))};
}
void StagePresentation::terrain(StageRuntime &w, bool preview) {
    ++terrainFrames;
    constexpr float rates[] = {.005235987715423107f, .009817477315664291f, .030500900000333786f,
                               .05927533283829689f};
    for (size_t i = 0; i < 4; ++i) {
        orbit_[i] = {std::sin(double(phase_[i])), std::cos(double(phase_[i]))};
        phase_[i] = phase_[i] + rates[i];
    }
    for (size_t i = 0; i < 16; ++i)
        tilePalette_[i + 16] = w.terrainPalette[i];
    if (!preview && !w.medium.splashSoundSuppressed &&
        (w.stageIndex % 10 == 7 || w.stageIndex % 10 == 8))
        tilePalette_[1] = w.terrainPalette[0];
    auto cell = w.layout.cellSize;
    neon::vec2d origin{w.gridOffset.x * cell.x - .5, w.gridOffset.y * cell.y - .5};
    if (preview) {
        // 0x41d20 +3b62c (game +3c880): .27 scale and integer occupied-
        // extents centring, with a four-cell upward shift. No actor simulation.
        cell = cell * .27;
        origin = overviewOrigin(w);
    }
    double py = origin.y;
    for (int y = 0; y < w.layout.rows; ++y, py += cell.y) {
        // 0x41d20 skips rows strictly ABOVE the top bound. Rows below the
        // viewport still visit 0x506c0: collapsing terrain keeps advancing.
        if (!preview && !editor && py < -.5 - cell.y)
            continue;
        double px = origin.x;
        for (int x = 0; x < w.layout.columns; ++x, px += cell.x) {
            if (!preview &&
                (py <= -.5 - cell.y || py >= w.layout.scale.y + cell.y - .5 - cell.y * .5 ||
                 px <= -.5 - cell.x || px >= cell.x * .5 + .5)) {
                decayTile(w, y * 100 + x, w.working.at(size_t(y) * 100 + size_t(x)));
                continue;
            }
            StageDraw d;
            d.position = {px, py};
            d.size = cell;
            d.flags = 0;
            if (tile(w, x, y, d)) {
                d.colour.a = d.colour.a * double(w.displayFade);
                draws_.push_back(d);
            }
        }
    }
    camera.fastFrames = w.cameraSettleFrames;
    if (!preview && w.goat)
        camera.finishTerrain(w.gridOffset, w.goat->velocity, w.goat->horizontalImpulse,
                             w.goat->verticalImpulse, editor, attract);
    w.cameraSettleFrames = camera.fastFrames;
}
} // namespace goatup2
