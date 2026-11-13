#version 300 es
// Recovered source: sm3-13-RGBBGR_HLSL.hlsl; program 13.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
                                                                                                                     vec4 RGBBGR_HLSL(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                           const vec4 Diffuse )    {                                                                    vec4 col=vec4(vec4(texture( image0, TexCoord0 )));  	   vec4 col2=vec4(vec4(texture( image1, TexCoord1 )));    vec4 col3=vec4(vec4(col.bgra));    float interp=Diffuse.b*(col2.r+col2.g+col2.b);    vec4 col4=vec4(vec4(Diffuse.aaaa*mix(col,col3,interp)));    return col4;                                                      }  
void main(){colour=RGBBGR_HLSL(t0,t1,diffuse)*materialGain;}
