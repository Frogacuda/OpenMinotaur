#version 300 es
// Recovered source: sm3-73-OutlineMerge1_HLSL3.hlsl; program 73.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
          vec4 OutlineMerge1_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec4 outpix=vec4(vec4(0.0));  vec3 sampix=vec3(vec3(0.0));  vec4 inpix=vec4(vec4(0.0));  vec4 inpix2=vec4(vec4(0.0));   float onePixel=(200.0*c25.a)/1024.0;   vec2 pIndex=vec2(vec2(TexCoord0-onePixel));   float interp=0.0;   pIndex.y+=onePixel;   sampix=vec3(texture(image0,pIndex).rgb);   pIndex.x+=onePixel;   sampix+=texture(image0,pIndex).rgb;   pIndex.x+=onePixel;   sampix+=texture(image0,pIndex).rgb;   pIndex.y+=onePixel;   pIndex.x-=onePixel;   sampix+=texture(image0,pIndex).rgb;   pIndex.y-=onePixel;   pIndex.y-=onePixel;   sampix+=texture(image0,pIndex).rgb;   float minterp=8.0*c26.r;   interp=(sampix.x*c25.x+sampix.y*c25.y+sampix.z*c25.z)*Diffuse.b*1.4;   if(interp>minterp) interp=minterp-(interp-minterp);   inpix=vec4(texture(image0,TexCoord0));   inpix2=vec4(texture(image1,TexCoord1));   outpix=vec4(mix(inpix2,inpix,interp));   return outpix*Diffuse.a;  } 
void main(){colour=OutlineMerge1_HLSL3(t0,t1,diffuse)*materialGain;}
