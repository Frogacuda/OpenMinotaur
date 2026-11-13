#version 300 es
// Recovered source: sm3-71-PointField_HLSL3.hlsl; program 71.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
     vec4 PointField_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec4 outpix;  vec4 inpix;  vec2 pInc;  vec4 p3D;  float dist;  float rsize;  vec4 centre;    vec2 thIndex=vec2(vec2((TexCoord0-0.5)*100.0*c25.b));   vec4 pIndex=vec4(vec4(0.0));   float size=c25.r*50.0;   pInc.x=0.15;   pInc.y=0.23;   p3D.xy=thIndex;   p3D.zw=vec2(0.0);   float cumdist=0.0;   float ii=1.0/((c25.g*100.0)+1.0);   float mI=0.0;   float i=0.0;   pIndex.w = 1.0;   while(mI<20.0)  {    mI++;    if (i >= 1.0) break;    centre=vec4(sampleLod(image0,pIndex));    rsize=centre.z;    centre=vec4((centre-0.5)*100.0);    pIndex.x += pInc.x;    pIndex.y += pInc.y;    centre.zw=vec2(0.0);    dist=length(centre-p3D);    cumdist+=rsize/dist;    i+=ii;   }   outpix=vec4(cumdist*Diffuse.b);   return outpix*Diffuse.a;  } 
void main(){colour=PointField_HLSL3(t0,t1,diffuse)*materialGain;}
