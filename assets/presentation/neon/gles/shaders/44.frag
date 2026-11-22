#version 300 es
// Recovered source: sm3-44-ThingyDisplace_HLSL3.hlsl; program 44.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
     vec4 ThingyDisplace_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {   vec2 pOrig;   vec2 pOrig2;   vec4 outpix;   outpix=vec4(vec4(0.0));   pOrig=vec2(TexCoord1-0.5);   vec4 inpix=vec4(vec4(texture(image0,TexCoord0)));  inpix.a=0.0;   float scal=length(inpix);   vec2 pNorm=vec2(vec2(scal*normalize(pOrig)*Diffuse.b));   pOrig.x=pNorm.x+0.0;   pOrig.y=pNorm.y+0.0;   outpix=vec4(texture(image0,pOrig));   return outpix*Diffuse.aaaa;  } 
void main(){colour=ThingyDisplace_HLSL3(t0,t1,diffuse)*materialGain;}
