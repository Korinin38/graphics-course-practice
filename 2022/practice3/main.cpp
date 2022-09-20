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

const std::uint8_t RED[4] = {255, 0, 0, 255};
const std::uint8_t GREEN[4] = {0, 255, 0, 255};
const std::uint8_t BLUE[4] = {0, 0, 255, 255};
const std::uint8_t BLACK[4] = {0, 0, 0, 255};

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

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec4 in_color;
layout (location = 2) in float in_dist;

out vec4 color;
out float dist;

void main()
{
    gl_Position = view * vec4(in_position, 0.0, 1.0);
    color = in_color;
    dist = in_dist;
}
)";

const char fragment_shader_source[] =
        R"(#version 330 core

in vec4 color;

layout (location = 0) out vec4 out_color;
in float dist;
uniform int dash;
uniform float time;

void main()
{
    out_color = color;
    if (dash == 1 && mod(dist - time * 10, 40.0) < 20.0)
        discard;
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

struct vec2
{
    float x;
    float y;
};

struct vertex
{
    vec2 position;
    std::uint8_t color[4];

    void set_color(std::uint8_t const other[]) {
        for (int i = 0; i < 4; ++i) {
            color[i] = other[i];
        }
    }
};

vec2 bezier(std::vector<vertex> const & vertices, float t)
{
    std::vector<vec2> points(vertices.size());

    for (std::size_t i = 0; i < vertices.size(); ++i)
        points[i] = vertices[i].position;

    // De Casteljau's algorithm
    for (std::size_t k = 0; k + 1 < vertices.size(); ++k) {
        for (std::size_t i = 0; i + k + 1 < vertices.size(); ++i) {
            points[i].x = points[i].x * (1.f - t) + points[i + 1].x * t;
            points[i].y = points[i].y * (1.f - t) + points[i + 1].y * t;
        }
    }
    return points[0];
}

void count_bez(std::vector<vertex> &bez, const std::vector<vertex> &vertices, int quality) {
    bez.resize((vertices.size() - 1) * quality + 1);

    for (int i = 0; i < bez.size(); ++i) {
        float t = i * 1.f / (bez.size() - 1);
//        std::cout << t << std::endl;
        bez[i].position = bezier(vertices, t);
//        switch (i % 3) {
//            case 0:
//                bez[i].set_color(RED);
//                break;
//            case 1:
//                bez[i].set_color(GREEN);
//                break;
//            case 2:
//                bez[i].set_color(BLUE);
//                break;
//        }
                bez[i].set_color(BLACK);
//        std::cout << bez[i].position.x << " " << bez[i].position.y << std::endl;
    }
}

void count_dist(std::vector<float> &dist, const std::vector<vertex> &bez) {
    dist.resize(bez.size());
    dist[0] = 0;
    for (int i = 1; i < dist.size(); ++i) {
        dist[i] = dist[i - 1] + std::hypotf(bez[i].position.x - bez[i - 1].position.x,
                                            bez[i].position.y - bez[i - 1].position.y);
        std::cout << dist[i] - dist[i-1] << " ";
    }
    std::cout << std::endl;
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

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 3",
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

    SDL_GL_SetSwapInterval(0);

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);


    int quality = 4;

    vertex _v[3] = {
        {{width * 1.f, height / 2.f}, {255, 0, 0, 255}},
        {{width / 2.f, height / 2.f}, {0, 255, 0, 255}},
        {{width / 2.f, height * 1.f},  {0, 0, 255, 255}},
    };
    std::vector<vertex> v (_v, _v + 3);
    bool draw_points = true;

    GLuint points_vbo;
    glGenBuffers(1, &points_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(vertex), v.data(), GL_STATIC_DRAW);

    float test;
    glGetBufferSubData(GL_ARRAY_BUFFER, sizeof(vertex) * 2 + sizeof(float), sizeof(float), &test);
    std::cout << test << std::endl;

    GLuint points_vao;
    glGenVertexArrays(1, &points_vao);
    glBindVertexArray(points_vao);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void*)(8));

    std::vector<vertex> bez((v.size() - 1) * quality + 1);
    count_bez(bez, v, quality);

    GLuint bezier_vao;
    glGenVertexArrays(1, &bezier_vao);
    glBindVertexArray(bezier_vao);

    GLuint bezier_vbo;
    glGenBuffers(1, &bezier_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, bezier_vbo);
    glBufferData(GL_ARRAY_BUFFER, bez.size() * sizeof(vertex), bez.data(), GL_DYNAMIC_DRAW);

    std::vector<float> dist(bez.size());
    count_dist(dist, bez);
    GLuint dist_vbo;
    glGenBuffers(1, &dist_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, dist_vbo);
    glBufferData(GL_ARRAY_BUFFER, dist.size() * sizeof(float), dist.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, bezier_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void*)(8));

    glBindBuffer(GL_ARRAY_BUFFER, dist_vbo);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, (void*)(0));

    GLint view_location = glGetUniformLocation(program, "view");
    GLint dash = glGetUniformLocation(program, "dash");
    GLint dash_pos = glGetUniformLocation(program, "time");

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

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
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT)
                    {
                        int mouse_x = event.button.x;
                        int mouse_y = event.button.y;

                        vertex nw = {1.f * mouse_x, 1.f * (height - mouse_y)};
                        switch (v.size() % 3) {
                            case 0:
                                nw.set_color(RED);
                                break;
                            case 1:
                                nw.set_color(GREEN);
                                break;
                            case 2:
                                nw.set_color(BLUE);
                                break;
                        }
                        v.push_back(nw);

                        if (v.size() >= 3) {
                            count_bez(bez, v, quality);
                            count_dist(dist, bez);
                        }

                        glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
                        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(vertex), v.data(), GL_DYNAMIC_DRAW);
                        glBindBuffer(GL_ARRAY_BUFFER, bezier_vbo);
                        glBufferData(GL_ARRAY_BUFFER, bez.size() * sizeof(vertex), bez.data(), GL_DYNAMIC_DRAW);
                        glBindBuffer(GL_ARRAY_BUFFER, dist_vbo);
                        glBufferData(GL_ARRAY_BUFFER, dist.size() * sizeof(float), dist.data(), GL_DYNAMIC_DRAW);
//                        glEnableVertexAttribArray(0);
//                        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(0));
//
//                        glEnableVertexAttribArray(1);
//                        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void*)(8));
                    }
                    else if (event.button.button == SDL_BUTTON_RIGHT)
                    {
                        if (v.size() > 0)
                            v.pop_back();
                        if (v.size() >= 3) {
                            count_bez(bez, v, quality);
                            count_dist(dist, bez);
                        }

                        glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
                        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(vertex), v.data(), GL_DYNAMIC_DRAW);
                        glBindBuffer(GL_ARRAY_BUFFER, bezier_vbo);
                        glBufferData(GL_ARRAY_BUFFER, bez.size() * sizeof(vertex), bez.data(), GL_DYNAMIC_DRAW);
                        glBindBuffer(GL_ARRAY_BUFFER, dist_vbo);
                        glBufferData(GL_ARRAY_BUFFER, dist.size() * sizeof(float), dist.data(), GL_DYNAMIC_DRAW);
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_LEFT)
                    {
                        if (quality > 1) {
                            quality -= 1;
                            if (v.size() >= 3) {
                                count_bez(bez, v, quality);
                                count_dist(dist, bez);
                                glBindBuffer(GL_ARRAY_BUFFER, bezier_vbo);
                                glBufferData(GL_ARRAY_BUFFER, bez.size() * sizeof(vertex), bez.data(), GL_DYNAMIC_DRAW);
                                glBindBuffer(GL_ARRAY_BUFFER, dist_vbo);
                                glBufferData(GL_ARRAY_BUFFER, dist.size() * sizeof(float), dist.data(), GL_DYNAMIC_DRAW);
                            }
                        }
                    }
                    else if (event.key.keysym.sym == SDLK_RIGHT)
                    {
                        quality += 1;
                        if (v.size() >= 3) {
                            count_bez(bez, v, quality);
                            count_dist(dist, bez);
                            glBindBuffer(GL_ARRAY_BUFFER, bezier_vbo);
                            glBufferData(GL_ARRAY_BUFFER, bez.size() * sizeof(vertex), bez.data(), GL_DYNAMIC_DRAW);
                            glBindBuffer(GL_ARRAY_BUFFER, dist_vbo);
                            glBufferData(GL_ARRAY_BUFFER, dist.size() * sizeof(float), dist.data(), GL_DYNAMIC_DRAW);
                        }
                    }
                    else if (event.key.keysym.sym == SDLK_b) {
                        draw_points ^= true;
                    }
                    break;
            }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        glClear(GL_COLOR_BUFFER_BIT);

        float view[16] =
                {
                        2.f / width, 0.f, 0.f, -1.f,
                        0.f, 2.f / height, 0.f, -1.f,
                        0.f, 0.f, 1.f, 0.f,
                        0.f, 0.f, 0.f, 1.f,
                };

        glUseProgram(program);
        if (draw_points) {
            glUniform1i(dash, 0);
            glBindVertexArray(points_vao);
            glPointSize(10);
            glLineWidth(5.f);
            glDrawArrays(GL_LINE_STRIP, 0, v.size());
            glDrawArrays(GL_POINTS, 0, v.size());
        }

        if (v.size() >= 3) {
            glUniform1i(dash, 1);
            glBindVertexArray(bezier_vao);
            glLineWidth(3.f);
            glDrawArrays(GL_LINE_STRIP, 0, bez.size());
        }
        glUniform1f(dash_pos, time);
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);

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
