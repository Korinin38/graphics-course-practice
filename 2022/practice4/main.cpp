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
#include <cmath>
#include <map>

#include "obj_parser.hpp"

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

uniform mat4 view;
uniform mat4 transform;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;

out vec3 normal;

void main()
{
    gl_Position = view * transform * 0.01 * vec4(in_position, 1.0);
    normal = mat3(transform) * in_normal;
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

in vec3 normal;

layout (location = 0) out vec4 out_color;

void main()
{
    float lightness = 0.5 + 0.5 * dot(normalize(normal), normalize(vec3(1.0, 2.0, 3.0)));
    out_color = vec4(vec3(lightness), 1.0);
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

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 4",
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

//    glClearColor(0.8f, 0.8f, 1.f, 0.f);
    glClearColor(0.3f, 0.3f, 0.3f, 0.f);

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    GLuint view_location = glGetUniformLocation(program, "view");
    GLuint transform_location = glGetUniformLocation(program, "transform");

    std::string project_root = PROJECT_ROOT;
    obj_data bunny = parse_obj(project_root + "/17498_Octagonal_Lighthouse_v1_NEW.obj");

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    GLuint points_vao;
    glGenVertexArrays(1, &points_vao);
    glBindVertexArray(points_vao);

    GLuint points_vbo;
    glGenBuffers(1, &points_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
    glBufferData(GL_ARRAY_BUFFER, bunny.vertices.size() * sizeof(obj_data::vertex), bunny.vertices.data(), GL_STATIC_DRAW);

    GLuint points_ebo;
    glGenBuffers(1, &points_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, points_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, bunny.indices.size() * sizeof(std::uint32_t), bunny.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void*)(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void*)(12));

    float time = 0.f;

    float near = 0.0001f;
    float far = 1000.f;
    float fov = M_PI_4;
    float aspect_ratio = 1.0f * width / height;
    float right = near * tan(fov);
    float top = right / aspect_ratio;

    float bunny_x = 0;
    float bunny_y = 0;
    float speed = .6f;

    std::map<SDL_Keycode, bool> button_down;

    bool running = true;
    while (running)
    {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
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

        bunny_x += speed * dt * (1.f * button_down[SDLK_RIGHT] - 1.f * button_down[SDLK_LEFT]);
        bunny_y += speed * dt * (1.f * button_down[SDLK_UP] - 1.f * button_down[SDLK_DOWN]);

        if (!running)
            break;

        last_frame_start = now;
        time += dt;

        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        float angle = time;
        float scale = 0.7f;


        float view[16] =
        {
            near / right, 0.f, 0.f, 0.f,
            0.f, near/top, 0.f, 0.f,
            0.f, 0.f, -(far + near) / (far - near), - 2 * (far * near) / (far - near),
            0.f, 0.f, -1.f, 0.f,
        };

        float transform[16] =
        {
            cos(angle) * scale, 0.f, sin(angle) * scale, bunny_x,
            0.f, scale, 0.f, bunny_y,
            -sin(angle) * scale, 0.f, cos(angle) * scale, -2.f,
            0.f, 0.f, 0.f, 1.f,
        };

        glUseProgram(program);
        glBindVertexArray(points_vao);

        // Bunny 1 - XZ
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);
        glUniformMatrix4fv(transform_location, 1, GL_TRUE, transform);
        glDrawElements(GL_TRIANGLES, bunny.indices.size(), GL_UNSIGNED_INT, (void*)(0));

        // Bunny 2 - XY
        glCullFace(GL_FRONT);
        float transform2[16] =
                {
                        cos(angle) * scale, sin(angle) * scale, 0.f, -1.1f + bunny_x,
                        -sin(angle) * scale, cos(angle) * scale, 0.f, -0.3f + bunny_y,
                        0.f, 0.f, scale, -2.f,
                        0.f, 0.f, 0.f, 1.f,
                };

        glUniformMatrix4fv(transform_location, 1, GL_TRUE, transform2);
        glDrawElements(GL_TRIANGLES, bunny.indices.size(), GL_UNSIGNED_INT, (void*)(0));

        // Bunny 3 - YZ
        glCullFace(GL_BACK);
        float transform3[16] =
                {
                        scale, 0.f, 0.f, 0.9f + bunny_x,
                        0.f, cos(angle) * scale, -sin(angle) * scale, -0.8f + bunny_y,
                        0.f, sin(angle) * scale, cos(angle) * scale, -2.f,
                        0.f, 0.f, 0.f, 1.f,
                };

        glUniformMatrix4fv(transform_location, 1, GL_TRUE, transform3);
        glDrawElements(GL_TRIANGLES, bunny.indices.size(), GL_UNSIGNED_INT, (void*)(0));

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
