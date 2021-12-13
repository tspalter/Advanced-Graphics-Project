/////////////////////////////////////////////////////////////////////////
// Vertex shader for final pass
//
// Copyright 2013 DigiPen Institute of Technology
////////////////////////////////////////////////////////////////////////
#version 330

uniform mat4 WorldInverse;

void LightingVertex(vec3 eye);
void main()
{      
    vec3 eye = (WorldInverse*vec4(0,0,0,1)).xyz;
    LightingVertex(eye);
}