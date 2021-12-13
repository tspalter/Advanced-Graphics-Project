/////////////////////////////////////////////////////////////////////////
// Vertex shader for lighting
//
// Copyright 2013 DigiPen Institute of Technology
////////////////////////////////////////////////////////////////////////
#version 330

uniform mat4 WorldView, WorldInverse, WorldProj, ModelTr, ShadowMatrix, NormalTr;

in vec4 vertex;
in vec3 vertexNormal;
in vec2 vertexTexture;
in vec3 vertexTangent;


out vec3 normalVec, lightVec, eyeVec, tanVec;
out vec2 texCoord;
out vec4 shadowCoord;

uniform vec3 lightPos;

void LightingVertex(vec3 eyePos)
{      
    gl_Position = vertex;
}
