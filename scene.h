////////////////////////////////////////////////////////////////////////
// The scene class contains all the parameters needed to define and
// draw a simple scene, including:
//   * Geometry
//   * Light parameters
//   * Material properties
//   * Viewport size parameters
//   * Viewing transformation values
//   * others ...
//
// Some of these parameters are set when the scene is built, and
// others are set by the framework in response to user mouse/keyboard
// interactions.  All of them can be used to draw the scene.

#include "shapes.h"
#include "object.h"
#include "texture.h"
#include "fbo.h"

enum ObjectIds {
    nullId	= 0,
    skyId	= 1,
    seaId	= 2,
    groundId	= 3,
    roomId	= 4,
    boxId	= 5,
    frameId	= 6,
    lPicId	= 7,
    rPicId	= 8,
    teapotId	= 9,
    spheresId	= 10,
    floorId     = 11,
    other       = 12
};

class Shader;


class Scene
{
public:
    GLFWwindow* window;

    // @@ Declare interactive viewing variables here. (spin, tilt, ry, front back, ...)
    float spin, tilt, ty, tx, zoom, ry, rx, front, back, speed, time_since_last_refresh, step;
    bool w_down;
    bool s_down;
    bool a_down;
    bool d_down;
    bool transformation_mode;
    glm::vec3 eye;
    // Light parameters
    float lightSpin, lightTilt, lightDist;
    glm::vec3 lightPos;
    // @@ Perhaps declare additional scene lighting values here. (lightVal, lightAmb)
    glm::vec3 Light, Ambient;
    
    ProceduralGround* ground;


    int mode; // Extra mode indicator hooked up to number keys and sent to shader
    int unit;
    int bindpoint;
    
    // Viewport
    int width, height;

    // FBO Viewport
    int fboWidth, fboHeight;

    // Transformations
    glm::mat4 WorldProj, WorldView, WorldInverse;

    // All objects in the scene are children of this single root object.
    Object* objectRoot;
    Object* quad;
    Object* sphere;
    std::vector<Object*> animated;

    // Local Light vars
    unsigned int numLocalLights;
    std::vector<glm::vec3> localLightPositions;
    std::vector<glm::vec3> localLightColors;
    std::vector<float> localLightRadii;

    // FBOs
    FBO* shadowMap;
    FBO* blurMap;
    FBO* reflectionTopMap;
    FBO* reflectionBottomMap;
    FBO* gBufferMap;
    FBO* AOScalarMap;
    FBO* AOTempMap;

    // Shader programs
    ShaderProgram* lightingProgram;
    ShaderProgram* shadowProgram;
    ShaderProgram* reflectionProgram;
    ShaderProgram* gBufferProgram;
    ShaderProgram* localLightProgram;
    ShaderProgram* computeShadowProgramV;
    ShaderProgram* computeShadowProgramH;
    ShaderProgram* AOProgramV;
    ShaderProgram* AOProgramH;

    // Light matrices
    glm::mat4 vL, pL, B, ShadowMatrix;

    // check for reflective (teapot) or not
    bool isReflectiveObject;

    // skybox
    Texture* skybox;

    // IBL Textures
    Texture* cloudsIBL;
    Texture* cloudsIRRIBL;

    void InitializeScene();
    void BuildTransforms();
    void DrawScene();

};
