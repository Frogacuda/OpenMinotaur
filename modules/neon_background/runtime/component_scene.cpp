#include "component_scene.h"
#include "pc_shapes.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <istream>
#include <ostream>
#include <set>
#include <stdexcept>
namespace neon::effects {
const char *componentKindName(ComponentKind k) {
    static const char *names[] = {
        "Generated texture",           "Textured plane",   "Recovered tunnel",
        "Vector layer / source",       "Recovered flower", "Generated shapes / source",
        "Saved layer effect / source", "3D primitive"};
    return int(k) >= 0 && int(k) < 8 ? names[int(k)] : "Unknown component";
}
const std::vector<int> &componentShaders() {
    static const std::vector<int> ids = {26, 12, 14, 46, 80, 0,  1,  2,  4,  5,  6,  8,  11,
                                         13, 15, 16, 18, 29, 39, 40, 47, 48, 49, 57, 59, 61,
                                         69, 70, 71, 72, 73, 74, 79, 81, 83, 84, 3,  85, 34};
    return ids;
}
const std::vector<ComponentFieldSpec> &componentFields(ComponentKind kind) {
    static const std::vector<ComponentFieldSpec> texture = {
        {Phase, "Phase", 0, 1},
        {Speed, "Phase speed", -1, 1},
        {RepeatX, "Texture width", .1f, 8},
        {RepeatY, "Texture height", .1f, 8},
        {Roll, "Texture rotation", -3.141593f, 3.141593f},
        {Gain, "Brightness", 0, 4},
        {Colour, "Material colour / mix", 0, 1},
        {C0, "Shader constant 0", 0, 1},
        {C1, "Shader constant 1", 0, 1},
        {C2, "Shader constant 2", 0, 1},
        {C3, "Shader constant 3", 0, 1},
        {C4, "Shader constant 4", 0, 1},
        {C5, "Shader constant 5", 0, 1},
        {C6, "Shader constant 6", 0, 1},
        {C7, "Shader constant 7", 0, 1}};
    static const std::vector<ComponentFieldSpec> planeBase = {
        {X, "Horizontal position", -4, 4},
        {Y, "Vertical position", -4, 4},
        {Scale, "Size", .05f, 3},
        {Roll, "Rotation", -3.141593f, 3.141593f},
        {Yaw, "Yaw", -1.4f, 1.4f},
        {Pitch, "Pitch", -1.4f, 1.4f},
        {Opacity, "Opacity", 0, 1},
        {RepeatX, "Texture repeats X", .1f, 16},
        {RepeatY, "Texture repeats Y", .1f, 16},
        {Speed, "Rotation speed", -1, 1}};
    static const std::vector<ComponentFieldSpec> tunnelBase = {
        {X, "Camera horizontal shift", -250, 250},
        {Y, "Camera vertical shift", -250, 250},
        {Yaw, "Camera yaw", -.7f, .7f},
        {Pitch, "Camera pitch", -.7f, .7f},
        {Roll, "Tunnel roll", -3.141593f, 3.141593f},
        {Opacity, "Opacity", 0, 1},
        {Speed, "Travel speed multiplier", 0, 3},
        {RepeatX, "Texture repeats around", 1, 16},
        {RepeatY, "Texture repeats along", 1, 32},
        {Morph, "Profile morph", 0, 1},
        {DeformX, "X distortion", -.25f, .25f},
        {DeformY, "Y distortion", -.25f, .25f},
        {DeformZ, "Z distortion", -.25f, .25f}};
    static const std::vector<ComponentFieldSpec> vector = {
        {X, "Horizontal position", -4, 4}, {Y, "Vertical position", -4, 4},
        {Scale, "Size", .05f, 3},          {Roll, "Rotation", -3.141593f, 3.141593f},
        {Opacity, "Opacity", 0, 1},        {Speed, "Animation speed", 0, 3},
        {BeamWidth, "Beam width", .5f, 4}, {BeamGlow, "Beam glow", 0, 2}};
    auto withConstants = [](std::vector<ComponentFieldSpec> fields) {
        for (int n = 0; n < 8; n++) {
            static const char *labels[] = {
                "Shader constant 0", "Shader constant 1", "Shader constant 2", "Shader constant 3",
                "Shader constant 4", "Shader constant 5", "Shader constant 6", "Shader constant 7"};
            fields.push_back({ComponentField(C0 + n), labels[n], 0, 1});
        }
        fields.push_back({Gain, "Material brightness", 0, 4});
        fields.push_back({Colour, "Material colour / mix", 0, 1});
        return fields;
    };
    static const auto flower =
        withConstants({{X, "Horizontal position", -500, 500},
                       {Y, "Vertical position", -500, 500},
                       {Scale, "Overall size", .1f, 3},
                       {Forward, "Forward position", -1000, 2000},
                       {InnerRadius, "Inner radius", 0, 1200},
                       {MiddleRadius, "Middle radius", 0, 1200},
                       {OuterRadius, "Outer radius", 0, 1200},
                       {MiddleDepth, "Middle depth", -1000, 1000},
                       {OuterDepth, "Outer depth", -1000, 1000},
                       {Segments, "Segments", 3, 1024},
                       {FlowerMode, "Mode: 0 flowing, 1 reflected, 2 folded UV, 3 both", 0, 3},
                       {Opacity, "Opacity", 0, 1},
                       {RepeatX, "Angular texture repeats", 1, 32},
                       {Yaw, "Yaw", -3.141593f, 3.141593f},
                       {Pitch, "Pitch", -3.141593f, 3.141593f},
                       {Roll, "Roll", -3.141593f, 3.141593f},
                       {Speed, "Rotation speed", -1, 1},
                       {DeformX, "X distortion", -1, 1},
                       {DeformY, "Y distortion", -1, 1},
                       {DeformZ, "Z distortion", -1, 1}});
    static const auto shape =
        withConstants({{Forward, "Layer depth (3D)", -5000, 12000},
                       {X, "Layer horizontal position", -8, 8},
                       {Y, "Layer vertical position", -8, 8},
                       {Scale, "Layer size", .05f, 3},
                       {Yaw, "Layer yaw", -1.4f, 1.4f},
                       {Pitch, "Layer pitch", -1.4f, 1.4f},
                       {Roll, "Layer roll", -3.141593f, 3.141593f},
                       {Opacity, "Layer opacity", 0, 1},
                       {Speed, "Generator animation speed", -1, 1},
                       {ShapeOrderA, "Primary order", 1.f, 32.f},
                       {ShapeOrderB, "Secondary order", 1.f, 32.f},
                       {ShapeWaveCountA, "Primary waves / clusters", 1.f, 32.f},
                       {ShapeWaveCountB, "Secondary waves / clusters", 1.f, 32.f},
                       {ShapePhaseA, "Primary phase", -1.f, 1.f},
                       {ShapeWavePhaseA, "Primary wave phase / arc", -1.f, 1.f},
                       {ShapePhaseB, "Secondary phase", -1.f, 1.f},
                       {ShapeWavePhaseB, "Secondary wave phase / arc", -1.f, 1.f},
                       {ShapeRadiusA, "Primary radius / width", -400.f, 400.f},
                       {ShapeRippleA, "Primary ripple / height", -400.f, 400.f},
                       {ShapeRadiusB, "Secondary radius / width", -400.f, 400.f},
                       {ShapeRippleB, "Secondary ripple / height", -400.f, 400.f},
                       {ShapeScaleAX, "Primary X / radius scaling", -1.25f, 1.25f},
                       {ShapeScaleAY, "Primary Y / recursive scaling", -1.25f, 1.25f},
                       {ShapeScaleBX, "Secondary X / radius scaling", -1.25f, 1.25f},
                       {ShapeScaleBY, "Secondary Y / recursive scaling", -1.25f, 1.25f},
                       {ShapeOffsetAX, "Primary centre X", -400.f, 400.f},
                       {ShapeOffsetAY, "Primary centre Y", -400.f, 400.f},
                       {ShapeOffsetBX, "Secondary centre X", -400.f, 400.f},
                       {ShapeOffsetBY, "Secondary centre Y", -400.f, 400.f},
                       {ShapeMirrorA, "Primary symmetry: 0 off, 1 on", 0.f, 1.f},
                       {ShapeMirrorB, "Secondary symmetry: 0 off, 1 on", 0.f, 1.f},
                       {ShapeSignedScale, "Signed fan / ribbon scale", -1.f, 1.f},
                       {ShapePalette0, "Palette colour 1", 0.f, 255.f},
                       {ShapePalette1, "Palette colour 2", 0.f, 255.f},
                       {ShapePalette2, "Palette colour 3", 0.f, 255.f},
                       {ShapePalette3, "Palette colour 4", 0.f, 255.f},
                       {ShapeAlpha0, "Colour 1 opacity", 0.f, 255.f},
                       {ShapeAlpha1, "Colour 2 opacity", 0.f, 255.f},
                       {ShapeAlpha2, "Colour 3 opacity", 0.f, 255.f},
                       {ShapeAlpha3, "Colour 4 opacity", 0.f, 255.f},
                       {ShapeGateDrive, "Threshold disc drive", -2.f, 2.f},
                       {ShapePaletteMode, "Palette: 0 original snapshot, 1 spectrum", 0, 1}});
    static const auto plane = withConstants(planeBase), tunnel = withConstants(tunnelBase);
    static const auto primitive =
        withConstants({{X, "Horizontal position", -8, 8},
                       {Y, "Vertical position", -8, 8},
                       {Forward, "Depth", -5000, 12000},
                       {PrimitiveNearClip, "Camera near clip (preserves view scale)", 1, 1000},
                       {Scale, "Overall size", .05f, 3},
                       {Yaw, "Yaw", -3.141593f, 3.141593f},
                       {Pitch, "Pitch", -3.141593f, 3.141593f},
                       {Roll, "Roll", -3.141593f, 3.141593f},
                       {Opacity, "Opacity", 0, 1},
                       {RepeatX, "Texture repeats U", .1f, 16},
                       {RepeatY, "Texture repeats V", .1f, 16},
                       {PrimitiveWidth, "Width", .05f, 4},
                       {PrimitiveHeight, "Height", .05f, 4},
                       {PrimitiveDepth, "Length / depth", .05f, 4},
                       {PrimitiveSegments, "Segments around", 3, 96},
                       {PrimitiveRings, "Rings along", 3, 48},
                       {PrimitiveTube, "Torus tube radius", .05f, .8f},
                       {PrimitiveDistortion, "Radial distortion", 0, .8f},
                       {PrimitiveWaves, "Distortion waves", 1, 16},
                       {Phase, "Distortion phase", 0, 1},
                       {Speed, "Animation speed", -1, 1}});
    static const auto layer = [] {
        auto fields = plane;
        for (auto &f : fields)
            if (f.field == Speed) {
                f.label = "Effect playback speed";
                f.minimum = 0;
                f.maximum = 3;
            }
        return fields;
    }();
    static const auto all = [&] {
        std::array<std::vector<ComponentFieldSpec>, 8> result = {texture, plane, tunnel, vector,
                                                                 flower,  shape, layer,  primitive};
        static const char *labels[3][10] = {
            {"Input 1 / U scale", "Input 1 / V scale", "Input 1 / U offset", "Input 1 / V offset",
             "Input 1 / Rotation", "Input 1 / Red tint", "Input 1 / Green tint",
             "Input 1 / Blue tint", "Input 1 / Brightness", "Input 1 / Opacity"},
            {"Input 2 / U scale", "Input 2 / V scale", "Input 2 / U offset", "Input 2 / V offset",
             "Input 2 / Rotation", "Input 2 / Red tint", "Input 2 / Green tint",
             "Input 2 / Blue tint", "Input 2 / Brightness", "Input 2 / Opacity"},
            {"Input 3 / U scale", "Input 3 / V scale", "Input 3 / U offset", "Input 3 / V offset",
             "Input 3 / Rotation", "Input 3 / Red tint", "Input 3 / Green tint",
             "Input 3 / Blue tint", "Input 3 / Brightness", "Input 3 / Opacity"}};
        for (auto &fields : result) {
            for (int i = 0; i < 3; ++i)
                for (int n = 0; n < 10; ++n)
                    fields.push_back({ComponentField(Input0ScaleU + i * 10 + n), labels[i][n],
                                      n < 2    ? .05f
                                      : n < 4  ? -8.f
                                      : n == 4 ? -3.141593f
                                               : 0.f,
                                      n < 2    ? 16.f
                                      : n < 4  ? 8.f
                                      : n == 4 ? 3.141593f
                                      : n == 9 ? 1.f
                                               : 4.f});
            for (auto f : {ComponentFieldSpec{SourceRed, "Source / red tint", 0, 4},
                           {SourceGreen, "Source / green tint", 0, 4},
                           {SourceBlue, "Source / blue tint", 0, 4},
                           {SourceBrightness, "Source / brightness", 0, 4},
                           {SourceOpacity, "Source / opacity", 0, 1}})
                fields.push_back(f);
        }
        for (auto f : {ComponentFieldSpec{GridColumns, "Grid / columns", 2, 48},
                       {GridRows, "Grid / rows", 2, 48},
                       {GridFade, "Grid / distant brightness", 0, 1},
                       {GridFadePower, "Grid / distance fade curve", .1f, 4}})
            result[int(ComponentKind::Vector)].push_back(f);
        return result;
    }();
    return all.at(int(kind));
}
Component Component::make(ComponentKind kind, int id) {
    if (int(kind) < 0 || int(kind) > 7)
        throw std::runtime_error("Unsupported component kind.");
    Component c;
    c.kind = kind;
    c.id = id;
    c.name = std::string(componentKindName(kind)) + " " + std::to_string(id);
    c.values[Scale] = c.values[Opacity] = c.values[Gain] = c.values[Colour] = 1;
    c.values[RepeatX] = c.values[RepeatY] = 2;
    c.values[Speed] = kind == ComponentKind::Tunnel ? 1 : .1f;
    c.values[PrimitiveNearClip] = 16;
    c.values[PrimitiveWidth] = c.values[PrimitiveHeight] = c.values[PrimitiveDepth] = 1;
    c.values[PrimitiveSegments] = 24;
    c.values[PrimitiveRings] = 12;
    c.values[PrimitiveTube] = .25f;
    c.values[PrimitiveWaves] = 3;
    if (kind == ComponentKind::Primitive) {
        c.values[Forward] = 700;
        c.values[RepeatX] = c.values[RepeatY] = 1;
    }
    if (kind == ComponentKind::Layer) {
        c.animated = true;
        c.updateHz = 60;
        c.resolution = 512;
        c.values[Speed] = 1;
        c.values[RepeatX] = c.values[RepeatY] = 1;
    }
    for (int i = 0; i < 3; ++i) {
        int n = Input0ScaleU + i * 10;
        c.values[n] = c.values[n + 1] = 1;
        for (int k = 5; k < 10; ++k)
            c.values[n + k] = 1;
    }
    for (int n = SourceRed; n <= SourceOpacity; ++n)
        c.values[n] = 1;
    c.values[GridColumns] = c.values[GridRows] = 24;
    c.values[GridFade] = .008f;
    c.values[GridFadePower] = 1.6f;
    c.values[BeamWidth] = 2;
    c.values[BeamGlow] = .38f;
    c.values[C0] = .28f;
    c.values[C1] = .35f;
    c.values[C2] = .31f;
    c.values[C3] = .3f;
    c.values[C4] = .21f;
    c.values[C5] = .17f;
    c.values[C6] = c.values[C7] = .5f;
    if (kind == ComponentKind::Texture) {
        c.shader = 46;
        c.animated = true;
    }
    if (kind == ComponentKind::Tunnel)
        c.animated = true;
    if (kind == ComponentKind::Vector)
        c.resolution = 512;
    if (kind == ComponentKind::Flower) {
        c.values[MiddleRadius] = 200;
        c.values[OuterRadius] = 300;
        c.values[Forward] = 400;
        c.values[Segments] = 256;
    }
    if (kind == ComponentKind::Shape) {
        c.values[Forward] = 500;
        c.profile = 2;
        c.profileB = 30;
        c.shader = 34;
        c.resolution = 256;
        c.values[C0] = c.values[C1] = c.values[C2] = 0;
        c.values[C3] = 1;
        c.values[ShapeOrderA] = c.values[ShapeOrderB] = 16;
        c.values[ShapeWavePhaseA] = c.values[ShapeWavePhaseB] = 1;
        c.values[ShapeWaveCountA] = 7;
        c.values[ShapeWaveCountB] = 3;
        c.values[ShapeRadiusA] = 220;
        c.values[ShapeRippleA] = 45;
        c.values[ShapeRadiusB] = 90;
        c.values[ShapeRippleB] = 30;
        c.values[ShapeScaleAX] = c.values[ShapeScaleBX] = 1;
        c.values[ShapeScaleAY] = c.values[ShapeScaleBY] = .9f;
        c.values[ShapePaletteMode] = 1;
        c.values[ShapeSignedScale] = 1;
        c.values[ShapeGateDrive] = .37f;
        for (int k = 0; k < 4; k++) {
            c.values[ShapePalette0 + k] = float(24 + k * 53);
            c.values[ShapeAlpha0 + k] = 255.f - k * 45;
        }
    }
    return c;
}
ComponentScene ComponentScene::shapeStudy() {
    ComponentScene s;
    s.items.clear();
    s.nextId = 1;
    s.name = "Recovered generated shapes";
    int a = s.add(ComponentKind::Shape);
    auto &first = s.at(a);
    first.animated = true;
    first.values[Speed] = .035f;
    first.values[X] = -.28f;
    first.values[Scale] = 1.25f;
    first.values[Yaw] = .25f;
    int b = s.add(ComponentKind::Shape);
    auto &second = s.at(b);
    second.profile = 8;
    second.profileB = 30;
    second.values[ShapeOrderA] = 12;
    second.values[ShapeRadiusA] = 130;
    second.values[ShapeRippleA] = 85;
    second.values[ShapeScaleAX] = .92f;
    second.values[ShapeScaleAY] = .92f;
    second.values[ShapeWavePhaseA] = .12f;
    second.values[X] = .4f;
    second.values[Yaw] = -.3f;
    second.animated = true;
    second.values[Speed] = -.025f;
    second.values[ShapePalette0] = 160;
    second.values[ShapePalette1] = 220;
    second.values[ShapePalette2] = 30;
    second.values[ShapePalette3] = 90;
    return s;
}
ComponentScene::ComponentScene() {
    int texture = add(ComponentKind::Texture);
    int tunnel = add(ComponentKind::Tunnel);
    at(tunnel).source = 10000 + texture;
}
int ComponentScene::add(ComponentKind kind) {
    if (items.size() >= 24)
        throw std::runtime_error("A scene supports up to 24 components.");
    int id = nextId++;
    items.push_back(Component::make(kind, id));
    auto error = validate();
    if (!error.empty()) {
        items.pop_back();
        throw std::runtime_error(error);
    }
    return id;
}
Component &ComponentScene::at(int id) {
    auto i = std::find_if(items.begin(), items.end(), [&](const auto &c) { return c.id == id; });
    if (i == items.end())
        throw std::runtime_error("Component no longer exists.");
    return *i;
}
const Component &ComponentScene::at(int id) const {
    return const_cast<ComponentScene *>(this)->at(id);
}
int ComponentScene::duplicate(int id) {
    auto original = at(id);
    if (items.size() >= 24)
        throw std::runtime_error("A scene supports up to 24 components.");
    auto pending = *this;
    original.id = pending.nextId++;
    original.name += " copy";
    auto i = std::find_if(pending.items.begin(), pending.items.end(),
                          [&](const auto &c) { return c.id == id; });
    pending.items.insert(i + 1, original);
    auto error = pending.validate();
    if (!error.empty())
        throw std::runtime_error(error);
    *this = std::move(pending);
    return original.id;
}
void ComponentScene::remove(int id) {
    for (auto &c : items)
        if ((c.source == 10000 + id || c.sourceB == 10000 + id || c.sourceC == 10000 + id))
            throw std::runtime_error(
                "This source is in use. Reconnect its consumers before removing it.");
    auto &c = at(id);
    items.erase(items.begin() + (&c - items.data()));
}
void ComponentScene::move(int id, int direction) {
    auto pending = *this;
    auto &c = pending.at(id);
    int index = int(&c - pending.items.data()), next = index + direction;
    if (next < 0 || next >= int(items.size()))
        return;
    std::swap(pending.items[index], pending.items[next]);
    auto error = pending.validate();
    if (!error.empty())
        throw std::runtime_error(error);
    *this = std::move(pending);
}
std::string ComponentScene::validate() const {
    if (items.size() > 24 || name.size() > 128 || name.find_first_of("\r\n") != std::string::npos)
        return "Invalid component scene.";
    std::set<int> ids, producers;
    int textures = 0, layers = 0;
    for (auto &c : items) {
        if (c.id < 1 || c.id > 100000 || !ids.insert(c.id).second || int(c.kind) < 0 ||
            int(c.kind) > 7 || c.name.size() > 128 ||
            c.name.find_first_of("\r\n") != std::string::npos)
            return "Invalid component identity.";
        if (std::find(componentShaders().begin(), componentShaders().end(), c.shader) ==
                componentShaders().end() ||
            c.blend < 0 || c.blend > 1 || c.profile < 0 ||
            c.profile > (c.kind == ComponentKind::Primitive ? 4
                         : c.kind == ComponentKind::Shape   ? 30
                                                            : 13) ||
            c.profileB < 0 || c.profileB > (c.kind == ComponentKind::Shape ? 30 : 13) ||
            c.updateHz < 1 || c.updateHz > 60 ||
            (c.resolution != 128 && c.resolution != 256 && c.resolution != 512 &&
             c.resolution != 1024))
            return "Invalid component material or quality.";
        if (c.vectorDocument.size() > 131072 || c.layerScene.size() > 1048576 ||
            c.layerOutput < -1 || c.layerOutput > 15 || c.layerComponent < -1 ||
            c.layerComponent > 100000)
            return "Invalid embedded layer/vector data.";
        if (c.kind == ComponentKind::Layer && (c.layerScene.empty() || ++layers > 4))
            return "A scene supports up to four saved layer sources.";
        for (int input :
             {c.source, c.sourceB ? c.sourceB : c.source, c.sourceC ? c.sourceC : c.source}) {
            if (input >= 10000) {
                if (!producers.count(input - 10000))
                    return "A texture source must appear before the component that uses it. "
                           "Forward "
                           "reads and cycles are not allowed.";
            } else if (input != 1 && input != 2 && input != 3 && input != 4 && input != 5 &&
                       input != 6 && input != 8 && input != 9 && input != 147)
                return "Unavailable source texture.";
        }
        if (c.kind == ComponentKind::Vector && (c.profile > 4 || c.shader != 26))
            return "Vector layers use the beam material and one of four vector scenes.";
        if (c.kind == ComponentKind::Shape &&
            (!neon::pc::recoveredShapeKind(c.profile) || !neon::pc::recoveredShapeKind(c.profileB)))
            return "Generated shapes require recovered families.";
        for (float f : c.values)
            if (!std::isfinite(f))
                return "Non-finite component parameter.";
        for (auto f : componentFields(c.kind))
            if (c.values[f.field] < f.minimum || c.values[f.field] > f.maximum)
                return "Component parameter outside supported range.";
        if (c.producer()) {
            producers.insert(c.id);
            if (++textures > 8)
                return "A scene supports up to eight generated sources.";
        }
    }
    return {};
}
void ComponentScene::write(std::ostream &out) const {
    auto error = validate();
    if (!error.empty())
        throw std::runtime_error(error);
    out << "COMPONENTS 6\n"
        << std::quoted(name) << ' ' << items.size() << ' ' << nextId << '\n'
        << std::setprecision(9);
    for (auto &c : items) {
        out << c.id << ' ' << int(c.kind) << ' ' << std::quoted(c.name) << ' ' << c.enabled << ' '
            << c.animated << ' ' << c.visible << ' ' << c.source << ' ' << c.sourceB << ' '
            << c.sourceC << ' ' << c.shader << ' ' << c.blend << ' ' << c.resolution << ' '
            << c.profile << ' ' << c.profileB << ' ' << c.updateHz;
        for (float f : c.values)
            out << ' ' << f;
        out << ' ' << c.layerOutput << ' ' << c.layerComponent << ' ' << std::quoted(c.layerScene)
            << ' ' << std::quoted(c.vectorDocument) << '\n';
    }
}
ComponentScene ComponentScene::read(std::istream &in) {
    ComponentScene scene;
    scene.items.clear();
    scene.nextId = 1;
    std::string magic;
    int version, count;
    if (!(in >> magic >> version) || magic != "COMPONENTS" || (version < 1 || version > 6) ||
        !(in >> std::quoted(scene.name) >> count) || count < 0 || count > 24)
        throw std::runtime_error("Invalid component scene header.");
    if (version >= 4 && (!(in >> scene.nextId) || scene.nextId < 1 || scene.nextId > 100001))
        throw std::runtime_error("Invalid next component identity");
    for (int n = 0; n < count; n++) {
        Component c;
        int kind, enabled, animated, visible;
        if (!(in >> c.id >> kind >> std::quoted(c.name) >> enabled >> animated >> visible >>
              c.source >> c.sourceB >> c.sourceC >> c.shader >> c.blend >> c.resolution >>
              c.profile >> c.profileB >> c.updateHz) ||
            kind < 0 || kind > (version >= 4 ? 7 : 5) || enabled < 0 || enabled > 1 ||
            animated < 0 || animated > 1 || visible < 0 || visible > 1)
            throw std::runtime_error("Invalid component record.");
        c.values = Component::make(ComponentKind(kind), c.id).values;
        c.kind = ComponentKind(kind);
        c.enabled = enabled;
        c.animated = animated;
        c.visible = visible;
        for (int field = 0; field < (version == 1   ? int(InnerRadius)
                                     : version == 2 ? int(ShapeOrderA)
                                     : version == 3 ? int(PrimitiveWidth)
                                     : version == 4 ? int(Input0ScaleU)
                                     : version == 5 ? int(PrimitiveNearClip)
                                                    : int(FieldCount));
             ++field)
            if (!(in >> c.values[field]))
                throw std::runtime_error("Invalid component parameter.");
        if (version >= 4 && !(in >> c.layerOutput >> c.layerComponent >>
                              std::quoted(c.layerScene) >> std::quoted(c.vectorDocument)))
            throw std::runtime_error("Invalid embedded component data");
        if (version <= 5 && c.kind == ComponentKind::Primitive && c.profile != 0)
            c.values[PrimitiveNearClip] = 400;
        scene.items.push_back(c);
        scene.nextId = std::max(scene.nextId, c.id + 1);
    }
    auto error = scene.validate();
    if (!error.empty())
        throw std::runtime_error(error);
    return scene;
}

// Names and constant usage from the local recovered shader ports.
const char *componentShaderName(int id) {
    switch (id) {
    case 34:
        return "Vertex colour / original modulation";
    case 3:
        return "Overlay / maximum";
    case 85:
        return "Planet2 / texture-displaced surface";
    case 26:
        return "Copy Pure";
    case 12:
        return "RGB-GRB";
    case 14:
        return "RGB-RBG";
    case 46:
        return "RadialColourise sm3";
    case 80:
        return "Galactica sm3";
    case 0:
        return "Modulate";
    case 1:
        return "unresolved-name-2";
    case 2:
        return "Crossfade";
    case 4:
        return "Burnthru";
    case 5:
        return "Displace";
    case 6:
        return "Merge 2";
    case 8:
        return "Spinlerp";
    case 11:
        return "Burnthru 2";
    case 13:
        return "RGB-BGR";
    case 15:
        return "RGB-GBR";
    case 16:
        return "RGB-BRG";
    case 18:
        return "G Blend";
    case 29:
        return "Inv G Blend";
    case 39:
        return "Ripplewarp 1 sm3";
    case 40:
        return "Ripplewarp 2 sm3";
    case 47:
        return "WobblyWarp sm3";
    case 48:
        return "Fluffinator sm3";
    case 49:
        return "Bifluffinator sm3";
    case 57:
        return "SoftMix3Glow sm3";
    case 59:
        return "RGBSplit sm3";
    case 61:
        return "RGBSplit2 sm3";
    case 69:
        return "Round tunnel 2A sm3";
    case 70:
        return "Ribbed tunnel sm3";
    case 71:
        return "Pointfield sm3";
    case 72:
        return "Outline Merge 0 sm3";
    case 73:
        return "Outline Merge 1 sm3";
    case 74:
        return "zomg sm3";
    case 79:
        return "Lisspoint";
    case 81:
        return "GalacticaAlt sm3";
    case 83:
        return "Glargle3 sm3";
    case 84:
        return "Planet";
    default:
        return "Unknown";
    }
}
unsigned componentConstantMask(int id) {
    switch (id) {
    case 34:
        return 15;
    case 3:
        return 0;
    case 85:
        return 255;
    case 26:
        return 0u;
    case 12:
        return 0u;
    case 14:
        return 0u;
    case 46:
        return 0u;
    case 80:
        return 255u;
    case 0:
        return 0u;
    case 1:
        return 0u;
    case 2:
        return 0u;
    case 4:
        return 0u;
    case 5:
        return 0u;
    case 6:
        return 0u;
    case 8:
        return 0u;
    case 11:
        return 0u;
    case 13:
        return 0u;
    case 15:
        return 0u;
    case 16:
        return 0u;
    case 18:
        return 0u;
    case 29:
        return 0u;
    case 39:
        return 1u;
    case 40:
        return 15u;
    case 47:
        return 0u;
    case 48:
        return 15u;
    case 49:
        return 7u;
    case 57:
        return 15u;
    case 59:
        return 0u;
    case 61:
        return 0u;
    case 69:
        return 31u;
    case 70:
        return 127u;
    case 71:
        return 7u;
    case 72:
        return 0u;
    case 73:
        return 31u;
    case 74:
        return 31u;
    case 79:
        return 255u;
    case 81:
        return 255u;
    case 83:
        return 247u;
    case 84:
        return 255u;
    default:
        return 0;
    }
}
std::vector<ComponentFieldSpec> editableComponentFields(const Component &c) {
    std::vector<ComponentFieldSpec> result;
    unsigned mask = componentConstantMask(c.shader);
    static const char *gal[] = {"Sample count",    "Phase X",           "Phase Y",
                                "Orbit radius",    "Phase increment X", "Phase increment Y",
                                "Radius change X", "Radius change Y"};
    for (auto f : componentFields(c.kind)) {
        if (f.field >= Input0ScaleU && f.field < Input0ScaleU + 30) {
            int input = (f.field - Input0ScaleU) / 10;
            if (c.kind == ComponentKind::Vector || !(componentSamplerMask(c.shader) & (1 << input)))
                continue;
        }
        if (f.field >= GridColumns && f.field <= GridFadePower &&
            (c.kind != ComponentKind::Vector || c.profile != 4))
            continue;
        if (c.kind == ComponentKind::Shape) {
            int field = int(f.field), family = -1;
            if (field == ShapeOrderA || field == ShapeWaveCountA || field == ShapePhaseA ||
                field == ShapeWavePhaseA || field == ShapeRadiusA || field == ShapeRippleA ||
                field == ShapeScaleAX || field == ShapeScaleAY || field == ShapeOffsetAX ||
                field == ShapeOffsetAY || field == ShapeMirrorA)
                family = c.profile;
            if (field == ShapeOrderB || field == ShapeWaveCountB || field == ShapePhaseB ||
                field == ShapeWavePhaseB || field == ShapeRadiusB || field == ShapeRippleB ||
                field == ShapeScaleBX || field == ShapeScaleBY || field == ShapeOffsetBX ||
                field == ShapeOffsetBY || field == ShapeMirrorB)
                family = c.profileB;
            bool secondaryRecursiveCount = field == ShapeWaveCountA && c.profileB == 8;
            if (secondaryRecursiveCount) {
                family = 8;
                f.label = "Secondary recursive count";
            }
            if (field == ShapeOrderB && c.profileB == 8)
                continue;
            if (family == 30)
                continue;
            if ((field == ShapeOrderA || field == ShapeOrderB || field == ShapePhaseA ||
                 field == ShapePhaseB) &&
                (family == 3 || family == 5))
                continue;
            if ((field == ShapeWavePhaseA || field == ShapeWavePhaseB) &&
                (family == 3 || family == 5 || family == 9))
                continue;
            if (!secondaryRecursiveCount &&
                (field == ShapeWaveCountA || field == ShapeWaveCountB) &&
                (family == 0 || family == 3 || family == 4 || family == 5 || family == 8))
                continue;
            if ((field >= ShapeScaleAX && field <= ShapeScaleBY) && family != 0 && family != 1 &&
                family != 4 && family != 6 && family != 8 && family != 9)
                continue;
            if (field == ShapeSignedScale && c.profile != 7 && c.profile != 9 && c.profileB != 7 &&
                c.profileB != 9)
                continue;
            if (family == 6) {
                if (field == ShapeScaleAX || field == ShapeScaleBX)
                    f.label = "Wave threshold";
                if (field == ShapeScaleAY || field == ShapeScaleBY)
                    f.label = "Driven radius multiplier";
            }
            if (field == ShapeGateDrive && c.profile != 6 && c.profileB != 6)
                continue;
        }

        if (f.field >= C0 && f.field <= C7) {
            int n = f.field - C0;
            if (!(mask & (1u << n)))
                continue;
            if (c.shader == 34) {
                static const char *names[] = {"Red modulation", "Green modulation",
                                              "Blue modulation", "Colour intensity"};
                f.label = names[n];
            }
            if (c.shader == 80 || c.shader == 81)
                f.label = gal[n];
            if (c.shader == 85) {
                static const char *planet[] = {"Sphere centre X",   "Sphere centre Y",
                                               "Sphere depth",      "Light depth",
                                               "Surface repeats U", "Surface repeats V",
                                               "Surface scroll U",  "Surface scroll V"};
                f.label = planet[n];
            }
        }
        result.push_back(f);
    }
    return result;
}

unsigned componentSamplerMask(int id) {
    switch (id) {
    case 34:
        return 0;
    case 3:
    case 85:
        return 3;
    case 0:
        return 1u;
    case 1:
        return 1u;
    case 2:
        return 3u;
    case 4:
        return 3u;
    case 5:
        return 3u;
    case 6:
        return 3u;
    case 8:
        return 3u;
    case 11:
        return 3u;
    case 12:
        return 3u;
    case 13:
        return 3u;
    case 14:
        return 3u;
    case 15:
        return 3u;
    case 16:
        return 3u;
    case 18:
        return 3u;
    case 26:
        return 1u;
    case 29:
        return 3u;
    case 39:
        return 1u;
    case 40:
        return 1u;
    case 46:
        return 1u;
    case 47:
        return 3u;
    case 48:
        return 3u;
    case 49:
        return 3u;
    case 57:
        return 7u;
    case 59:
        return 1u;
    case 61:
        return 3u;
    case 69:
        return 3u;
    case 70:
        return 1u;
    case 71:
        return 1u;
    case 72:
        return 3u;
    case 73:
        return 3u;
    case 74:
        return 1u;
    case 79:
        return 1u;
    case 80:
        return 1u;
    case 81:
        return 1u;
    case 83:
        return 7u;
    case 84:
        return 1u;
    default:
        return 1;
    }
}
} // namespace neon::effects
