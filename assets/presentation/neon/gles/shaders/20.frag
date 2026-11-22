#version 300 es
// Recovered source: sm3-20-Blurradial_fix_HLSL.hlsl; program 20.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
                           const vec4 cons_1=vec4(vec4(vec4(0.5,0.5,1.5,0.2)));  const vec4 cons_2=vec4(vec4(vec4(0.0,0.0,0.1,0.0)));  vec4 Blurradial_fix_HLSL(                       const vec2 TexCoord0,                          const vec4 Diffuse )    {    vec2 offset0=vec2(vec2(TexCoord0+cons_2.xy));    vec2 offset1=vec2(vec2(cons_1.xy-offset0));    offset1=vec2(offset1*Diffuse.bb);    vec4 accum_pixel=vec4(vec4(cons_1.w*texture(image0,offset0)));    for(float i=0.0;i<8.0;i++)    {      offset0=vec2(offset1+offset0);      accum_pixel+=cons_1.w*texture(image0,offset0);    }    vec4 outpix=vec4(vec4(Diffuse.aaaa*accum_pixel));    return outpix;  } 
void main(){colour=Blurradial_fix_HLSL(t0,diffuse)*materialGain;}
