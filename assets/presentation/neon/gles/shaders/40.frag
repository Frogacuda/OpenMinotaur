#version 300 es
// Recovered source: sm3-40-Ripplewarp2_HLSL3.hlsl; program 40.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
     vec4 Ripplewarp2_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec2 pOrig;  vec2 pOrig2;  vec4 outpix;   outpix.a = 0.0;   pOrig=vec2(TexCoord0-0.5);   pOrig2=vec2(TexCoord1-0.5);   vec2 pNorm=vec2(vec2(c25.b*normalize(pOrig)*Diffuse.b));   vec2 pNorm2=vec2(vec2(c25.a*normalize(pOrig2)*Diffuse.b));   float scal=sin(length(pOrig)*30.0*c25.r);   float scal2=sin(length(pOrig2)*30.0*c25.g);   pNorm.x=pNorm.x*scal+pNorm2.x*scal2+0.5;   pNorm.y=pNorm.y*scal+pNorm2.y*scal2+0.5;   outpix=vec4(texture(image0,pNorm));   return outpix*Diffuse.aaaa;  } 
void main(){colour=Ripplewarp2_HLSL3(t0,t1,diffuse)*materialGain;}
