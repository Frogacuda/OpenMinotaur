#version 300 es
// Shape coordinates are independent of material scale, repeat, rotation and scroll.
precision highp float;
uniform sampler2D image0,image1,image2;
uniform vec4 c25,c26;
uniform float materialGain;
in vec4 diffuse;
in vec2 t0;
in vec2 surfaceUV;
out vec4 colour;
void main(){
    vec2 uv=t0;
    float strength=clamp(c25.r,0.0,1.0);
    if(strength>0.0){
        float angle=mod(c25.g,4.0)*1.5707963267948966;
        vec2 axis=vec2(cos(angle),sin(angle));
        vec2 side=vec2(-axis.y,axis.x);
        vec2 span=exp2(clamp(c26.rg,vec2(-4.0),vec2(4.0)));
        vec2 centre=c25.ba;
        vec2 p=(surfaceUV-centre)*2.0/span;
        float along=dot(p,axis),across=dot(p,side);
        float factor=1.0+4.0*strength*along*along;
        across *= factor;
        vec2 warped=centre+0.5*(axis*along+side*across)*span;
        vec2 delta=warped-surfaceUV;
        // Convert the shape displacement through the material's local UV transform.
        // This retains tiling and rotation without feeding them into the shape.
        vec2 dx=dFdx(surfaceUV),dy=dFdy(surfaceUV);
        float determinant=dx.x*dy.y-dx.y*dy.x;
        if(abs(determinant)>1e-12){
            vec2 pixels=vec2(dy.y*delta.x-dy.x*delta.y,
                            -dx.y*delta.x+dx.x*delta.y)/determinant;
            uv+=dFdx(t0)*pixels.x+dFdy(t0)*pixels.y;
        }
    }
    colour=texture(image0,uv)*diffuse.aaaa*materialGain;
}
