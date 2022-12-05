#ifndef HOMEWORK3_GRAPHIC_OBJECT_H
#define HOMEWORK3_GRAPHIC_OBJECT_H
#ifdef WIN32
#include <SDL.h>
#undef main
#else

#include <SDL2/SDL.h>

#endif

#include <GL/glew.h>

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>Н. печатает
5 новых сообщений
5
￼
15:08

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/string_cast.hpp>

class GraphicObject {
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    glm::mat4 model;
    int texture_unit;
public:
    void Init();
    void Draw(float dt, glm::mat4 view);
};

#endif //HOMEWORK3_GRAPHIC_OBJECT_H
