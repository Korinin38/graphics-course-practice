#include "main.h"
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
#include <fstream>
#include <chrono>
#include <vector>
#include <map>

std::string to_string(std::string_view str) {
    return {str.begin(), str.end()};
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char*>(glewGetErrorString(error)));
}

int main() try {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window* window = SDL_CreateWindow("Graphics course practice 3",
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

    glClearColor(1.f, 1.f, 1.f, 0.f);
//    glClearColor(.2f, .3f, 0.8f, 0.f);

    std::string iso_v_shader_source = readFile("shaders/iso.vert");
    std::string iso_frag_shader_source = readFile("shaders/iso.frag");
    auto iso_program = create_program(
            create_shader(GL_VERTEX_SHADER, iso_v_shader_source.data()),
            create_shader(GL_FRAGMENT_SHADER, iso_frag_shader_source.data()));

    std::string grid_v_shader_source = readFile("shaders/grid.vert");
    std::string grid_frag_shader_source = readFile("shaders/grid.frag");

    auto grid_program = create_program(
            create_shader(GL_VERTEX_SHADER, grid_v_shader_source.data()),
            create_shader(GL_FRAGMENT_SHADER, grid_frag_shader_source.data()));

    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(config.PRIMITIVE_RESTART_INDEX);

    float time = 0.f;

    // Number of points calculated in every row, as well as the number of said rows
    // Total number of points calculated = grid_density ^ 2

    GLint view_location = glGetUniformLocation(grid_program, "view");
    GLint view_location_iso = glGetUniformLocation(iso_program, "view");
    GLint value_limit = glGetUniformLocation(grid_program, "max_value");

    std::vector<float> values((config.W() + 1) * (config.H() + 1));
    std::vector<vec2> pos((config.W() + 1) * (config.H() + 1));
    std::vector<std::vector<vec2>> isolines(config.isolines());
    std::vector<std::vector<std::uint32_t>> iso_indices(config.isolines());
    std::vector<std::uint32_t> indices(10);
    bool scale_up = true;
    calculate(values, isolines, 0, f);
    place_grid(pos, width, height);
    set_indices(indices);
    set_isolines(isolines, iso_indices, values, width, height, scale_up);


    GLuint grid_vao, grid_pos_vbo, grid_val_vbo, grid_ebo;
    glGenVertexArrays(1, &grid_vao);
    glGenBuffers(1, &grid_pos_vbo);
    glGenBuffers(1, &grid_val_vbo);
    glGenBuffers(1, &grid_ebo);
    set_buffers_grid(grid_vao, grid_pos_vbo, grid_val_vbo, grid_ebo, pos, values, indices);

    auto last_frame_start = std::chrono::high_resolution_clock::now();


    GLuint iso_vao, iso_vbo[config.MAX_ISOLINES], iso_ebo[config.MAX_ISOLINES];

    glGenVertexArrays(1, &iso_vao);
    glGenBuffers(config.MAX_ISOLINES, iso_vbo);
    glGenBuffers(config.MAX_ISOLINES, iso_ebo);
    set_buffers_iso(iso_vao, iso_vbo, iso_ebo, isolines, iso_indices);
    vec2 test = {0, 0};
    for (int i = 0; i < 8; ++i) {
        std::cout << isolines[0][i].x << " " << isolines[0][i].y << ": ";
        glGetBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * (2 * i), sizeof(float), &test.x);
        glGetBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * (2 * i) + sizeof(float), sizeof(float), &test.y);

        std::cout << test.x << " " << test.y << std::endl;
    }


    std::map<SDL_Keycode, bool> button_down;
    bool update_pos = true;
    bool update_quality = true;
    bool hold_b = false;

    bool running = true;
    while (running) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();

        for (SDL_Event event; SDL_PollEvent(&event);)
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                            width = event.window.data1;
                            height = event.window.data2;
                            glViewport(0, 0, width, height);
                            update_pos = true;
                            break;
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {}
                    else if (event.button.button == SDL_BUTTON_RIGHT) {}
                    break;
                case SDL_KEYDOWN:
                    button_down[event.key.keysym.sym] = true;
                    break;
                case SDL_KEYUP:
                    button_down[event.key.keysym.sym] = false;
                    break;
            }
        primitive_button_handler(button_down, dt, update_pos, update_quality, scale_up, hold_b);

        if (!running)
            break;

        last_frame_start = now;
        time += dt;

        calculate(values, isolines, time, f);
        glBindBuffer(GL_ARRAY_BUFFER, grid_val_vbo);
        glBufferData(GL_ARRAY_BUFFER, values.size() * sizeof(float), values.data(), GL_STREAM_DRAW);
        if (update_pos) {
            update_pos = false;
            place_grid(pos, width, height, scale_up);
            set_isolines(isolines, iso_indices, values, width, height, scale_up);

            glBindBuffer(GL_ARRAY_BUFFER, grid_pos_vbo);
            glBufferData(GL_ARRAY_BUFFER, pos.size() * sizeof(vec2), pos.data(), GL_DYNAMIC_DRAW);
            set_buffers_iso(iso_vao, iso_vbo, iso_ebo, isolines, iso_indices);
        }
        if (update_quality) {
            update_quality = false;
            set_indices(indices);
            set_isolines(isolines, iso_indices, values, width, height, scale_up);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grid_ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(),
                         GL_DYNAMIC_DRAW);
            set_buffers_iso(iso_vao, iso_vbo, iso_ebo, isolines, iso_indices);
        }

        glClear(GL_COLOR_BUFFER_BIT);

        float view[16] =
                {
                        2.f / width, 0.f, 0.f, -1.f,
                        0.f, 2.f / height, 0.f, -1.f,
                        0.f, 0.f, 1.f, 0.f,
                        0.f, 0.f, 0.f, 1.f,
                };


        glUseProgram(grid_program);
        glBindVertexArray(grid_vao);
        glLineWidth(1.0f);
        glDrawElements(GL_TRIANGLE_STRIP, indices.size(), GL_UNSIGNED_INT, (void*) (0));
//        glDrawElements(GL_LINE_STRIP, indices.size(), GL_UNSIGNED_INT, (void*) (0));

        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);
        glUniform1f(value_limit, config.MAX_VALUE);

        glUseProgram(iso_program);
        for (int i = 0; i < isolines.size(); ++i) {
            glBindVertexArray(iso_vao);
            glBindBuffer(GL_ARRAY_BUFFER, iso_vbo[i]);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iso_ebo[i]);

            glLineWidth(i ? 1.0f : 4.0f);
            glDrawElements(GL_LINE_STRIP, iso_indices[i].size(), GL_UNSIGNED_INT, (void*) (0));
        }

        glUniformMatrix4fv(view_location_iso, 1, GL_TRUE, view);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
