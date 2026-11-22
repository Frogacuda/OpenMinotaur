#version 300 es
// Recovered source: sm3-52-ColourMixer3_HLSL3.hlsl; program 52.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
           vec4 ColourMixer3_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec4 inpix;  vec4 inpix2;  vec4 inpix3;  vec4 outpix;  outpix=vec4(vec4(0.0));  inpix=vec4(vec4(0.0));  inpix2=vec4(vec4(0.0));  inpix3=vec4(vec4(0.0));   inpix=vec4(texture(image0,TexCoord0));   inpix2=vec4(texture(image1,TexCoord1));   inpix3=vec4((Diffuse.b-0.5)*4.0*texture(image2,TexCoord1));   inpix3*=c25*4.0;   outpix=vec4(mix(inpix,inpix2,inpix3));   return outpix*Diffuse.aaaa;  } 
void main(){colour=ColourMixer3_HLSL3(t0,t1,diffuse)*materialGain;}
