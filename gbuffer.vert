/////////////////////////////////////////////////////////////////////////
// Vertex shader for the G-Buffer
//
// Copyright 2013 DigiPen Institute of Technology
////////////////////////////////////////////////////////////////////////
#version 330

uniform mat4 WorldView, WorldProj, ModelTr, NormalTr, WorldInverse, ShadowMatrix;
uniform vec3 lightPos, eyePos;

in vec4 vertex;
in vec3 vertexNormal, vertexTangent;
in vec2 vertexTexture;


out vec3 normalVec, lightVec, eyeVec, tanVec;
out vec2 texCoord;
out vec4 shadowCoord, worldPos;

void main()
{      
    gl_Position = WorldProj*WorldView*ModelTr*vertex;
    
    worldPos.xyz = (ModelTr*vertex).xyz;

    normalVec = vertexNormal*mat3(NormalTr);
    lightVec = lightPos - worldPos.xyz;
    eyeVec = eyePos - worldPos.xyz;

    texCoord = vertexTexture;
    tanVec = mat3(ModelTr) * vertexTangent;
}
