#version 300 es
// Recovered source: sm3-48-Fluffinator_HLSL3.hlsl; program 48.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
        vec4 Fluffinator_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec2 pOrig;  vec2 pNorm;  vec4 inpix;  vec4 outpix;  vec4 pOrig2;   pOrig=vec2(TexCoord1-0.5);   inpix=vec4(texture(image0,TexCoord0));   inpix.a=0.0;   inpix.z = 1.0;   float maxFurLen=c25.r*80.0;   float thisFurLen=maxFurLen-(length(inpix)/1.42)*(maxFurLen-1.0)*c25.a+1.0;   float furbright=c25.g;   float furinc=(c25.b-furbright)/thisFurLen;   float fimlim=1.0/thisFurLen;   float fcount=0.0;   pNorm=vec2(normalize(pOrig)*((Diffuse.b-0.5)*0.08));   outpix=vec4(vec4(0.0));   pOrig2.x=TexCoord0.x;   pOrig2.y=TexCoord0.y;   pOrig2.z=0.0;   pOrig2.w=1.0;   float mI=0.0;   while(mI<50.0)   {    mI++;    outpix+=sampleLod(image0,pOrig2)*furbright;    pOrig2.x += pNorm.x;    pOrig2.y += pNorm.y;    furbright+=furinc;    fcount+=fimlim;    if ( fcount >= 1.0 ) break;   }   return outpix*Diffuse.aaaa;  } 
void main(){colour=Fluffinator_HLSL3(t0,t1,diffuse)*materialGain;}
