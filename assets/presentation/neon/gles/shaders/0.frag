#version 300 es
// Recovered source: sm3-00-Modulate_HLSL.hlsl; program 0.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
                                                                                            vec4 Modulate_HLSL(                       const vec2 TexCoord0,                           const vec4 Diffuse )    {                                                                    vec4 col=vec4(vec4(Diffuse.aaaa*texture( image0, TexCoord0 )));         vec4 col2=vec4(vec4(Diffuse*col.gggg));								   return col2;                                                      }  
void main(){colour=Modulate_HLSL(t0,diffuse)*materialGain;}
