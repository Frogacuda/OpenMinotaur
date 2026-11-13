#version 300 es
// Recovered source: sm3-83-glargle3_HLSL3.hlsl; program 83.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
             vec4 glargle3_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec4 outpix;  vec4 inpix;  vec4 inpix2;  vec4 inpix3;  float dist;  float rsize;   vec2 sc0;  vec2 sc1;  vec2 sc2;  float minge;  float len;    vec4 thIndex;   thIndex.x = TexCoord0.x;   thIndex.y = TexCoord0.y;   thIndex.z = 0.0;   thIndex.w = 1.0;    vec4 mooIndex; TexCoord1;   mooIndex.x = TexCoord1.x;   mooIndex.y = TexCoord1.y;   mooIndex.z = 0.0;   mooIndex.w = 1.0;    vec4 thIndex2=vec4(vec4( 0.0));    sc0=vec2(c25.gb-0.5);   sc1=vec2(c26.rg-0.5);   sc2=vec2(c26.ba-0.5);   outpix=vec4(vec4(0.0));   float i=0.0;   float cunt=0.5*Diffuse.b;   float cumdist=0.0;   float ii=1.0/((c25.r*100.0)+1.0);   float mI=0.0;   while(mI<50.0)   {    mI++;    if (i >=1.0) break; 	inpix=vec4(sampleLod(image0,thIndex)); 	inpix2=vec4(sampleLod(image1,mooIndex)); 	inpix3=vec4(sampleLod(image2,thIndex2));    minge=0.2;     thIndex.x += sc0.x;    thIndex.y += sc0.y;     mooIndex.x += sc1.x;    mooIndex.y += sc1.y;     thIndex2.x += sc2.x;    thIndex2.y += sc2.y;     len=length((inpix3.xy*inpix2.xy)-(TexCoord0*inpix.xy))*2.0;    dist=1.0/len;    outpix+=inpix2*minge*dist;    cumdist+=dist*minge;    i+=ii;   }   outpix=vec4(outpix*cumdist*Diffuse.b*0.02);   return outpix*Diffuse.a;  } 
void main(){colour=glargle3_HLSL3(t0,t1,diffuse)*materialGain;}
