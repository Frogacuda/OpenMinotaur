#version 300 es
// Recovered source: sm3-82-glargle2_HLSL3.hlsl; program 82.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
          vec4 glargle2_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec4 outpix;  vec4 inpix;  vec4 inpix2;  float dist;  float rsize;  vec2 phases;  vec2 phinc;  vec2 istep;  vec2 sines;  vec2 scale;  vec2 scalei;  vec2 point0;  float minge;  float len;    vec2 thIndex=vec2(vec2(TexCoord0-0.5));   vec2 mooScale=vec2(vec2((TexCoord0-TexCoord1)-0.5));    vec4 mooIndex=vec4(vec4(vec4(0.0,0.0,0.0,0.0)));   mooIndex.x = mooScale.x*TexCoord0.x;   mooIndex.x = mooScale.y*TexCoord0.y;   mooIndex.z = 0.0;   mooIndex.w = 1.0;    vec4 tempTex1;   tempTex1.x = TexCoord1.x;   tempTex1.y = TexCoord1.y;   tempTex1.z = 0.0;   tempTex1.w = 1.0;    phases=vec2(c25.gb*6.283185308);   phinc=vec2(c26.rg*3.141592654);   scale=vec2(c25.a);   scalei=vec2((c26.ba-0.5)*0.2);   outpix=vec4(vec4(0.0));   float i=0.0;   float cunt=0.5*Diffuse.b;   float cumdist=0.0;   float ii=1.0/((c25.r*100.0)+1.0);   float mI=0.0;   while(mI<20.0)   {    mI++;    if ( i >=1.0 ) break; 	inpix=vec4(sampleLod(image0,mooIndex)); 	inpix2=vec4(sampleLod(image1,tempTex1));    minge=0.1;    point0=vec2((sin(phases)*scale));    mooIndex.x += point0.x*mooScale.x;    mooIndex.y += point0.y*mooScale.y;    phases+=phinc;    scale+=scalei;    len=length((point0-inpix2.xy)-(thIndex+inpix.xy))*2.0;    dist=1.0/len;    outpix+=inpix*minge*dist;    cumdist+=dist;    i+=ii;   }   outpix=vec4(outpix*cumdist*Diffuse.b*0.02);   return outpix*Diffuse.a;  } 
void main(){colour=glargle2_HLSL3(t0,t1,diffuse)*materialGain;}
