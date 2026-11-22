#version 300 es
// Recovered source: sm3-19-Copydrive_HLSL.hlsl; program 19.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
    vec4 Copydrive_HLSL(                       const vec2 TexCoord0,                       const vec4 Diffuse )  {    vec4 r_1=vec4(vec4(texture(image0,TexCoord0)));    vec4 r_2=vec4(vec4(r_1*Diffuse.aaaa));    vec4 outpix=vec4(vec4(r_2*(Diffuse.bbbb*8.0)));    if(outpix.x>1.0) outpix.x-=1.0;    if(outpix.y>1.0) outpix.y-=1.0;    if(outpix.z>1.0) outpix.z-=1.0;    return outpix;  } 
void main(){colour=Copydrive_HLSL(t0,diffuse)*materialGain;}
