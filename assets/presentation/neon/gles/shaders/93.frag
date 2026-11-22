#version 300 es
// Authored cylindrical barrel warp; separate from the recovered original shaders.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25;
uniform float materialGain;
in vec4 diffuse;
in vec2 t0,t1;
out vec4 colour;
void main(){
    vec2 uv=t0;
    float strength=clamp(c25.r,0.0,1.0);
    if(strength>0.0){
        float angle=clamp(c25.g,0.0,1.0)*1.5707963267948966;
        vec2 axis=vec2(cos(angle),sin(angle));
        vec2 side=vec2(-axis.y,axis.x);
        vec2 centre=c25.ba;
        vec2 p=(t0-centre)*2.0;
        float along=dot(p,axis),across=dot(p,side);
        float factor=1.0+4.0*strength*along*along;
        across *= factor;
        uv=centre+0.5*(axis*along+side*across);
    }
    colour=texture(image0,uv)*diffuse.aaaa*materialGain;
}
