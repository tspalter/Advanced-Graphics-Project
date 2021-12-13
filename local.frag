/////////////////////////////////////////////////////////////////////////
// Pixel shader for lighting
////////////////////////////////////////////////////////////////////////
#version 330

out vec4 FragColor;

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
const int     otherId   = 12;

uniform vec3 Light;  
uniform vec3 Ambient;     

uniform vec3 lightPos;
uniform vec3 eyePos;
uniform float lightRadius;

uniform sampler2D shadowMap;
uniform sampler2D G0;
uniform sampler2D G1;
uniform sampler2D G2;
uniform sampler2D G3;
uniform int screenWidth;
uniform int screenHeight;
uniform mat4 ShadowMatrix;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(screenWidth,screenHeight); 

    vec3 N = normalize(texture(G1, uv).xyz);
    vec3 worldPos = texture(G0, uv).xyz;

    vec3 distance = worldPos - lightPos;
    float rSQ = abs(pow(distance.x,2) + pow(distance.y,2) + pow(distance.z, 2));
    if( rSQ >= pow(lightRadius,2)) {
        FragColor.xyz = vec3(0,0,0);
        return;
    }

    vec3 V = normalize(eyePos - worldPos);
    vec3 L = normalize(lightPos - worldPos);

    vec3 Ii = Light;
    vec3 Ia = Ambient;

    vec3 Kd = texture(G2, uv).rgb;
    vec3 Ks = texture(G3, uv).rgb;
    float a = texture(G3, uv).w;
           
    vec3 H = normalize(L + V);
    float NdotL = max(dot(N,L),0.0);
    float NdotH = max(dot(N,H),0.0);
    float VdotN = max(dot(V,N),0.0);
    float LdotH = max(dot(L,H),0.0);

    vec3 FLH = Ks + ((1,1,1) - Ks) * pow(1-LdotH, 5);
    float DH = ((a + 2) / (2 * PI)) * pow(NdotH, a);

    vec3 BRDF = (Kd/PI) + ((FLH * DH) / (pow(LdotH,2)));

    FragColor.xyz = Ii/rSQ - Ii/pow(lightRadius,2);
}