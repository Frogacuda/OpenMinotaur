#version 300 es
// Recovered source: sm3-53-Clipscale_HLSL3.hlsl; program 53.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
   vec4 Clipscale_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec4 outpix;  outpix=vec4(vec4(0.0));   vec2 thIndex=vec2(vec2(((TexCoord0-0.5)*((1.0-Diffuse.b)*8.0))+0.5));   outpix=vec4(vec4(0.0));   if(thIndex.x>=0.0)    if(thIndex.x<1.0)     if(thIndex.y>=0.0)      if(thIndex.y<1.0)       outpix=vec4(texture(image0,thIndex));   return outpix*Diffuse.aaaa;  } 
void main(){colour=Clipscale_HLSL3(t0,t1,diffuse)*materialGain;}
