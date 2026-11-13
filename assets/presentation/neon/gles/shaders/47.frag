#version 300 es
// Recovered source: sm3-47-WobblyWarp_HLSL3.hlsl; program 47.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
        vec4 WobblyWarp_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec2 pOrig;  vec2 pOrig2;  vec2 pNorm;  vec4 inpix;  vec4 outpix;    outpix.a = 0.0;   pOrig=vec2(TexCoord1-0.5);   inpix=vec4(texture(image1,TexCoord1));   float scal2=(pOrig.y/length(pOrig))*(inpix.x-0.5)*(Diffuse.b-0.5);   float scal=(pOrig.x/length(pOrig))*(inpix.y-0.5)*(Diffuse.b-0.5);   pNorm=vec2(normalize(pOrig));   pOrig.x=pNorm.x*scal2+TexCoord0.x;   pOrig.y=pNorm.y*scal+TexCoord0.y;   outpix=vec4(texture(image0,pOrig));   return outpix*Diffuse.aaaa;  } 
void main(){colour=WobblyWarp_HLSL3(t0,t1,diffuse)*materialGain;}
