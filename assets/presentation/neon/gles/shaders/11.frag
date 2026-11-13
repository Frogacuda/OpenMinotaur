#version 300 es
// Recovered source: sm3-11-Burnthru2_HLSL.hlsl; program 11.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
    vec4 Burnthru2_HLSL(                       const vec2 TexCoord0,                       const vec2 TexCoord1,                       const vec4 Diffuse )  {    vec4 r_1=vec4(vec4(texture(image0,TexCoord0)));    vec4 r_2=vec4(vec4(texture(image1,TexCoord1)));    float r_5=r_2.r+r_2.g+r_2.b;    vec4 outpix=vec4(vec4(Diffuse.aaaa*mix(r_1,r_2,r_5*Diffuse.b)));    return outpix;  } 
void main(){colour=Burnthru2_HLSL(t0,t1,diffuse)*materialGain;}
