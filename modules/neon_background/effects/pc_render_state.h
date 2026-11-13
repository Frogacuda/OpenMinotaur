#pragma once
#include "asset_reader.h"
#include "pc_geometry.h"
#include "pc_texture.h"
#include <filesystem>
namespace neon::pc {
using Matrix4 = std::array<float, 16>;
Matrix4 identityMatrix();
Matrix4 multiplyMatrix(const Matrix4 &, const Matrix4 &);
Matrix4 translationMatrix(float x, float y, float z);
Matrix4 lookAtMatrix(Point eye, Point target, Point up);
// Matches 004409a0's argument order and epsilon expansion, then D3DX LH math.
Matrix4 originalPerspective(float top, float bottom, float left, float right, float nearZ,
                            float farZ);
Matrix4 originalStackView();
Matrix4 originalStackProjection(int width, int height, bool mixerPass = false);
Matrix4 originalCubeModelView(const ShapeSettings &, const Matrix4 &parent);
struct ShapeDisplayInstance {
    Matrix4 modelView;
    std::array<float, 4> constant25;
};
// Display-list placement from 004747b0, separate from base mesh generation.
// Special list12 requires the original configuration's instance count explicitly.
std::vector<ShapeDisplayInstance> originalShapeDisplay(const ShapeSettings &, const Matrix4 &parent,
                                                       float pitchInput, float yawInput,
                                                       float seconds, int list12Count);
struct StackRenderSetup {
    int width, height;
    bool clearScratch;
    // Stored TEXCON metadata; original effect draws retain global MIRROR sampling.
    unsigned addressU, addressV;
    std::array<float, 4> constant25, constant26;
};
StackRenderSetup stackRenderSetup(const ShapeSettings &terminal);
enum class TextureSourceKind { Retained, Root, Stack, Alternate };
struct TextureBinding {
    TextureSourceKind kind;
    int index;
};
TextureBinding originalTextureBinding(int selector);
RootPixels loadOriginalRoot(const std::filesystem::path &, unsigned selector,
                            const AssetReader &reader = {});
} // namespace neon::pc
