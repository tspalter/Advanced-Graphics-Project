/////////////////////////////////////////////////////////////////////////
// Pixel shader for BDRF lighting
////////////////////////////////////////////////////////////////////////
#version 330

#define PI 3.1415926535897932384626433832795

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

uniform vec3 Light, Ambient, eyePos, lightPos;
uniform sampler2D shadowMap;
uniform sampler2D G0;
uniform sampler2D G1;
uniform sampler2D G2;
uniform sampler2D G3;
uniform sampler2D AOMap;
uniform int screenWidth, screenHeight;
uniform int objectId;
uniform mat4 ShadowMatrix;

uniform sampler2D IBL;
uniform sampler2D IRRIBL;
uniform int iblWidth, iblHeight;

uniform HammersleyBlock {
 float hammN;
 float hammersley[200]; };

float det3(vec3 a, vec3 b, vec3 c) {
	return a.x*(b.y*c.z-b.z*c.y) + a.y*(b.z*c.x-b.x*c.z) + a.z*(b.x*c.y-b.y*c.x);
}

vec3 Cholesky(float m11, float m12, float m13, float m22, float m23, float m33, float z1, float z2, float z3) {
	float a = sqrt(m11);
	float b = m12 / a;
	float c = m13 / a;
	float d = sqrt(m22 - pow(b,2));
	float e = (m23 - b * c) / d;
	float f = sqrt(m33 - pow(c,2) - pow(e,2));

	float c1hat = z1 / a;
	float c2hat = (z2 - b * c1hat) / d;
	float c3hat = (z3 - c * c1hat - e * c2hat) / f;

	float c3 = c3hat / f;
	float c2 = (c2hat - e * c3) / d;
	float c1 = (c1hat - b * c2 - c * c3) / a;
	return vec3(c1, c2, c3);
}

vec3 g(vec3 Wk, vec3 V, vec3 Ks) {
    vec3 H = normalize(Wk + V);
    float WkH = max(dot(Wk, H), 0.0);

    vec3 FWkH = Ks + ((1,1,1) - Ks) * pow(1 - WkH, 5);
    vec3 BRDF = ((FWkH) / (4 * pow(WkH, 2)));

    return BRDF;
}

void main() {
	bool debug = false;
	int debugMode = 5;
	vec2 uv = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
	if (debug) {
		// Debug Images
		// read texture based on debug mode
		if (debugMode == 0)
			gl_FragColor.xyz = texture(G0, uv).xyz;
		if (debugMode == 1)
			gl_FragColor.xyz = abs(texture(G1, uv)).xyz;
		if (debugMode == 2)
			gl_FragColor.rgb = texture(G2, uv).rgb;
		if (debugMode == 3)
			gl_FragColor.xyz = texture(G3, uv).xyz;
		if (debugMode == 4)
			gl_FragColor.xyz = texture(shadowMap, uv).xyz;
        if (debugMode == 5)
            gl_FragColor.xyz = vec3(texture(AOMap, uv).x);
		return;
	}
	else {
		vec3 N = normalize(texture(G1, uv).xyz);
		vec3 worldPos = texture(G0, uv).xyz;

		vec3 V = normalize(eyePos - worldPos);
		vec3 L = normalize(lightPos - worldPos);

		vec3 Ii = Light;
		vec3 Ia = Ambient;

		vec3 Kd = texture(G2, uv).rgb;
		vec3 Ks = texture(G3, uv).rgb;
		float a = texture(G3, uv).w;
		if (a == -1.0f) {
			gl_FragColor.xyz = Kd;
			return;
		}

		vec3 H = normalize(L + V);
		float NL = max(dot(N,L), 0.0);
		float NH = max(dot(N,H), 0.0);
		float VN = max(dot(N,V), 0.0);
		float LH = max(dot(L,H), 0.0);

		vec3 FLH = Ks + ((1.0, 1.0, 1.0) - Ks) * pow(1 - LH, 5);
		float DH = ((a + 2) / (2 * PI)) * pow(NH, a);

		vec3 BRDF = (Kd / PI) + ((FLH * DH) / (pow(LH, 2)));

		vec4 shadowCoord = ShadowMatrix * vec4(worldPos.x, worldPos.y, worldPos.z, 1.0);

		vec2 shadowIndex = shadowCoord.xy / shadowCoord.w;

		float offset = 0.01;

        // Shader code for Project 3
        // IBL diffuse
        uv = vec2(0.5 - atan(N.y, N.x) / (2 * PI), -acos(N.z) / PI);
        vec3 C = texture(IRRIBL, uv).xyz;
        vec3 diffuse = (Kd / PI) * C;

        // Store specular for averaging
        vec3 spec = vec3(0, 0, 0);

        vec3 R = 2.0 * VN * N - V;
        vec3 A = normalize(vec3(-R.y, R.x, 0.0));
        vec3 B = normalize(cross(R, A));
        float e = 0.8;
        for (int i = 0; i < hammN; i++) {
            // random values assigned in scene.cpp
            float E1 = hammersley[2 * i];
            float E2 = hammersley[2 * i + 1];

            // Phong
            float theta = acos(pow(E2, 1 / (a + 1)));
            E2 = theta / PI;

            // Direction Vector
            vec3 L = vec3(cos(2 * PI * (0.5 - E1)) * sin(PI * E2), sin(2 * PI * (0.5 - E1)) * sin(PI * E2), cos(PI * E2));

            // Rotate L towards Reflection direction
            vec3 Wk = normalize(L.x * A + L.y * B + L.z * R);
            
            vec3 H2 = normalize(Wk + V);
            float NH2 = max(dot(N, H2), 0.0);
            float DH2 = ((a + 2) / (2 * PI) * pow(NH2, a));

            // Level of Detail
            float level = 0.5 * log2((iblWidth * iblHeight) / hammN) - 0.5 * log2(DH2);
            //level = 0;
            uv = vec2(0.5 - (atan(Wk.y, Wk.x) / (2*PI)), -acos(Wk.z) / PI);
            // color
            C = textureLod(IBL, uv, level).xyz;
            spec += g(Wk, V, Ks) * C * cos(theta);
        }
        spec /= hammN;
        
        C = diffuse + spec;
        
        C = (e * C) / (e * C + vec3(1,1,1));
        C = vec3(pow(C.x, 1/2.2), pow(C.y, 1/2.2), pow(C.z, 1/2.2));

        uv = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
        float ambientScalar = texture(AOMap, uv).x;
        if (ambientScalar > 0) {
            C = ambientScalar * C;
        }
        else {
            C = 0.0 * C;
        }

        gl_FragColor.xyz = C;
        return;


		if (shadowCoord.w > 0 && shadowIndex.x > 0 + offset && shadowIndex.x < 1 - offset && shadowIndex.y > 0 + offset && shadowIndex.y < 1 - offset) {
			vec4 b = texture(shadowMap, shadowIndex);
            float alpha = 1.0E-3;
            vec4 half_max_depth = vec4(0.5,0.5,0.5,0.5);
            vec4 bPrime = (1-alpha) * b + alpha * half_max_depth;
            
            float zf = shadowCoord.w/150;

            gl_FragColor = vec4(zf);
            //return;

            float z1 = 1;
            float z2 = zf;
            float z3 = zf*zf;

            // Begin Cholesky   
            float m11 = 1;
            float m12 = bPrime.x;
            float m13 = bPrime.y;
            float m22 = bPrime.y;
            float m23 = bPrime.z;
            float m33 = bPrime.w;

            vec3 cs = Cholesky(m11,m12,m13,m22,m23,m33,z1,z2,z3);
            float c1 = cs.x;
            float c2 = cs.y;
            float c3 = cs.z;
            // End Cholesky

            // Cholesky Test
            mat3 choleskyMat = mat3(1,        bPrime.x, bPrime.y,
                                    bPrime.x, bPrime.y, bPrime.z,
                                    bPrime.y, bPrime.z, bPrime.w);

            vec3 zTest = choleskyMat * cs;
            bool Successful = true;
            float tolerance = 1.0E-3;
            if (zTest.x >(1 + tolerance) || zTest.x < (1-tolerance)) {
                Successful = false;
            }
            if (zTest.y > (zf + tolerance) || zTest.y < (zf-tolerance)) {
                Successful = false;
            }
            if (zTest.z > (pow(zf,2) + tolerance) || zTest.z < (pow(zf,2) - tolerance)) {
                Successful = false;
            }

            if (Successful)
                gl_FragColor.xyz = vec3(0,1,0);
            else {
                gl_FragColor.xyz = vec3(1,0,0);
            }
            //return;
            // End Cholesky Test
               
            float zeta2 = (-c2 + sqrt( pow(c2, 2) - (4 * c3 * c1))) / (2 * c3);
            float zeta3 = (-c2 - sqrt( pow(c2, 2) - (4 * c3 * c1))) / (2 * c3);
            
            if(zeta2 > zeta3) {
                float temp = zeta3;
                zeta3 = zeta2;
                zeta2 = temp;
            }

            float G = 0;

            if(zf <= zeta2) {
                G = 0;
                gl_FragColor.xyz = vec3(1,0,0);
            }
            else if(zf <= zeta3) {
                G = (zf * zeta3 - bPrime.x * (zf + zeta3) + bPrime.y) / ((zeta3-zeta2)*(zf-zeta2));
                gl_FragColor.xyz = vec3(0,1,0);
            }
            else {
                G = 1 - ((zeta2*zeta3-bPrime.x * (zeta2+zeta3)+bPrime.y) / ((zf-zeta2)*(zf-zeta3)));
                gl_FragColor.xyz = vec3(0,0,1);
            }
            //return;
            gl_FragColor.xyz = Ia * Kd + ((1-G) * (Ii * NL * BRDF));
        }
        
        else {
           gl_FragColor.xyz = Ia * Kd + Ii * NL * BRDF;
        }
	}
}