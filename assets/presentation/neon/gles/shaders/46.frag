#version 300 es
// Recovered source: sm3-46-RadialColourise_HLSL3.hlsl; program 46.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
     vec4 RadialColourise_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec2 pOrig;  vec4 outpix;    pOrig=vec2(TexCoord1-0.5);   vec4 inpix=vec4(vec4(texture(image0,TexCoord0)));   inpix.a=0.0;   outpix.a = 0.0;   float scal=length(inpix);   vec2 pNorm=vec2(vec2(scal*normalize(pOrig)*Diffuse.b));   outpix.r=pNorm.x;   outpix.g=pNorm.y;   outpix.b=inpix.b;   return outpix*Diffuse.aaaa;  } 
void main(){colour=RadialColourise_HLSL3(t0,t1,diffuse)*materialGain;}
