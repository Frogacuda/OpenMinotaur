#version 300 es
// Recovered source: sm3-33-Overbright_cut_HLSL.hlsl; program 33.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
     vec4 Overbright_cut_HLSL(                       const vec2 TexCoord0,                          const vec4 Diffuse )    {    vec4 r_1=vec4(vec4(texture(image0,TexCoord0)));    vec4 r_2=vec4(vec4(r_1*Diffuse.bbbb));    vec4 r_0=vec4(vec4((c25+c25)*12.0*r_2));    r_1=vec4(r_1*r_0);    float thresh=(r_1.r+r_1.g+r_1.b)-c25.a*12.0*Diffuse.b;    vec4 outpix=vec4(vec4(r_1));    if(thresh<0.0) outpix=vec4(outpix-outpix);    return outpix*Diffuse.aaaa;  } 
void main(){colour=Overbright_cut_HLSL(t0,diffuse)*materialGain;}
