#version 300 es
// Recovered source: sm3-74-zomg_HLSL3.hlsl; program 74.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
       vec4 zomg_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec4 outpix;  vec4 inpix;  vec2 pInc;  vec2 p2D;  float dist;  float rsize;  vec2 centre;    vec2 thIndex=vec2(vec2((TexCoord0-0.5)*100.0*c25.a));   vec4 pIndex=vec4(vec4(0.0));   pIndex.w = 1.0;   pInc=vec2(0.5*(TexCoord1-0.5)*c26.r);   p2D=vec2(thIndex);   float cumdist=0.0;   outpix=vec4(vec4(0.0));   float i=0.0;   float ii=1.0/((c25.r*100.0)+1.0);   float cunt=0.1*c25.b;   float mI=0.0;   while(mI<50.0)   {    mI++;    if (i >=1.0 ) break;    inpix=vec4(sampleLod(image0,pIndex));    rsize=inpix.z;    centre=vec2((inpix.xy-0.5)*100.0);    pIndex.x += pInc.x;    pIndex.y += pInc.y;    dist=length(centre-p2D);    outpix+=inpix*cunt;    cumdist+=(rsize*c25.g)/dist;    i+=ii;   }   outpix=vec4(outpix*cumdist*Diffuse.b);   return outpix*Diffuse.a;  } 
void main(){colour=zomg_HLSL3(t0,t1,diffuse)*materialGain;}
