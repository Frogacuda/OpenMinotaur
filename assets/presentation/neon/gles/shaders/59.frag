#version 300 es
// Recovered source: sm3-59-RGBSplit_HLSL3.hlsl; program 59.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
     vec4 RGBSplit_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec4 inpix1=vec4(vec4( 0.0));  vec4 outpix=vec4(vec4( 0.0));   vec2 baseV2=vec2(vec2((TexCoord1-0.5)*(Diffuse.b-0.5)));   vec2 baseV1=vec2(vec2(baseV2));   outpix.r=texture(image0,TexCoord0+baseV1).r;   baseV1+=baseV2;   outpix.g=texture(image0,TexCoord0+baseV1).g;   baseV1+=baseV2;   outpix.b=texture(image0,TexCoord0+baseV1).b;   outpix.a=0.0;   return outpix*Diffuse.aaaa;  } 
void main(){colour=RGBSplit_HLSL3(t0,t1,diffuse)*materialGain;}
