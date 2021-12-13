// All the includes neeed across several cpp files:
//   OpenGL, GLFW, GLM, etc

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>
using namespace gl;

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>

#include "shader.h"
#include "scene.h"
#include "interact.h"
