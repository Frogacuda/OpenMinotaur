#version 300 es
precision highp float;
in vec2 strokeLocal;in vec2 strokeBeam;in vec4 strokeTint;
uniform vec4 beamStyle; // intensity, halo, spread, white-hot
out vec4 colour;
void main(){
 float endpoint=max(max(-strokeLocal.x,strokeLocal.x-strokeBeam.x),0.0);
 float d2=endpoint*endpoint+strokeLocal.y*strokeLocal.y;
 float w=strokeBeam.y,aa2=w*w+0.46209812;
 float core=exp2(-4.0*d2/aa2)*w*inversesqrt(aa2);
 float halo2=w*w*beamStyle.z*beamStyle.z;
 float nearHalo=0.0,farHalo=0.0;if(beamStyle.y>0.0){nearHalo=exp2(-4.0*d2/halo2);farHalo=exp2(-4.0*d2/(halo2*5.29));float reach=min(64.0,max(2.0,w*beamStyle.z*2.0));float fade=clamp((reach*reach-d2)/(reach*reach*0.4375),0.0,1.0);fade=fade*fade*(3.0-2.0*fade);nearHalo*=fade;farHalo*=fade;}
 vec3 hot=mix(strokeTint.rgb,vec3(1.0),clamp(beamStyle.w,0.0,1.0)*core);
 vec3 light=(hot*core+strokeTint.rgb*beamStyle.y*(0.22*nearHalo+0.018*farHalo))*strokeTint.a*beamStyle.x;
 colour=vec4(light,clamp(max(light.r,max(light.g,light.b)),0.0,1.0));
}
