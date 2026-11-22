#version 300 es
// Recovered source: sm3-31-Cut2_HLSL.hlsl; program 31.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
      vec4 Cut2_HLSL(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {    vec4 r_5=vec4(vec4(texture(image1,TexCoord1)));    vec4 r_1=vec4(vec4(texture(image0,TexCoord0)));    vec4 r_3=vec4(vec4(r_5-Diffuse.bbbb));    float thresh=r_3.r+r_3.g+r_3.b;    vec4 outpix=vec4(vec4(r_1));    if(thresh<0.0) outpix=vec4(r_5);    return outpix*Diffuse.aaaa;  } 
void main(){colour=Cut2_HLSL(t0,t1,diffuse)*materialGain;}
