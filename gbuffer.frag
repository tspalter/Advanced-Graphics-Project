/////////////////////////////////////////////////////////////////////////
// Pixel shader for G-Buffer
////////////////////////////////////////////////////////////////////////
#version 330

const float PI = 3.141592653589793f;

// These definitions agree with the ObjectIds enum in scene.h
const int     nullId	= 0;
const int     skyId	    = 1;
const int     seaId	    = 2;
const int     groundId	= 3;
const int     roomId	= 4;
const int     boxId	    = 5;
const int     frameId	= 6;
const int     lPicId	= 7;
const int     rPicId	= 8;
const int     teapotId	= 9;
const int     spheresId	= 10;
const int     floorId	= 11;
const int     otherId	= 12;

in vec3 normalVec, lightVec, eyeVec, tanVec;
in vec2 texCoord;
in vec4 worldPos;

uniform int objectId, useTexture, useNormal;

uniform vec3 diffuse; // Kd
uniform vec3 specular; // Ks
uniform float shininess; // alpha

uniform vec3 Light; // Ii
uniform vec3 Ambient; // Ia

uniform sampler2D shadowMap, texMap, normalMap;

void main() {
    bool debug = false;
    int debugMode = 0;
    if (debug) {
    
    }
    else {
        vec3 N = normalize(normalVec);
        vec3 L = normalize(lightVec);
        vec3 V = normalize(eyeVec);

        vec3 Ii = Light;
        vec3 Ia = Ambient;

        vec3 Kd = diffuse;
        vec3 Ks = specular;
        float a = shininess;

        int uT = useTexture;
        int uN = useNormal;

        if (uT != 0) {
            if (objectId == floorId) {
                Kd = texture(texMap, fract(5.0*texCoord)).xyz;
            }
            else if (objectId == roomId) {
                Kd = texture(texMap, fract(20.0*texCoord.yx)).xyz;
            }
            else if (objectId == groundId) {
                Kd = texture(texMap, fract(50.0*texCoord)).xyz;
            }
            else if (objectId == skyId) {
                vec2 uv = vec2(-atan(V.y, V.x)/(2*PI), acos(V.z)/PI);
                Kd = texture(texMap, uv).xyz;
                a = -1;
            }
            else if (objectId == rPicId) {
                if (fract(texCoord).x < 0.1 || fract(texCoord).y < 0.1 || fract(texCoord).x > 0.9 || fract(texCoord).y > 0.9) {
                    Kd = vec3(0.6, 0.6, 0.6);
                }
                else {
                    Kd = texture(texMap, fract((texCoord * 1.2) - vec2(0.1, 0.1))).xyz;
                }
            }
            else {
                Kd = texture(texMap, texCoord).xyz;
            }
        }

        if (objectId == lPicId) {
            if (fract(4.0 * texCoord).x * fract(4.0 * texCoord).x * fract(4.0 * texCoord).y * fract(4.0 * texCoord).y < 1.0) {
                Kd = vec3(1.0, 1.0, 1.0);
            }
            else {
                Kd = vec3(0.0, 0.0, 0.0);
            }
        }

        if (uN != 0) {
            vec3 delta = texture(normalMap, texCoord).xyz;

            if (objectId == floorId) {
                delta = texture(normalMap, fract(5.0*texCoord)).xyz;
            }
            else if (objectId == roomId) {
                delta = texture(normalMap, fract(20.0*texCoord.yx)).xyz;
            }
            else if (objectId == seaId) {
                delta = texture(normalMap, fract(50.0*texCoord)).xyz;
            }

            delta = delta * 2.0 - vec3(1.0, 1.0, 1.0);

            vec3 T = normalize(tanVec);
            vec3 B = normalize(cross(T,N));

            N = delta.x * T + delta.y * B + delta.z * N;
        }

        if (objectId == seaId) {
            vec3 R = -(2 * dot(V, N) * N - V);
            vec2 uv = vec2(-atan(R.y, R.x)/(2*PI), acos(R.z)/PI);
            Kd = texture(texMap, uv).xyz;
        }

        gl_FragData[0].xyz = worldPos.xyz;
        gl_FragData[1].xyz = normalVec.xyz;
        gl_FragData[2].rgb = Kd.rgb;
        gl_FragData[3].rgb = Ks.rgb;
        gl_FragData[3].w = a;
    }
}
