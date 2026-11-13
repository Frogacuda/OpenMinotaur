#version 300 es
// Recovered source: sm3-57-SoftMix3Glow_HLSL3.hlsl; program 57.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
           vec4 SoftMix3Glow_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  float interp;  vec4 maskpix=vec4(vec4( 0.0));  vec4 inpix1=vec4(vec4(0.0));  vec4 inpix2=vec4(vec4(0.0));  vec4 outpix=vec4(vec4(0.0));   vec2 thIndex=vec2(vec2(TexCoord0));   float bWidth=Diffuse.b;   float bCentre=(c25.a*4.0)-1.5;   maskpix=vec4(texture(image2,TexCoord0));   inpix1=vec4(texture(image0,TexCoord0));   inpix2=vec4(texture(image1,TexCoord1));   maskpix.a=0.0;   float thresh=length(maskpix);   if(thresh<(bCentre-bWidth))   {     return inpix1*Diffuse.aaaa;   }   if(thresh>(bCentre+bWidth))   {     return inpix2*Diffuse.aaaa;   }   if(thresh<bCentre)   {     interp=(bCentre-thresh)/bWidth;     outpix=vec4(mix(inpix1,c25,1.0-interp));     return outpix*Diffuse.aaaa;   }   interp=(thresh-bCentre)/bWidth;   outpix=vec4(mix(c25,inpix2,interp));   return outpix*Diffuse.aaaa;  } 
void main(){colour=SoftMix3Glow_HLSL3(t0,t1,diffuse)*materialGain;}
