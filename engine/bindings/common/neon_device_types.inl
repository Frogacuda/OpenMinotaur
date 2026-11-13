// Compatibility names; the device implementation is shared by all adopters.
#include "neon/render_device.h"
namespace NEON_GAME {
namespace Neon_Internals {
using neon::render::_D3DPRIMITIVETYPE;
using neon::render::D3DPT_LINELIST;
using neon::render::D3DPT_LINESTRIP;
using neon::render::D3DPT_POINTLIST;
using neon::render::D3DPT_TRIANGLEFAN;
using neon::render::D3DPT_TRIANGLELIST;
using neon::render::D3DPT_TRIANGLESTRIP;
using neon::render::DeviceFlags;
using neon::render::DF_ALPHA_BLEND;
using neon::render::DF_DEPTH_TEST;
using neon::render::DF_FIRST_DRAW;
using neon::render::DF_STENCIL;
using neon::render::kMaxLiterals;
using neon::render::kMaxRenderTargets;
using neon::render::kMaxVertexBuffers;
using neon::render::kNumPrograms;
using neon::render::kNumUniforms;
using neon::render::LiteralTexture;
using neon::render::NeonBaseRender;
using neon::render::NeonDevice;
using neon::render::NeonVertex;
using neon::render::PROJ_ORTHO;
using neon::render::PROJ_UNSIGNED;
using neon::render::ProjectionFlags;
using neon::render::RenderTarget;
using neon::render::ShaderProgram;
using neon::render::U_C0;
using neon::render::U_C1;
using neon::render::U_C10;
using neon::render::U_C2;
using neon::render::U_C25;
using neon::render::U_C5;
using neon::render::U_C7;
using neon::render::U_C8;
using neon::render::U_C9;
using neon::render::U_MatTex0a;
using neon::render::U_MatTex0b;
using neon::render::U_MatTex1a;
using neon::render::U_MatTex1b;
using neon::render::U_MatTex2a;
using neon::render::U_MatTex2b;
using neon::render::U_Tex0;
using neon::render::U_Tex1;
using neon::render::U_Tex2;
using neon::render::U_Tex3;
using neon::render::U_Tex4;
using neon::render::U_Tex5;
using neon::render::U_WorldViewProj;
using neon::render::UniformSlot;
using neon::render::VertexBufferSlot;
using neon::render::vertexStride;
} // namespace Neon_Internals
} // namespace NEON_GAME
