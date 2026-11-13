#version 300 es
// Recovered source: sm3-08-Spinlerp_HLSL.hlsl; program 8.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
                                                   vec4 Spinlerp_HLSL(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                           const vec4 Diffuse )    {                                                                    vec2 r_1=vec2(vec2(TexCoord0));    vec4 r_2=vec4(vec4(texture(image1,TexCoord1)));    vec2 r_0=vec2(vec2(mix(r_1,TexCoord1,r_2.r*Diffuse.bb)));    vec4 outpix=vec4(vec4(Diffuse.aaaa*texture(image0,r_0)));    return outpix;  } 
void main(){colour=Spinlerp_HLSL(t0,t1,diffuse)*materialGain;}
