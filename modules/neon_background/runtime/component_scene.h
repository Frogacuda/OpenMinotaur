#pragma once
#include <array>
#include <iosfwd>
#include <string>
#include <vector>
namespace neon::effects {
enum class ComponentKind { Texture, Plane, Tunnel, Vector, Flower, Shape, Layer, Primitive };
enum ComponentField {
    X,
    Y,
    Scale,
    Roll,
    Yaw,
    Pitch,
    Opacity,
    Speed,
    Phase,
    RepeatX,
    RepeatY,
    Morph,
    DeformX,
    DeformY,
    DeformZ,
    Gain,
    C0,
    C1,
    C2,
    C3,
    C4,
    C5,
    C6,
    C7,
    BeamWidth,
    BeamGlow,
    Colour,
    InnerRadius,
    MiddleRadius,
    OuterRadius,
    MiddleDepth,
    OuterDepth,
    Segments,
    FlowerMode,
    Forward,
    ShapeOrderA,
    ShapeOrderB,
    ShapeWaveCountA,
    ShapeWaveCountB,
    ShapePhaseA,
    ShapeWavePhaseA,
    ShapePhaseB,
    ShapeWavePhaseB,
    ShapeRadiusA,
    ShapeRippleA,
    ShapeRadiusB,
    ShapeRippleB,
    ShapeScaleAX,
    ShapeScaleAY,
    ShapeScaleBX,
    ShapeScaleBY,
    ShapeOffsetAX,
    ShapeOffsetAY,
    ShapeOffsetBX,
    ShapeOffsetBY,
    ShapeMirrorA,
    ShapeMirrorB,
    ShapeSignedScale,
    ShapePalette0,
    ShapePalette1,
    ShapePalette2,
    ShapePalette3,
    ShapeAlpha0,
    ShapeAlpha1,
    ShapeAlpha2,
    ShapeAlpha3,
    ShapeGateDrive,
    ShapePaletteMode,
    PrimitiveWidth,
    PrimitiveHeight,
    PrimitiveDepth,
    PrimitiveSegments,
    PrimitiveRings,
    PrimitiveTube,
    PrimitiveDistortion,
    PrimitiveWaves,
    Input0ScaleU,
    Input0ScaleV,
    Input0OffsetU,
    Input0OffsetV,
    Input0Rotation,
    Input0Red,
    Input0Green,
    Input0Blue,
    Input0Brightness,
    Input0Opacity,
    Input1ScaleU,
    Input1ScaleV,
    Input1OffsetU,
    Input1OffsetV,
    Input1Rotation,
    Input1Red,
    Input1Green,
    Input1Blue,
    Input1Brightness,
    Input1Opacity,
    Input2ScaleU,
    Input2ScaleV,
    Input2OffsetU,
    Input2OffsetV,
    Input2Rotation,
    Input2Red,
    Input2Green,
    Input2Blue,
    Input2Brightness,
    Input2Opacity,
    SourceRed,
    SourceGreen,
    SourceBlue,
    SourceBrightness,
    SourceOpacity,
    GridColumns,
    GridRows,
    GridFade,
    GridFadePower,
    PrimitiveNearClip,
    FieldCount
};
struct ComponentFieldSpec {
    ComponentField field;
    const char *label;
    float minimum, maximum;
};
struct Component {
    int id = 0;
    ComponentKind kind = ComponentKind::Plane;
    std::string name;
    bool enabled = true, animated = false, visible = true;
    int source = 3, sourceB = 0, sourceC = 0, shader = 26, blend = 0, resolution = 256, profile = 0,
        profileB = 1, updateHz = 30;
    std::array<float, FieldCount> values{};
    std::string layerScene, vectorDocument;
    int layerOutput = -1, layerComponent = -1;
    static Component make(ComponentKind, int);
    bool producer() const {
        return kind == ComponentKind::Texture || kind == ComponentKind::Vector ||
               kind == ComponentKind::Shape || kind == ComponentKind::Layer;
    }
};
struct ComponentScene {
    std::vector<Component> items;
    std::string name = "Component scene";
    int nextId = 1;
    ComponentScene();
    static ComponentScene shapeStudy();
    int add(ComponentKind);
    int duplicate(int);
    void remove(int);
    void move(int, int);
    Component &at(int);
    const Component &at(int) const;
    std::string validate() const;
    void write(std::ostream &) const;
    static ComponentScene read(std::istream &);
};
const char *componentKindName(ComponentKind);
const std::vector<ComponentFieldSpec> &componentFields(ComponentKind);
const std::vector<int> &componentShaders();
const char *componentShaderName(int);
unsigned componentConstantMask(int);
unsigned componentSamplerMask(int);
std::vector<ComponentFieldSpec> editableComponentFields(const Component &);
} // namespace neon::effects
