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
const int     otherId	= 12;

in vec3 normalVec, lightVec, eyeVec, tanVec;
in vec2 texCoord;
in vec4 shadowCoord;

uniform int objectId;

uniform vec3 diffuse; // Kd
uniform vec3 specular; // Ks
uniform float shininess; // alpha

uniform vec3 Light; // Ii
uniform vec3 Ambient; // Ia

uniform sampler2D shadowMap, reflectionTopMap, reflectionBottomMap, skyboxTexture, tex, normalMap;

uniform bool reflectiveObject;

vec3 LightingFrag()
{
    vec3 N = normalize(normalVec);
    vec3 L = normalize(lightVec);
    vec3 V = normalize(eyeVec);
    vec3 T = normalize(tanVec);
    vec3 B = normalize(cross(T,N));
    vec3 lighting = vec3(0.0, 0.0, 0.0);

    vec3 Kd = diffuse;
    vec3 Ks = specular;
    float a = shininess;

    // A checkerboard pattern to break up larte flat expanses.  Remove when using textures.
    if (objectId==groundId || objectId==floorId || objectId==seaId) {
        ivec2 uv = ivec2(floor(100.0*texCoord));
        if ((uv[0]+uv[1])%2==0)
            Kd *= 0.9; }
    if (objectId == skyId) {
        vec2 uv = vec2(-atan(V.y/V.x)/(2*PI), acos(V.z)/PI);
        FragColor = texture(skyboxTexture, uv);
    }
    else {
        vec2 uv = vec2(0.0, 0.0);
        if (objectId == roomId) {
            uv = vec2(2.0 * texCoord.y, texCoord.x);
            vec3 delta = texture(normalMap, 20.0*uv).xyz;
            delta = 2.0*delta - vec3(1,1,1);
            N = (delta.x*T + delta.y*B + delta.z*N);
            Kd = texture(tex, 20.0*uv).xyz;
        }
        if (objectId == boxId) {
            vec3 delta = texture(normalMap, texCoord).xyz;
            delta = 2.0*delta - vec3(1,1,1);
            N = (delta.x*T + delta.y*B + delta.z*N);
            Kd = texture(tex, texCoord).xyz;
        }
        if (objectId == floorId) {
            vec3 delta = texture(normalMap, texCoord).xyz;
            delta = 2.0*delta - vec3(1,1,1);
            N = (delta.x*T + delta.y*B + delta.z*N);
            Kd = texture(tex, texCoord).xyz;
        }
        if (objectId == teapotId) {
            Kd = texture(tex, texCoord).xyz;
        }
        if (objectId == seaId) {
            vec3 delta = texture(normalMap, 500.0*texCoord).xyz;
            delta = 2.0*delta - vec3(1,1,1);
            N = (delta.x*T + delta.y*B + delta.z*N);
            vec3 R = -(2 * dot(V,N)*N - V);
            uv = vec2(-atan(R.y/R.x)/(2*PI), acos(R.z)/PI);
            Kd = texture(skyboxTexture, uv).xyz;    
        }
        if (objectId == lPicId) {
            ivec2 uv = ivec2(floor(8.0*texCoord));
            if ((uv[0]+uv[1])%2==0)
                Kd = vec3(1.0,1.0,1.0);
        }
        if (objectId == rPicId) {
            Kd = texture(tex, 100.0*texCoord).xyz;
        }
        if (objectId == groundId) {
            Kd = texture(tex, 500.0*texCoord).xyz;
        }
        vec3 H = normalize(L+V);
        float LN = max(dot(L,N),0.0);
        float HN = max(dot(H,N),0.0);
        float HL = max(dot(H,L),0.0);

        vec3 F = Ks + (vec3(1,1,1) - Ks)*(pow(1-HL,5));
        float G = 1 / (pow(HL,2));
        float D = ((a+2)/(2*PI))*pow(HN,a);
        vec3 BDRF = (Kd/PI) + ((F*G*D)/4);

        vec2 shadowIndex = shadowCoord.xy/shadowCoord.w;
        float lightDepth = 0;
        float pixelDepth = 0;
        if (shadowCoord.w > 0 && shadowIndex.x >= 0 && shadowIndex.x <= 1 && shadowIndex.y >= 0 && shadowIndex.y <= 1) {
		    lightDepth = texture(shadowMap, shadowIndex).w;   
		    pixelDepth = shadowCoord.w;
            lightDepth += 0.01;
            if (pixelDepth > lightDepth) {
                FragColor.xyz = Ambient*Kd;
                lighting = FragColor.xyz;
            }
            else {
                FragColor.xyz = (Ambient*Kd + Light*Kd*LN*BDRF);
                lighting = FragColor.xyz;
            }
        }
        else {
            FragColor.xyz = 0.6*(Ambient*Kd + Light*Kd*LN*BDRF);
            lighting = FragColor.xyz;
        }
        if (reflectiveObject) {
            vec3 R = 2 * dot(V,N)*N - V;
            float RMag = length(R);
            float a = R.x / RMag;
            float b = R.y / RMag;
            float c = R.z / RMag;
            if (c > 0) {
                uv = vec2(a/(1+c),b/(1+c)) * 0.5 + (0.5, 0.5);
                FragColor += 0.1 * texture(reflectionTopMap, uv);
            }
            else {
                uv = vec2(a/(1-c),b/(1-c)) * 0.5 + (0.5, 0.5);
                FragColor += 0.1 * texture(reflectionBottomMap, uv);
            }
            FragColor.xyz += 0.4 * lighting;
        }
    }

    return FragColor.xyz;

}
