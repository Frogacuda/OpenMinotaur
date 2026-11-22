#version 300 es
// Recovered source: sm3-55-Disturbed_SIN_HLSL3.hlsl; program 55.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
        vec4 Disturbed_SIN_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec4 inpix=vec4(vec4( 0.0));  vec4 outpix=vec4(vec4( 0.0));   vec2 thIndex=vec2(vec2(TexCoord0));   inpix=vec4(texture(image1,TexCoord1));   inpix.a=0.0;   vec2 ind1=vec2(vec2(inpix.xy));   vec2 ind2=vec2(vec2(inpix.yz));   float pterb1=length(ind1)*10.0*Diffuse.b;   float pterb2=length(ind2)*10.0*Diffuse.b;   thIndex.x+=sin(pterb1+(TexCoord0.y+20.0*(TexCoord1.y-TexCoord0.y))*20.0*(c25.g-0.5))*c25.r;   thIndex.y+=sin(pterb2+(TexCoord0.x+20.0*(TexCoord1.x-TexCoord0.x))*20.0*(c25.a-0.5))*c25.b;       outpix=vec4(texture(image0,thIndex));   return outpix*Diffuse.aaaa;  } 
void main(){colour=Disturbed_SIN_HLSL3(t0,t1,diffuse)*materialGain;}
