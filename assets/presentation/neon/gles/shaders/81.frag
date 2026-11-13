#version 300 es
// Recovered source: sm3-81-GalacticaAlt_HLSL3.hlsl; program 81.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
       vec4 GalacticaAlt_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec4 outpix;  vec4 inpix;  float dist;  float rsize;  vec2 phases;  vec2 phinc;  vec2 istep;  vec2 sines;  vec2 scale;  vec2 scalei;  vec2 point0;  float minge;    vec2 thIndex=vec2(vec2(TexCoord0-0.5));   vec4 iIndex=vec4(vec4(0.0));   iIndex.w = 1.0;    istep=vec2((TexCoord0-TexCoord1)*0.1);   phases=vec2(c25.gb*6.283185308);   phinc=vec2(c26.rg*3.141592654);   scale=vec2(c25.a);   scalei=vec2((c26.ba-0.5)*0.2);   outpix=vec4(vec4(0.0));   float i=0.0;   float cunt=0.5*Diffuse.b;   float cumdist=0.0;   float ii=1.0/((c25.r*100.0)+1.0);   float mI=0.0;   while(mI<50.0)   {    mI++;    if ( i >= 1.0) break; 	inpix=vec4(sampleLod(image0,iIndex));    iIndex.x += istep.x;    iIndex.y += istep.y;     minge=(1.0+inpix.z)*cunt;    point0=vec2(sin(phases)*scale);    phases+=phinc;    scale+=scalei;    dist=length(point0-(thIndex+inpix.xy));    outpix+=minge*inpix;    cumdist+=(ii*minge)/dist;    i+=ii;   }   outpix=vec4(outpix*cumdist*Diffuse.b*0.8);   return outpix*Diffuse.a;  } 
void main(){colour=GalacticaAlt_HLSL3(t0,t1,diffuse)*materialGain;}
