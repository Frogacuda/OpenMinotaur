#version 300 es
// Recovered source: sm3-34-Vertex_Colour_Tweaked_HLSL.hlsl; program 34.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
  vec4 Vertex_Colour_Tweaked_HLSL(                        const vec4 Diffuse )    {                                                                   float intens=c25.a*Diffuse.a;   vec4 col=vec4(vec4(sin(c25)*0.125+0.875));   vec4 col2=vec4(vec4(col*Diffuse));    return col2*intens;                                                      }  
void main(){colour=Vertex_Colour_Tweaked_HLSL(diffuse)*materialGain;}
