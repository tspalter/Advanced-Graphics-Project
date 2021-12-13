/////////////////////////////////////////////////////////////////////////
// Vertex shader for reflection pass
//
// Copyright 2013 DigiPen Institute of Technology
////////////////////////////////////////////////////////////////////////
#version 330

in vec4 vertex;

uniform mat4 ModelTr;
uniform bool topHalf;

void LightingVertex(vec3 eye);
void main()
{      
    vec3 eye = vec3(0.0, 0.0, 1.5);
    vec3 P = (ModelTr*vertex).xyz;
    vec3 R = P - eye;
    float RMag = length(R);
    float a = R.x / RMag;
    float b = R.y / RMag;
    float c = R.z / RMag;
    LightingVertex(eye);
    if (topHalf) {
       gl_Position = vec4(a/(1+c), b/(1+c), c*RMag/1000 - 1.0, 1.0);
    }
    else {
       gl_Position = vec4(a/(1-c), b/(1-c), -c*RMag/1000 - 1.0, 1.0);
    }
}