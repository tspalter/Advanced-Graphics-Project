/////////////////////////////////////////////////////////////////////////
// Pixel shader for reflection pass
////////////////////////////////////////////////////////////////////////
#version 330

out vec4 FragColor;
const float PI = 3.141592653589793f;

// These definitions agree with the ObjectIds enum in scene.h
const int     nullId	= 0;
const int     skyId	= 1;
const int     seaId	= 2;
const int     groundId	= 3;
const int     roomId	= 4;
const int     boxId	= 5;
const int     frameId	= 6;
const int     lPicId	= 7;
const int     rPicId	= 8;
const int     teapotId	= 9;
const int     spheresId	= 10;
const int     floorId	= 11;

uniform int objectId;

in vec3 normalVec, eyeVec;

uniform sampler2D skyboxTexture;

vec3 LightingFrag();
void main() {
    vec3 N = normalize(normalVec);
    vec3 V = normalize(eyeVec);
    vec3 R = 2 * dot(V,N)*N - V;

	FragColor.xyz = LightingFrag();
    if (objectId == skyId) {
        vec2 uv = vec2(-atan(V.y/V.x)/(2*PI), acos(V.z)/PI);
        FragColor = texture(skyboxTexture, uv);
    }
}