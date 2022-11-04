#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <cmath>

#include "obj_parser.hpp"
#include "stb_image.h"

std::string to_string(std::string_view str)
{
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message)
{
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error)
{
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 transform;
uniform mat4 projection;
uniform float time;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texcoord;

out vec3 normal;
out vec2 texcoord;

void main()
{
    gl_Position = projection * transform * vec4(in_position, 1.0);
    normal = mat3(transform) * in_normal;
    texcoord = vec2(in_texcoord.x + time, in_texcoord.y);
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform sampler2D col;

in vec3 normal;
in vec2 texcoord;

layout (location = 0) out vec4 out_color;

void main()
{
    float lightness = 0.5 + 0.5 * dot(normalize(normal), normalize(vec3(1.0, 2.0, 3.0)));
//    vec3 albedo = vec3(texcoord, 0.8);
    vec4 albedo = texture(col, texcoord);
    out_color = vec4(lightness * albedo);
}
)";

GLuint create_shader(GLenum type, const char * source)
{
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader)
{
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

typedef struct texture_t {
    int size;
    int parts;
    std::vector<uint32_t > pixels;

    explicit texture_t(int _size) {
        size = _size;
        parts = size;
        pixels.resize(size * size);
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                pixels[i * size + j] = ((i / (size / parts)) % 2) == ((j / (size / parts)) % 2) ? 0xFF000000u : 0xFFFFFFFFu;
            }
        }
    }
} texture_t;

int main() try
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 5",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);

//    glEnable(GL_DEPTH_TEST);
//    glEnable(GL_CULL_FACE);

    GLuint transform_location = glGetUniformLocation(program, "transform");
    GLuint projection_location = glGetUniformLocation(program, "projection");
    GLuint texture_color = glGetUniformLocation(program, "col");
    GLuint tex_time = glGetUniformLocation(program, "time");

    std::string project_root = PROJECT_ROOT;
    std::string cow_texture_path = project_root + "/cow.png";
    obj_data cow = parse_obj(project_root + "/cow.obj");

    GLuint points_vao;
    glGenVertexArrays(1, &points_vao);
    glBindVertexArray(points_vao);

    GLuint points_vbo;
    glGenBuffers(1, &points_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
    glBufferData(GL_ARRAY_BUFFER, cow.vertices.size() * sizeof(obj_data::vertex), cow.vertices.data(), GL_STATIC_DRAW);

    GLuint points_ebo;
    glGenBuffers(1, &points_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, points_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, cow.indices.size() * sizeof(std::uint32_t), cow.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void*)(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void*)(12));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void*)(24));

    GLuint checkers_texture;
    glGenTextures(1, &checkers_texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, checkers_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    texture_t checkers(512);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, checkers.size, checkers.size, 0, GL_RGBA, GL_UNSIGNED_BYTE, checkers.pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, checkers.size / 2, checkers.size / 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 std::vector<uint32_t>(checkers.size * checkers.size / 4, 0xFF0000FFu).data());
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, checkers.size / 4, checkers.size / 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 std::vector<uint32_t>(checkers.size * checkers.size / 16, 0xFF00FF00u).data());
    glTexImage2D(GL_TEXTURE_2D, 3, GL_RGBA8, checkers.size / 8, checkers.size / 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 std::vector<uint32_t>(checkers.size * checkers.size / 64, 0xFFFF0000u).data());

    GLuint real_texture;
    glGenTextures(1, &real_texture);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, real_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    int x, y, n;
    unsigned char* texture_data = stbi_load(cow_texture_path.c_str(), &x, &y, &n, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, texture_data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(texture_data);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    float angle_y = M_PI;
    float offset_z = -2.f;

    std::map<SDL_Keycode, bool> button_down;

    int which_is_to_draw = 1;

    bool running = true;
    while (running)
    {
        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_WINDOWEVENT: switch (event.window.event)
            {
            case SDL_WINDOWEVENT_RESIZED:
                width = event.window.data1;
                height = event.window.data2;
                glViewport(0, 0, width, height);
                break;
            }
            break;
        case SDL_KEYDOWN:
            button_down[event.key.keysym.sym] = true;
            break;
        case SDL_KEYUP:
            button_down[event.key.keysym.sym] = false;
            break;
        }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        if (button_down[SDLK_UP]) offset_z -= 4.f * dt;
        if (button_down[SDLK_DOWN]) offset_z += 4.f * dt;
        if (button_down[SDLK_LEFT]) angle_y += 4.f * dt;
        if (button_down[SDLK_RIGHT]) angle_y -= 4.f * dt;
        if (button_down[SDLK_1]) which_is_to_draw = 0;
        if (button_down[SDLK_2]) which_is_to_draw = 1;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        float near = 0.1f;
        float far = 100.f;
        float top = near;
        float right = (top * width) / height;

        float transform[16] =
        {
            std::cos(angle_y), 0.f, -std::sin(angle_y), 0.f,
            0.f, 1.f, 0.f, 0.f,
            std::sin(angle_y), 0.f, std::cos(angle_y), offset_z,
            0.f, 0.f, 0.f, 1.f,
        };

        float projection[16] =
        {
            near / right, 0.f, 0.f, 0.f,
            0.f, near / top, 0.f, 0.f,
            0.f, 0.f, - (far + near) / (far - near), - 2.f * far * near / (far - near),
            0.f, 0.f, -1.f, 0.f,
        };

        glUseProgram(program);
        glBindVertexArray(points_vao);
        glDrawElements(GL_TRIANGLES, cow.indices.size(), GL_UNSIGNED_INT, (void*)(0));
        glUniformMatrix4fv(transform_location, 1, GL_TRUE, transform);
        glUniformMatrix4fv(projection_location, 1, GL_TRUE, projection);
        glUniform1i(texture_color, which_is_to_draw);
        glUniform1f(tex_time, time / 8);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
