#version 300 es
// Recovered source: sm3-23-threetex_warp_HLSL.hlsl; program 23.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
         const vec4 c_1=vec4(vec4(vec4(0.5,0.5,1.5,0.2)));  vec4 threetex_warp_HLSL(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {    vec4 r_5=vec4(vec4(texture(image1,TexCoord1)));    vec4 r_4=vec4(vec4(Diffuse.bbbb*r_5));    vec4 r_3=vec4(vec4(c_1.zzzz*r_5));    vec2 r_1=vec2(vec2(TexCoord0+r_4.xy));    vec4 r_6=vec4(vec4(texture(image0,TexCoord0)));    vec4 outpix=vec4(vec4(texture(image2,r_1)));    outpix=vec4(Diffuse.aaaa*mix(outpix,r_6,r_3.y));    return outpix;  } 
void main(){colour=threetex_warp_HLSL(t0,t1,diffuse)*materialGain;}
