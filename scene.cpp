////////////////////////////////////////////////////////////////////////
// The scene class contains all the parameters needed to define and
// draw a simple scene, including:
//   * Geometry
//   * Light parameters
//   * Material properties
//   * viewport size parameters
//   * Viewing transformation values
//   * others ...
//
// Some of these parameters are set when the scene is built, and
// others are set by the framework in response to user mouse/keyboard
// interactions.  All of them can be used to draw the scene.

const bool fullPolyCount = true; // Use false when emulating the graphics pipeline in software
#ifdef REFL
const bool showSpheres = true;  // Use true for shadows and reflections test scenes
#else
const bool showSpheres = true;  // Use true for shadows and reflections test scenes
#endif

#include "math.h"
#include <iostream>
#include <stdlib.h>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>
using namespace gl;

#include <glu.h>                // For gluErrorString

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/ext.hpp>          // For printing GLM objects with to_string

#include "framework.h"
#include "shapes.h"
#include "object.h"
#include "texture.h"
#include "transform.h"

const float PI = 3.141592653589793f;
const float rad = PI/180.0f;    // Convert degrees to radians

glm::mat4 Identity;

const float grndSize = 100.0;    // Island radius;  Minimum about 20;  Maximum 1000 or so
const float grndOctaves = 4.0;  // Number of levels of detail to compute
const float grndFreq = 0.03;    // Number of hills per (approx) 50m
const float grndPersistence = 0.03; // Terrain roughness: Slight:0.01  rough:0.05
const float grndLow = -3.0;         // Lowest extent below sea level
const float grndHigh = 5.0;        // Highest extent above sea level

const int blurWidth = 10;

////////////////////////////////////////////////////////////////////////
// This macro makes it easy to sprinkle checks for OpenGL errors
// throughout your code.  Most OpenGL calls can record errors, and a
// careful programmer will check the error status *often*, perhaps as
// often as after every OpenGL call.  At the very least, once per
// refresh will tell you if something is going wrong.
#define CHECKERROR {GLenum err = glGetError(); if (err != GL_NO_ERROR) { fprintf(stderr, "OpenGL error (at line scene.cpp:%d): %s\n", __LINE__, gluErrorString(err)); exit(-1);} }

// Create an RGB color from human friendly parameters: hue, saturation, value
glm::vec3 HSV2RGB(const float h, const float s, const float v)
{
    if (s == 0.0)
        return glm::vec3(v,v,v);

    int i = (int)(h*6.0) % 6;
    float f = (h*6.0f) - i;
    float p = v*(1.0f - s);
    float q = v*(1.0f - s*f);
    float t = v*(1.0f - s*(1.0f-f));
    if      (i == 0)     return glm::vec3(v,t,p);
    else if (i == 1)  return glm::vec3(q,v,p);
    else if (i == 2)  return glm::vec3(p,v,t);
    else if (i == 3)  return glm::vec3(p,q,v);
    else if (i == 4)  return glm::vec3(t,p,v);
    else   /*i == 5*/ return glm::vec3(v,p,q);
}

////////////////////////////////////////////////////////////////////////
// Constructs a hemisphere of spheres of varying hues
Object* SphereOfSpheres(Shape* SpherePolygons)
{
    Object* ob = new Object(NULL, nullId);
    float alpha = 50.0f;
    for (float angle = 0.0; angle < 360.0; angle += 18.0) {
        for (float row = 0.075; row < PI / 2.0; row += PI / 2.0 / 6.0) {
            glm::vec3 hue = HSV2RGB(angle / 360.0, 1.0f - 2.0f * row / PI, 1.0f);
            Object* sp = new Object(SpherePolygons, spheresId,
                0.5f * hue, glm::vec3(1.0, 1.0, 1.0), alpha);
            alpha += 100.0f;
            float s = sin(row);
            float c = cos(row);
            ob->add(sp, Rotate(2, angle) * Translate(c, 0, s) * Scale(0.075 * c, 0.075 * c, 0.075 * c));
        }    
    }
        
    return ob;
}

////////////////////////////////////////////////////////////////////////
// Constructs a -1...+1  quad (canvas) framed by four (elongated) boxes
Object* FramedPicture(const glm::mat4& modelTr, const int objectId, 
                      Shape* BoxPolygons, Shape* QuadPolygons)
{
    // This draws the frame as four (elongated) boxes of size +-1.0
    float w = 0.05;             // Width of frame boards.
    
    Object* frame = new Object(NULL, nullId);
    Object* ob;
    
    glm::vec3 woodColor(87.0/255.0,51.0/255.0,35.0/255.0);
    ob = new Object(BoxPolygons, frameId,
                    woodColor, glm::vec3(0.2, 0.2, 0.2), 10.0);
    frame->add(ob, Translate(0.0, 0.0, 1.0+w)*Scale(1.0, w, w));
    frame->add(ob, Translate(0.0, 0.0, -1.0-w)*Scale(1.0, w, w));
    frame->add(ob, Translate(1.0+w, 0.0, 0.0)*Scale(w, w, 1.0+2*w));
    frame->add(ob, Translate(-1.0-w, 0.0, 0.0)*Scale(w, w, 1.0+2*w));

    ob = new Object(QuadPolygons, objectId,
                    woodColor, glm::vec3(0.0, 0.0, 0.0), 10.0);
    frame->add(ob, Rotate(0,90));

    return frame;
}

Object* QuadObject(Shape* QuadPolygons) {
    Object* ob;

    glm::vec3 blankColor(0.0, 0.0, 0.0);
    ob = new Object(QuadPolygons, 12, blankColor, glm::vec3(0.0, 0.0, 0.0), 1.0);

    return ob;
}

Object* SphereObject(Shape* SpherePolygons) {
    glm::vec3 hue = HSV2RGB(0.0, 0.0, 1.0f);
    Object* sp = new Object(SpherePolygons, spheresId, hue, glm::vec3(1.0, 1.0, 1.0), 120.0);
    return sp;
}

struct {
    float N = 40;
    float hammersley[200];
} block;

////////////////////////////////////////////////////////////////////////
// InitializeScene is called once during setup to create all the
// textures, shape VAOs, and shader programs as well as setting a
// number of other parameters.
void Scene::InitializeScene()
{
    glEnable(GL_DEPTH_TEST);
    CHECKERROR;
    // @@ Initialize interactive viewing variables here. (spin, tilt, ry, front back, ...)
    spin = 0.0;
    tilt = 30.0;
    tx = 0.0;
    ty = 0.0;
    zoom = 25.0;
    ry = 0.4;
    rx = ry * width / height;
    front = 0.5;
    back = 5000.0;
    eye = glm::vec3(0.0, -20.0, 2.0);
    speed = 0.05;
    time_since_last_refresh = 0.0;
    step = 0.0;
    w_down = false;
    s_down = false;
    a_down = false;
    d_down = false;
    transformation_mode = false;
    unit = 1;
    numLocalLights = 100;
    bindpoint = 0;

    fboWidth = 1000;
    fboHeight = 1000;

    // Set initial light parameters
    lightSpin = 150.0;
    lightTilt = -45.0;
    lightDist = 100.0;

    // @@ Perhaps initialize additional scene lighting values here. (lightVal, lightAmb)
    Light = glm::vec3(3.0, 3.0, 3.0); // Ii
    Ambient = glm::vec3(0.2, 0.2, 0.2);  // Ia

    // random points for Project 3
    int kk;
    int pos = 0;
    float p;
    float u;
    for (int k = 0; k < block.N; k++) {
        for (p = 0.5f, kk = k, u = 0.0f; kk; p *= 0.5f, kk >>= 1) {
            if (kk & 1)
                u += p;
        }
        float v = (k + 0.5) / block.N;
        block.hammersley[pos++] = u;
        block.hammersley[pos++] = v;

        std::cout << block.hammersley[pos - 2] << " " << block.hammersley[pos - 1] << std::endl;
    }

    // Random lights
    srand(13);
    for (unsigned int i = 0; i < numLocalLights; i++)
    {
        // calculate random offsets
        float xPos = ((rand() % 100) / 100.0) * 200.0 - 100;
        float yPos = ((rand() % 100) / 100.0) * 200.0 - 100;
        float zPos = ((rand() % 100) / 100.0) * 50.0;
        localLightPositions.push_back(glm::vec3(xPos, yPos, zPos));
        // calculate random color
        float rColor = ((rand() % 100) / 200.0f) + 0.5;
        float gColor = ((rand() % 100) / 200.0f) + 0.5; 
        float bColor = ((rand() % 100) / 200.0f) + 0.5;
        localLightColors.push_back(glm::vec3(rColor, gColor, bColor));
        float radius = ((rand() % 100) * 1.0f) + 0.5;
        localLightRadii.push_back(radius);
    }

    shadowMap = new FBO();
    shadowMap->CreateFBO(fboWidth, fboHeight, false);
    blurMap = new FBO();
    blurMap->CreateFBO(fboWidth, fboHeight, false);
    reflectionTopMap = new FBO();
    reflectionTopMap->CreateFBO(1000, 1000, false);
    reflectionBottomMap = new FBO();
    reflectionBottomMap->CreateFBO(1000, 1000, false);
    gBufferMap = new FBO();
    gBufferMap->CreateFBO(2000, 2000, true);
    AOTempMap = new FBO();
    AOTempMap->CreateFBO(fboWidth, fboHeight, false);
    AOScalarMap = new FBO();
    AOScalarMap->CreateFBO(fboWidth, fboHeight, false);

    isReflectiveObject = false;

    CHECKERROR;
    objectRoot = new Object(NULL, nullId);

    
    // Enable OpenGL depth-testing
    glEnable(GL_DEPTH_TEST);

    // Create the lighting shader program from source code files.
    // @@ Initialize additional shaders if necessary
    lightingProgram = new ShaderProgram();
    lightingProgram->AddShader("bdrf.vert", GL_VERTEX_SHADER);
    lightingProgram->AddShader("bdrf.frag", GL_FRAGMENT_SHADER);

    glBindAttribLocation(lightingProgram->programId, 0, "vertex");
    glBindAttribLocation(lightingProgram->programId, 1, "vertexNormal");
    glBindAttribLocation(lightingProgram->programId, 2, "vertexTexture");
    glBindAttribLocation(lightingProgram->programId, 3, "vertexTangent");
    lightingProgram->LinkProgram();

    shadowProgram = new ShaderProgram();
    shadowProgram->AddShader("shadow.vert", GL_VERTEX_SHADER);
    shadowProgram->AddShader("shadow.frag", GL_FRAGMENT_SHADER);

    glBindAttribLocation(shadowProgram->programId, 0, "vertex");
    shadowProgram->LinkProgram();

    //reflectionProgram = new ShaderProgram();
    //reflectionProgram->AddShader("reflection.vert", GL_VERTEX_SHADER);
    //reflectionProgram->AddShader("reflection.frag", GL_FRAGMENT_SHADER);
    //reflectionProgram->AddShader("lighting.vert", GL_VERTEX_SHADER);
    //reflectionProgram->AddShader("lighting.frag", GL_FRAGMENT_SHADER);

    //glBindAttribLocation(reflectionProgram->programId, 0, "vertex");
    //glBindAttribLocation(reflectionProgram->programId, 1, "vertexNormal");
    //glBindAttribLocation(reflectionProgram->programId, 2, "vertexTexture");
    //glBindAttribLocation(reflectionProgram->programId, 3, "vertexTangent");
    //reflectionProgram->LinkProgram();

    gBufferProgram = new ShaderProgram();
    gBufferProgram->AddShader("gbuffer.vert", GL_VERTEX_SHADER);
    gBufferProgram->AddShader("gbuffer.frag", GL_FRAGMENT_SHADER);

    glBindAttribLocation(gBufferProgram->programId, 0, "vertex");
    glBindAttribLocation(gBufferProgram->programId, 1, "vertexNormal");
    glBindAttribLocation(gBufferProgram->programId, 2, "vertexTexture");
    glBindAttribLocation(gBufferProgram->programId, 3, "vertexTangent");
    gBufferProgram->LinkProgram();

    localLightProgram = new ShaderProgram();
    localLightProgram->AddShader("local.vert", GL_VERTEX_SHADER);
    localLightProgram->AddShader("local.frag", GL_FRAGMENT_SHADER);

    glBindAttribLocation(localLightProgram->programId, 0, "vertex");
    glBindAttribLocation(localLightProgram->programId, 1, "vertexNormal");
    glBindAttribLocation(localLightProgram->programId, 2, "vertexTexture");
    glBindAttribLocation(localLightProgram->programId, 3, "vertexTangent");
    localLightProgram->LinkProgram();

    computeShadowProgramV = new ShaderProgram();
    computeShadowProgramV->AddShader("shadowv.compute", GL_COMPUTE_SHADER);
    computeShadowProgramV->LinkProgram();

    computeShadowProgramH = new ShaderProgram();
    computeShadowProgramH->AddShader("shadowh.compute", GL_COMPUTE_SHADER);
    computeShadowProgramH->LinkProgram();

    AOProgramV = new ShaderProgram();
    AOProgramV->AddShader("aov.compute", GL_COMPUTE_SHADER);
    AOProgramV->LinkProgram();

    AOProgramH = new ShaderProgram();
    AOProgramH->AddShader("aoh.compute", GL_COMPUTE_SHADER);
    AOProgramH->LinkProgram();
    
    // Create all the Polygon shapes
    Shape* TeapotPolygons =  new Teapot(fullPolyCount?12:2);
    Shape* BoxPolygons = new Box();
    Shape* SpherePolygons = new Sphere(32);
    Shape* RoomPolygons = new Ply("room.ply");
    Shape* FloorPolygons = new Plane(10.0, 10);
    Shape* QuadPolygons = new Quad();
    Shape* SeaPolygons = new Plane(2000.0, 50);
    ground = new ProceduralGround(grndSize, 400,
                                     grndOctaves, grndFreq, grndPersistence,
                                     grndLow, grndHigh);
    Shape* GroundPolygons = ground;

    // Various colors used in the subsequent models
    glm::vec3 woodColor(87.0/255.0, 51.0/255.0, 35.0/255.0);
    glm::vec3 brickColor(134.0/255.0, 60.0/255.0, 56.0/255.0);
    glm::vec3 floorColor(6*16/255.0, 5.5*16/255.0, 3*16/255.0);
    glm::vec3 brassColor(0.5, 0.5, 0.1);
    glm::vec3 grassColor(62.0/255.0, 102.0/255.0, 38.0/255.0);
    glm::vec3 waterColor(0.3, 0.3, 1.0);

    glm::vec3 black(0.0, 0.0, 0.0);
    glm::vec3 brightSpec(0.03, 0.03, 0.03);
    glm::vec3 polishedSpec(0.01, 0.01, 0.01);
 
    // Creates all the models from which the scene is composed.  Each
    // is created with a polygon shape (possibly NULL), a
    // transformation, and the surface lighting parameters Kd, Ks, and
    // alpha.

    // @@ This is where you could read in all the textures and
    // associate them with the various objects being created in the
    // next dozen lines of code.

    // @@ To change an object's surface parameters (Kd, Ks, or alpha),
    // modify the following lines.
    
    Object* central    = new Object(NULL, nullId);
    Object* anim       = new Object(NULL, nullId);
    //Object* room       = new Object(RoomPolygons, roomId, brickColor, black, 1);
    //Object* floor      = new Object(FloorPolygons, floorId, floorColor, black, 1);
    Object* teapot     = new Object(TeapotPolygons, teapotId, brassColor, glm::vec3(1.0, 1.0, 1.0), 2000);
    //Object* podium     = new Object(BoxPolygons, boxId, glm::vec3(woodColor), polishedSpec, 10); 
    Object* sky        = new Object(SpherePolygons, skyId, black, black, 0);
    //Object* ground     = new Object(GroundPolygons, groundId, grassColor, black, 1);
    //Object* sea        = new Object(SeaPolygons, seaId, waterColor, brightSpec, 120);
    Object* spheres    = SphereOfSpheres(SpherePolygons);
    //Object* leftFrame  = FramedPicture(Identity, lPicId, BoxPolygons, QuadPolygons);
    //Object* rightFrame = FramedPicture(Identity, rPicId, BoxPolygons, QuadPolygons); 


    // @@ To change the scene hierarchy, examine the hierarchy created
    // by the following object->add() calls and adjust as you wish.
    // The objects being manipulated and their polygon shapes are
    // created above here.

    // Scene is composed of sky, ground, sea, room and some central models
    if (fullPolyCount) {
        objectRoot->add(sky, Scale(2000.0, 2000.0, 2000.0));
        //objectRoot->add(sea); 
        /*objectRoot->add(ground);*/ }
    objectRoot->add(central);
#ifndef REFL
     //objectRoot->add(room,  Translate(0.0, 0.0, 0.02));
#endif
    //objectRoot->add(floor, Translate(0.0, 0.0, 0.02));

    // Central model has a rudimentary animation (constant rotation on Z)
    animated.push_back(anim);

    // Central contains a teapot on a podium and an external sphere of spheres
    //central->add(podium, Translate(0.0, 0,0));
    central->add(anim, Translate(0.0, 0,0));
    anim->add(teapot, Translate(0.1, 0.0, 1.5)*TeapotPolygons->modelTr);
    if (showSpheres && fullPolyCount)
        anim->add(spheres, Translate(0.0, 0.0, 0.0)*Scale(30.0, 30.0, 30.0));
    
    // Room contains two framed pictures
    if (fullPolyCount) {
        //room->add(leftFrame, Translate(-1.5, 9.85, 1.)*Scale(0.8, 0.8, 0.8));
        //room->add(rightFrame, Translate( 1.5, 9.85, 1.)*Scale(0.8, 0.8, 0.8));
    }

    CHECKERROR;

    //floor->objTexture = new Texture("./textures/6670-diffuse.jpg");
    //floor->normalTexture = new Texture("./textures/6670-normal.jpg");
    sky->objTexture = new Texture("./skys/Tropical_Beach_3k.hdr");
    skybox = sky->objTexture;
    //sea->objTexture = new Texture("./skys/Tropical_Beach_3k.hdr");
    //sea->normalTexture = new Texture("./textures/ripples_normalmap.png");
    //room->objTexture = new Texture("./textures/Standard_red_pxr128.png");
    //room->normalTexture = new Texture("./textures/Standard_red_pxr128_normal.png");
    //podium->objTexture = new Texture("./textures/Brazilian_rosewood_pxr128.png");
    //podium->normalTexture = new Texture("./textures/Brazilian_rosewood_pxr128_normal.png");
    teapot->objTexture = new Texture("./textures/cracks.png");
    //ground->objTexture = new Texture("./textures/grass.jpg");
    //rightFrame->objTexture = new Texture("./textures/my-house-01.png");

    Texture* clouds = new Texture("./skys/Tropical_Beach_3k.hdr");
    cloudsIBL = new Texture("./skys/Tropical_Beach_3k.hdr");
    cloudsIRRIBL = new Texture("./skys/Tropical_Beach_3k.irr.hdr");

    quad = QuadObject(QuadPolygons);
    sphere = SphereObject(SpherePolygons);
}

void Scene::BuildTransforms()
{
    rx = ry * width / height;

    // @@ When you are ready to try interactive viewing, replace the
    // following hard coded values for WorldProj and WorldView with
    // transformation matrices calculated from variables such as spin,
    // tilt, tr, ry, front, and back.

    if (transformation_mode) {
        WorldView = Rotate(0, tilt - 90) * Rotate(2, spin) * Translate(-eye.x, -eye.y, -eye.z);
        WorldProj = Perspective(rx, ry, front, back);
    }
    else {
        WorldView = Translate(tx, ty, -zoom) * Rotate(0, tilt - 90) * Rotate(2, spin);
        WorldProj = Perspective(rx, ry, front, back);
    }

    // The lighting algorithm needs the inverse of the WorldView matrix
    WorldInverse = glm::inverse(WorldView);

    float lightRx, lightRy;
    float lightMag = glm::length(lightPos);
    lightRx = rx * (1 / (lightMag / 100));
    lightRy = ry * (1 / (lightMag / 100));

    // WorldView should be set to LookAt matrix
    vL = LookAt(lightPos, glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));

    // WorldProj should be set to Perspective(lightRx, lightRy, front, back)
    pL = Perspective(lightRx, lightRy, front, back);

    // Build the ShadowMatrix
    B = Translate(0.5, 0.5, 0.5) * Scale(0.5, 0.5, 0.5);
    ShadowMatrix = B * pL * vL;

    // @@ Print the two matrices (in column-major order) for
    // comparison with the project document.
    //std::cout << "WorldView: " << glm::to_string(WorldView) << std::endl;
    //std::cout << "WorldProj: " << glm::to_string(WorldProj) << std::endl;

}

////////////////////////////////////////////////////////////////////////
// Procedure DrawScene is called whenever the scene needs to be
// drawn. (Which is often: 30 to 60 times per second are the common
// goals.)
void Scene::DrawScene()
{
    time_since_last_refresh = glfwGetTime() - time_since_last_refresh;
    step = speed * time_since_last_refresh;
    if (transformation_mode) {
        if (w_down) {
            eye += step * glm::vec3(sin(spin * PI / 180), cos(spin * PI / 180), 0.0);
        }
        if (s_down) {
            eye -= step * glm::vec3(sin(spin * PI / 180), cos(spin * PI / 180), 0.0);
        }
        if (d_down) {
            eye += step * glm::vec3(cos(spin * PI / 180), -sin(spin * PI / 180), 0.0);
        }
        if (a_down) {
            eye -= step * glm::vec3(cos(spin * PI / 180), -sin(spin * PI / 180), 0.0);
        }
    }

    eye.z = ground->HeightAt(eye.x, eye.y) + 2.0;
    
    // Set the viewport
    glfwGetFramebufferSize(window, &width, &height);
    if (width != gBufferMap->width || height != gBufferMap->height)
    {
        gBufferMap->Destroy();
        gBufferMap->CreateFBO(width, height, true);

        AOTempMap->Destroy();
        AOTempMap->CreateFBO(width, height, false);

        AOScalarMap->Destroy();
        AOScalarMap->CreateFBO(width, height, false);
    }
    glViewport(0, 0, width, height);

    CHECKERROR;
    // Calculate the light's position from lightSpin, lightTilt, lightDist
    lightPos = glm::vec3(lightDist * cos(lightSpin * rad) * sin(lightTilt * rad),
        lightDist * sin(lightSpin * rad) * sin(lightTilt * rad),
        lightDist * cos(lightTilt * rad));

    // Update position of any continuously animating objects
    double atime = 360.0*glfwGetTime()/36;
    atime = 0;
    for (std::vector<Object*>::iterator m=animated.begin();  m<animated.end();  m++)
        (*m)->animTr = Rotate(2, atime);

    BuildTransforms();
    

    ////////////////////////////////////////////////////////////////////////////////
    // Anatomy of a pass:
    //   Choose a shader  (create the shader in InitializeScene above)
    //   Choose and FBO/Render-Target (if needed; create the FBO in InitializeScene above)
    //   Set the viewport (to the pixel size of the screen or FBO)
    //   Clear the screen.
    //   Set the uniform variables required by the shader
    //   Draw the geometry
    //   Unset the FBO (if one was used)
    //   Unset the shader
    ////////////////////////////////////////////////////////////////////////////////

    CHECKERROR;
    int loc, programId;
    

    ////////////////////////////////////////////////////////////////////////////////
    // G-Buffer pass
    ////////////////////////////////////////////////////////////////////////////////

    // Choose the gBuffer shader
    gBufferProgram->Use();
    gBufferMap->Bind();
    programId = gBufferProgram->programId;

    GLenum attachments[4] = { GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_COLOR_ATTACHMENT3_EXT };
    glDrawBuffers(4, attachments);
    CHECKERROR;

    // Set the viewport, and clear the screen
    glViewport(0, 0, width, height);
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // @@ The scene specific parameters (uniform variables) used by
    // the shader are set here.  Object specific parameters are set in
    // the Draw procedure in object.cpp

    // Light & Ambient coloring & brightness
    loc = glGetUniformLocation(programId, "Light");
    glUniform3fv(loc, 1, &(Light[0]));

    loc = glGetUniformLocation(programId, "Ambient");
    glUniform3fv(loc, 1, &(Ambient[0]));

    // default shader parameters
    loc = glGetUniformLocation(programId, "WorldProj");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldProj));
    loc = glGetUniformLocation(programId, "WorldView");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldView));
    loc = glGetUniformLocation(programId, "WorldInverse");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldInverse));
    loc = glGetUniformLocation(programId, "eyePos");
    glUniform3fv(loc, 1, &(eye[0]));
    loc = glGetUniformLocation(programId, "lightPos");
    glUniform3fv(loc, 1, &(lightPos[0]));

    loc = glGetUniformLocation(programId, "ShadowMatrix");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ShadowMatrix));
    CHECKERROR;

    objectRoot->Draw(gBufferProgram, Identity);

    // Turn off the shader
    gBufferMap->Unbind();
    gBufferProgram->Unuse();

    ////////////////////////////////////////////////////////////////////////////////
    // End of G-Buffer pass
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    // Shadow pass
    ////////////////////////////////////////////////////////////////////////////////
    shadowProgram->Use();
    shadowMap->Bind();
    programId = shadowProgram->programId;

    // Set viewport to size of FBO
    glViewport(0, 0, shadowMap->width, shadowMap->height);
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // default shader parameters
    loc = glGetUniformLocation(programId, "WorldProj");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(pL));
    loc = glGetUniformLocation(programId, "WorldView");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(vL));
    loc = glGetUniformLocation(programId, "mode");
    glUniform1i(loc, mode);
    CHECKERROR;
    
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    // Draw all objects (This recursively traverses the object hierarchy.)
    objectRoot->Draw(shadowProgram, Identity);
    glDisable(GL_CULL_FACE);
    CHECKERROR;

    // Turn off the shader and unbind FBO
    shadowMap->Unbind();
    shadowProgram->Unuse();
    ////////////////////////////////////////////////////////////////////////////////
    // End of Shadow pass
    ////////////////////////////////////////////////////////////////////////////////

    // Shadow Compute
    {
        ////////////////////////////////////////////////////////////////////////////////
        // Shadow Compute pass - V
        ////////////////////////////////////////////////////////////////////////////////
        float s = (float)blurWidth / 2;
        const int length = blurWidth * 2 + 1;
        float weights[length] = { 0.0f };
        float sum = 0.0f;
        for (int i = 0; i < length; i++) {
            weights[i] = (float)exp(-0.5 * pow((i-blurWidth)/s, 2));
            sum += weights[i];
        }
        for (int i = 0; i < length; i++) {
            weights[i] /= sum;
        }

        computeShadowProgramH->Use();
        programId = computeShadowProgramH->programId;

        GLuint blockID;
        glGenBuffers(1, &blockID);
        bindpoint = 0;

        loc = glGetUniformBlockIndex(programId, "blurKernel");
        glUniformBlockBinding(programId, loc, bindpoint);

        glBindBuffer(GL_UNIFORM_BUFFER, blockID);
        glBindBufferBase(GL_UNIFORM_BUFFER, bindpoint, blockID);
        glBufferData(GL_UNIFORM_BUFFER, length * sizeof(float), &weights, GL_STATIC_DRAW);

        loc = glGetUniformLocation(programId, "w");
        glUniform1i(loc, blurWidth);

        loc = glGetUniformLocation(programId, "src");
        glBindImageTexture(0, shadowMap->textureIDs[0], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
        glUniform1i(loc, 0);

        loc = glGetUniformLocation(programId, "dst");
        glBindImageTexture(1, blurMap->textureIDs[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glUniform1i(loc, 1);

        glDispatchCompute((fboWidth/128) + 1, fboHeight, 1);
        computeShadowProgramH->Unuse();
        ////////////////////////////////////////////////////////////////////////////////
        // End of Shadow Compute pass - V
        ////////////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////////////
        // Shadow Compute pass - H
        ////////////////////////////////////////////////////////////////////////////////
        computeShadowProgramV->Use();
        programId = computeShadowProgramV->programId;

        loc = glGetUniformBlockIndex(programId, "blurKernel");
        glUniformBlockBinding(programId, loc, bindpoint);

        glBindBuffer(GL_UNIFORM_BUFFER, blockID);
        glBindBufferBase(GL_UNIFORM_BUFFER, bindpoint, blockID);
        glBufferData(GL_UNIFORM_BUFFER, length * sizeof(float), &weights, GL_STATIC_DRAW);

        loc = glGetUniformLocation(programId, "w");
        glUniform1i(loc, blurWidth);

        loc = glGetUniformLocation(programId, "src");
        glBindImageTexture(0, blurMap->textureIDs[0], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
        glUniform1i(loc, 0);

        loc = glGetUniformLocation(programId, "dst");
        glBindImageTexture(1, shadowMap->textureIDs[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glUniform1i(loc, 1);

        glDispatchCompute((fboWidth / 128) + 1, fboHeight, 1);
        computeShadowProgramV->Unuse();
        ////////////////////////////////////////////////////////////////////////////////
        // End of Shadow Compute pass - H
        ////////////////////////////////////////////////////////////////////////////////
    }
    // End of Shadow Compute

    // AO Compute Pass
    CHECKERROR;
    {
        float s = (float)blurWidth / 2;
        float weights[blurWidth * 2 + 1] = { 0 };
        float sum = 0.0f;
        for (int i = 0; i < blurWidth * 2 + 1; i++) {
            weights[i] = (float)exp(-0.5 * pow((i - blurWidth) / s, 2));
            sum += weights[i];
        }
        for (int i = 0; i < blurWidth * 2 + 1; i++) {
            weights[i] /= sum;
        }

        /////////////////////////////////////
        // Vertical AO Pass //
        /////////////////////////////////////

        AOProgramV->Use();
        programId = AOProgramV->programId;

        GLuint blockID;
        glGenBuffers(1, &blockID);
        bindpoint = 0;

        CHECKERROR;
        loc = glGetUniformBlockIndex(programId, "blurKernel");
        glUniformBlockBinding(programId, loc, bindpoint);

        glBindBuffer(GL_UNIFORM_BUFFER, blockID);
        glBindBufferBase(GL_UNIFORM_BUFFER, bindpoint, blockID);
        glBufferData(GL_UNIFORM_BUFFER, (blurWidth * 2 + 1) * sizeof(float), &weights, GL_STATIC_DRAW);
        CHECKERROR;

        loc = glGetUniformLocation(programId, "w");
        glUniform1i(loc, blurWidth);
        CHECKERROR;

        glActiveTexture(GL_TEXTURE1 + unit); // Activate texture unit 3
        glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[0]);
        loc = glGetUniformLocation(programId, "G0");
        glUniform1i(loc, unit + 1);

        glActiveTexture(GL_TEXTURE2 + unit); // Activate texture unit 4
        glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[1]);
        loc = glGetUniformLocation(programId, "G1");
        glUniform1i(loc, unit + 2);

        glActiveTexture(GL_TEXTURE3 + unit); // Activate texture unit 5
        glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[2]);
        loc = glGetUniformLocation(programId, "G2");
        glUniform1i(loc, unit + 3);

        glActiveTexture(GL_TEXTURE4 + unit); // Activate texture unit 6
        glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[3]);
        loc = glGetUniformLocation(programId, "G3");
        glUniform1i(loc, unit + 4);
        CHECKERROR;

        loc = glGetUniformLocation(programId, "screenWidth");
        glUniform1i(loc, width);

        loc = glGetUniformLocation(programId, "screenHeight");
        glUniform1i(loc, height);

        loc = glGetUniformLocation(programId, "eyePos");
        glUniform3fv(loc, 1, &(eye[0]));
        CHECKERROR;

        loc = glGetUniformLocation(programId, "src");
        glBindImageTexture(0, AOScalarMap->textureIDs[0], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
        glUniform1i(loc, 0);
        CHECKERROR;

        loc = glGetUniformLocation(programId, "dst");
        glBindImageTexture(1, AOTempMap->textureIDs[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glUniform1i(loc, 1);
        CHECKERROR;

        glDispatchCompute(fboWidth, (fboHeight / 128) + 1, 1);

        glUseProgram(0);
        CHECKERROR;

        /////////////////////////////////////
        // End of Vertical AO Pass //
        /////////////////////////////////////

        /////////////////////////////////////
        // Horizontal AO Pass //
        /////////////////////////////////////

        AOProgramH->Use();
        programId = AOProgramH->programId;

        CHECKERROR;
        loc = glGetUniformBlockIndex(programId, "blurKernel");
        glUniformBlockBinding(programId, loc, bindpoint);

        glBindBuffer(GL_UNIFORM_BUFFER, blockID);
        glBindBufferBase(GL_UNIFORM_BUFFER, bindpoint, blockID);
        glBufferData(GL_UNIFORM_BUFFER, (blurWidth * 2 + 1) * sizeof(float), &weights, GL_STATIC_DRAW);
        CHECKERROR;

        loc = glGetUniformLocation(programId, "w");
        glUniform1i(loc, blurWidth);
        CHECKERROR;

        glActiveTexture(GL_TEXTURE1 + unit); // Activate texture unit 3
        glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[0]);
        loc = glGetUniformLocation(programId, "G0");
        glUniform1i(loc, unit + 1);

        glActiveTexture(GL_TEXTURE2 + unit); // Activate texture unit 4
        glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[1]);
        loc = glGetUniformLocation(programId, "G1");
        glUniform1i(loc, unit + 2);

        glActiveTexture(GL_TEXTURE3 + unit); // Activate texture unit 5
        glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[2]);
        loc = glGetUniformLocation(programId, "G2");
        glUniform1i(loc, unit + 3);

        glActiveTexture(GL_TEXTURE4 + unit); // Activate texture unit 6
        glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[3]);
        loc = glGetUniformLocation(programId, "G3");
        glUniform1i(loc, unit + 4);
        CHECKERROR;

        loc = glGetUniformLocation(programId, "screenWidth");
        glUniform1i(loc, width);

        loc = glGetUniformLocation(programId, "screenHeight");
        glUniform1i(loc, height);

        loc = glGetUniformLocation(programId, "eyePos");
        glUniform3fv(loc, 1, &(eye[0]));
        CHECKERROR;

        loc = glGetUniformLocation(programId, "src");
        glBindImageTexture(0, AOTempMap->textureIDs[0], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
        glUniform1i(loc, 0);
        CHECKERROR;

        loc = glGetUniformLocation(programId, "dst");
        glBindImageTexture(1, AOScalarMap->textureIDs[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glUniform1i(loc, 1);
        CHECKERROR;

        glDispatchCompute((width / 128) + 1, height, 1);

        glUseProgram(0);
        CHECKERROR;

        /////////////////////////////////////
        // End of Horizontal AO Pass //
        /////////////////////////////////////

    }
    // End AO Compute Pass

    //////////////////////////////////////////////////////////////////////////////////
    //// Reflection pass #1
    //////////////////////////////////////////////////////////////////////////////////
    //reflectionProgram->Use();
    //reflectionTopMap->Bind();
    //programId = reflectionProgram->programId;

    //// Set the viewport, and clear the screen
    //glViewport(0, 0, reflectionTopMap->width, reflectionTopMap->height);
    //glClearColor(0.5, 0.5, 0.5, 1.0);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //// @@ The scene specific parameters (uniform variables) used by
    //// the shader are set here.  Object specific parameters are set in
    //// the Draw procedure in object.cpp

    //// Light & Ambient coloring & brightness
    //loc = glGetUniformLocation(programId, "Light");
    //glUniform3fv(loc, 1, &(Light[0]));

    //loc = glGetUniformLocation(programId, "Ambient");
    //glUniform3fv(loc, 1, &(Ambient[0]));

    //// default shader parameters
    //loc = glGetUniformLocation(programId, "WorldProj");
    //glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldProj));
    //loc = glGetUniformLocation(programId, "WorldView");
    //glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldView));
    //loc = glGetUniformLocation(programId, "WorldInverse");
    //glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldInverse));
    //loc = glGetUniformLocation(programId, "lightPos");
    //glUniform3fv(loc, 1, &(lightPos[0]));
    //loc = glGetUniformLocation(programId, "ShadowMatrix");
    //glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ShadowMatrix));
    //glActiveTexture(GL_TEXTURE2); // Activate texture unit 2
    //glBindTexture(GL_TEXTURE_2D, shadowMap->textureIDs[0]); // Load texture into it
    //loc = glGetUniformLocation(programId, "shadowMap");
    //glUniform1i(loc, 2); // Tell shader texture is in unit 2
    //loc = glGetUniformLocation(programId, "mode");
    //glUniform1i(loc, mode);
    //loc = glGetUniformLocation(programId, "topHalf");
    //glUniform1i(loc, 1);
    //CHECKERROR;

    //unit = 5;
    //skybox->Bind(unit, programId, "skyboxTexture");
    //// Draw all objects (This recursively traverses the object hierarchy.)
    //objectRoot->DrawWithoutTeapot(reflectionProgram, Identity);
    //CHECKERROR;
    //skybox->Unbind();

    //// Turn off the shader
    //reflectionTopMap->Unbind();
    //reflectionProgram->Unuse();
    //////////////////////////////////////////////////////////////////////////////////
    //// End of Reflection pass #1
    //////////////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////////////
    //// Reflection pass #2
    //////////////////////////////////////////////////////////////////////////////////
    //reflectionProgram->Use();
    //reflectionBottomMap->Bind();
    //programId = reflectionProgram->programId;

    //// Set the viewport, and clear the screen
    //glViewport(0, 0, reflectionBottomMap->width, reflectionBottomMap->height);
    //glClearColor(0.5, 0.5, 0.5, 1.0);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //// Build the ShadowMatrix
    //B = Translate(0.5, 0.5, 0.5) * Scale(0.5, 0.5, 0.5);
    //ShadowMatrix = B * pL * vL;

    //// @@ The scene specific parameters (uniform variables) used by
    //// the shader are set here.  Object specific parameters are set in
    //// the Draw procedure in object.cpp

    //// Light & Ambient coloring & brightness
    //loc = glGetUniformLocation(programId, "Light");
    //glUniform3fv(loc, 1, &(Light[0]));

    //loc = glGetUniformLocation(programId, "Ambient");
    //glUniform3fv(loc, 1, &(Ambient[0]));

    //// default shader parameters
    //loc = glGetUniformLocation(programId, "WorldProj");
    //glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldProj));
    //loc = glGetUniformLocation(programId, "WorldView");
    //glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldView));
    //loc = glGetUniformLocation(programId, "WorldInverse");
    //glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldInverse));
    //loc = glGetUniformLocation(programId, "lightPos");
    //glUniform3fv(loc, 1, &(lightPos[0]));
    //loc = glGetUniformLocation(programId, "ShadowMatrix");
    //glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ShadowMatrix));
    //glActiveTexture(GL_TEXTURE2); // Activate texture unit 2
    //glBindTexture(GL_TEXTURE_2D, shadowMap->textureIDs[0]); // Load texture into it
    //loc = glGetUniformLocation(programId, "shadowMap");
    //glUniform1i(loc, 2); // Tell shader texture is in unit 2
    //loc = glGetUniformLocation(programId, "mode");
    //glUniform1i(loc, mode);
    //loc = glGetUniformLocation(programId, "topHalf");
    //glUniform1i(loc, 0);
    //CHECKERROR;

    //unit = 5;
    //skybox->Bind(unit, programId, "skyboxTexture");
    //// Draw all objects (This recursively traverses the object hierarchy.)
    //objectRoot->DrawWithoutTeapot(reflectionProgram, Identity);
    //CHECKERROR;
    //skybox->Unbind();

    //// Turn off the shader
    //reflectionBottomMap->Unbind();
    //reflectionProgram->Unuse();
    //////////////////////////////////////////////////////////////////////////////////
    //// End of Reflection pass #2
    //////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    // Lighting pass
    ////////////////////////////////////////////////////////////////////////////////

    // Choose the lighting shader
    lightingProgram->Use();
    programId = lightingProgram->programId;

    // Set the viewport, and clear the screen
    glViewport(0, 0, width, height);
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT| GL_DEPTH_BUFFER_BIT);

    // Build the ShadowMatrix
    B = Translate(0.5, 0.5, 0.5) * Scale(0.5, 0.5, 0.5);
    ShadowMatrix = B * pL * vL;
    
    // @@ The scene specific parameters (uniform variables) used by
    // the shader are set here.  Object specific parameters are set in
    // the Draw procedure in object.cpp

    // Light & Ambient coloring & brightness
    loc = glGetUniformLocation(programId, "Light");
    glUniform3fv(loc, 1, &(Light[0]));

    loc = glGetUniformLocation(programId, "Ambient");
    glUniform3fv(loc, 1, &(Ambient[0]));

    // default shader parameters
    loc = glGetUniformLocation(programId, "WorldProj");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldProj));
    loc = glGetUniformLocation(programId, "WorldView");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldView));
    loc = glGetUniformLocation(programId, "WorldInverse");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldInverse));
    loc = glGetUniformLocation(programId, "lightPos");
    glUniform3fv(loc, 1, &(lightPos[0]));
    loc = glGetUniformLocation(programId, "eyePos");
    glUniform3fv(loc, 1, &(eye[0]));
    loc = glGetUniformLocation(programId, "ShadowMatrix");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ShadowMatrix));
    loc = glGetUniformLocation(programId, "screenWidth");
    glUniform1i(loc, width);
    loc = glGetUniformLocation(programId, "screenHeight");
    glUniform1i(loc, height);
    loc = glGetUniformLocation(programId, "objectId");
    glUniform1i(loc, objectRoot->objectId);


    glActiveTexture(GL_TEXTURE1 + unit); // Activate texture unit 2
    glBindTexture(GL_TEXTURE_2D, shadowMap->textureIDs[0]);
    loc = glGetUniformLocation(programId, "shadowMap");
    glUniform1i(loc, unit + 1);

    glActiveTexture(GL_TEXTURE2 + unit); // Activate texture unit 3
    glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[0]);
    loc = glGetUniformLocation(programId, "G0");
    glUniform1i(loc, unit + 2);

    glActiveTexture(GL_TEXTURE3 + unit); // Activate texture unit 4
    glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[1]);
    loc = glGetUniformLocation(programId, "G1");
    glUniform1i(loc, unit + 3);

    glActiveTexture(GL_TEXTURE4 + unit); // Activate texture unit 5
    glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[2]);
    loc = glGetUniformLocation(programId, "G2");
    glUniform1i(loc, unit + 4);

    glActiveTexture(GL_TEXTURE5 + unit); // Activate texture unit 6
    glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[3]);
    loc = glGetUniformLocation(programId, "G3");
    glUniform1i(loc, unit + 5);

    glActiveTexture(GL_TEXTURE6 + unit);
    glBindTexture(GL_TEXTURE_2D, AOScalarMap->textureIDs[0]);
    loc = glGetUniformLocation(programId, "AOMap");
    glUniform1i(loc, unit + 6);

    cloudsIBL->Bind(unit + 7, programId, "IBL");
    cloudsIRRIBL->Bind(unit + 8, programId, "IRRIBL");


    loc = glGetUniformLocation(programId, "iblWidth");
    glUniform1i(loc, cloudsIBL->width);

    loc = glGetUniformLocation(programId, "iblHeight");
    glUniform1i(loc, cloudsIBL->height);

    CHECKERROR;

    GLuint blockID;
    glGenBuffers(1, &blockID);
    bindpoint = 1; // Increment this for other blocks.

    loc = glGetUniformBlockIndex(programId, "HammersleyBlock");
    glUniformBlockBinding(programId, loc, bindpoint);
    glBindBufferBase(GL_UNIFORM_BUFFER, bindpoint, blockID);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(block), &block, GL_STATIC_DRAW);



    CHECKERROR;
 
    //unit = 5;
    //skybox->Bind(unit, programId, "skyboxTexture");
    //// Draw all objects (This recursively traverses the object hierarchy.)
    //objectRoot->Draw(lightingProgram, Identity);
    //CHECKERROR; 
    //skybox->Unbind();

    objectRoot->Draw(lightingProgram, Identity);

    // Turn off the shader
    lightingProgram->Unuse();
    ////////////////////////////////////////////////////////////////////////////////
    // End of Lighting pass
    ////////////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////////////
    //// Local Lights Pass
    //////////////////////////////////////////////////////////////////////////////////

    //// Choose the lighting shader
    //localLightProgram->Use();

    //CHECKERROR;
    //programId = localLightProgram->programId;

    //// Set the viewport, and clear the screen
    ////glViewport(0, 0, width, height);
    ////glClearColor(0.5, 0.5, 0.5, 1.0);
    ////glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    //// @@ The scene specific parameters (uniform variables) used by
    //// the shader are set here.  Object specific parameters are set in
    //// the Draw procedure in object.cpp

    //CHECKERROR;

    //loc = glGetUniformLocation(programId, "WorldProj");
    //glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldProj));
    //loc = glGetUniformLocation(programId, "WorldView");
    //glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldView));
    //loc = glGetUniformLocation(programId, "WorldInverse");
    //glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldInverse));
    //loc = glGetUniformLocation(programId, "eyePos");
    //glUniform3fv(loc, 1, &(eye[0]));
    //CHECKERROR;

    //loc = glGetUniformLocation(programId, "ShadowMatrix");
    //glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ShadowMatrix));

    //loc = glGetUniformLocation(programId, "Ambient");
    //glUniform3fv(loc, 1, &(Ambient[0]));

    //loc = glGetUniformLocation(programId, "screenWidth");
    //glUniform1i(loc, width);

    //loc = glGetUniformLocation(programId, "screenHeight");
    //glUniform1i(loc, height);
    //CHECKERROR;

    ////Repeat for each gBuffer FBO Texture
    //glActiveTexture(GL_TEXTURE1 + unit); // Activate texture unit 2
    //glBindTexture(GL_TEXTURE_2D, shadowMap->textureIDs[0]); // Load texture
    //loc = glGetUniformLocation(programId, "shadowMap");
    //glUniform1i(loc, unit + 1); // Tell shader texture location

    //glActiveTexture(GL_TEXTURE2 + unit); // Activate texture unit 3
    //glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[0]);
    //loc = glGetUniformLocation(programId, "G0");
    //glUniform1i(loc, unit + 2);

    //glActiveTexture(GL_TEXTURE3 + unit); // Activate texture unit 4
    //glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[1]);
    //loc = glGetUniformLocation(programId, "G1");
    //glUniform1i(loc, unit + 3);

    //glActiveTexture(GL_TEXTURE4 + unit); // Activate texture unit 5
    //glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[2]);
    //loc = glGetUniformLocation(programId, "G2");
    //glUniform1i(loc, unit + 4);

    //glActiveTexture(GL_TEXTURE5 + unit); // Activate texture unit 6
    //glBindTexture(GL_TEXTURE_2D, gBufferMap->textureIDs[3]);
    //loc = glGetUniformLocation(programId, "G3");
    //glUniform1i(loc, unit + 5);

    //glDisable(GL_DEPTH_TEST);
    //glEnable(GL_BLEND);
    //glBlendFunc(GL_ONE, GL_ONE);

    //Object* lightSphere = new Object(NULL, nullId);
    //CHECKERROR;

    //for (int i = 0; i < numLocalLights; i++) {
    //    loc = glGetUniformLocation(programId, "lightPos");
    //    glUniform3fv(loc, 1, &(localLightPositions[i][0]));
    //    CHECKERROR;

    //    loc = glGetUniformLocation(programId, "Light");
    //    glUniform3fv(loc, 1, &(localLightColors[i][0]));
    //    CHECKERROR;

    //    loc = glGetUniformLocation(programId, "lightRadius");
    //    glUniform1f(loc, localLightRadii[i]);
    //    CHECKERROR;

    //    lightSphere = new Object(NULL, nullId);
    //    lightSphere->add(sphere, Translate(localLightPositions[i].x, localLightPositions[i].y, localLightPositions[i].z) * Scale(localLightRadii[i], localLightRadii[i], localLightRadii[i]));
    //    lightSphere->Draw(localLightProgram, Identity);
    //}

    //glEnable(GL_DEPTH_TEST);
    //glDisable(GL_BLEND);
    //CHECKERROR;

    //// Turn off the shader
    //localLightProgram->Unuse();

    //////////////////////////////////////////////////////////////////////////////////
    //// End of Local Lights Pass
    //////////////////////////////////////////////////////////////////////////////////
}
