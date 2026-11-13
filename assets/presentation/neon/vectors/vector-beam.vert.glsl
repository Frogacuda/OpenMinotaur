#version 300 es
precision highp float;
layout(location=0) in vec2 position;
layout(location=1) in vec2 localPosition;
layout(location=2) in vec2 beam;
layout(location=3) in vec4 tint;
uniform vec2 viewportSize;
out vec2 strokeLocal;out vec2 strokeBeam;out vec4 strokeTint;
void main(){gl_Position=vec4(position*vec2(2.0,-2.0)/viewportSize+vec2(-1.0,1.0),0.0,1.0);strokeLocal=localPosition;strokeBeam=beam;strokeTint=tint;}
