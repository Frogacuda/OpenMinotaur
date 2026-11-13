#version 300 es
// Recovered source: sm3-70-RibbedTunnel_HLSL3.hlsl; program 70.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
       vec4 RibbedTunnel_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec4 outpix;  vec4 inpix;  float fov;  vec2 sc;  vec2 nuIndex;    vec2 thIndex=vec2(vec2(TexCoord0-0.5));   vec2 offs=vec2(vec2(TexCoord0-TexCoord1));   float wang=atan(thIndex.y,thIndex.x)*3.141592654*(c25.b-0.5)*2.0;   sincos(-wang,sc.x,sc.y);   nuIndex.x=(sc.y*thIndex.x)-(sc.x*thIndex.y);   nuIndex.y=(sc.x*thIndex.x)+(sc.y*thIndex.y);   float fadeStart=8.0*c25.a;   float fadeEnd=fadeStart+1.0;   fov=4.0*(c25.r-0.5);   float wxpos=c26.b*4.0+sin(wang*16.0*Diffuse.b)*(c26.g-0.5)*4.0;   float crang=fov*nuIndex.x;   float z=(wxpos/tan(crang))*0.3;   float zabs=abs(z);   if(zabs>fadeEnd) return vec4(0.0);   float i=1.0;   if(zabs>fadeStart) i=1.0-(zabs-fadeStart);   thIndex.x=zabs*c26.r+offs.x;   thIndex.y=(wang/3.141592654)*4.0*c25.g+offs.y;       outpix=vec4(texture(image0,thIndex)*i);   return outpix*Diffuse.a;  } 
void main(){colour=RibbedTunnel_HLSL3(t0,t1,diffuse)*materialGain;}
