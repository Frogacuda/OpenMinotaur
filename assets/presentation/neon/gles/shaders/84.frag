#version 300 es
// Recovered source: sm3-84-Planet_HLSL3.hlsl; program 84.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;uniform float materialGain;
in vec4 diffuse;in vec2 t0,t1;out vec4 colour;
#define saturate(x) clamp(x,0.0,1.0)
vec4 sampleLod(sampler2D image,vec4 uv){return textureLod(image,uv.xy,uv.w);}
void sincos(float x,out float s,out float c){s=sin(x);c=cos(x);}
       vec4 Planet_HLSL3(                       const vec2 TexCoord0,                          const vec2 TexCoord1,                          const vec4 Diffuse )    {  vec4 outpix;  vec3 rO;  vec3 rD;  vec3 sC;  vec3 pI;  vec3 pL;  vec3 n1;  vec3 n2;  vec2 iI;  float q;  float t;  float temp;   float r=5000.0;   outpix=vec4(vec4(0.0));   pL.xy=2000.0*(TexCoord0-TexCoord1);   pL.z=2000.0*(c25.a-0.5);   vec2 thIndex=vec2(vec2((TexCoord0-0.5)*100.0));   sC.xy=(c25.rg-0.5)*5000.0;   sC.z=c25.b*500.0+r;   rD.xy=thIndex;   rD.z=10.0;   rD=vec3(normalize(rD)+0.0001);   rO=vec3(-rD*500.0*Diffuse.b);   rO-=sC;   pL-=sC;   float a=dot(rD,rD);   float b=dot(rD,rO);   float c=dot(rO,rO)-(r*r);   float disc=b*b-4.0*a*c;   if (disc<0.0) return outpix;   float distsq=sqrt(disc);   if(b<0.0) q=(-b-distsq)/2.0;   if(b>0.0) q=(-b+distsq)/2.0;   float t0=q/a;   float t1=c/q;   if(t0>t1)   {    temp=t0;    t0=t1;    t1=temp;   }   if(t1<0.0) return outpix;   t=t0;   if(t0<0.0) t=t1;   pI=vec3((rO+rD*t));   iI.y=acos(pI.y/r)/3.141592654;   iI.x=(acos(pI.x/(r*sin(3.141592654*iI.y))))/6.283185308;   iI*=c26.rg*50.0;   n1=vec3(normalize(pL-pI));   n2=vec3(normalize(pI));   q=dot(n1,n2)*10.0;   n1=vec3(normalize(rO-pI));   t=dot(n1,n2);   outpix=vec4(texture(image0,iI+(c26.ba*10.0)));   t-=Diffuse.a;   if(t<0.0) t=0.0;   if(t>0.1) t=0.1;   q*=t*10.0;   return outpix*q;  } 
void main(){colour=Planet_HLSL3(t0,t1,diffuse)*materialGain;}
